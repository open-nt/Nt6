// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>

#include <atomic>
#include <chrono>
#include <vector>

class QAbstractEventDispatcher;
class QObject;
class QSocketNotifier;
class QThread;

namespace QtPrivate {

struct ThreadData;

// The thread's event work goes through these, which use the QAbstractEventDispatcher subclass that
// QThread::setEventDispatcher() or QCoreApplication::setEventDispatcher() installed, or the built-in dispatcher.
int startTimer(ThreadData *td, QObject *object, std::chrono::milliseconds interval);
bool killTimer(ThreadData *td, int id);
void killTimers(ThreadData *td, QObject *object);
// `stop` is the exit flag of the running loop, for the built-in dispatcher.
bool processEvents(ThreadData *td, QEventLoop::ProcessEventsFlags flags, const std::atomic<bool> *stop);
// Registers the notifier while it is enabled and has a socket, and unregisters it otherwise.
void updateNotifier(ThreadData *td, QSocketNotifier *notifier, bool active);
// Forgets the notifier, which is being destroyed or given another socket.
void removeNotifier(ThreadData *td, QSocketNotifier *notifier);
// Moves the timers, notifiers and posted events of `objects` from one thread to another.
void migrateEvents(ThreadData *from, ThreadData *to, const std::vector<QObject *> &objects);

// Installs `dispatcher` as the event dispatcher of `td`, whose QThread is `thread`; `who` names the caller in warnings.
bool installEventDispatcher(QThread *thread, ThreadData *td, QAbstractEventDispatcher *dispatcher, const char *who);
// Calls startingUp() on the installed dispatcher, as its thread starts.
void eventDispatcherStarting(ThreadData *td);
// Calls closingDown() on the installed dispatcher and deletes it, as its thread finishes.
void eventDispatcherFinished(ThreadData *td);

} // namespace QtPrivate
