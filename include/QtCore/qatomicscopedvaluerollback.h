// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <atomic>

/*!
    \class QAtomicScopedValueRollback
    \brief The QAtomicScopedValueRollback class resets an atomic variable to its previous value on destruction.

    It is the counterpart of QScopedValueRollback for \c std::atomic variables.
    The object stores the variable's value when it is created, and writes it
    back when it goes out of scope, unless commit() was called:

    \code
    std::atomic<int> state = 0;
    {
        QAtomicScopedValueRollback<int> rollback(state, 1);
        // state is 1 here; it is 0 again after the block
    }
    \endcode

    \differences
    The variable is a \c std::atomic<T>; Qt also accepts a \c QAtomicInteger
    and a \c QAtomicPointer.
    \enddifferences

    \sa QScopedValueRollback
*/
template <typename T> class QAtomicScopedValueRollback {
public:
    /*!
        Constructs an object that restores \a var to its current value when it is
        destroyed. The value is read and written with the memory order \a mo.
    */
    explicit QAtomicScopedValueRollback(std::atomic<T> &var, std::memory_order mo = std::memory_order_seq_cst)
        : m_var(var), m_mo(mo), m_saved(var.load(mo)) {}
    /*!
        \overload

        Stores \a value in \a var and restores the previous value when the
        object is destroyed.
    */
    QAtomicScopedValueRollback(std::atomic<T> &var, T value, std::memory_order mo = std::memory_order_seq_cst)
        : m_var(var), m_mo(mo), m_saved(var.exchange(value, mo)) {}
    /*!
        Restores the saved value, unless commit() was called.
    */
    ~QAtomicScopedValueRollback() {
        if (m_rollback)
            m_var.store(m_saved, m_mo);
    }
    QAtomicScopedValueRollback(const QAtomicScopedValueRollback &) = delete;
    QAtomicScopedValueRollback &operator=(const QAtomicScopedValueRollback &) = delete;

    /*!
        Makes the variable keep its current value: nothing is restored when
        the object is destroyed.
    */
    void commit() { m_rollback = false; }

private:
    std::atomic<T> &m_var;
    std::memory_order m_mo;
    T m_saved;
    bool m_rollback = true;
};
