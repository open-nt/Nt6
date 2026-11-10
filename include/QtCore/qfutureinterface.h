// MIT License - see LICENSE
#pragma once

#include <QtCore/qexception.h>
#include <QtCore/qglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QThreadPool;
class QFutureCallOutInterface;
class QFutureInterfaceBase;
template <typename T>
class QFuture;
template <typename T>
class QPromise;
class QFutureInterfaceBasePrivate;

namespace QtPrivate {

// Frees a stored result: a single object, or a QList of results when `isVector` is set.
using ResultDeleter = void (*)(void *item, bool isVector);

// Where a result lives: the stored item, and the position in it when the item is a QList.
struct ResultLocation {
    void *item = nullptr;
    int offset = 0;
    bool isVector = false;
};

// Reaches the shared state of a future for the future classes and the continuation machinery.
struct FutureAccess {
    // Calls `fn` with the finished interface once it has finished, in the thread that finishes it, or
    // at once if it has. Returns false, and keeps `fn` unused, if a continuation is already set.
    static bool setContinuation(const QFutureInterfaceBase &parent, std::function<void(const QFutureInterfaceBase &)> fn);
    // Lets cancelChain() of `child` reach `parent` without keeping it alive.
    static void setParent(const QFutureInterfaceBase &child, const QFutureInterfaceBase &parent);
    static std::exception_ptr exception(const QFutureInterfaceBase &iface);
    static void markInvalid(const QFutureInterfaceBase &iface);
    static void connectOutput(const QFutureInterfaceBase &iface, QFutureCallOutInterface *out);
    static void disconnectOutput(const QFutureInterfaceBase &iface, QFutureCallOutInterface *out);
    static bool sameState(const QFutureInterfaceBase &a, const QFutureInterfaceBase &b);
    // Sets the throttle flag from a callout, which runs with the lock of the future held.
    static void setThrottledFromCallOut(const QFutureInterfaceBase &iface, bool enable);
};

} // namespace QtPrivate

class QFutureInterfaceBase {
public:
    /*!
        \enum QFutureInterfaceBase::State

        This enum describes the state of a computation. A future is in a combination of these.

        \value NoState No flag is set; the state of a default-constructed interface.
        \value Running The computation has been started and has not finished.
        \value Started The computation has been started. The flag stays set.
        \value Finished The computation has finished.
        \value Canceled The computation was canceled, or ended with an exception.
        \value Suspending A suspension was requested, and the computation has not acted on it yet.
        \value Suspended The computation has acted on a suspension request and waits to be resumed.
        \value Throttled The consumer cannot keep up with the results and asks the computation to slow down.
        \value Pending The computation has not started, but is expected to; waiting for it blocks.
    */
    enum State {
        NoState = 0x00,
        Running = 0x01,
        Started = 0x02,
        Finished = 0x04,
        Canceled = 0x08,
        Suspending = 0x10,
        Suspended = 0x20,
        Throttled = 0x40,
        Pending = 0x80
    };

    QFutureInterfaceBase(State initialState = NoState);
    QFutureInterfaceBase(const QFutureInterfaceBase &other);
    QFutureInterfaceBase(QFutureInterfaceBase &&other) noexcept;
    QFutureInterfaceBase &operator=(const QFutureInterfaceBase &other);
    QFutureInterfaceBase &operator=(QFutureInterfaceBase &&other) noexcept;
    virtual ~QFutureInterfaceBase();

    bool isValid() const;
    bool isStarted() const;
    bool isRunning() const;
    bool isFinished() const;
    bool isCanceled() const;
    bool isSuspending() const;
    bool isSuspended() const;
    bool isPaused() const;
    bool isThrottled() const;
    bool hasException() const;
    bool queryState(State state) const;

    void reportStarted();
    void reportFinished();
    void reportCanceled();
    void reportException(std::exception_ptr exception);
    void reportException(const QException &exception);
    void reportSuspended() const;

    void cancel();
    void cancelChain();
    void setSuspended(bool suspend);
    void toggleSuspended();
    void setPaused(bool paused);
    void togglePaused();
    void setThrottled(bool enable);

    void waitForFinished() const;
    void waitForResult(int resultIndex) const;
    void waitForResume() const;

    int resultCount() const;
    bool isResultReadyAt(int index) const;
    void setExpectedResultCount(int resultCount);
    int expectedResultCount() const;

    int progressMinimum() const;
    int progressMaximum() const;
    int progressValue() const;
    QString progressText() const;
    void setProgressRange(int minimum, int maximum);
    void setProgressValue(int progressValue);
    void setProgressValueAndText(int progressValue, const QString &progressText);

    void setThreadPool(QThreadPool *pool);
    QThreadPool *threadPool() const;

    bool operator==(const QFutureInterfaceBase &other) const;
    bool operator!=(const QFutureInterfaceBase &other) const;

protected:
    /*!
        \internal
        Stores `item`, which holds `count` consecutive results from `index` (-1: after the last one). Returns the
        index of the first result, or -1 and leaves the item to the caller if the future no longer takes results.
    */
    int storeResult(int index, void *item, int count, bool isVector, QtPrivate::ResultDeleter deleter);
    /*!
        \internal
    */
    QtPrivate::ResultLocation locateResult(int index) const;
    /*!
        \internal
        Throws the exception that the computation ended with, if there is one.
    */
    void throwPossibleException() const;

private:
    friend struct QtPrivate::FutureAccess;
    explicit QFutureInterfaceBase(std::shared_ptr<QFutureInterfaceBasePrivate> d);

    std::shared_ptr<QFutureInterfaceBasePrivate> d;
};

/*!
    \class QFutureInterface
    \brief The QFutureInterface class allows the compution to report its state and results to a QFuture.

    QFutureInterface is the base that QFuture is built on, and the older way to write a computation; QPromise
    is the one to prefer. An interface and the futures that it makes share the state of the computation, and
    the results are freed when the last of them goes. Every function can be called from any thread.

    \differences
    The interface has no \c reportResults() with a count of results and no filter mode. A result that is
    reported while the future is canceled or finished is dropped and reported as such by the return value.
    \enddifferences

    \sa QPromise, QFuture, QFutureInterfaceBase
*/
template <typename T>
class QFutureInterface : public QFutureInterfaceBase {
    static void destroyItem(void *item, bool isVector) {
        if (isVector)
            delete static_cast<QList<T> *>(item);
        else
            delete static_cast<T *>(item);
    }

public:
    /*!
        Constructs an interface in the state \a initialState.
    */
    QFutureInterface(State initialState = NoState) : QFutureInterfaceBase(initialState) {}
    /*!
        Constructs an interface that shares the state of \a dd, which must come from a
        QFutureInterface of the same result type.
    */
    explicit QFutureInterface(const QFutureInterfaceBase &dd) : QFutureInterfaceBase(dd) {}
    /*!
        Constructs an interface that shares its state with \a other.
    */
    QFutureInterface(const QFutureInterface &other) = default;
    /*!
        Moves the state of \a other into this interface. \a other can only be destroyed or assigned to afterwards.
    */
    QFutureInterface(QFutureInterface &&other) noexcept = default;
    /*!
        Shares the state of \a other with this interface and returns a reference to it.
    */
    QFutureInterface &operator=(const QFutureInterface &other) = default;
    /*!
        Moves the state of \a other into this interface and returns a reference to it.
    */
    QFutureInterface &operator=(QFutureInterface &&other) noexcept = default;
    /*!
        Destroys the interface. The results are freed with the last interface or future that shares the state.
    */
    ~QFutureInterface() override = default;

    /*!
        Returns a future that observes the state of this interface.
    */
    QFuture<T> future();

    /*!
        Returns an interface in the state of a canceled and finished computation without results. A default-constructed
        QFuture uses it.
    */
    static QFutureInterface canceledResult() {
        QFutureInterface iface(State(Started | Finished | Canceled));
        QtPrivate::FutureAccess::markInvalid(iface);
        return iface;
    }

    /*!
        Copies \a result into the future at \a index, or after the last result if \a index is -1, and
        returns \c true. Returns \c false, and drops \a result, if the future is finished or canceled, if \a result is
        \c nullptr or if there already is a result at \a index.
    */
    bool reportResult(const T *result, int index = -1) {
        if (!result)
            return false;
        return reportAndEmplaceResult(index, *result);
    }
    /*!
        \overload
        Copies \a result into the future at \a index.
    */
    bool reportResult(const T &result, int index = -1) { return reportAndEmplaceResult(index, result); }
    /*!
        Moves \a result into the future at \a index, or after the last result if \a index is -1. Returns
        \c false if the result was not stored.
    */
    bool reportAndMoveResult(T &&result, int index = -1) { return reportAndEmplaceResult(index, std::move(result)); }
    /*!
        Constructs a result from \a args in the future at \a index, or after the last result if \a index is -1.
        Returns \c false if the result was not stored.
    */
    template <typename... Args>
    bool reportAndEmplaceResult(int index, Args &&...args) {
        if (isFinished() || isCanceled())
            return false;
        auto *item = new T(std::forward<Args>(args)...);
        if (storeResult(index, item, 1, false, &destroyItem) < 0) {
            delete item;
            return false;
        }
        return true;
    }
    /*!
        Copies \a results into the future, starting at \a beginIndex or after the last result if it is -1. All of
        them become available together. Returns \c false if they were not stored.
    */
    bool reportResults(const QList<T> &results, int beginIndex = -1) {
        if (results.isEmpty() || isFinished() || isCanceled())
            return false;
        auto *item = new QList<T>(results);
        if (storeResult(beginIndex, item, static_cast<int>(item->size()), true, &destroyItem) < 0) {
            delete item;
            return false;
        }
        return true;
    }
    /*!
        Reports \a result, if it is not \c nullptr, as the last result, and then finishes the computation.
    */
    void reportFinished(const T *result) {
        if (result)
            reportResult(result);
        QFutureInterfaceBase::reportFinished();
    }
    /*!
        Finishes the computation.
    */
    void reportFinished() { QFutureInterfaceBase::reportFinished(); }

    /*!
        Returns a reference to the result at \a index, which must be available; see waitForResult() and
        isResultReadyAt(). The reference stays valid as long as the future shares this state.
    */
    const T &resultReference(int index) const {
        const QtPrivate::ResultLocation loc = locateResult(index);
        Q_ASSERT(loc.item);
        if (loc.isVector)
            return static_cast<const QList<T> *>(loc.item)->at(loc.offset);
        return *static_cast<const T *>(loc.item);
    }
    /*!
        Returns \c true if the result at \a index exists, and stores a pointer to it in \a pointer; otherwise
        returns \c false.
    */
    bool resultPointer(int index, const T **pointer) const {
        const QtPrivate::ResultLocation loc = locateResult(index);
        if (!loc.item)
            return false;
        *pointer = loc.isVector ? &static_cast<const QList<T> *>(loc.item)->at(loc.offset)
                                : static_cast<const T *>(loc.item);
        return true;
    }
    /*!
        Waits for the first result and returns a copy of it. Returns a default-constructed value if the
        computation ends without one. Throws the exception that the computation ended with, if any.
    */
    T result() const {
        waitForResult(0);
        const T *p = nullptr;
        if (resultPointer(0, &p))
            return *p;
        if constexpr (std::is_default_constructible_v<T>)
            return T();
        else
            std::terminate();
    }
    /*!
        Waits for the computation to finish and returns a copy of all results. Returns an empty list
        if the computation was canceled. Throws the exception that the computation ended with, if any.
    */
    QList<T> results() {
        if (isCanceled()) {
            throwPossibleException();
            return QList<T>();
        }
        QFutureInterfaceBase::waitForFinished();
        QList<T> res;
        const int n = resultCount();
        res.reserve(n);
        for (int i = 0; i < n; ++i)
            res.append(resultReference(i));
        return res;
    }
    /*!
        Waits for the first result and moves it out of the future. The result is left in its moved-from
        state, so no other consumer should read it.
    */
    T takeResult() {
        waitForResult(0);
        const QtPrivate::ResultLocation loc = locateResult(0);
        if (!loc.item) {
            if constexpr (std::is_default_constructible_v<T>)
                return T();
            else
                std::terminate();
        }
        if (loc.isVector)
            return std::move((*static_cast<QList<T> *>(loc.item))[loc.offset]);
        return std::move(*static_cast<T *>(loc.item));
    }
};

template <>
class QFutureInterface<void> : public QFutureInterfaceBase {
public:
    QFutureInterface(State initialState = NoState) : QFutureInterfaceBase(initialState) {}
    explicit QFutureInterface(const QFutureInterfaceBase &dd) : QFutureInterfaceBase(dd) {}
    QFutureInterface(const QFutureInterface &other) = default;
    QFutureInterface(QFutureInterface &&other) noexcept = default;
    QFutureInterface &operator=(const QFutureInterface &other) = default;
    QFutureInterface &operator=(QFutureInterface &&other) noexcept = default;
    ~QFutureInterface() override = default;
    QFuture<void> future();
    static QFutureInterface canceledResult() {
        QFutureInterface iface(State(Started | Finished | Canceled));
        QtPrivate::FutureAccess::markInvalid(iface);
        return iface;
    }
    void reportResult(const void *, int = -1) {}
    void reportFinished() { QFutureInterfaceBase::reportFinished(); }
};
