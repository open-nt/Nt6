// MIT License - see LICENSE
#pragma once

// The operating system macros of Qt (Q_OS_*), from the compiler's own predefined macros, so that
// code written for Qt that tests them (#ifdef Q_OS_ANDROID, Q_OS_LINUX, Q_OS_UNIX, ...) takes the
// same branches with Nt. As in Qt, Android is also Linux and Unix, and every Apple system is Darwin.

#if defined(__ANDROID__)
#define Q_OS_ANDROID
#define Q_OS_LINUX
#elif defined(__linux__) || defined(__linux)
#define Q_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define Q_OS_DARWIN
#define Q_OS_BSD4
#include <TargetConditionals.h>
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define Q_OS_IOS
#else
#define Q_OS_MACOS
#define Q_OS_MAC
#define Q_OS_OSX
#endif
#elif defined(_WIN64)
#define Q_OS_WIN32
#define Q_OS_WIN64
#elif defined(_WIN32) || defined(__WIN32__)
#define Q_OS_WIN32
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#define Q_OS_FREEBSD
#define Q_OS_BSD4
#elif defined(__NetBSD__)
#define Q_OS_NETBSD
#define Q_OS_BSD4
#elif defined(__OpenBSD__)
#define Q_OS_OPENBSD
#define Q_OS_BSD4
#elif defined(__QNXNTO__)
#define Q_OS_QNX
#elif defined(__EMSCRIPTEN__)
#define Q_OS_WASM
#endif

#if defined(Q_OS_WIN32)
#define Q_OS_WIN
#define Q_OS_WINDOWS
#endif

// Zephyr has no Q_OS_ macro in Qt; it is not Unix.
#if !defined(Q_OS_WIN) && !defined(Q_OS_WASM) && !defined(__ZEPHYR__)
#define Q_OS_UNIX
#endif
