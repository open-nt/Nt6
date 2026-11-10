// MIT License - see LICENSE
#pragma once

#include <QtConcurrent/qtconcurrent_p.h>

namespace QtConcurrent {

/*!
    \enum QtConcurrent::FutureResult

    This enum says what QTaskBuilder::spawn() does with the future of the task.

    \value Ignore The future is dropped. The task runs all the same.
*/
enum class FutureResult { Ignore };

/*!
    \class QtConcurrent::QTaskBuilder
    \inmodule NtConcurrent
    \inheaderfile QtConcurrentTask
    \brief The QTaskBuilder class is used for adjusting the parameters of a task before it is started.

    QtConcurrent::task() returns a builder. Give the task its arguments, a thread pool and a
    priority, in any order, and start it with spawn():

    \code
    QFuture<int> sum = QtConcurrent::task([](int a, int b) { return a + b; })
                           .withArguments(2, 3)
                           .onThreadPool(pool)
                           .withPriority(1)
                           .spawn();
    \endcode

    \differences
    The arguments are copied, or moved, into the builder by withArguments(), not kept as
    references until spawn(), so the second template parameter pack holds decayed types.
    \enddifferences

    \sa QtConcurrent::task(), QtConcurrent::run()
*/
template <class Task, class... Args> class QTaskBuilder {
public:
    /*!
        Starts the task and returns a future for it: a \c{QFuture<T>} if the task takes a
        \c{QPromise<T> &} first, and a future of what the task returns otherwise. The builder
        gives its task and arguments away, so call spawn() once.

        \sa QtConcurrent::run()
    */
    auto spawn() {
        return std::apply(
            [this](auto &...args) {
                return QtPrivate::concurrentRun(m_pool, m_priority, std::move(m_task), std::move(args)...);
            },
            m_args);
    }

    /*!
        \overload
        Starts the task and drops its future. The only value of \a result is
        \c{FutureResult::Ignore}.
    */
    void spawn(FutureResult result) {
        Q_UNUSED(result);
        (void)spawn();
    }

    /*!
        Returns a builder for the same task with \a args as its arguments. They are copied, or
        moved if they are rvalues. It can be called once, on a builder without arguments.
    */
    template <class... ExtraArgs>
    [[nodiscard]] QTaskBuilder<Task, std::decay_t<ExtraArgs>...> withArguments(ExtraArgs &&...args) {
        static_assert(sizeof...(Args) == 0, "This task already has arguments");
        static_assert(sizeof...(ExtraArgs) > 0, "No arguments were given");
        return QTaskBuilder<Task, std::decay_t<ExtraArgs>...>(std::move(m_task), m_pool, m_priority,
                                                              std::forward<ExtraArgs>(args)...);
    }

    /*!
        Makes the task run in \a newThreadPool instead of the global QThreadPool, and returns
        a reference to this builder.
    */
    [[nodiscard]] QTaskBuilder &onThreadPool(QThreadPool &newThreadPool) {
        m_pool = &newThreadPool;
        return *this;
    }

    /*!
        Sets the priority that the task is queued with to \a newPriority, and returns a
        reference to this builder.

        \sa QThreadPool::start()
    */
    [[nodiscard]] QTaskBuilder &withPriority(int newPriority) {
        m_priority = newPriority;
        return *this;
    }

    /*!
        \internal
    */
    template <class T, class... A>
    QTaskBuilder(T &&task, QThreadPool *pool, int priority, A &&...args)
        : m_task(std::forward<T>(task)), m_pool(pool), m_priority(priority), m_args(std::forward<A>(args)...) {}

private:
    Task m_task;
    QThreadPool *m_pool;
    int m_priority;
    std::tuple<Args...> m_args;
};

} // namespace QtConcurrent
