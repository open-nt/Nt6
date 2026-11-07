// MIT License - see LICENSE
#include <QTestEventLoop>

#include <QtCore/qcoreevent.h>

/*!
    \class QTestEventLoop
    \brief The QTestEventLoop class runs an event loop in a test, with a timeout.

    Use it to wait for something asynchronous when QTest::qWaitFor() or
    QSignalSpy::wait() do not fit. Connect the event to exitLoop(), enter the
    loop, and check timeout():

    \code
    QTestEventLoop &loop = QTestEventLoop::instance();
    QObject::connect(&socket, &QTcpSocket::connected, &loop, &QTestEventLoop::exitLoop);
    socket.connectToHost(QHostAddress::LocalHost, port);
    loop.enterLoop(5);
    QVERIFY(!loop.timeout());
    \endcode

    \differences
    \c enterLoopMSecs() and \c changeInterval() take an \c int number of
    milliseconds. There is no \c aboutToEnter handling of test failures.
    \enddifferences
*/

/*!
    Constructs an event loop object with the given \a parent.
*/
QTestEventLoop::QTestEventLoop(QObject *parent) : QObject(parent) {}

/*!
    Destroys the event loop object.
*/
QTestEventLoop::~QTestEventLoop() = default;

/*!
    Returns the shared event loop object.
*/
QTestEventLoop &QTestEventLoop::instance() {
    static QTestEventLoop loop;
    return loop;
}

/*!
    Runs the event loop until exitLoop() is called, or for at most \a seconds
    seconds.
*/
void QTestEventLoop::enterLoop(int seconds) { enterLoopMSecs(seconds * 1000); }

/*!
    \overload

    Runs the event loop for at most \a timeout.
*/
void QTestEventLoop::enterLoop(std::chrono::milliseconds timeout) {
    enterLoopMSecs(static_cast<int>(timeout.count()));
}

/*!
    Runs the event loop until exitLoop() is called, or for at most
    \a milliseconds milliseconds. Afterwards timeout() tells which it was.
*/
void QTestEventLoop::enterLoopMSecs(int milliseconds) {
    QEventLoop loop;
    m_loop = &loop;
    m_timedOut = false;
    m_timer = startTimer(milliseconds);
    loop.exec();
    m_loop = nullptr;
    if (m_timer) {
        killTimer(m_timer);
        m_timer = 0;
    }
}

/*!
    Restarts the timeout of the running loop, with \a milliseconds milliseconds
    left. It does nothing if the loop is not running.
*/
void QTestEventLoop::changeInterval(int milliseconds) {
    if (!m_loop)
        return;
    killTimer(m_timer);
    m_timer = startTimer(milliseconds);
}

/*!
    Returns \c true if the last run of the loop ended because of its timeout.
*/
bool QTestEventLoop::timeout() const noexcept { return m_timedOut; }

/*!
    Ends the running loop, if there is one.
*/
void QTestEventLoop::exitLoop() {
    if (m_loop)
        m_loop->quit();
}

/*!
    \reimp

    Ends the loop when the timeout of \a event elapses.
*/
void QTestEventLoop::timerEvent(QTimerEvent *event) {
    if (event->timerId() != m_timer) {
        QObject::timerEvent(event);
        return;
    }
    killTimer(m_timer);
    m_timer = 0;
    m_timedOut = true;
    exitLoop();
}
