// MIT License - see LICENSE
#include <QtCore/qcoreapplication.h>

#include "dispatcher.h"

#include <cstdio>
#include <cstdlib>

/*!
    \module NtCore
    \title Nt Core C++ Classes
    \brief Core non-graphical classes: objects, signals, events, strings and containers.
    \cmaketarget Nt::Core

    Nt Core provides the classes every Nt program builds on. Headers keep Qt's
    names, so \c{#include <QObject>} and \c{#include <QtCore/QTimer>} both work.

    See \l{Signals and Slots} for how objects communicate.
*/

namespace {
QCoreApplication *g_self = nullptr;
}

QCoreApplication::QCoreApplication(int &argc, char **argv) {
    Q_UNUSED(argc)
    Q_UNUSED(argv)
    if (g_self) {
        std::fputs("QCoreApplication: there should be only one application object\n", stderr);
        std::abort();
    }
    g_self = this;
}

QCoreApplication::~QCoreApplication() {
    if (g_self == this)
        g_self = nullptr;
}

QCoreApplication *QCoreApplication::instance() { return g_self; }

int QCoreApplication::exec() {
    QEventLoop loop;
    const int code = loop.exec();
    if (g_self)
        g_self->aboutToQuit();
    return code;
}

void QCoreApplication::exit(int returnCode) { QtPrivate::Dispatcher::instance().exitAll(returnCode); }

void QCoreApplication::quit() { exit(0); }

void QCoreApplication::processEvents(QEventLoop::ProcessEventsFlags flags) {
    QtPrivate::Dispatcher::instance().process(flags.testFlag(QEventLoop::WaitForMoreEvents), nullptr);
}

void QCoreApplication::postEvent(QObject *receiver, QEvent *event) {
    if (!event)
        return;
    if (!receiver) {
        delete event;
        return;
    }
    QtPrivate::Dispatcher::instance().post(receiver, event);
}

bool QCoreApplication::sendEvent(QObject *receiver, QEvent *event) {
    if (!receiver || !event)
        return false;
    if (g_self)
        return g_self->notify(receiver, event);
    return receiver->event(event);
}

void QCoreApplication::removePostedEvents(QObject *receiver, int eventType) {
    QtPrivate::Dispatcher::instance().removePosted(receiver, eventType);
}

bool QCoreApplication::notify(QObject *receiver, QEvent *event) { return receiver->event(event); }
