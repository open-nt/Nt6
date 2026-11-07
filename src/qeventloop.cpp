// MIT License - see LICENSE
#include <QtCore/qeventloop.h>

#include "dispatcher.h"

#include <atomic>

struct QEventLoop::Data {
    std::atomic<bool> exitFlag{false};
    std::atomic<int> code{0};
    bool running = false;
};

QEventLoop::QEventLoop(QObject *parent) : QObject(parent), m_d(new Data) {}

QEventLoop::~QEventLoop() = default;

bool QEventLoop::processEvents(ProcessEventsFlags flags) {
    const bool wait = flags.testFlag(WaitForMoreEvents);
    return QtPrivate::Dispatcher::instance().process(wait, m_d->running ? &m_d->exitFlag : nullptr);
}

int QEventLoop::exec(ProcessEventsFlags) {
    if (m_d->running)
        return -1;

    auto &disp = QtPrivate::Dispatcher::instance();
    m_d->exitFlag = false;
    m_d->code = 0;
    m_d->running = true;
    disp.addLoop(this);

    struct Guard {
        QEventLoop *self;
        ~Guard() {
            QtPrivate::Dispatcher::instance().removeLoop(self);
            self->m_d->running = false;
        }
    } guard{this};

    while (!m_d->exitFlag.load())
        disp.process(true, &m_d->exitFlag);
    return m_d->code.load();
}

void QEventLoop::exit(int returnCode) {
    m_d->code = returnCode;
    m_d->exitFlag = true;
    QtPrivate::Dispatcher::instance().wakeUp();
}

bool QEventLoop::isRunning() const { return m_d->running; }

void QEventLoop::wakeUp() { QtPrivate::Dispatcher::instance().wakeUp(); }

void QEventLoop::quit() { exit(0); }
