// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <utility>

/*!
    \class QScopeGuard
    \brief The QScopeGuard class calls a function when it goes out of scope.

    Use qScopeGuard() to create a guard from a callable. The callable runs
    when the guard is destroyed unless dismiss() was called.

    \code
    auto guard = qScopeGuard([&] { file.close(); });
    \endcode

    \differences
    The constructor accepts only an rvalue callable, and there is no
    assignment operator.
    \enddifferences

    \sa QScopedValueRollback, QSignalBlocker
*/
template <typename F>
class QScopeGuard {
public:
    /*!
        Constructs a guard that calls \a f on destruction.
    */
    explicit QScopeGuard(F &&f) noexcept : func(std::move(f)) {}
    /*!
        Move-constructs a guard from \a o. The moved-from guard no longer
        calls its function.
    */
    QScopeGuard(QScopeGuard &&o) noexcept : func(std::move(o.func)), active(o.active) { o.active = false; }
    QScopeGuard(const QScopeGuard &) = delete;
    /*!
        Calls the function unless the guard was dismissed.
    */
    ~QScopeGuard() { if (active) func(); }
    /*!
        Dismisses the guard so that the function is not called on
        destruction.
    */
    void dismiss() noexcept { active = false; }

private:
    F func;
    bool active = true;
};

template <typename F>
QScopeGuard<std::decay_t<F>> qScopeGuard(F &&f) { return QScopeGuard<std::decay_t<F>>(std::forward<F>(f)); }

/*!
    \class QSignalBlocker
    \brief The QSignalBlocker class exception-safely wraps QObject::blockSignals().

    A QSignalBlocker blocks the signals of an object for as long as it
    exists, and restores the previous blocked state on destruction.

    \differences
    There is no move constructor or move assignment.
    \enddifferences

    \sa QObject::blockSignals()
*/
class QSignalBlocker {
public:
    /*!
        Constructs a blocker that blocks the signals of \a o. Does nothing if
        \a o is \c nullptr.
    */
    explicit QSignalBlocker(QObject *o) noexcept : obj(o), previous(o ? o->blockSignals(true) : false) {}
    /*!
        \overload
        Constructs a blocker that blocks the signals of \a o.
    */
    explicit QSignalBlocker(QObject &o) noexcept : QSignalBlocker(&o) {}
    /*!
        Restores the object's previous signal-blocking state.
    */
    ~QSignalBlocker() { if (obj) obj->blockSignals(previous); }
    QSignalBlocker(const QSignalBlocker &) = delete;
    QSignalBlocker &operator=(const QSignalBlocker &) = delete;
    /*!
        Blocks the object's signals again after unblock().

        \sa unblock()
    */
    void reblock() noexcept { if (obj) obj->blockSignals(true); }
    /*!
        Restores the object's previous signal-blocking state, as the
        destructor would, without destroying the blocker.

        \sa reblock()
    */
    void unblock() noexcept { if (obj) obj->blockSignals(previous); }

private:
    QObject *obj;
    bool previous;
};

/*!
    \class QScopedValueRollback
    \brief The QScopedValueRollback class resets a variable to its previous value on destruction.

    A QScopedValueRollback saves the value of a variable when constructed
    and assigns it back when destroyed, unless commit() was called.

    \differences
    There is no move constructor or assignment.
    \enddifferences

    \sa QScopeGuard
*/
template <typename T>
class QScopedValueRollback {
public:
    /*!
        Constructs a rollback that saves the current value of \a v and
        restores it on destruction.
    */
    explicit QScopedValueRollback(T &v) : ref(v), saved(v) {}
    /*!
        \overload
        Saves the current value of \a v, assigns \a value to \a v, and
        restores the saved value on destruction.
    */
    QScopedValueRollback(T &v, T value) : ref(v), saved(v) { ref = std::move(value); }
    /*!
        Assigns the saved value back to the variable.
    */
    ~QScopedValueRollback() { ref = std::move(saved); }
    QScopedValueRollback(const QScopedValueRollback &) = delete;
    /*!
        Saves the variable's current value, so that destruction restores
        that value instead of the original one.
    */
    void commit() { saved = ref; }

private:
    T &ref;
    T saved;
};
