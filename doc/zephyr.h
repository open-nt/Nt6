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

    The port is at an early stage. With Zephyr 4.5 and the Zephyr SDK 1.0.1, the sample for
    QEMU below runs and passes on the emulated \c mps2/an385 board, and the other sample compiles
    and links for \c mimxrt1170_evk/mimxrt1176/cm7, at about 920 KB of code; neither has been run
    on a real board yet.

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

    \section1 Running in QEMU

    \c examples/zephyr/qemu needs no hardware. It checks JSON, a timer, a TCP connection to
    itself over the loopback interface and a worker thread that calls back into the main
    thread, and prints \c PASS or \c FAIL for each. With \c qemu-system-arm installed, for
    example from the host tools of the Zephyr SDK:

    \code
    west build -b mps2/an385 examples/zephyr/qemu -t run
    \endcode

    \code
    *** Booting Zephyr OS build v4.5.0 ***
    Nt 6.0.0 on Zephyr
      JSON round trip                    ok
      QTcpServer::listen                 ok
      QTimer ticks                       ok
      TCP echo over loopback             ok
      QThread ran                        ok
      queued call from the thread        ok
    Nt sample: PASS
    \endcode

    Leave QEMU with Ctrl+A, then X.

    \section1 Configuration

    \c CONFIG_NT selects what Nt needs from Zephyr: the full C++ standard library, exceptions,
    run-time type information, the POSIX API with \c eventfd and the file system calls, thread
    local storage and dynamically created threads. \c CONFIG_NT_NETWORK needs
    \c CONFIG_NETWORKING and \c CONFIG_NET_SOCKETS.

    The event loop of a thread waits in one \c poll() for all of its sockets, so
    \c CONFIG_ZVFS_POLL_MAX bounds how many a thread can serve; Nt raises Zephyr's default of
    3 to 16. A \c poll() with more descriptors fails, which the event loop reports once with a
    warning. \c CONFIG_ZVFS_OPEN_MAX bounds the open descriptors of the whole application, and
    \c CONFIG_NET_MAX_CONTEXTS and \c CONFIG_NET_MAX_CONN the sockets of the network stack.

    \section1 Host Tools

    \c ntmoc and \c ntrcc run on the build machine while the libraries are built. A cross build
    compiles them with the native compiler as CMake configures the project, into
    \c nt-host-tools under the build directory. Set \c NT_HOST_CXX_COMPILER to choose that
    compiler, or \c NT_HOST_TOOLS_DIR to use tools that are built already. These tools are
    built without zlib and Zstandard, so resources are stored uncompressed.

    \section1 TLS

    \c CONFIG_NT_TLS builds QSslSocket, QSslServer, QSslCertificate, QSslKey and QSslConfiguration,
    and \c https: in QNetworkAccessManager, on Mbed TLS 4 and its PSA cryptography, both of which
    Zephyr provides as a module. It selects TLS 1.2 with the ECDHE ciphers of AES-GCM, X.509
    certificates and the reading of keys; add TLS 1.3 and other ciphers with the options of
    Mbed TLS, for example \c CONFIG_MBEDTLS_SSL_PROTO_TLS1_3.

    \code
    QSslConfiguration configuration;
    configuration.setCaCertificates(QSslCertificate::fromData(pem));   // the certificates to trust
    QSslSocket socket;
    socket.setSslConfiguration(configuration);
    socket.connectToHostEncrypted(QString("example.com"), 443);
    \endcode

    The sample for QEMU runs a TLS server and client against each other over the loopback
    interface, and checks that a client that trusts no certificate refuses the server.

    \list
        \li \b Trust. Zephyr has no store of certificate authorities, so the certificates to trust
            have to be named with QSslConfiguration::setCaCertificates() or addCaCertificate(),
            for example from flash or a resource. Without any, a client verifies nothing and so
            refuses every server.
        \li \b Time. Mbed TLS checks the validity dates of a certificate only with
            \c CONFIG_MBEDTLS_HAVE_TIME_DATE, and then against the time of day, which a board
            without a battery-backed clock has to be given first, for example from SNTP.
        \li \b Randomness. The cryptography draws on the entropy driver of the board; the
            sample uses \c CONFIG_TEST_RANDOM_GENERATOR, which is predictable and must not
            be used in a product.
        \li \b Memory. Mbed TLS keeps a connection's keys and buffers in the heap of the C
            library, in addition to the sizes in \c CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN and
            \c CONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN, which can be lowered when the peers use
            small records.
        \li \b Speed. A handshake computes elliptic curves, which takes seconds on a core with
            no help from hardware and minutes under QEMU, where the sample therefore waits up
            to two minutes and gives QSslServer::setHandshakeTimeout() a long time.
        \li \b Drivers. With \c CONFIG_MBEDTLS_PSA_P256M_DRIVER_ENABLED the handshake of the
            sample failed with the error \c HW_ACCEL_FAILED, so leave it off unless you test it.
    \endlist

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
        \li QSslSocket and \c https: (\c QT_NO_SSL), unless \c CONFIG_NT_TLS is set
    \endlist

    \section1 Differences on Zephyr

    \list
        \li \b Threads. The C++ library of the Zephyr SDK is built without thread support. Nt
            supplies \c std::mutex, \c std::condition_variable and their relatives on the kernel's
            \c k_mutex and \c k_condvar, in a header that the build includes ahead of every
            source file that uses Nt. That library still counts the references of a
            \c std::shared_ptr without atomic operations and keeps one exception state for all
            threads. A QThread that talks to the rest of the application with queued
            signals works in the sample for QEMU, but sharing such objects between threads is not
            safe yet, so keep to one thread where you can.
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
