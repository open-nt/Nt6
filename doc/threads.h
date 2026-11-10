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

    \section1 Futures and Promises

    A QPromise reports the progress and the results of a computation, which can run
    in any thread, to the QFuture that it makes. The future can be waited for,
    asked for its results while they arrive, canceled and suspended, and it
    rethrows the exception that ended the computation. QFuture::then(),
    QFuture::onFailed() and QFuture::onCanceled() attach a continuation that runs
    where the previous future finishes, in a thread pool, or in the event loop of a
    context object. QFutureWatcher turns what a future does into signals of its own
    thread, and QFutureSynchronizer waits for a set of futures.

    \code
    QPromise<int> promise;
    QFuture<int> future = promise.future();
    QFuture<QString> text = future.then(qApp, [](int answer) { return QString::number(answer); });
    pool.start([p = std::make_shared<QPromise<int>>(std::move(promise))] {
        p->start();
        p->addResult(42);
        p->finish();
    });
    \endcode

    A promise that is destroyed without finishing cancels its future, so no thread
    waits forever. All the functions of a future, a promise and a watcher are
    thread-safe, with one rule: a result that is moved out with
    QFuture::takeResult() must not be read at the same time by another thread.

    \section1 Nt Concurrent

    The functions of the QtConcurrent namespace, in the \c Nt::Concurrent library, start such
    computations without a promise in sight. QtConcurrent::run() runs one function in a thread
    pool and returns the future of its result; QtConcurrent::mapped(), QtConcurrent::filtered()
    and QtConcurrent::mappedReduced() share the items of a sequence between the threads of the
    pool:

    \code
    QFuture<QString> text = QtConcurrent::run(readFile, path);
    QFuture<int> words = QtConcurrent::mappedReduced(files, countWords, [](int &total, int n) { total += n; });
    \endcode

    \sa QThread, QThreadPool, QRunnable, QThreadStorage, QObject, QFuture, QPromise, QtConcurrent, {Signals and Slots}
*/
