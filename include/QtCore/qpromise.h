// MIT License - see LICENSE
#pragma once

#include <QtCore/qexception.h>
#include <QtCore/qfutureinterface.h>
#include <QtCore/qglobal.h>

#include <exception>
#include <type_traits>
#include <utility>

/*!
    \class QPromise
    \brief The QPromise class provides a way to store computation results to be accessed by QFuture.

    A QPromise is the writing end of a QFuture. The computation calls start(), reports results with
    addResult(), looks at isCanceled() and suspendIfRequested() now and then, and calls finish() when done. The
    reading side is future().

    \code
    QPromise<int> promise;
    QFuture<int> future = promise.future();
    pool.start([p = std::make_shared<QPromise<int>>(std::move(promise))] {
        p->start();
        for (int i = 0; i < 100 && !p->isCanceled(); ++i)
            p->addResult(i);
        p->finish();
    });
    \endcode

    A promise can be moved but not copied. If it is destroyed before finish() was called, the future is
    canceled and finished, so nobody waits for it forever. Every function can be called from any thread, but only
    one thread should report.

    \differences
    The constructor leaves the future pending, which waiting blocks on, until start() or finish() is called.
    addResults() and emplaceResultAt() are additions. A moved-from promise can only be destroyed or assigned to.
    \enddifferences

    \sa QFuture, QFutureInterface
*/
template <typename T>
class QPromise {
public:
    /*!
        Constructs a promise whose future is pending: waiting for it blocks until the promise finishes, and
        isStarted() of the future is \c false until start() is called.

        \sa future(), start()
    */
    QPromise() : d(QFutureInterfaceBase::Pending) {}
    /*!
        Constructs a promise that completes the computation that \a other observes. At most one promise should
        exist for a computation.
    */
    explicit QPromise(const QFutureInterface<T> &other) : d(other) {}
    /*!
        Moves \a other into this promise. \a other can only be destroyed or assigned to afterwards.
    */
    QPromise(QPromise &&other) noexcept : d(std::move(other.d)) {}
    /*!
        Finishes the computation of this promise, as the destructor does, and then takes over the one of
        \a other. Returns a reference to this promise.
    */
    QPromise &operator=(QPromise &&other) noexcept {
        if (this != &other) {
            abandon();
            d = std::move(other.d);
        }
        return *this;
    }
    Q_DISABLE_COPY(QPromise)
    /*!
        Destroys the promise. A computation that has not finished is canceled and finished, so that the
        threads that wait for it wake up. Waiting then throws nothing, and the future has no further results.
    */
    ~QPromise() { abandon(); }

    /*!
        Swaps this promise with \a other.
    */
    void swap(QPromise &other) noexcept { std::swap(d, other.d); }

    /*!
        Returns a future for the computation of this promise. It can be called any number of times, from any thread.
    */
    QFuture<T> future() const { return QFuture<T>(d); }

    /*!
        Stores a copy of, or moves, \a result as the next result, or at \a index. Returns \c true if it was
        stored. It is not if the computation was canceled or finished, or if there already is a result at \a index.

        \sa emplaceResult(), addResults()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<std::decay_t<U>>>>
    bool addResult(U &&result, int index = -1) {
        return d.reportAndEmplaceResult(index, std::forward<U>(result));
    }
    /*!
        Constructs a result in place from \a args as the next result. Returns \c true if it was stored.

        \sa addResult(), emplaceResultAt()
    */
    template <typename... Args>
    bool emplaceResult(Args &&...args) {
        return d.reportAndEmplaceResult(-1, std::forward<Args>(args)...);
    }
    /*!
        Constructs a result in place from \a args at \a index. Returns \c true if it was stored.

        \sa emplaceResult()
    */
    template <typename... Args>
    bool emplaceResultAt(int index, Args &&...args) {
        return d.reportAndEmplaceResult(index, std::forward<Args>(args)...);
    }
    /*!
        Stores a copy of \a results as consecutive results, from \a beginIndex or after the last one. They become
        available together. Returns \c true if they were stored.

        \sa addResult()
    */
    template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
    bool addResults(const QList<U> &results, int beginIndex = -1) {
        return d.reportResults(results, beginIndex);
    }

    /*!
        Stores \a e, and cancels the computation. Waiting for the future then throws \a e. It is
        copied with clone(), so a class derived from QException keeps its type if it reimplements it.

        \sa finish()
    */
    void setException(const QException &e) { d.reportException(e); }
    /*!
        \overload
        Stores \a e, which can be any exception, and cancels the computation.
    */
    void setException(std::exception_ptr e) { d.reportException(std::move(e)); }

    /*!
        Reports that the computation has started. The state of the future changes to running.

        \sa finish(), QFuture::isStarted()
    */
    void start() { d.reportStarted(); }
    /*!
        Reports that the computation has finished, wakes the threads that wait for the future and runs its
        continuation. Calling it again does nothing.

        \sa start()
    */
    void finish() { d.reportFinished(); }
    /*!
        Returns \c true if the future was canceled. The computation should then stop and call finish().

        \sa QFuture::cancel()
    */
    bool isCanceled() const { return d.isCanceled(); }
    /*!
        Blocks while the future is suspended, until it is resumed or canceled. Call it regularly in a long
        computation to honour QFuture::suspend(). Returns at once if no suspension was requested.

        \sa QFuture::suspend(), QFuture::resume()
    */
    void suspendIfRequested() { d.waitForResume(); }

    /*!
        Sets the range of progress values to \a minimum to \a maximum.

        \sa setProgressValue()
    */
    void setProgressRange(int minimum, int maximum) { d.setProgressRange(minimum, maximum); }
    /*!
        Sets the progress value to \a progressValue.

        \sa setProgressValueAndText()
    */
    void setProgressValue(int progressValue) { d.setProgressValue(progressValue); }
    /*!
        Sets the progress value to \a progressValue and the progress text to \a progressText.

        \sa setProgressValue()
    */
    void setProgressValueAndText(int progressValue, const QString &progressText) {
        d.setProgressValueAndText(progressValue, progressText);
    }

private:
    // A promise that was moved from has no state to finish.
    void abandon() {
        if (!d.isValid() || d.isFinished())
            return;
        d.reportStarted();
        d.cancel();
        d.reportFinished();
    }

    QFutureInterface<T> d;
};

#include <QtCore/qfuture.h>
