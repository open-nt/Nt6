// MIT License - see LICENSE
#pragma once

#include "dispatcher.h"

#include <QtCore/qobject.h>

#include <atomic>
#include <cstddef>
#include <map>

class QThread;

namespace QtPrivate {

struct StorageSlot {
    void *data;
    void (*destroy)(void *);
};

// Everything that belongs to one thread: its dispatcher, its QThread object and its thread
// storage. Reference counted by the QObjects living in the thread, the running thread and the
// QThread object.
struct ThreadData {
    ThreadData();
    ~ThreadData();
    ThreadData(const ThreadData &) = delete;
    ThreadData &operator=(const ThreadData &) = delete;

    void retain() { refs.fetch_add(1, std::memory_order_relaxed); }
    void release() {
        if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }

    // Runs and clears the thread storage; called by the owning thread as it ends.
    void clearStorage();

    // The data of the calling thread. A thread that was not started by QThread is adopted on first use.
    static ThreadData *current();
    // Makes `td` the data of the calling thread; used by QThread for the threads it starts.
    static void bindCurrent(ThreadData *td);
    // Stands for "no thread": objects moved there get no events delivered.
    static ThreadData *orphan();
    // The data of the first thread that was adopted; taken to be the main thread.
    static ThreadData *mainThread();

    std::atomic<int> refs{1};
    Dispatcher dispatcher;
    std::atomic<QThread *> thread{nullptr};
    std::atomic<bool> quitNow{false};
    bool adopted = false;
    bool isOrphan = false;
    // Only touched by the owning thread (and by the destructor).
    std::map<std::size_t, StorageSlot> storage;
};

// The thread an object lives in, without taking a reference: good for comparing pointers only.
ThreadData *objectThreadData(const QObject *o);
// Same, but with a reference that the caller releases with release().
ThreadData *acquireThreadData(const QObject *o);
// Called by the QObject constructor and destructor.
void initObjectThread(QObject *o);
void releaseObjectThread(QObject *o);
// Posts to the thread that the receiver lives in, following it if it moves meanwhile.
void postToObject(QObject *receiver, QEvent *event);
// Waits for the tasks of the global QThreadPool, if it was ever created.
void waitForGlobalThreadPool();

// Lets the thread machinery reach the private parts of QThread. Defined in qthread.cpp.
struct ThreadAccess {
    // Creates the QThread object that stands for an adopted thread.
    static QThread *adopt(ThreadData *td);
    static void destroyAdopted(QThread *thread);
    static ThreadData *data(const QThread *thread);
    static void adoptedThreadExited(QThread *thread);
    // Entry point of the threads that QThread::start() creates.
    static void *threadMain(void *thread);
};

} // namespace QtPrivate
