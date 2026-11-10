// MIT License - see LICENSE
#include <QtCore/qexception.h>

#include <utility>

/*!
    \class QException
    \brief The QException class provides a base class for exceptions that can be transferred across threads.

    Throw a QException, or a class derived from it, from a function that runs
    in a thread pool or in a continuation, and the exception is stored in the
    QFuture of that function. It is thrown again in the thread that calls
    QFuture::waitForFinished() or QFuture::result(). A class derived from
    QException should reimplement raise() and clone() so that the stored
    exception keeps its type:

    \code
    class MyException : public QException {
    public:
        void raise() const override { throw *this; }
        MyException *clone() const override { return new MyException(*this); }
    };
    \endcode

    \differences
    QException is derived from \c std::exception, as in Qt, but the futures
    of Nt store any exception as a \c std::exception_ptr, so an exception of a
    class that does not reimplement raise() and clone() keeps its type when
    it travels as an exception pointer. Only QPromise::setException() with a
    QException argument goes through clone() and raise().
    \enddifferences

    \sa QUnhandledException, QFuture, QPromise
*/

/*!
    \fn QException::QException()

    Constructs an exception.
*/

/*!
    \fn QException::QException(const QException &)

    Constructs a copy of an exception.
*/

/*!
    \fn QException &QException::operator=(const QException &)

    Assigns another exception to this one and returns a reference to it.
*/

/*!
    Destroys the exception.
*/
QException::~QException() noexcept = default;

/*!
    Throws a copy of this exception. Reimplement it in a derived class to
    throw a copy of the derived class, for example \c{throw *this;}.

    \sa clone()
*/
void QException::raise() const {
    QException e = *this;
    throw e;
}

/*!
    Returns a copy of this exception, allocated with \c new. The caller owns
    it. Reimplement it in a derived class to return a copy of the derived
    class.

    \sa raise()
*/
QException *QException::clone() const { return new QException(*this); }

/*!
    \class QUnhandledException
    \brief The QUnhandledException class represents an unhandled exception in a worker thread.

    A QUnhandledException wraps a \c std::exception_ptr, so that an
    exception that is not a QException can be carried through the places that
    handle QException only. exception() returns the wrapped pointer.

    \code
    try {
        future.waitForFinished();
    } catch (const QUnhandledException &e) {
        if (e.exception())
            std::rethrow_exception(e.exception());
    }
    \endcode

    \differences
    The futures of Nt do not wrap exceptions in QUnhandledException on their
    own: an exception that is not a QException is stored and thrown again with
    its own type. The class is provided for code that wants to do the wrapping.
    \enddifferences

    \sa QException, QFuture
*/

/*!
    Constructs an exception that wraps \a exception.
*/
QUnhandledException::QUnhandledException(std::exception_ptr exception) noexcept : m_exception(std::move(exception)) {}

/*!
    Constructs a copy of \a other.
*/
QUnhandledException::QUnhandledException(const QUnhandledException &other) noexcept
    : QException(other), m_exception(other.m_exception) {}

/*!
    Assigns \a other to this exception and returns a reference to it.
*/
QUnhandledException &QUnhandledException::operator=(const QUnhandledException &other) noexcept {
    QException::operator=(other);
    m_exception = other.m_exception;
    return *this;
}

/*!
    Destroys the exception.
*/
QUnhandledException::~QUnhandledException() noexcept = default;

/*!
    \reimp

    Throws a copy of this exception.
*/
void QUnhandledException::raise() const {
    QUnhandledException e = *this;
    throw e;
}

/*!
    \reimp

    Returns a copy of this exception, allocated with \c new.
*/
QUnhandledException *QUnhandledException::clone() const { return new QUnhandledException(*this); }

/*!
    Returns the exception that this object wraps, or a null pointer if it is
    empty.
*/
std::exception_ptr QUnhandledException::exception() const { return m_exception; }
