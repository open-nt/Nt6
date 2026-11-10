// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <functional>
#include <type_traits>
#include <utility>

class QRunnable;

namespace QtPrivate {
// Selects the overloads of QRunnable::create() and QThreadPool that take any callable, including
// move-only ones; a std::function goes to the existing overloads.
template <typename F>
using IfRunnableCallable =
    std::enable_if_t<std::is_invocable_v<std::decay_t<F> &> && !std::is_convertible_v<F, QRunnable *> &&
                         !std::is_same_v<std::decay_t<F>, std::function<void()>>,
                     bool>;
} // namespace QtPrivate

class QRunnable {
public:
    QRunnable() = default;
    virtual ~QRunnable();
    Q_DISABLE_COPY(QRunnable)

    virtual void run() = 0;

    static QRunnable *create(std::function<void()> functionToRun);
    /*!
        \overload
        \since 6.0
        Creates a QRunnable that calls \a functionToRun, which may be a
        move-only callable such as a lambda that owns a \c std::unique_ptr.
        The callable is moved into the runnable.
    */
    template <typename Callable, QtPrivate::IfRunnableCallable<Callable> = true>
    static QRunnable *create(Callable &&functionToRun);

    bool autoDelete() const { return m_autoDelete; }
    void setAutoDelete(bool autoDelete) { m_autoDelete = autoDelete; }

private:
    bool m_autoDelete = true;
};

namespace QtPrivate {
template <typename F> class GenericRunnable final : public QRunnable {
public:
    explicit GenericRunnable(const F &f) : m_f(f) {}
    explicit GenericRunnable(F &&f) : m_f(std::move(f)) {}
    void run() override { m_f(); }

private:
    F m_f;
};
} // namespace QtPrivate

template <typename Callable, QtPrivate::IfRunnableCallable<Callable>>
QRunnable *QRunnable::create(Callable &&functionToRun) {
    return new QtPrivate::GenericRunnable<std::decay_t<Callable>>(std::forward<Callable>(functionToRun));
}
