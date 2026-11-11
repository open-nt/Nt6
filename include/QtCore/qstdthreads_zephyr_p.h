// MIT License - see LICENSE
#pragma once

// The C++ library of the Zephyr SDK is built without thread support: <mutex> and
// <condition_variable> have the lock classes, but not std::mutex, std::timed_mutex,
// std::recursive_mutex, std::recursive_timed_mutex, std::condition_variable or
// std::condition_variable_any. Nt and its public headers use those names, so this file supplies
// them on top of the Zephyr kernel's k_mutex and k_condvar. The build includes it ahead of every
// source file of Nt and of the targets that link with Nt (see NT_ZEPHYR in CMakeLists.txt).
//
// It does nothing on a library that has the classes, so a toolchain that gains thread support
// drops it without a change here. Two things stay missing and are worked around in Nt instead:
// std::thread cannot start a thread, and std::this_thread::get_id() is the same in every thread.
//
// The kernel objects are kept as opaque storage so that this file does not bring <zephyr/kernel.h>
// and its macros into every translation unit. They are set up on first use, which keeps the
// constructors constexpr as the standard requires.

#if defined(__ZEPHYR__)

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>

#if defined(__GLIBCXX__) && !defined(_GLIBCXX_HAS_GTHREADS)

namespace QtPrivate {
namespace ZephyrStd {

// Room for a struct k_mutex and a struct k_condvar; checked in qstdthreads_zephyr.cpp.
struct KernelObject {
    void *words[8] = {};
    std::atomic<unsigned char> state{0};
};

// A negative timeout waits for ever. The lock functions return false when the time ran out.
void mutexInit(KernelObject &object) noexcept;
bool mutexLock(KernelObject &object, long long timeoutNs) noexcept;
void mutexUnlock(KernelObject &object) noexcept;
void condInit(KernelObject &object) noexcept;
bool condWait(KernelObject &cond, KernelObject &mutex, long long timeoutNs) noexcept;
void condNotify(KernelObject &cond, bool all) noexcept;

template <typename Rep, typename Period> long long toTimeout(const std::chrono::duration<Rep, Period> &time) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(time).count();
    return ns < 0 ? 0 : static_cast<long long>(ns);
}

// k_mutex counts the locks of its owner, so one class serves the plain and the recursive mutexes.
class Mutex {
public:
    constexpr Mutex() noexcept = default;
    Mutex(const Mutex &) = delete;
    Mutex &operator=(const Mutex &) = delete;

    void lock() { (void)mutexLock(m_object, -1); }
    bool try_lock() noexcept { return mutexLock(m_object, 0); }
    void unlock() { mutexUnlock(m_object); }

    template <typename Rep, typename Period> bool try_lock_for(const std::chrono::duration<Rep, Period> &time) {
        return mutexLock(m_object, toTimeout(time));
    }
    template <typename Clock, typename Duration>
    bool try_lock_until(const std::chrono::time_point<Clock, Duration> &time) {
        return mutexLock(m_object, toTimeout(time - Clock::now()));
    }

    KernelObject &object() noexcept { return m_object; }

private:
    KernelObject m_object;
};

} // namespace ZephyrStd
} // namespace QtPrivate

namespace std {

class mutex : public QtPrivate::ZephyrStd::Mutex {};
class recursive_mutex : public QtPrivate::ZephyrStd::Mutex {};
class timed_mutex : public QtPrivate::ZephyrStd::Mutex {};
class recursive_timed_mutex : public QtPrivate::ZephyrStd::Mutex {};

enum class cv_status { no_timeout, timeout };

class condition_variable {
public:
    constexpr condition_variable() noexcept = default;
    condition_variable(const condition_variable &) = delete;
    condition_variable &operator=(const condition_variable &) = delete;

    void notify_one() noexcept { QtPrivate::ZephyrStd::condNotify(m_object, false); }
    void notify_all() noexcept { QtPrivate::ZephyrStd::condNotify(m_object, true); }

    void wait(unique_lock<mutex> &lock) { (void)QtPrivate::ZephyrStd::condWait(m_object, lock.mutex()->object(), -1); }
    template <typename Predicate> void wait(unique_lock<mutex> &lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template <typename Rep, typename Period>
    cv_status wait_for(unique_lock<mutex> &lock, const chrono::duration<Rep, Period> &time) {
        const bool woken =
            QtPrivate::ZephyrStd::condWait(m_object, lock.mutex()->object(), QtPrivate::ZephyrStd::toTimeout(time));
        return woken ? cv_status::no_timeout : cv_status::timeout;
    }
    template <typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock<mutex> &lock, const chrono::duration<Rep, Period> &time, Predicate pred) {
        return wait_until(lock, chrono::steady_clock::now() + time, std::move(pred));
    }

    template <typename Clock, typename Duration>
    cv_status wait_until(unique_lock<mutex> &lock, const chrono::time_point<Clock, Duration> &time) {
        (void)wait_for(lock, time - Clock::now());
        return Clock::now() < time ? cv_status::no_timeout : cv_status::timeout;
    }
    template <typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock<mutex> &lock, const chrono::time_point<Clock, Duration> &time, Predicate pred) {
        while (!pred()) {
            if (wait_until(lock, time) == cv_status::timeout)
                return pred();
        }
        return true;
    }

private:
    QtPrivate::ZephyrStd::KernelObject m_object;
};

// Waits with any lock: a mutex of its own closes the gap between unlocking the caller's lock and
// starting to wait.
class condition_variable_any {
public:
    condition_variable_any() noexcept = default;
    condition_variable_any(const condition_variable_any &) = delete;
    condition_variable_any &operator=(const condition_variable_any &) = delete;

    void notify_one() noexcept {
        const lock_guard<mutex> guard(m_mutex);
        m_cond.notify_one();
    }
    void notify_all() noexcept {
        const lock_guard<mutex> guard(m_mutex);
        m_cond.notify_all();
    }

    template <typename Lock> void wait(Lock &lock) {
        unique_lock<mutex> inner(m_mutex);
        const Relock<Lock> relock(lock);
        // Declared last so that the inner mutex is released before the caller's lock is taken again.
        unique_lock<mutex> waiting(std::move(inner));
        m_cond.wait(waiting);
    }
    template <typename Lock, typename Predicate> void wait(Lock &lock, Predicate pred) {
        while (!pred())
            wait(lock);
    }

    template <typename Lock, typename Clock, typename Duration>
    cv_status wait_until(Lock &lock, const chrono::time_point<Clock, Duration> &time) {
        unique_lock<mutex> inner(m_mutex);
        const Relock<Lock> relock(lock);
        unique_lock<mutex> waiting(std::move(inner));
        return m_cond.wait_until(waiting, time);
    }
    template <typename Lock, typename Clock, typename Duration, typename Predicate>
    bool wait_until(Lock &lock, const chrono::time_point<Clock, Duration> &time, Predicate pred) {
        while (!pred()) {
            if (wait_until(lock, time) == cv_status::timeout)
                return pred();
        }
        return true;
    }

    template <typename Lock, typename Rep, typename Period>
    cv_status wait_for(Lock &lock, const chrono::duration<Rep, Period> &time) {
        return wait_until(lock, chrono::steady_clock::now() + time);
    }
    template <typename Lock, typename Rep, typename Period, typename Predicate>
    bool wait_for(Lock &lock, const chrono::duration<Rep, Period> &time, Predicate pred) {
        return wait_until(lock, chrono::steady_clock::now() + time, std::move(pred));
    }

private:
    template <typename Lock> struct Relock {
        explicit Relock(Lock &l) : lock(l) { lock.unlock(); }
        ~Relock() { lock.lock(); }
        Relock(const Relock &) = delete;
        Relock &operator=(const Relock &) = delete;
        Lock &lock;
    };

    mutex m_mutex;
    condition_variable m_cond;
};

} // namespace std

#endif // libstdc++ without thread support
#endif // __ZEPHYR__
