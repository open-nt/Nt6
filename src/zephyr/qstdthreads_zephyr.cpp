// MIT License - see LICENSE
// The kernel side of <QtCore/qstdthreads_zephyr_p.h>: std::mutex and std::condition_variable on
// k_mutex and k_condvar, for a C++ library that was built without thread support.
#include <QtCore/qstdthreads_zephyr_p.h>

#if defined(__GLIBCXX__) && !defined(_GLIBCXX_HAS_GTHREADS)

#include <zephyr/kernel.h>

namespace QtPrivate {
namespace ZephyrStd {

namespace {

static_assert(sizeof(struct k_mutex) <= sizeof(KernelObject::words), "KernelObject is too small for a k_mutex");
static_assert(sizeof(struct k_condvar) <= sizeof(KernelObject::words), "KernelObject is too small for a k_condvar");
static_assert(alignof(struct k_mutex) <= alignof(void *) && alignof(struct k_condvar) <= alignof(void *),
              "KernelObject is not aligned for the kernel objects");

enum State : unsigned char { Fresh = 0, Ready = 1 };

struct k_mutex *asMutex(KernelObject &object) noexcept {
    return reinterpret_cast<struct k_mutex *>(object.words); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

struct k_condvar *asCond(KernelObject &object) noexcept {
    return reinterpret_cast<struct k_condvar *>(object.words); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// Objects are set up on first use, once, whichever threads get there first.
template <typename Init>
void ensure(KernelObject &object, Init init) noexcept {
    if (object.state.load(std::memory_order_acquire) == Ready)
        return;
    const unsigned int key = irq_lock();
    if (object.state.load(std::memory_order_relaxed) != Ready) {
        init();
        object.state.store(Ready, std::memory_order_release);
    }
    irq_unlock(key);
}

k_timeout_t toKernelTimeout(long long timeoutNs) noexcept {
    if (timeoutNs < 0)
        return K_FOREVER;
    if (timeoutNs == 0)
        return K_NO_WAIT;
    // Rounded up, so that a wait is never shorter than asked for.
    return K_USEC((timeoutNs + 999) / 1000);
}

} // namespace

void mutexInit(KernelObject &object) noexcept {
    ensure(object, [&object] { k_mutex_init(asMutex(object)); });
}

bool mutexLock(KernelObject &object, long long timeoutNs) noexcept {
    mutexInit(object);
    return k_mutex_lock(asMutex(object), toKernelTimeout(timeoutNs)) == 0;
}

void mutexUnlock(KernelObject &object) noexcept { k_mutex_unlock(asMutex(object)); }

void condInit(KernelObject &object) noexcept {
    ensure(object, [&object] { k_condvar_init(asCond(object)); });
}

bool condWait(KernelObject &cond, KernelObject &mutex, long long timeoutNs) noexcept {
    condInit(cond);
    return k_condvar_wait(asCond(cond), asMutex(mutex), toKernelTimeout(timeoutNs)) == 0;
}

void condNotify(KernelObject &cond, bool all) noexcept {
    condInit(cond);
    if (all)
        k_condvar_broadcast(asCond(cond));
    else
        k_condvar_signal(asCond(cond));
}

} // namespace ZephyrStd
} // namespace QtPrivate

#endif
