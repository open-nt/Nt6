// MIT License - see LICENSE
//
// Page about the Zephyr port for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-zephyr.html
    \title Nt on Zephyr
    \brief Cross building Nt Core and Nt Network for a board that runs the Zephyr RTOS.

    Nt Core and Nt Network build as a Zephyr module, for headless devices in the class of the
    NXP i.MX RT1170: a 32-bit core with megabytes of flash and RAM. The other modules, the
    tests and the examples for the desktop are left out of such a build.

    The port is at an early stage. The sample below compiles and links for
    \c mimxrt1170_evk/mimxrt1176/cm7 with Zephyr 4.5 and the Zephyr SDK 1.0.1, at about 920 KB
    of code; it has not been run on a board yet.

    \section1 Building

    Nt is a Zephyr module: \c zephyr/module.yml at the root of the repository names its
    \c CMakeLists.txt and \c Kconfig. Add the repository to the west manifest of the workspace,
    or name its directory in \c ZEPHYR_EXTRA_MODULES, then turn it on in \c prj.conf and link
    the application with the libraries:

    \code
    # prj.conf
    CONFIG_CPP=y
    CONFIG_STD_CPP17=y
    CONFIG_NT=y
    CONFIG_NT_NETWORK=y
    CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=1048576
    CONFIG_HEAP_MEM_POOL_SIZE=65536
    CONFIG_MAIN_STACK_SIZE=16384
    \endcode

    \code
    # CMakeLists.txt
    find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
    project(app LANGUAGES CXX)
    target_sources(app PRIVATE src/main.cpp)
    target_link_libraries(app PRIVATE Nt::Core Nt::Network)
    \endcode

    \c examples/zephyr/hello is a complete application, a timer and a TCP echo server on one
    event loop:

    \code
    west build -b mimxrt1170_evk/mimxrt1176/cm7 examples/zephyr/hello
    \endcode

    \c CONFIG_NT selects what Nt needs from Zephyr: the full C++ standard library, exceptions,
    run-time type information, the POSIX API with \c eventfd and the file system calls, thread
    local storage and dynamically created threads. \c CONFIG_NT_NETWORK needs
    \c CONFIG_NETWORKING and \c CONFIG_NET_SOCKETS.

    \section1 Host Tools

    \c ntmoc and \c ntrcc run on the build machine while the libraries are built. A cross build
    compiles them with the native compiler as CMake configures the project, into
    \c nt-host-tools under the build directory. Set \c NT_HOST_CXX_COMPILER to choose that
    compiler, or \c NT_HOST_TOOLS_DIR to use tools that are built already. These tools are
    built without zlib and Zstandard, so resources are stored uncompressed.

    \section1 Memory

    Nt allocates from the heap of the C library, whose size is
    \c CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE. The event loop of the application runs on the main
    thread, so \c CONFIG_MAIN_STACK_SIZE has to hold it. The stack of every thread that Nt starts
    has \c CONFIG_NT_THREAD_STACK_SIZE bytes, unless QThread::setStackSize() asked for another
    size, and is allocated from the kernel heap, \c CONFIG_HEAP_MEM_POOL_SIZE.

    \section1 What Is Left Out

    Zephyr has one process, no shared libraries and no desktop, so these classes are not built.
    The macro beside each is defined for code that includes the headers of Nt:

    \list
        \li QProcess (\c QT_NO_PROCESS)
        \li QLibrary (\c QT_NO_LIBRARY)
        \li QSharedMemory (\c QT_NO_SHAREDMEMORY) and QSystemSemaphore (\c QT_NO_SYSTEMSEMAPHORE)
        \li QFileSystemWatcher (\c QT_NO_FILESYSTEMWATCHER)
        \li QStorageInfo (\c QT_NO_STORAGEINFO)
        \li QLocalSocket (\c QT_NO_LOCALSOCKET) and QLocalServer (\c QT_NO_LOCALSERVER)
        \li QSslSocket and \c https: (\c QT_NO_SSL): TLS is built on OpenSSL only
    \endlist

    \section1 Differences on Zephyr

    \list
        \li \b Threads. The C++ library of the Zephyr SDK is built without thread support. Nt
            supplies \c std::mutex, \c std::condition_variable and their relatives on the kernel's
            \c k_mutex and \c k_condvar, in a header that the build includes ahead of every
            source file that uses Nt. That library still counts the references of a
            \c std::shared_ptr without atomic operations and keeps one exception state for all
            threads, so for now use Nt from one thread; QThread and QThreadPool compile, but are
            not safe yet.
        \li \b Files. A path has to be absolute and start at a mounted file system: there is no
            working directory. There are no symbolic links, users or permissions, so a file that
            exists is readable, writable and owned by user 0. The locations of QStandardPaths
            are those of a desktop and do not exist.
        \li \b Time. Local time is UTC, as the C library has no time zones. The steady clock of
            the C++ library follows the system clock, so setting the time of day moves the
            timers that are running.
        \li \b Network. QNetworkInterface lists the interfaces of the Zephyr network stack, with
            their addresses, netmasks and hardware address. Host names are resolved with
            \c getaddrinfo(), which needs \c CONFIG_DNS_RESOLVER.
        \li \b Randomness. QRandomGenerator::system() reads \c getentropy(), which needs
            an entropy driver on the board.
    \endlist

    \sa {Installing Nt}, {Thread Support in Nt}
*/
