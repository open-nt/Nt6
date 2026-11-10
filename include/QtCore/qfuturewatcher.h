// MIT License - see LICENSE
#pragma once

#include <QtCore/qfuture.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QFutureWatcherBase : public QObject {
    Q_OBJECT
public:
    explicit QFutureWatcherBase(QObject *parent = nullptr);
    ~QFutureWatcherBase() override;

    int progressValue() const;
    int progressMinimum() const;
    int progressMaximum() const;
    QString progressText() const;

    bool isStarted() const;
    bool isFinished() const;
    bool isRunning() const;
    bool isCanceled() const;
    bool isSuspending() const;
    bool isSuspended() const;
    bool isPaused() const;

    void waitForFinished();

    void setPendingResultsLimit(int limit);

    bool event(QEvent *event) override;

public slots:
    void cancel();
    void setSuspended(bool suspend);
    void suspend();
    void resume();
    void toggleSuspended();
    void setPaused(bool paused);
    void pause();
    void togglePaused();

signals:
    /*!
        This signal is emitted, from the event loop of the thread the watcher lives in, when the watched
        computation starts.

        \sa finished()
    */
    void started() { emitSignal(&QFutureWatcherBase::started); }
    /*!
        This signal is emitted when the watched computation has finished, after the signals for its results.

        \sa started(), canceled()
    */
    void finished() { emitSignal(&QFutureWatcherBase::finished); }
    /*!
        This signal is emitted when the watched computation is canceled, or ends with an exception.

        \sa cancel(), finished()
    */
    void canceled() { emitSignal(&QFutureWatcherBase::canceled); }
    /*!
        This signal is emitted when a suspension of the watched computation is requested. It is emitted
        together with suspending().

        \sa suspending(), suspended(), resumed()
    */
    void paused() { emitSignal(&QFutureWatcherBase::paused); }
    /*!
        This signal is emitted when a suspension of the watched computation is requested.

        \sa suspended(), resumed()
    */
    void suspending() { emitSignal(&QFutureWatcherBase::suspending); }
    /*!
        This signal is emitted when the watched computation has acted on a suspension request.

        \sa suspending(), resumed()
    */
    void suspended() { emitSignal(&QFutureWatcherBase::suspended); }
    /*!
        This signal is emitted when the watched computation is resumed after a suspension was requested.

        \sa suspending(), suspended()
    */
    void resumed() { emitSignal(&QFutureWatcherBase::resumed); }
    /*!
        This signal is emitted when the result at \a index is available. It is not emitted for results that
        arrive after the computation was canceled.

        \sa resultsReadyAt()
    */
    void resultReadyAt(int index) { emitSignal(&QFutureWatcherBase::resultReadyAt, index); }
    /*!
        This signal is emitted when the results from \a beginIndex up to, but not including, \a endIndex are
        available, after resultReadyAt() was emitted for each of them.

        \sa resultReadyAt()
    */
    void resultsReadyAt(int beginIndex, int endIndex) {
        emitSignal(&QFutureWatcherBase::resultsReadyAt, beginIndex, endIndex);
    }
    /*!
        This signal is emitted when the range of the progress values changes to \a minimum to \a maximum.

        \sa progressValueChanged()
    */
    void progressRangeChanged(int minimum, int maximum) {
        emitSignal(&QFutureWatcherBase::progressRangeChanged, minimum, maximum);
    }
    /*!
        This signal is emitted when the progress value changes to \a progressValue.

        \sa progressRangeChanged(), progressTextChanged()
    */
    void progressValueChanged(int progressValue) {
        emitSignal(&QFutureWatcherBase::progressValueChanged, progressValue);
    }
    /*!
        This signal is emitted when the progress text changes to \a progressText.

        \sa progressValueChanged()
    */
    void progressTextChanged(const QString &progressText) {
        emitSignal(&QFutureWatcherBase::progressTextChanged, progressText);
    }

protected:
    /*!
        \internal
    */
    virtual QFutureInterfaceBase &futureInterface() const = 0;
    /*!
        \internal
    */
    void connectOutputInterface();
    /*!
        \internal
    */
    void disconnectOutputInterface(bool pendingAssignment = false);

private:
    struct Data;
    std::unique_ptr<Data> m_d;
};

/*!
    \class QFutureWatcher
    \brief The QFutureWatcher class allows monitoring a QFuture using signals and slots.

    A watcher turns what a future does into signals: started(), resultReadyAt(), progressValueChanged(),
    canceled(), finished() and more. The signals come from the event loop of the thread that the watcher lives in,
    whichever thread reports, so slots can touch the objects of that thread.

    \code
    QFutureWatcher<int> watcher;
    QObject::connect(&watcher, &QFutureWatcherBase::resultReadyAt, [&](int i) { show(watcher.resultAt(i)); });
    QObject::connect(&watcher, &QFutureWatcherBase::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(future);
    \endcode

    The slots cancel(), suspend() and resume() control the watched computation. A watcher can be moved to
    another thread with QObject::moveToThread(), which takes the signals that are on their way along. Destroy a watcher
    before its thread's event loop ends.

    \differences
    The signals are not connected lazily: the watcher always listens to its future. The class has no
    \c QFutureWatcher<void> specialization; the functions that give results are simply absent for it.
    Results and progress that were reported before a cancellation, but not delivered, are dropped, as in Qt.
    The signals \c paused() and \c suspending() are both emitted when a suspension is requested.
    \enddifferences

    \sa QFuture, QPromise, QFutureSynchronizer
*/
template <typename T>
class QFutureWatcher : public QFutureWatcherBase {
public:
    /*!
        Constructs a watcher with the given \a parent. It watches no future until setFuture() is called.
    */
    explicit QFutureWatcher(QObject *parent = nullptr) : QFutureWatcherBase(parent) {}
    /*!
        Destroys the watcher. Signals that were not delivered yet are dropped.
    */
    ~QFutureWatcher() override { disconnectOutputInterface(); }

    /*!
        Starts watching \a future, and stops watching the previous one. Events for what the future did
        already, its start, results, progress and end, are delivered from the event loop as if the watcher had
        been there from the start. It does nothing if the watcher watches \a future already.

        \sa future()
    */
    void setFuture(const QFuture<T> &future) {
        if (future == m_future)
            return;
        disconnectOutputInterface(true);
        m_future = future;
        connectOutputInterface();
    }
    /*!
        Returns the watched future, which is invalid if there is none.

        \sa setFuture()
    */
    QFuture<T> future() const { return m_future; }
    /*!
        Waits for the first result of the watched future and returns a copy of it.

        \sa QFuture::result()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    U result() const {
        return m_future.result();
    }
    /*!
        Waits for the result at \a index of the watched future and returns a copy of it.

        \sa QFuture::resultAt()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    U resultAt(int index) const {
        return m_future.resultAt(index);
    }

private:
    QFutureInterfaceBase &futureInterface() const override { return QtPrivate::ContinuationAccess::iface(m_future); }

    QFuture<T> m_future;
};
