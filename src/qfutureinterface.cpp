// MIT License - see LICENSE
#include <QtCore/qfutureinterface.h>

#include "qfutureinterface_p.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qthreadpool.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <vector>

// The shared state of a computation. Everything but `state` and `pool` is guarded by `mutex`; `state` is
// written under the mutex, so that waiters see every change, and may be read without it.
class QFutureInterfaceBasePrivate {
public:
    struct Item {
        void *ptr;
        int count;
        bool isVector;
    };

    explicit QFutureInterfaceBasePrivate(int initial) : state(initial) {}
    ~QFutureInterfaceBasePrivate() {
        for (auto &entry : items)
            deleter(entry.second.ptr, entry.second.isVector);
    }
    QFutureInterfaceBasePrivate(const QFutureInterfaceBasePrivate &) = delete;
    QFutureInterfaceBasePrivate &operator=(const QFutureInterfaceBasePrivate &) = delete;

    // The caller holds the lock.
    void sendCallOut(const QFutureCallOutEvent &event) const {
        for (QFutureCallOutInterface *out : outputs)
            out->callOutInterfaceEvent(event);
    }
    const Item *find(int index, int *offset) const {
        auto it = items.upper_bound(index);
        if (it == items.begin())
            return nullptr;
        --it;
        if (index >= it->first + it->second.count)
            return nullptr;
        *offset = index - it->first;
        return &it->second;
    }

    std::mutex mutex;
    std::condition_variable waitCond;
    std::condition_variable resumeCond;
    std::atomic<int> state;
    std::atomic<QThreadPool *> pool{nullptr};
    bool valid = true;
    int progressMin = 0;
    int progressMax = 0;
    int progressValue = 0;
    QString progressText;
    int expected = 0;
    std::map<int, Item> items;
    int contiguous = 0;
    int nextIndex = 0;
    QtPrivate::ResultDeleter deleter = nullptr;
    std::exception_ptr exception;
    std::vector<QFutureCallOutInterface *> outputs;
    std::function<void(const QFutureInterfaceBase &)> continuation;
    bool continuationSet = false;
    std::weak_ptr<QFutureInterfaceBasePrivate> parent;
};

namespace {

using Lock = std::unique_lock<std::mutex>;
constexpr int kSuspendFlags = QFutureInterfaceBase::Suspending | QFutureInterfaceBase::Suspended;

} // namespace

QFutureCallOutEvent::QFutureCallOutEvent(CallOutType type, int i1, int i2, QString txt)
    : QEvent(eventType()), callOutType(type), index1(i1), index2(i2), text(std::move(txt)) {}

QFutureCallOutEvent *QFutureCallOutEvent::clone() const { return new QFutureCallOutEvent(callOutType, index1, index2, text); }

QEvent::Type QFutureCallOutEvent::eventType() {
    static const QEvent::Type type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return type;
}

QFutureCallOutInterface::~QFutureCallOutInterface() = default;

namespace QtPrivate {

bool FutureAccess::setContinuation(const QFutureInterfaceBase &parent, std::function<void(const QFutureInterfaceBase &)> fn) {
    auto &d = *parent.d;
    {
        Lock lk(d.mutex);
        if (d.continuationSet)
            return false;
        d.continuationSet = true;
        if (!(d.state.load() & QFutureInterfaceBase::Finished)) {
            d.continuation = std::move(fn);
            return true;
        }
    }
    fn(parent);
    return true;
}

void FutureAccess::setParent(const QFutureInterfaceBase &child, const QFutureInterfaceBase &parent) {
    Lock lk(child.d->mutex);
    child.d->parent = parent.d;
}

std::exception_ptr FutureAccess::exception(const QFutureInterfaceBase &iface) {
    Lock lk(iface.d->mutex);
    return iface.d->exception;
}

void FutureAccess::markInvalid(const QFutureInterfaceBase &iface) {
    Lock lk(iface.d->mutex);
    iface.d->valid = false;
}

void FutureAccess::connectOutput(const QFutureInterfaceBase &iface, QFutureCallOutInterface *out) {
    auto &d = *iface.d;
    Lock lk(d.mutex);
    // Bring the observer up to date, as if it had been there all along.
    const int s = d.state.load();
    if (s & QFutureInterfaceBase::Started)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::Started));
    if (s & kSuspendFlags)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::Suspending));
    if (s & QFutureInterfaceBase::Suspended)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::Suspended));
    if (d.progressMin != 0 || d.progressMax != 0)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::ProgressRange, d.progressMin, d.progressMax));
    if (d.progressValue != 0 || !d.progressText.isEmpty())
        out->callOutInterfaceEvent(
            QFutureCallOutEvent(QFutureCallOutEvent::Progress, d.progressValue, 0, d.progressText));
    if (d.contiguous > 0)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::ResultsReady, 0, d.contiguous));
    if (s & QFutureInterfaceBase::Canceled)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::Canceled));
    if (s & QFutureInterfaceBase::Finished)
        out->callOutInterfaceEvent(QFutureCallOutEvent(QFutureCallOutEvent::Finished));
    d.outputs.push_back(out);
}

void FutureAccess::disconnectOutput(const QFutureInterfaceBase &iface, QFutureCallOutInterface *out) {
    Lock lk(iface.d->mutex);
    auto &v = iface.d->outputs;
    v.erase(std::remove(v.begin(), v.end(), out), v.end());
}

void FutureAccess::setThrottledFromCallOut(const QFutureInterfaceBase &iface, bool enable) {
    const int s = iface.d->state.load();
    iface.d->state = enable ? (s | QFutureInterfaceBase::Throttled) : (s & ~QFutureInterfaceBase::Throttled);
}

bool FutureAccess::sameState(const QFutureInterfaceBase &a, const QFutureInterfaceBase &b) { return a.d == b.d; }

} // namespace QtPrivate

/*!
    \class QFutureInterfaceBase
    \brief The QFutureInterfaceBase class is the non-template base of QFutureInterface.

    It holds the state that a QFuture and the QPromise or QFutureInterface that
    produces it share: the state flags, the progress, the exception and the
    bookkeeping of the results. The state is reference counted, so copies of the
    object refer to the same computation, and every function is thread-safe.

    There is rarely a reason to use the class directly; see QFutureInterface,
    QPromise and QFuture.

    \differences
    There is no \c mutex(), no \c setRunnable(), no \c setFilterMode() and no
    \c reportResultsReady(); the results are stored by the functions of
    QFutureInterface. Waiting functions are \c const, and progress changes are not
    limited to 25 per second. Results that arrive out of order are available
    through isResultReadyAt() at once, but resultCount() and the notification
    of a QFutureWatcher wait for the results before them.
    \enddifferences

    \sa QFutureInterface, QFuture, QPromise
*/

/*!
    Constructs an interface in the state \a initialState.
*/
QFutureInterfaceBase::QFutureInterfaceBase(State initialState)
    : d(std::make_shared<QFutureInterfaceBasePrivate>(initialState)) {}

QFutureInterfaceBase::QFutureInterfaceBase(std::shared_ptr<QFutureInterfaceBasePrivate> dd) : d(std::move(dd)) {}

/*!
    Constructs an interface that shares its state with \a other.
*/
QFutureInterfaceBase::QFutureInterfaceBase(const QFutureInterfaceBase &other) = default;

/*!
    Moves the state of \a other into this interface. \a other can only be destroyed or assigned to afterwards.
*/
QFutureInterfaceBase::QFutureInterfaceBase(QFutureInterfaceBase &&other) noexcept = default;

/*!
    Shares the state of \a other with this interface and returns a reference to it.
*/
QFutureInterfaceBase &QFutureInterfaceBase::operator=(const QFutureInterfaceBase &other) = default;

/*!
    Moves the state of \a other into this interface and returns a reference to it.
*/
QFutureInterfaceBase &QFutureInterfaceBase::operator=(QFutureInterfaceBase &&other) noexcept = default;

/*!
    Destroys the interface. The results are freed when the last interface or future that shares the state is destroyed.
*/
QFutureInterfaceBase::~QFutureInterfaceBase() = default;

/*!
    Returns \c true if the interface refers to a computation; returns \c false for the interface of a
    default-constructed QFuture and for an interface that was moved from.
*/
bool QFutureInterfaceBase::isValid() const {
    if (!d)
        return false;
    Lock lk(d->mutex);
    return d->valid;
}

/*!
    Returns \c true if the computation has been started. The answer stays \c true after it has finished.

    \sa reportStarted(), isRunning()
*/
bool QFutureInterfaceBase::isStarted() const { return queryState(Started); }

/*!
    Returns \c true if the computation has started and not finished.

    \sa isStarted(), isFinished()
*/
bool QFutureInterfaceBase::isRunning() const { return queryState(Running); }

/*!
    Returns \c true if the computation has finished.

    \sa reportFinished()
*/
bool QFutureInterfaceBase::isFinished() const { return queryState(Finished); }

/*!
    Returns \c true if the computation was canceled or ended with an exception.

    \sa cancel(), reportException()
*/
bool QFutureInterfaceBase::isCanceled() const { return queryState(Canceled); }

/*!
    Returns \c true if a suspension was requested and the computation has not acted on it yet.

    \sa isSuspended(), setSuspended()
*/
bool QFutureInterfaceBase::isSuspending() const { return queryState(Suspending); }

/*!
    Returns \c true if the computation acted on a suspension request and waits to be resumed.

    \sa isSuspending(), waitForResume()
*/
bool QFutureInterfaceBase::isSuspended() const { return queryState(Suspended); }

/*!
    Returns \c true if the computation is suspended or about to be. It is the same as
    \c{isSuspending() || isSuspended()}.
*/
bool QFutureInterfaceBase::isPaused() const { return (d->state.load() & kSuspendFlags) != 0; }

/*!
    Returns \c true if the consumer asked the computation to slow down because it has more results than it
    can handle.

    \sa setThrottled()
*/
bool QFutureInterfaceBase::isThrottled() const { return queryState(Throttled); }

/*!
    Returns \c true if the computation ended with an exception.

    \sa reportException()
*/
bool QFutureInterfaceBase::hasException() const {
    Lock lk(d->mutex);
    return d->exception != nullptr;
}

/*!
    Returns \c true if at least one of the flags in \a state is set.
*/
bool QFutureInterfaceBase::queryState(State state) const { return (d->state.load() & static_cast<int>(state)) != 0; }

/*!
    Reports that the computation has started. Does nothing if it has started or finished already.

    \sa reportFinished()
*/
void QFutureInterfaceBase::reportStarted() {
    Lock lk(d->mutex);
    const int s = d->state.load();
    if (s & (Started | Finished))
        return;
    d->state = (s & ~Pending) | Started | Running;
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Started));
}

/*!
    Reports that the computation has finished, wakes the threads that wait for it and runs the continuation
    that was attached to the future, in the calling thread. Does nothing if the computation has finished
    already.

    \sa reportStarted(), QFuture::then()
*/
void QFutureInterfaceBase::reportFinished() {
    std::function<void(const QFutureInterfaceBase &)> continuation;
    {
        Lock lk(d->mutex);
        const int s = d->state.load();
        if (s & Finished)
            return;
        d->state = (s & ~(Running | Pending | Suspending | Suspended | Throttled)) | Finished;
        d->waitCond.notify_all();
        d->resumeCond.notify_all();
        d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Finished));
        continuation = std::move(d->continuation);
        d->continuation = nullptr;
    }
    if (continuation)
        continuation(*this);
}

/*!
    Cancels the computation. This is the same as cancel().
*/
void QFutureInterfaceBase::reportCanceled() { cancel(); }

/*!
    Cancels the computation. The computation is not stopped: it has to look at isCanceled() and finish. Threads
    that are waiting for results or for a suspension are woken up.

    \sa cancelChain(), isCanceled()
*/
void QFutureInterfaceBase::cancel() {
    Lock lk(d->mutex);
    const int s = d->state.load();
    if (s & Canceled)
        return;
    d->state = (s & ~kSuspendFlags) | Canceled;
    d->waitCond.notify_all();
    d->resumeCond.notify_all();
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Canceled));
}

/*!
    Cancels the computation and the computations that it continues, that is the futures that this one was
    created from with QFuture::then() and its relatives, as long as they still exist.

    \sa cancel()
*/
void QFutureInterfaceBase::cancelChain() {
    std::shared_ptr<QFutureInterfaceBasePrivate> cur = d;
    while (cur) {
        QFutureInterfaceBase(cur).cancel();
        std::shared_ptr<QFutureInterfaceBasePrivate> next;
        {
            Lock lk(cur->mutex);
            next = cur->parent.lock();
        }
        cur = std::move(next);
    }
}

/*!
    Stores \a exception and cancels the computation. Waiting for the result or for the end of the computation
    throws it. Does nothing if the computation has finished or was canceled, or if an exception was stored
    already.

    \sa hasException()
*/
void QFutureInterfaceBase::reportException(std::exception_ptr exception) {
    Lock lk(d->mutex);
    const int s = d->state.load();
    if (d->exception || (s & (Canceled | Finished)))
        return;
    d->exception = std::move(exception);
    d->state = (s & ~kSuspendFlags) | Canceled;
    d->waitCond.notify_all();
    d->resumeCond.notify_all();
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Canceled));
}

/*!
    \overload
    Stores what \c{exception.clone()->raise()} throws, so that a class derived from QException that
    reimplements raise() and clone() keeps its type.
*/
void QFutureInterfaceBase::reportException(const QException &exception) {
    std::exception_ptr ptr;
    const std::unique_ptr<QException> copy(exception.clone());
    try {
        if (copy)
            copy->raise();
    } catch (...) {
        ptr = std::current_exception();
    }
    if (!ptr)
        ptr = std::make_exception_ptr(exception);
    reportException(std::move(ptr));
}

/*!
    Reports that the computation acted on a suspension request. Does nothing unless a suspension was requested.
    waitForResume() calls it.

    \sa waitForResume(), isSuspended()
*/
void QFutureInterfaceBase::reportSuspended() const {
    Lock lk(d->mutex);
    const int s = d->state.load();
    if (!(s & Suspending))
        return;
    d->state = (s & ~Suspending) | Suspended;
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Suspended));
}

/*!
    Asks the computation to suspend, if \a suspend is \c true, or to resume otherwise. A computation that
    is suspended acts on the request in waitForResume(). Does nothing if the computation has finished.

    \sa isSuspending(), isSuspended()
*/
void QFutureInterfaceBase::setSuspended(bool suspend) {
    Lock lk(d->mutex);
    const int s = d->state.load();
    if (suspend) {
        if ((s & (kSuspendFlags | Finished)))
            return;
        d->state = s | Suspending;
        d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Suspending));
    } else {
        if (!(s & kSuspendFlags))
            return;
        d->state = s & ~kSuspendFlags;
        d->resumeCond.notify_all();
        d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Resumed));
    }
}

/*!
    Suspends the computation if it is not suspended, and resumes it otherwise.

    \sa setSuspended()
*/
void QFutureInterfaceBase::toggleSuspended() { setSuspended(!isPaused()); }

/*!
    Suspends the computation if \a paused is \c true, and resumes it otherwise. This is the same as
    setSuspended().
*/
void QFutureInterfaceBase::setPaused(bool paused) { setSuspended(paused); }

/*!
    Suspends the computation if it is not suspended, and resumes it otherwise. This is the same as
    toggleSuspended().
*/
void QFutureInterfaceBase::togglePaused() { toggleSuspended(); }

/*!
    Tells the computation, if \a enable is \c true, that the consumer has more results than it can handle, or
    that it caught up otherwise. QFutureWatcher does so according to its pending results limit. The flag is
    informative: waitForResume() does not wait for it.

    \sa isThrottled(), QFutureWatcher::setPendingResultsLimit()
*/
void QFutureInterfaceBase::setThrottled(bool enable) {
    Lock lk(d->mutex);
    const int s = d->state.load();
    d->state = enable ? (s | Throttled) : (s & ~Throttled);
}

/*!
    Waits until the computation has finished, and throws the exception that it ended with, if any. Returns at
    once for a computation that is not running or pending.

    \sa isFinished(), reportFinished()
*/
void QFutureInterfaceBase::waitForFinished() const {
    {
        Lock lk(d->mutex);
        d->waitCond.wait(lk, [this] { return !(d->state.load() & (Running | Pending)); });
    }
    throwPossibleException();
}

/*!
    Waits until the result at \a resultIndex is available, or the computation has finished or ended with an
    exception, which this function throws.

    \sa isResultReadyAt()
*/
void QFutureInterfaceBase::waitForResult(int resultIndex) const {
    {
        Lock lk(d->mutex);
        d->waitCond.wait(lk, [this, resultIndex] {
            int offset = 0;
            return !(d->state.load() & (Running | Pending)) || d->exception || d->find(resultIndex, &offset);
        });
    }
    throwPossibleException();
}

/*!
    Blocks while a suspension is requested, until the computation is resumed or canceled. The computation
    that calls it becomes suspended, which isSuspended() reports. Returns at once otherwise. QPromise::suspendIfRequested()
    calls it.

    \sa reportSuspended(), setSuspended()
*/
void QFutureInterfaceBase::waitForResume() const {
    if (!(d->state.load() & kSuspendFlags))
        return;
    Lock lk(d->mutex);
    for (;;) {
        const int s = d->state.load();
        if (!(s & kSuspendFlags) || (s & (Canceled | Finished)))
            return;
        if (s & Suspending) {
            d->state = (s & ~Suspending) | Suspended;
            d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Suspended));
        }
        d->resumeCond.wait(lk);
    }
}

void QFutureInterfaceBase::throwPossibleException() const {
    std::exception_ptr e;
    {
        Lock lk(d->mutex);
        e = d->exception;
    }
    if (e)
        std::rethrow_exception(e);
}

/*!
    Returns the number of results that are available from the first one on, without a gap.

    \sa isResultReadyAt()
*/
int QFutureInterfaceBase::resultCount() const {
    Lock lk(d->mutex);
    return d->contiguous;
}

/*!
    Returns \c true if the result at \a index is available.

    \sa waitForResult(), resultCount()
*/
bool QFutureInterfaceBase::isResultReadyAt(int index) const {
    Lock lk(d->mutex);
    int offset = 0;
    return d->find(index, &offset) != nullptr;
}

/*!
    Sets the number of results that the computation is going to produce to \a resultCount. The number is
    informative.

    \sa expectedResultCount()
*/
void QFutureInterfaceBase::setExpectedResultCount(int resultCount) {
    Lock lk(d->mutex);
    d->expected = resultCount;
}

/*!
    Returns the number of results that the computation announced, or 0.

    \sa setExpectedResultCount()
*/
int QFutureInterfaceBase::expectedResultCount() const {
    Lock lk(d->mutex);
    return d->expected;
}

/*!
    Returns the lowest progress value. It is 0 unless setProgressRange() was called.

    \sa progressMaximum(), progressValue()
*/
int QFutureInterfaceBase::progressMinimum() const {
    Lock lk(d->mutex);
    return d->progressMin;
}

/*!
    Returns the highest progress value. It is 0 unless setProgressRange() was called.

    \sa progressMinimum(), progressValue()
*/
int QFutureInterfaceBase::progressMaximum() const {
    Lock lk(d->mutex);
    return d->progressMax;
}

/*!
    Returns the current progress value.

    \sa setProgressValue(), progressText()
*/
int QFutureInterfaceBase::progressValue() const {
    Lock lk(d->mutex);
    return d->progressValue;
}

/*!
    Returns the current progress text, which is empty unless setProgressValueAndText() was called.

    \sa progressValue()
*/
QString QFutureInterfaceBase::progressText() const {
    Lock lk(d->mutex);
    return d->progressText;
}

/*!
    Sets the range of progress values to \a minimum to \a maximum.

    \sa setProgressValue()
*/
void QFutureInterfaceBase::setProgressRange(int minimum, int maximum) {
    Lock lk(d->mutex);
    d->progressMin = minimum;
    d->progressMax = maximum;
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::ProgressRange, minimum, maximum));
}

/*!
    Sets the progress value to \a progressValue and clears the progress text.

    \sa setProgressValueAndText(), setProgressRange()
*/
void QFutureInterfaceBase::setProgressValue(int progressValue) { setProgressValueAndText(progressValue, QString()); }

/*!
    Sets the progress value to \a progressValue and the progress text to \a progressText. Does nothing if
    the computation is finished or canceled, or if neither changes.

    \sa setProgressValue()
*/
void QFutureInterfaceBase::setProgressValueAndText(int progressValue, const QString &progressText) {
    Lock lk(d->mutex);
    if (d->state.load() & (Canceled | Finished))
        return;
    if (d->progressValue == progressValue && d->progressText == progressText)
        return;
    d->progressValue = progressValue;
    d->progressText = progressText;
    d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::Progress, progressValue, 0, progressText));
}

/*!
    Sets the thread pool that the computation runs in to \a pool. Continuations that inherit the launch policy
    of the computation run in that pool. The pool must outlive the future.

    \sa threadPool(), QtFuture::Launch
*/
void QFutureInterfaceBase::setThreadPool(QThreadPool *pool) { d->pool = pool; }

/*!
    Returns the thread pool set with setThreadPool(), or \c nullptr.
*/
QThreadPool *QFutureInterfaceBase::threadPool() const { return d->pool; }

/*!
    Returns \c true if \a other refers to the same computation as this interface.
*/
bool QFutureInterfaceBase::operator==(const QFutureInterfaceBase &other) const { return d == other.d; }

/*!
    Returns \c true if \a other does not refer to the same computation as this interface.
*/
bool QFutureInterfaceBase::operator!=(const QFutureInterfaceBase &other) const { return d != other.d; }

int QFutureInterfaceBase::storeResult(int index, void *item, int count, bool isVector, QtPrivate::ResultDeleter deleter) {
    Lock lk(d->mutex);
    if (d->state.load() & (Finished | Canceled))
        return -1;
    if (index < 0)
        index = d->nextIndex;
    auto next = d->items.lower_bound(index);
    if (next != d->items.end() && next->first < index + count)
        return -1;
    if (next != d->items.begin()) {
        const auto prev = std::prev(next);
        if (prev->first + prev->second.count > index)
            return -1;
    }
    d->deleter = deleter;
    d->items.emplace(index, QFutureInterfaceBasePrivate::Item{item, count, isVector});
    d->nextIndex = std::max(d->nextIndex, index + count);
    const int before = d->contiguous;
    for (auto it = d->items.find(d->contiguous); it != d->items.end(); it = d->items.find(d->contiguous))
        d->contiguous += it->second.count;
    if (d->contiguous > before)
        d->sendCallOut(QFutureCallOutEvent(QFutureCallOutEvent::ResultsReady, before, d->contiguous));
    d->waitCond.notify_all();
    return index;
}

QtPrivate::ResultLocation QFutureInterfaceBase::locateResult(int index) const {
    Lock lk(d->mutex);
    int offset = 0;
    const auto *item = d->find(index, &offset);
    if (!item)
        return {};
    return {item->ptr, offset, item->isVector};
}
