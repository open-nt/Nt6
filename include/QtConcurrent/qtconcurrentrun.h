// MIT License - see LICENSE
#pragma once

#include <QtConcurrent/qtconcurrentthreadengine.h>

namespace QtConcurrent {

/*!
    \fn template <typename Function, typename... Args> auto QtConcurrent::run(QThreadPool *pool, Function &&function, Args &&...args)

    Runs \a function with \a args in a thread of \a pool, or queues it there until a thread is
    free, and returns a future for what it returns. \a function and \a args are copied, or moved
    if they are rvalues, into the task; pass \c std::ref() or a pointer for what the function
    should share with the caller. \a function may be a member function, with the object as the
    first of \a args.

    \code
    QFuture<QString> text = QtConcurrent::run(&pool, [](const QString &path) { return load(path); }, path);
    \endcode

    If the first parameter of \a function is a \c{QPromise<T> &}, the function gets a promise in
    front of \a args and the result is a \c{QFuture<T>}. Such a function can report several
    results and its progress, and honour cancellation and suspension:

    \code
    QFuture<int> squares = QtConcurrent::run(&pool, [](QPromise<int> &promise, int n) {
        for (int i = 0; i < n && !promise.isCanceled(); ++i)
            promise.addResult(i * i);
    }, 100);
    \endcode

    An exception that \a function throws is stored in the future. A future that is canceled before
    its task starts does not run \a function at all.

    \since 6.0
    \sa task(), QThreadPool::start()
*/
template <typename Function, typename... Args> auto run(QThreadPool *pool, Function &&function, Args &&...args) {
    return QtPrivate::concurrentRun(pool, 0, std::forward<Function>(function), std::forward<Args>(args)...);
}

/*!
    \fn template <typename Function, typename... Args> auto QtConcurrent::run(Function &&function, Args &&...args)

    \overload
    Runs \a function with \a args in a thread of the global QThreadPool.

    \since 6.0
*/
template <typename Function, typename... Args> auto run(Function &&function, Args &&...args) {
    return QtPrivate::concurrentRun(nullptr, 0, std::forward<Function>(function), std::forward<Args>(args)...);
}

} // namespace QtConcurrent
