// MIT License - see LICENSE
#include <QtCore/qeventloop.h>

#include "dispatcher.h"
#include "threaddata_p.h"

#include <atomic>
#include <mutex>

struct QEventLoop::Data {
    std::atomic<bool> exitFlag{false};
    std::atomic<int> code{0};
    std::atomic<bool> running{false};
    // While the loop runs: the thread it runs in, with a reference. exit() may come from any thread.
    std::mutex mutex;
    QtPrivate::ThreadData *td = nullptr;
};

/*!
    \class QEventLoop
    \brief The QEventLoop class provides a means of entering and leaving an event loop.

    An event loop dispatches timers, posted events and socket notifiers.
    Call exec() to run it until quit() or exit() is called, for example from
    a slot:

    \code
    QEventLoop loop;
    QTimer::singleShot(1000, [&loop] { loop.quit(); });
    loop.exec();
    \endcode

    \differences
    \c ExcludeUserInputEvents and \c ExcludeSocketNotifiers are accepted but
    ignored, and there is no \c processEvents() overload with a timeout and
    no \c QEventLoopLocker or \c QAbstractEventDispatcher. A loop always runs
    the event dispatcher of the thread that calls exec() or processEvents().
    \enddifferences

    \sa QCoreApplication, QTimer, QSocketNotifier
*/

/*!
    Constructs an event loop with the given \a parent.
*/
QEventLoop::QEventLoop(QObject *parent) : QObject(parent), m_d(new Data) {}

/*!
    Destroys the event loop.
*/
QEventLoop::~QEventLoop() = default;

/*!
    Processes pending events once, according to \a flags, and returns
    \c true if any event, timer or notifier was handled. With
    \c WaitForMoreEvents it blocks until something is ready.

    \sa exec()
*/
bool QEventLoop::processEvents(ProcessEventsFlags flags) {
    const bool wait = flags.testFlag(WaitForMoreEvents);
    return QtPrivate::ThreadData::current()->dispatcher.process(wait, m_d->running ? &m_d->exitFlag : nullptr);
}

/*!
    Enters the event loop and runs it until exit() or quit() is called, then
    returns the code passed to exit(). \a flags is accepted for
    compatibility and ignored. Returns -1 immediately if the loop is already
    running.

    \sa exit(), quit(), isRunning()
*/
int QEventLoop::exec(ProcessEventsFlags) {
    if (m_d->running.exchange(true))
        return -1;

    auto *td = QtPrivate::ThreadData::current();
    td->retain();
    {
        std::lock_guard<std::mutex> lk(m_d->mutex);
        m_d->td = td;
    }
    m_d->exitFlag = false;
    m_d->code = 0;
    td->dispatcher.addLoop(this);

    struct Guard {
        QEventLoop *self;
        QtPrivate::ThreadData *td;
        ~Guard() {
            td->dispatcher.removeLoop(self);
            {
                std::lock_guard<std::mutex> lk(self->m_d->mutex);
                self->m_d->td = nullptr;
            }
            self->m_d->running = false;
            td->release();
        }
    } guard{this, td};

    // QThread::exit() sets quitNow before it looks for loops, and this check follows addLoop(),
    // so an exit request cannot slip between the two.
    if (td->quitNow.load())
        return -1;
    while (!m_d->exitFlag.load())
        td->dispatcher.process(true, &m_d->exitFlag);
    return m_d->code.load();
}

/*!
    Tells the event loop to exit with the return code \a returnCode, which
    exec() then returns. It is safe to call from any thread.

    \sa quit(), exec()
*/
void QEventLoop::exit(int returnCode) {
    m_d->code = returnCode;
    m_d->exitFlag = true;
    wakeUp();
}

/*!
    Returns \c true if the event loop is running in exec(); otherwise
    returns \c false.

    \sa exec(), exit()
*/
bool QEventLoop::isRunning() const { return m_d->running; }

/*!
    Wakes the event loop if it is blocked waiting for events, so that it
    re-checks its state. It is safe to call from any thread.
*/
void QEventLoop::wakeUp() {
    std::lock_guard<std::mutex> lk(m_d->mutex);
    if (m_d->td)
        m_d->td->dispatcher.wakeUp();
}

/*!
    Tells the event loop to exit with return code 0. This is the same as
    calling exit(0).

    \sa exit()
*/
void QEventLoop::quit() { exit(0); }
