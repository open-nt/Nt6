// MIT License - see LICENSE
#pragma once

#include <QtConcurrent/qtaskbuilder.h>

namespace QtConcurrent {

/*!
    \fn template <typename Task> QTaskBuilder<std::decay_t<Task>> QtConcurrent::task(Task &&task)

    Returns a builder for running \a task, which is copied, or moved if it is an rvalue, in
    another thread. Set its arguments, thread pool and priority on the builder and start it with
    QTaskBuilder::spawn().

    \since 6.0
    \sa run(), QTaskBuilder
*/
template <typename Task> [[nodiscard]] QTaskBuilder<std::decay_t<Task>> task(Task &&task) {
    return QTaskBuilder<std::decay_t<Task>>(std::forward<Task>(task), nullptr, 0);
}

} // namespace QtConcurrent
