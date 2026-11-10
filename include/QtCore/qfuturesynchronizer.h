// MIT License - see LICENSE
#pragma once

#include <QtCore/qfuture.h>
#include <QtCore/qlist.h>

/*!
    \class QFutureSynchronizer
    \brief The QFutureSynchronizer class is a convenience class that simplifies QFuture synchronization.

    A synchronizer collects futures and waits for all of them when it is destroyed, so that the code that
    started the computations does not leave their data behind. With setCancelOnWait() it cancels them first.

    \code
    {
        QFutureSynchronizer<void> sync;
        sync.addFuture(startJob(one));
        sync.addFuture(startJob(two));
    } // waits for both jobs
    \endcode

    \differences
    The destructor does not throw: an exception that a future ended with is dropped there, and thrown only by
    waitForFinished(). There is no \c setFutures().
    \enddifferences

    \sa QFuture, QFutureWatcher
*/
template <typename T = void>
class QFutureSynchronizer {
public:
    /*!
        Constructs a synchronizer that holds no futures.
    */
    QFutureSynchronizer() = default;
    /*!
        Constructs a synchronizer that holds \a future.

        \sa addFuture()
    */
    explicit QFutureSynchronizer(const QFuture<T> &future) : m_futures{future} {}
    /*!
        Moves the futures of \a other into this synchronizer. \a other holds none afterwards.
    */
    QFutureSynchronizer(QFutureSynchronizer &&other) noexcept = default;
    /*!
        Waits for the futures that this synchronizer holds, as the destructor does, and then takes over those of
        \a other. Returns a reference to this synchronizer.
    */
    QFutureSynchronizer &operator=(QFutureSynchronizer &&other) noexcept {
        if (this != &other) {
            finishQuietly();
            m_futures = std::move(other.m_futures);
            m_cancelOnWait = other.m_cancelOnWait;
            other.m_futures.clear();
        }
        return *this;
    }
    Q_DISABLE_COPY(QFutureSynchronizer)
    /*!
        Waits for all the futures, after canceling them if cancelOnWait() is \c true. An exception that a future
        ended with is not thrown from here.

        \sa waitForFinished()
    */
    ~QFutureSynchronizer() { finishQuietly(); }

    /*!
        Waits for the futures that the synchronizer holds, forgets them, and then holds \a future only.

        \sa addFuture()
    */
    void setFuture(const QFuture<T> &future) {
        waitForFinished();
        m_futures.clear();
        addFuture(future);
    }
    /*!
        Adds \a future to the futures that the synchronizer holds.

        \sa setFuture(), futures()
    */
    void addFuture(const QFuture<T> &future) { m_futures.append(future); }
    /*!
        Waits for all the futures that the synchronizer holds, after canceling them if cancelOnWait() is \c true. It
        throws the exception of the first future that ended with one, after it has waited for all.

        \sa clearFutures(), setCancelOnWait()
    */
    void waitForFinished() {
        if (m_cancelOnWait)
            for (QFuture<T> &f : m_futures)
                f.cancel();
        std::exception_ptr first;
        for (QFuture<T> &f : m_futures) {
            try {
                f.waitForFinished();
            } catch (...) {
                if (!first)
                    first = std::current_exception();
            }
        }
        if (first)
            std::rethrow_exception(first);
    }
    /*!
        Forgets the futures without waiting for them.

        \sa waitForFinished()
    */
    void clearFutures() { m_futures.clear(); }
    /*!
        Returns the futures that the synchronizer holds.

        \sa addFuture()
    */
    QList<QFuture<T>> futures() const { return m_futures; }
    /*!
        Sets whether the futures are canceled before waitForFinished() or the destructor waits for them to
        \a enabled. It is \c false by default.

        \sa cancelOnWait()
    */
    void setCancelOnWait(bool enabled) { m_cancelOnWait = enabled; }
    /*!
        Returns \c true if the futures are canceled before waiting for them.

        \sa setCancelOnWait()
    */
    bool cancelOnWait() const { return m_cancelOnWait; }

private:
    void finishQuietly() noexcept {
        try {
            waitForFinished();
        } catch (...) {
        }
    }

    QList<QFuture<T>> m_futures;
    bool m_cancelOnWait = false;
};
