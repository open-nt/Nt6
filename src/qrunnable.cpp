// MIT License - see LICENSE
#include <QtCore/qrunnable.h>

/*!
    \class QRunnable
    \brief The QRunnable class is the base class for all runnable objects.

    A QRunnable is a task for a thread pool: subclass it, reimplement run() and hand
    the object to QThreadPool::start(). The pool runs it in one of its threads and, if
    autoDelete() is \c true, deletes it afterwards.

    \code
    class Hello : public QRunnable {
    public:
        void run() override { qDebug() << "Hello from a pool thread"; }
    };
    QThreadPool::globalInstance()->start(new Hello);
    \endcode

    \differences
    None.
    \enddifferences

    \sa QThreadPool
*/

/*!
    \fn void QRunnable::run()

    Implement this pure virtual function in your subclass. It is called in a thread of the
    pool that runs the object.
*/

/*!
    \fn bool QRunnable::autoDelete() const

    Returns \c true if the thread pool deletes this object after run() returns. The default
    is \c true.

    \sa setAutoDelete()
*/

/*!
    \fn void QRunnable::setAutoDelete(bool autoDelete)

    Sets whether the thread pool deletes this object after run() returns to \a autoDelete.
    Change it before the runnable is passed to a pool; changing it afterwards has an
    undefined effect.

    \sa autoDelete()
*/

/*!
    \fn QRunnable::QRunnable()

    Constructs a runnable that the thread pool deletes after it ran.
*/

/*!
    Destroys the runnable.
*/
QRunnable::~QRunnable() = default;

namespace {

class FunctionRunnable : public QRunnable {
public:
    explicit FunctionRunnable(std::function<void()> fn) : m_fn(std::move(fn)) {}
    void run() override { m_fn(); }

private:
    std::function<void()> m_fn;
};

} // namespace

/*!
    Creates a QRunnable that calls \a functionToRun from run(). The runnable has
    autoDelete() set to \c true, so the thread pool that runs it deletes it. The caller owns
    the object until it is passed to a pool.

    \sa QThreadPool::start()
*/
QRunnable *QRunnable::create(std::function<void()> functionToRun) {
    return new FunctionRunnable(std::move(functionToRun));
}
