// MIT License - see LICENSE
//
// Overview page about threads for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page threads.html
    \title Thread Support in Nt
    \brief How threads, event loops and objects fit together in Nt Core.

    Nt follows Qt's threading model. Every thread has its own event loop, every
    QObject lives in exactly one thread, and objects talk across threads with
    queued signals and posted events instead of locks.

    \section1 Threads and Event Loops

    A QThread runs an event loop in run() unless you reimplement it. The loop
    handles the timers, socket notifiers, posted events and queued signals of
    every object that lives in the thread. Threads that were not started by
    QThread, such as the main thread, get a QThread object and an event loop of
    their own as soon as they use Nt. QEventLoop and
    QCoreApplication::processEvents() always work on the calling thread's loop,
    and QCoreApplication::exit() stops the loops of the thread that created the
    application object.

    \section1 Thread Affinity

    An object lives in the thread that created it. QObject::moveToThread()
    pushes an object, with its children, its timers, its socket notifiers and
    its posted events, to another thread. QObject::thread() tells where an
    object lives. Starting or stopping a timer and enabling a socket notifier
    work only from the object's own thread.

    \section1 Signals Across Threads

    \c Qt::AutoConnection decides when the signal is emitted: if the receiver
    lives in the emitting thread the slot is called directly, otherwise the
    arguments are copied and the receiver's event loop calls the slot.
    \c Qt::BlockingQueuedConnection waits for the slot to return. Connecting,
    disconnecting and emitting are safe from any thread. A functor connected
    with a context object runs in the context object's thread.

    \section1 What Is Safe

    \list
    \li postEvent(), deleteLater(), connect(), disconnect(), emitting signals
        and QEventLoop::exit() may be called from any thread.
    \li An object may be deleted from its own thread at any time. Deleting it
        from another thread is safe only when its thread cannot be running
        its events, for example after QThread::wait(). Use deleteLater()
        otherwise.
    \li Deleting a receiver while another thread emits a signal that is queued
        to it is safe: the emission either reaches it or does not.
    \li Direct connections run the slot in the emitting thread. The receiver
        must not be deleted while that happens.
    \endlist

    \section1 Running Tasks

    QThreadPool runs QRunnable tasks on a limited set of reusable threads.
    QThreadStorage keeps per-thread data.

    \sa QThread, QThreadPool, QRunnable, QThreadStorage, QObject, {Signals and Slots}
*/
