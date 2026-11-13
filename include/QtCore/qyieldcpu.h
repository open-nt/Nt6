// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

// Tells the processor that the thread is waiting in a busy loop (spinning on an atomic variable, say), so that
// it can save power or give its resources to another hardware thread. Unlike std::this_thread::yield(), it does
// not give the time slice back to the scheduler. Does nothing on processors without such an instruction.
inline void qYieldCpu() noexcept {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
    _mm_pause();
#elif defined(__aarch64__) || (defined(__ARM_ARCH) && __ARM_ARCH >= 7)
    __asm__ __volatile__("yield");
#elif defined(__riscv)
    __asm__ __volatile__(".insn i 0x0F, 0, x0, x0, 0x010"); // pause (Zihintpause), a no-op without it
#endif
}
