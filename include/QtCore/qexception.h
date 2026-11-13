// MIT License - see LICENSE
#pragma once

#include <utility>

#include <QtCore/qglobal.h>

#include <exception>

/*!
    \class QException
    \brief The QException class provides a base class for exceptions that can transferred across threads.

    An exception that has to travel from a worker thread to the thread that
    waits for it, for example through a QFuture, must be copyable without
    knowing its dynamic type. Derive from QException and reimplement raise()
    and clone():

    \code
    class MyException : public QException {
    public:
        void raise() const override { throw *this; }
        MyException *clone() const override { return new MyException(*this); }
    };
    \endcode

    An exception that does not derive from QException reaches the waiting
    thread wrapped in a QUnhandledException.

    \sa QUnhandledException
*/
class QException : public std::exception {
public:
    /*!
        Destroys the exception.
    */
    ~QException() noexcept override;
    virtual void raise() const;
    virtual QException *clone() const;
};

/*!
    \class QUnhandledException
    \brief The QUnhandledException class represents an unhandled exception in a worker thread.

    It carries the \c std::exception_ptr of an exception that is not a
    QException, so that it can be rethrown in another thread. Use
    exception() to get at the original.

    \sa QException
*/
class QUnhandledException : public QException {
public:
    /*!
        Constructs a QUnhandledException holding \a exception, which is the
        current exception by default.
    */
    QUnhandledException(std::exception_ptr exception = std::current_exception()) noexcept
        : m_exception(std::move(exception)) {}
    QUnhandledException(const QUnhandledException &other) noexcept;
    ~QUnhandledException() noexcept override;
    /*!
        \since 6.0
        Exchanges this exception with \a other.
    */
    void swap(QUnhandledException &other) noexcept { std::swap(m_exception, other.m_exception); }
    void raise() const override;
    QUnhandledException *clone() const override;
    /*!
        Returns the exception that this one stands for.
    */
    std::exception_ptr exception() const { return m_exception; }

private:
    std::exception_ptr m_exception;
};
