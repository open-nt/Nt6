// MIT License - see LICENSE
#pragma once

#include <QtCore/qfutureinterface.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <iterator>
#include <type_traits>
#include <utility>

class QObject;
class QThreadPool;

/*!
    \namespace QtFuture
    \inmodule NtCore
    \brief The QtFuture namespace holds helpers for QFuture, such as QtFuture::Launch.

    Besides the enumeration, it has the functions makeReadyValueFuture(), makeReadyVoidFuture(),
    makeReadyRangeFuture() and makeExceptionalFuture(), which return a future that is finished already,
    and connect(), which returns a future for the next emission of a signal.
*/
namespace QtFuture {

/*!
    \enum QtFuture::Launch

    This enum describes where a continuation attached with QFuture::then() runs.

    \value Sync The continuation runs in the thread that finishes the previous future, or in the thread that calls
    then() if that future has finished already.
    \value Async The continuation runs in a thread of the global QThreadPool.
    \value Inherit The continuation runs in the thread pool that the previous future runs in, as set with
    QFutureInterfaceBase::setThreadPool(), and as a \c Sync continuation if the future has none.
*/
enum class Launch { Sync, Async, Inherit };

} // namespace QtFuture

namespace QtPrivate {

// How a continuation is started once the previous future has finished.
struct Launcher {
    enum Kind { Sync, Pool, Inherit, Context };
    Kind kind = Sync;
    QThreadPool *pool = nullptr;
    QObject *context = nullptr;
};

// A continuation takes the value of the previous future, or the future itself; for a QFuture<void> it takes
// nothing, or the future.
template <typename Fn, typename T>
struct ThenInvoke {
    static constexpr bool byValue = std::is_invocable_v<Fn, T>;
    static constexpr bool byFuture = !byValue && std::is_invocable_v<Fn, QFuture<T>>;
};
template <typename Fn>
struct ThenInvoke<Fn, void> {
    static constexpr bool byValue = std::is_invocable_v<Fn>;
    static constexpr bool byFuture = !byValue && std::is_invocable_v<Fn, QFuture<void>>;
};

template <typename Fn, typename T, bool ByValue = ThenInvoke<Fn, T>::byValue>
struct ThenResultOf {
    using type = std::decay_t<std::invoke_result_t<Fn, QFuture<T>>>;
};
template <typename Fn, typename T>
struct ThenResultOf<Fn, T, true> {
    using type = std::decay_t<std::invoke_result_t<Fn, T>>;
};
template <typename Fn>
struct ThenResultOf<Fn, void, true> {
    using type = std::decay_t<std::invoke_result_t<Fn>>;
};
template <typename Fn, typename T>
using ThenResult = typename ThenResultOf<Fn, T>::type;

template <typename T, typename Fn>
QFuture<ThenResult<std::decay_t<Fn>, T>> startThen(const QFuture<T> &parent, Fn &&fn, Launcher launcher);
template <typename T, typename Fn>
QFuture<T> startOnCanceled(const QFuture<T> &parent, Fn &&fn, Launcher launcher);
template <typename T, typename Fn>
QFuture<T> startOnFailed(const QFuture<T> &parent, Fn &&fn, Launcher launcher);

template <typename T>
struct FutureConstIterator;
struct ContinuationAccess;

} // namespace QtPrivate

/*!
    \class QFuture
    \brief The QFuture class represents the result of an asynchronous computation.

    A QFuture is the reading end of a QPromise: it tells whether the computation has started, finished or was
    canceled, gives its results as they arrive, and rethrows the exception that ended it. Copies of a future
    share the computation, and every function can be called from any thread.

    \code
    QPromise<int> promise;
    QFuture<int> future = promise.future();
    std::thread worker([promise = std::move(promise)]() mutable {
        promise.start();
        promise.addResult(6 * 7);
        promise.finish();
    });
    int answer = future.result(); // waits for the result
    worker.join();
    \endcode

    A continuation is attached with then(), onFailed() or onCanceled(), and a QFutureWatcher reports the
    progress of a future to an event loop. \c QFuture<void> has no results, but everything else.

    \differences
    A QFuture is not created by QtConcurrent, which Nt does not have; use QPromise, QFutureInterface or
    QThreadPool::start() with a QPromise. The function \c then() does not unwrap a continuation that returns a
    QFuture, and the continuation of a future that finished without a result is not called. A future takes one
    continuation; a second one is refused with a warning and a canceled future. \c onCanceled() and \c onFailed()
    require that the handler returns the type of the future. There is no \c QFutureIterator. Waiting
    functions are \c const. \c takeResult() leaves the result in its moved-from state instead of removing it.
    \enddifferences

    \sa QPromise, QFutureWatcher, QFutureSynchronizer, QtFuture::Launch
*/
template <typename T>
class QFuture {
public:
    /*!
        \typedef QFuture::const_iterator

        An iterator over the results of a future that waits for each result it reads, so that a loop over the
        future follows the computation. It is a bidirectional iterator.
    */
    using const_iterator = QtPrivate::FutureConstIterator<T>;
    /*!
        \typedef QFuture::ConstIterator

        A synonym for \c const_iterator.
    */
    using ConstIterator = const_iterator;

    /*!
        Constructs an invalid future. It is canceled and finished, and has no results.

        \sa isValid()
    */
    QFuture() : d(QFutureInterface<T>::canceledResult()) {}
    /*!
        Constructs a future that observes the state of \a p. It shares the state, so \a p can go away.
    */
    explicit QFuture(QFutureInterface<T> *p) : d(*p) {}
    /*!
        \overload
        Constructs a future that observes the state of \a other.
    */
    explicit QFuture(const QFutureInterface<T> &other) : d(other) {}
    /*!
        Constructs a copy of \a other. Both refer to the same computation.
    */
    QFuture(const QFuture &other) = default;
    /*!
        Moves \a other into this future. \a other can only be destroyed or assigned to afterwards.
    */
    QFuture(QFuture &&other) noexcept = default;
    /*!
        Makes this future refer to the computation of \a other and returns a reference to it.
    */
    QFuture &operator=(const QFuture &other) = default;
    /*!
        Moves \a other into this future and returns a reference to it.
    */
    QFuture &operator=(QFuture &&other) noexcept = default;
    /*!
        Destroys the future. The computation is not affected.
    */
    ~QFuture() = default;

    /*!
        Returns \c true if \a other refers to the same computation as this future.
    */
    bool operator==(const QFuture &other) const { return d == other.d; }
    /*!
        Returns \c true if \a other does not refer to the same computation as this future.
    */
    bool operator!=(const QFuture &other) const { return d != other.d; }

    /*!
        Converts the future to one that does not give access to the results, so that futures of every result type
        can be kept together. The new future refers to the same computation.
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    operator QFuture<void>() const {
        return QFuture<void>(QFutureInterface<void>(d));
    }

    /*!
        Returns \c true if the future refers to a computation, that is, if it was not default-constructed or
        moved from.
    */
    bool isValid() const { return d.isValid(); }
    /*!
        Returns \c true if the computation has been started. The answer stays \c true after it finished.

        \sa isRunning(), isFinished()
    */
    bool isStarted() const { return d.isStarted(); }
    /*!
        Returns \c true if the computation is running, that is started and not finished.

        \sa isStarted(), isFinished()
    */
    bool isRunning() const { return d.isRunning(); }
    /*!
        Returns \c true if the computation has finished. A canceled computation finishes when it notices.

        \sa isRunning(), waitForFinished()
    */
    bool isFinished() const { return d.isFinished(); }
    /*!
        Returns \c true if the computation was canceled, or ended with an exception.

        \sa cancel()
    */
    bool isCanceled() const { return d.isCanceled(); }
    /*!
        Returns \c true if a suspension was requested and the computation has not acted on it yet.

        \sa isSuspended(), suspend()
    */
    bool isSuspending() const { return d.isSuspending(); }
    /*!
        Returns \c true if the computation acted on a suspension request and waits to be resumed.

        \sa isSuspending(), resume()
    */
    bool isSuspended() const { return d.isSuspended(); }
    /*!
        Returns \c true if the computation is suspended or about to be.

        \sa isSuspending(), isSuspended()
    */
    bool isPaused() const { return d.isPaused(); }

    /*!
        Cancels the computation. The computation stops only if it looks at isCanceled() or
        QPromise::isCanceled(). Continuations that have not run yet are skipped.

        \sa cancelChain(), isCanceled()
    */
    void cancel() { d.cancel(); }
    /*!
        Cancels this future and the futures that it continues, as long as they still exist.

        \sa cancel()
    */
    void cancelChain() { d.cancelChain(); }
    /*!
        Asks the computation to suspend if \a suspend is \c true, and to resume otherwise.

        \sa suspend(), resume()
    */
    void setSuspended(bool suspend) { d.setSuspended(suspend); }
    /*!
        Asks the computation to suspend. It does so the next time it calls QPromise::suspendIfRequested().

        \sa resume(), isSuspending()
    */
    void suspend() { d.setSuspended(true); }
    /*!
        Resumes a suspended computation, or cancels a suspension request that was not acted upon.

        \sa suspend()
    */
    void resume() { d.setSuspended(false); }
    /*!
        Suspends the computation if it is not suspended, and resumes it otherwise.
    */
    void toggleSuspended() { d.toggleSuspended(); }
    /*!
        Suspends the computation if \a paused is \c true, and resumes it otherwise. This is the same as setSuspended().
    */
    void setPaused(bool paused) { d.setSuspended(paused); }
    /*!
        Suspends the computation. This is the same as suspend().
    */
    void pause() { d.setSuspended(true); }
    /*!
        Suspends the computation if it is not suspended, and resumes it otherwise. This is the same as
        toggleSuspended().
    */
    void togglePaused() { d.toggleSuspended(); }

    /*!
        Blocks until the computation has finished. It throws the exception that the computation ended with, if any.

        \sa isFinished()
    */
    void waitForFinished() const { d.waitForFinished(); }

    /*!
        Returns the number of results that are available from the first one on without a gap.

        \sa isResultReadyAt(), results()
    */
    int resultCount() const { return d.resultCount(); }
    /*!
        Returns \c true if the result at \a index is available.

        \sa resultCount(), resultAt()
    */
    bool isResultReadyAt(int index) const { return d.isResultReadyAt(index); }
    /*!
        Returns the progress value, between progressMinimum() and progressMaximum().

        \sa progressText()
    */
    int progressValue() const { return d.progressValue(); }
    /*!
        Returns the lowest progress value.

        \sa progressMaximum()
    */
    int progressMinimum() const { return d.progressMinimum(); }
    /*!
        Returns the highest progress value.

        \sa progressMinimum()
    */
    int progressMaximum() const { return d.progressMaximum(); }
    /*!
        Returns the text that describes the progress, if the computation set one.

        \sa progressValue()
    */
    QString progressText() const { return d.progressText(); }

    /*!
        Waits for the first result and returns a copy of it. Returns a default-constructed value if the computation
        ends without a result. It throws the exception that the computation ended with, if any.

        \sa resultAt(), takeResult(), results()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    U result() const {
        return d.result();
    }
    /*!
        Waits for the result at \a index and returns a copy of it. It throws the exception that the computation
        ended with, if any.

        \sa result(), isResultReadyAt()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    U resultAt(int index) const {
        d.waitForResult(index);
        const U *p = nullptr;
        if (d.resultPointer(index, &p))
            return *p;
        if constexpr (std::is_default_constructible_v<U>)
            return U();
        else
            std::terminate();
    }
    /*!
        Waits for the first result and moves it out of the future, which leaves the result in its moved-from state.
        No other thread may read the result meanwhile. Use it for a result that cannot be copied.

        \sa result()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    U takeResult() {
        return d.takeResult();
    }
    /*!
        Waits for the computation to finish and returns a copy of all results. Returns an empty list if the
        computation was canceled. It throws the exception that the computation ended with, if any.

        \sa result(), resultCount()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    QList<U> results() const {
        return d.results();
    }
    /*!
        Returns the first result, like result(), so that a future can be used where its value is expected.
    */
    template <typename U, typename = std::enable_if_t<std::is_same_v<U, T> && !std::is_void_v<U>>>
    operator U() const {
        return d.result();
    }

    /*!
        Returns an iterator to the first result. Dereferencing an iterator waits for its result.

        \sa end()
    */
    const_iterator begin() const { return const_iterator(this, 0); }
    /*!
        Returns an iterator that follows the last result. The future may still be running when it is
        compared with, so the comparison waits for the result that the other iterator points to.

        \sa begin()
    */
    const_iterator end() const { return const_iterator(this, -1); }
    /*!
        Returns an iterator to the first result. This is the same as begin().
    */
    const_iterator cbegin() const { return begin(); }
    /*!
        Returns an iterator that follows the last result. This is the same as end().
    */
    const_iterator cend() const { return end(); }
    /*!
        Returns an iterator that follows the last result. This is the same as end().
    */
    const_iterator constBegin() const { return begin(); }
    /*!
        Returns an iterator that follows the last result. This is the same as end().
    */
    const_iterator constEnd() const { return end(); }

    /*!
        Attaches the continuation \a function, which runs when this future has finished, and returns a future for
        its result. \a function takes the result of this future, or this future itself, and runs in the thread that
        finishes this future. A \c QFuture<void> passes nothing. \a function is skipped if this future was canceled
        or ended with an exception, unless it takes the future; the returned future is canceled, or gets the
        exception, then. A future has at most one continuation, so a second call returns a canceled future.

        \sa onCanceled(), onFailed()
    */
    template <typename Function>
    QFuture<QtPrivate::ThenResult<std::decay_t<Function>, T>> then(Function &&function) {
        return QtPrivate::startThen(*this, std::forward<Function>(function), QtPrivate::Launcher());
    }
    /*!
        \overload
        Attaches \a function, which runs as \a policy says.

        \sa QtFuture::Launch
    */
    template <typename Function>
    QFuture<QtPrivate::ThenResult<std::decay_t<Function>, T>> then(QtFuture::Launch policy, Function &&function) {
        QtPrivate::Launcher l;
        l.kind = policy == QtFuture::Launch::Sync ? QtPrivate::Launcher::Sync
                 : policy == QtFuture::Launch::Async ? QtPrivate::Launcher::Pool
                                                     : QtPrivate::Launcher::Inherit;
        return QtPrivate::startThen(*this, std::forward<Function>(function), l);
    }
    /*!
        \overload
        Attaches \a function, which runs in a thread of \a pool.
    */
    template <typename Function>
    QFuture<QtPrivate::ThenResult<std::decay_t<Function>, T>> then(QThreadPool *pool, Function &&function) {
        QtPrivate::Launcher l;
        l.kind = QtPrivate::Launcher::Pool;
        l.pool = pool;
        return QtPrivate::startThen(*this, std::forward<Function>(function), l);
    }
    /*!
        \overload
        Attaches \a function, which runs in the thread that \a context lives in, from its event loop. If \a context is
        destroyed before then, \a function is not called and the returned future is canceled.
    */
    template <typename Function>
    QFuture<QtPrivate::ThenResult<std::decay_t<Function>, T>> then(QObject *context, Function &&function) {
        QtPrivate::Launcher l;
        l.kind = QtPrivate::Launcher::Context;
        l.context = context;
        return QtPrivate::startThen(*this, std::forward<Function>(function), l);
    }

    /*!
        Attaches \a handler, which is called without arguments if this future was canceled, and returns a future
        for its result. \a handler returns a value of the type of this future, or nothing for a
        \c QFuture<void>, and the returned future carries that value. If this future was not canceled, the
        returned future gets the results of this one. If it ended with an exception, the returned future gets
        the exception.

        \sa then(), onFailed()
    */
    template <typename Function>
    QFuture<T> onCanceled(Function &&handler) {
        return QtPrivate::startOnCanceled(*this, std::forward<Function>(handler), QtPrivate::Launcher());
    }
    /*!
        \overload
        Attaches \a handler, which runs in the thread that \a context lives in.
    */
    template <typename Function>
    QFuture<T> onCanceled(QObject *context, Function &&handler) {
        QtPrivate::Launcher l;
        l.kind = QtPrivate::Launcher::Context;
        l.context = context;
        return QtPrivate::startOnCanceled(*this, std::forward<Function>(handler), l);
    }
    /*!
        Attaches \a handler, which is called with the exception if this future ended with an exception of the type
        that \a handler takes by reference, or with any exception if it takes nothing, and returns a future for its
        result. \a handler returns a value of the type of this future, or nothing for a \c QFuture<void>. If this
        future did not fail, the returned future gets its results; an exception of another type passes on.

        \sa then(), onCanceled()
    */
    template <typename Function>
    QFuture<T> onFailed(Function &&handler) {
        return QtPrivate::startOnFailed(*this, std::forward<Function>(handler), QtPrivate::Launcher());
    }
    /*!
        \overload
        Attaches \a handler, which runs in the thread that \a context lives in.
    */
    template <typename Function>
    QFuture<T> onFailed(QObject *context, Function &&handler) {
        QtPrivate::Launcher l;
        l.kind = QtPrivate::Launcher::Context;
        l.context = context;
        return QtPrivate::startOnFailed(*this, std::forward<Function>(handler), l);
    }

private:
    friend class QFutureInterface<T>;
    template <typename U>
    friend class QFuture;
    template <typename U>
    friend struct QtPrivate::FutureConstIterator;
    friend struct QtPrivate::ContinuationAccess;

    mutable QFutureInterface<T> d;
};

namespace QtPrivate {

template <typename T>
struct FutureConstIterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = int;
    using value_type = T;
    using pointer = const T *;
    using reference = const T &;

    FutureConstIterator() = default;
    FutureConstIterator(const QFuture<T> *f, int i) : future(f), index(i) {}

    T operator*() const { return future->resultAt(index); }
    pointer operator->() const {
        const T *p = nullptr;
        future->d.waitForResult(index);
        future->d.resultPointer(index, &p);
        return p;
    }
    FutureConstIterator &operator++() {
        ++index;
        return *this;
    }
    FutureConstIterator operator++(int) {
        auto r = *this;
        ++index;
        return r;
    }
    FutureConstIterator &operator--() {
        --index;
        return *this;
    }
    FutureConstIterator operator--(int) {
        auto r = *this;
        --index;
        return r;
    }
    FutureConstIterator &operator+=(int n) {
        index += n;
        return *this;
    }
    FutureConstIterator &operator-=(int n) {
        index -= n;
        return *this;
    }
    FutureConstIterator operator+(int n) const { return FutureConstIterator(future, index + n); }
    FutureConstIterator operator-(int n) const { return FutureConstIterator(future, index - n); }
    bool operator==(const FutureConstIterator &o) const {
        if (index == o.index)
            return true;
        if (index < 0 && o.index >= 0)
            return o.atEnd();
        if (o.index < 0 && index >= 0)
            return atEnd();
        return false;
    }
    bool operator!=(const FutureConstIterator &o) const { return !(*this == o); }

private:
    // Waits until the result at the position is there, or the computation is done without it.
    bool atEnd() const {
        future->d.waitForResult(index);
        return !future->d.isResultReadyAt(index);
    }

    const QFuture<T> *future = nullptr;
    int index = -1;
};

} // namespace QtPrivate

template <typename T>
QFuture<T> QFutureInterface<T>::future() {
    return QFuture<T>(*this);
}

inline QFuture<void> QFutureInterface<void>::future() { return QFuture<void>(*this); }

#include <QtCore/qfuture_impl.h>
