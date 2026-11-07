// MIT License - see LICENSE
#include <QtCore/qexception.h>

QException::~QException() noexcept = default;

/*!
    Throws this exception, as an object of its own type. Reimplement it in
    every subclass so that the exception can be rethrown in another thread;
    the default throws a QException, which loses the subclass.

    \sa clone()
*/
void QException::raise() const { throw QException(*this); }

/*!
    Returns a copy of this exception, allocated with \c new, of its own type.
    Reimplement it in every subclass; the default returns a QException.

    \sa raise()
*/
QException *QException::clone() const { return new QException(*this); }

/*!
    Constructs a copy of \a other.
*/
QUnhandledException::QUnhandledException(const QUnhandledException &other) noexcept = default;

/*!
    Destroys the exception.
*/
QUnhandledException::~QUnhandledException() noexcept = default;

/*!
    Throws a copy of this exception.

    \sa clone()
*/
void QUnhandledException::raise() const { throw QUnhandledException(*this); }

/*!
    Returns a copy of this exception, allocated with \c new.

    \sa raise()
*/
QUnhandledException *QUnhandledException::clone() const { return new QUnhandledException(*this); }
