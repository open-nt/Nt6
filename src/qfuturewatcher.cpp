// MIT License - see LICENSE
#include <QtCore/qfuturewatcher.h>

#include "qfutureinterface_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qthread.h>

#include <atomic>

struct QFutureWatcherBase::Data : QFutureCallOutInterface {
    explicit Data(QFutureWatcherBase *watcher)
        : q(watcher), maxPending(std::max(1, QThread::idealThreadCount()) * 100) {}

    // Called by the thread that reports, with the lock of the future held.
    void callOutInterfaceEvent(const QFutureCallOutEvent &event) override {
        if (event.callOutType == QFutureCallOutEvent::ResultsReady &&
            pending.fetch_add(1) + 1 >= maxPending.load())
            QtPrivate::FutureAccess::setThrottledFromCallOut(q->futureInterface(), true);
        QCoreApplication::postEvent(q, event.clone());
    }

    QFutureWatcherBase *q;
    std::atomic<int> pending{0};
    std::atomic<int> maxPending;
};

/*!
    \class QFutureWatcherBase
    \brief The QFutureWatcherBase class is the base of QFutureWatcher, and holds its signals and slots.

    \differences
    There is no \c connectNotify(): the watcher always listens to its future. The \c futureInterface()
    functions and the \c connectOutputInterface() functions are internal.
    \enddifferences

    \sa QFutureWatcher
*/

/*!
    Constructs a watcher with the given \a parent.
*/
QFutureWatcherBase::QFutureWatcherBase(QObject *parent) : QObject(parent), m_d(new Data(this)) {}

/*!
    Destroys the watcher.
*/
QFutureWatcherBase::~QFutureWatcherBase() = default;

/*!
    Returns the progress value of the watched future.

    \sa progressMinimum(), progressMaximum()
*/
int QFutureWatcherBase::progressValue() const { return futureInterface().progressValue(); }

/*!
    Returns the lowest progress value of the watched future.

    \sa progressMaximum()
*/
int QFutureWatcherBase::progressMinimum() const { return futureInterface().progressMinimum(); }

/*!
    Returns the highest progress value of the watched future.

    \sa progressMinimum()
*/
int QFutureWatcherBase::progressMaximum() const { return futureInterface().progressMaximum(); }

/*!
    Returns the progress text of the watched future.

    \sa progressValue()
*/
QString QFutureWatcherBase::progressText() const { return futureInterface().progressText(); }

/*!
    Returns \c true if the watched computation has started. The state is the one of the future, not of the
    signals that were delivered.

    \sa started()
*/
bool QFutureWatcherBase::isStarted() const { return futureInterface().isStarted(); }

/*!
    Returns \c true if the watched computation has finished.

    \sa finished()
*/
bool QFutureWatcherBase::isFinished() const { return futureInterface().isFinished(); }

/*!
    Returns \c true if the watched computation is running.

    \sa isStarted()
*/
bool QFutureWatcherBase::isRunning() const { return futureInterface().isRunning(); }

/*!
    Returns \c true if the watched computation was canceled.

    \sa cancel()
*/
bool QFutureWatcherBase::isCanceled() const { return futureInterface().isCanceled(); }

/*!
    Returns \c true if a suspension of the watched computation was requested and it has not acted on it.

    \sa isSuspended()
*/
bool QFutureWatcherBase::isSuspending() const { return futureInterface().isSuspending(); }

/*!
    Returns \c true if the watched computation is suspended.

    \sa isSuspending()
*/
bool QFutureWatcherBase::isSuspended() const { return futureInterface().isSuspended(); }

/*!
    Returns \c true if the watched computation is suspended or about to be.

    \sa isSuspending(), isSuspended()
*/
bool QFutureWatcherBase::isPaused() const { return futureInterface().isPaused(); }

/*!
    Waits for the watched computation to finish. Signals for it are delivered by the event loop, not by this
    function. It throws the exception that the computation ended with, if any.

    \sa isFinished()
*/
void QFutureWatcherBase::waitForFinished() { futureInterface().waitForFinished(); }

/*!
    Sets the number of results that may wait to be delivered to \a limit. The watched future is throttled while
    that many results are waiting, which QFutureInterfaceBase::isThrottled() reports to the computation. The
    default is 100 for each ideal thread.

    \sa QFutureInterfaceBase::setThrottled()
*/
void QFutureWatcherBase::setPendingResultsLimit(int limit) { m_d->maxPending = std::max(1, limit); }

/*!
    \reimp
*/
bool QFutureWatcherBase::event(QEvent *event) {
    if (event->type() != QFutureCallOutEvent::eventType())
        return QObject::event(event);

    const auto &e = *static_cast<const QFutureCallOutEvent *>(event);
    QFutureInterfaceBase &iface = futureInterface();
    switch (e.callOutType) {
    case QFutureCallOutEvent::Started:
        started();
        break;
    case QFutureCallOutEvent::Finished:
        finished();
        break;
    case QFutureCallOutEvent::Canceled:
        canceled();
        break;
    case QFutureCallOutEvent::Suspending:
        suspending();
        paused();
        break;
    case QFutureCallOutEvent::Suspended:
        suspended();
        break;
    case QFutureCallOutEvent::Resumed:
        resumed();
        break;
    case QFutureCallOutEvent::Progress:
        if (!iface.isCanceled()) {
            progressValueChanged(e.index1);
            if (!e.text.isEmpty())
                progressTextChanged(e.text);
        }
        break;
    case QFutureCallOutEvent::ProgressRange:
        if (!iface.isCanceled())
            progressRangeChanged(e.index1, e.index2);
        break;
    case QFutureCallOutEvent::ResultsReady:
        // The results are delivered, so the computation may go on.
        if (m_d->pending.fetch_sub(1) <= m_d->maxPending.load())
            iface.setThrottled(false);
        if (!iface.isCanceled()) {
            for (int i = e.index1; i < e.index2; ++i)
                resultReadyAt(i);
            resultsReadyAt(e.index1, e.index2);
        }
        break;
    }
    return true;
}

/*!
    Cancels the watched future.

    \sa QFuture::cancel()
*/
void QFutureWatcherBase::cancel() { futureInterface().cancel(); }

/*!
    Suspends the watched computation if \a suspend is \c true, and resumes it otherwise.

    \sa suspend(), resume()
*/
void QFutureWatcherBase::setSuspended(bool suspend) { futureInterface().setSuspended(suspend); }

/*!
    Asks the watched computation to suspend.

    \sa resume()
*/
void QFutureWatcherBase::suspend() { futureInterface().setSuspended(true); }

/*!
    Resumes the watched computation.

    \sa suspend()
*/
void QFutureWatcherBase::resume() { futureInterface().setSuspended(false); }

/*!
    Suspends the watched computation if it is not suspended, and resumes it otherwise.

    \sa setSuspended()
*/
void QFutureWatcherBase::toggleSuspended() { futureInterface().toggleSuspended(); }

/*!
    Suspends the watched computation if \a paused is \c true, and resumes it otherwise. This is the same as
    setSuspended().
*/
void QFutureWatcherBase::setPaused(bool paused) { futureInterface().setSuspended(paused); }

/*!
    Suspends the watched computation. This is the same as suspend().
*/
void QFutureWatcherBase::pause() { futureInterface().setSuspended(true); }

/*!
    Suspends the watched computation if it is not suspended, and resumes it otherwise. This is the same as
    toggleSuspended().
*/
void QFutureWatcherBase::togglePaused() { futureInterface().toggleSuspended(); }

void QFutureWatcherBase::connectOutputInterface() {
    QtPrivate::FutureAccess::connectOutput(futureInterface(), m_d.get());
}

void QFutureWatcherBase::disconnectOutputInterface(bool pendingAssignment) {
    QtPrivate::FutureAccess::disconnectOutput(futureInterface(), m_d.get());
    if (pendingAssignment) {
        m_d->pending = 0;
        QCoreApplication::removePostedEvents(this, QFutureCallOutEvent::eventType());
    }
}

#include "moc_qfuturewatcher.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
