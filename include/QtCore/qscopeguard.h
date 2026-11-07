// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

#include <utility>

template <typename F>
class QScopeGuard {
public:
    explicit QScopeGuard(F &&f) noexcept : func(std::move(f)) {}
    QScopeGuard(QScopeGuard &&o) noexcept : func(std::move(o.func)), active(o.active) { o.active = false; }
    QScopeGuard(const QScopeGuard &) = delete;
    ~QScopeGuard() { if (active) func(); }
    void dismiss() noexcept { active = false; }

private:
    F func;
    bool active = true;
};

template <typename F>
QScopeGuard<std::decay_t<F>> qScopeGuard(F &&f) { return QScopeGuard<std::decay_t<F>>(std::forward<F>(f)); }

class QSignalBlocker {
public:
    explicit QSignalBlocker(QObject *o) noexcept : obj(o), previous(o ? o->blockSignals(true) : false) {}
    explicit QSignalBlocker(QObject &o) noexcept : QSignalBlocker(&o) {}
    ~QSignalBlocker() { if (obj) obj->blockSignals(previous); }
    QSignalBlocker(const QSignalBlocker &) = delete;
    QSignalBlocker &operator=(const QSignalBlocker &) = delete;
    void reblock() noexcept { if (obj) obj->blockSignals(true); }
    void unblock() noexcept { if (obj) obj->blockSignals(previous); }

private:
    QObject *obj;
    bool previous;
};

template <typename T>
class QScopedValueRollback {
public:
    explicit QScopedValueRollback(T &v) : ref(v), saved(v) {}
    QScopedValueRollback(T &v, T value) : ref(v), saved(v) { ref = std::move(value); }
    ~QScopedValueRollback() { ref = std::move(saved); }
    QScopedValueRollback(const QScopedValueRollback &) = delete;
    void commit() { saved = ref; }

private:
    T &ref;
    T saved;
};
