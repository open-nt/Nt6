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
    and links for \c mimxrt1170_evk/mimxrt1176/cm7, at about 920 KB of code. The samples for
    GPIO and audio run on the MIMXRT1170-EVKB, the primary board; see \l{Supported Hardware}.

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

    \c zephyr/hello in the \l{Nt Examples}{examples repository} is a complete application, a timer and a TCP echo
    server on one event loop. The examples repository is not a west workspace, so name a checkout of Nt as a module:

    \code
    west build -b mimxrt1170_evk/mimxrt1176/cm7 zephyr/hello -- -DNT_ROOT=/path/to/Nt
    \endcode

    \section1 Running in QEMU

    \c zephyr/qemu in the examples repository needs no hardware. It checks JSON, a timer, TCP and TLS connections
    to itself over the loopback interface and a worker thread that calls back into the main
    thread, prints \c PASS or \c FAIL for each, and serves a web page with the results. With \c qemu-system-arm installed, for
    example from the host tools of the Zephyr SDK:

    \code
    west build -b mps2/an385 zephyr/qemu -t run -- -DNT_ROOT=/path/to/Nt
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

    \section2 The Web Page

    The sample starts Nt HTTP Server on port 8080 of the board, ahead of the checks, so the page
    can be opened while they run and shows each result as it comes in. The board has an
    Ethernet controller that QEMU connects to its user-mode network, where the board is
    \c 10.0.2.15 (set in \c prj.conf), and QEMU can forward a port of the host to it. Start QEMU
    yourself for that, with the image that west built:

    \code
    qemu-system-arm -cpu cortex-m3 -machine mps2-an385 -display none \
        -nic user,model=lan9118,hostfwd=tcp:127.0.0.1:8080-:8080 -rtc clock=vm \
        -serial mon:stdio -kernel build/zephyr/zephyr.elf
    \endcode

    Then open \c{http://127.0.0.1:8080/} in a browser, or ask for the same data as JSON with
    \c{curl http://127.0.0.1:8080/api/status}. Leave out \c -icount: it makes the clock of the
    board run ahead of real time whenever the board is idle, and the page would show it.

    \note Zephyr 4.5's driver for the LAN9118 that QEMU emulates stops receiving for good after the
    first frame of 1514 bytes, the largest at the usual MTU of 1500: it reads a frame in whole
    32-bit words, into a packet that Zephyr limits to the MTU plus the Ethernet header, and a
    frame of that length does not fit. A browser sends such a frame at once, because its request
    carries its cookies for localhost, and the board then never answers again, while a small
    request from \c curl works. The sample avoids it by setting the MTU to 1498, whose largest frame
    is a multiple of four bytes (\c zephyr/qemu/src/board.cpp in the examples repository). An application of your own
    that serves to a browser through the emulated controller needs the same; a board with another
    Ethernet controller does not.

    The page also describes the board, as the program finds it out: the processor, the clock, the
    flash and RAM with how much of each is used, the heaps and stacks, the network buffers, the
    versions of the software, and the network interfaces, which it lists with QNetworkInterface.
    The same facts are in the \c hardware object of \c /api/status. The sample sets
    \c CONFIG_INIT_STACKS so that it can report how much of the main stack was ever used.

    \image zephyr-qemu-page-light.png The page of the sample for QEMU, in the light color scheme, served by the emulated board
    \image zephyr-qemu-page-dark.png The same page in the dark color scheme

    \section2 Boot Time

    From the reset of the emulated board to the first page takes about 2.6 s on a desktop, and the
    page itself reports the board's own figure, 2.37 s, as "Reset to serving". Of that, 2.1 s are
    sleeps in Zephyr's driver for the LAN9118: 100 ms to reset the PHY and 2000 ms that its source
    calls compulsory. The remaining 0.27 s cover the kernel, the drivers, the cryptography and the
    network stack, Nt's start-up and the listen, and the first request is answered about 60 ms
    after that. A board with another Ethernet controller starts without those sleeps. The times
    are those of the emulated processor on one machine, not of real hardware.

    The code is in \c zephyr/qemu/src/webpage.cpp: four routes on a QHttpServer, bound
    to a QTcpServer, with the page built from a string. It shows the Nt logo, which the server
    sends from \c{/logo.svg} unchanged and the page also uses as its icon. The checks stay in the sample's
    \c main.cpp and report to the page with \c recordResult().

    \section1 Configuration

    \c CONFIG_NT selects what Nt needs from Zephyr: the full C++ standard library, exceptions,
    run-time type information, the POSIX API with \c eventfd and the file system calls, thread
    local storage and dynamically created threads. \c CONFIG_NT_NETWORK needs
    \c CONFIG_NETWORKING and \c CONFIG_NET_SOCKETS.

    The event loop of a thread waits for all of its sockets in one \c poll() if it can.
    Zephyr's \c poll() fails when it has to wait for more events than
    \c CONFIG_ZVFS_POLL_MAX, and counts one event for reading and another for writing on a
    connected TCP socket, plus one for the descriptor that wakes the loop up. Its default of 3
    is used up by a single connection, so Nt raises it to 16. Any limit of 3 or more works: if
    the sockets of a thread need more events than that, the loop polls them in groups, each
    with the wake-up descriptor, taking a few milliseconds per group to notice a socket in
    another group, and the sample for QEMU passes with the limit at 3. A higher limit gives lower
    latency with many sockets, at the price of that many events on the stack of the polling
    thread. \c CONFIG_ZVFS_OPEN_MAX bounds the open descriptors of the whole application, and
    \c CONFIG_NET_MAX_CONTEXTS and \c CONFIG_NET_MAX_CONN the sockets of the network stack.

    \section1 GPIO and Peripherals

    \c CONFIG_NT_GPIO builds Nt GPIO, linked as \c Nt::Gpio: QGpioLine, and QPwmChannel,
    QAnalogInput, QAnalogOutput, QI2cDevice and QSpiDevice for each of \c CONFIG_PWM,
    \c CONFIG_ADC, \c CONFIG_DAC, \c CONFIG_I2C and \c CONFIG_SPI that is set. A GPIO line is
    named by its controller and pin, with the controller's node label or device name (\c gpio9:3,
    \c gpio@40c64000:3), or by the alias or node label of an LED or key of the board (\c led0,
    \c sw0, \c green_led), whose pin and flags come from the devicetree. The other devices are named
    by the name \c device_get_binding() takes and a channel or chip select number, such as
    \c i2c@40044000 or \c pwm@40018000:1; see \l{Using Nt GPIO} and
    \l{Using PWM, Analog I/O, I2C and SPI}.

    On the MIMXRT1170-EVKB, \c led0 and \c led1 are the green and red user LEDs and \c sw0 the
    user button SW7. Inputs on GPIO1 to GPIO6 and GPIO13 work by interrupt; Zephyr gives the
    Cortex-M7 no interrupts for GPIO7 to GPIO12, so inputs there are polled. \c zephyr/gpio in
    the \l{Nt Examples}{examples repository} blinks \c led0 and lights \c led1 while the button is
    held:

    \code
    west build -b mimxrt1170_evk/mimxrt1176/cm7 zephyr/gpio -- -DNT_ROOT=/path/to/Nt
    west flash
    \endcode

    \section1 Files and Settings

    \c CONFIG_NT_STORAGE mounts LittleFS on the flash partition labelled \c storage_partition at
    \c CONFIG_NT_HOME_PATH (\c{/lfs}) before \c main() runs, and formats the partition the first
    time. QDir::homePath() is that directory, so QFile, QSaveFile and QTemporaryFile (in
    \c{/lfs/tmp}) work on it, the locations of QStandardPaths lie below it (AppDataLocation is
    \c{/lfs/.local/share/<application>}), and QSettings keeps its file in
    \c{/lfs/.config/<organization>/<application>.conf}, across reboots:

    \code
    CONFIG_NT_STORAGE=y
    \endcode

    \code
    QCoreApplication::setOrganizationName(QStringLiteral("acme"));
    QCoreApplication::setApplicationName(QStringLiteral("sensor"));
    QSettings settings;
    const int boots = settings.value(QStringLiteral("boots"), 0).toInt() + 1;
    settings.setValue(QStringLiteral("boots"), boots);
    \endcode

    The MIMXRT1170-EVKB has a \c storage_partition of almost 2 MB in its QSPI flash. A board
    without one adds it in an overlay; a file system that the fstab of the devicetree mounts
    elsewhere works too, with \c CONFIG_NT_HOME_PATH set to its mount point. Each write goes to
    the flash, which wears out after many erase cycles, so do not write settings many times a
    second.

    \section1 Watchdog and Firmware Updates

    \c CONFIG_NT_DEVICE builds Nt Device, linked as \c Nt::Device. QWatchdog arms the watchdog that
    the alias \c watchdog0 names (\c CONFIG_WATCHDOG) and, with autoFeed, resets the board when the
    event loop hangs; QFirmwareUpdate writes a signed image into \c slot1_partition for MCUboot and
    reboots into it, and the new image confirms itself or is rolled back. See \l{Using Nt Device}.
    \c zephyr/update in the \l{Nt Examples}{examples repository} does both: build it with
    \c{west build --sysbuild ... -- -DZEPHYR_EXTRA_MODULES=/path/to/Nt}, since with sysbuild
    \c -DNT_ROOT does not reach the application. Signing needs the Python package \c cryptography
    in the environment of west.

    \section1 MQTT

    \c CONFIG_NT_MQTT, with \c CONFIG_NT_NETWORK, builds Nt Mqtt, linked as \c Nt::Mqtt: QMqttClient
    publishes the readings of a device to an MQTT broker and receives its commands, over TCP or,
    with \c CONFIG_NT_TLS, over TLS; see \l{Using Nt Mqtt}. \c zephyr/mqtt in the
    \l{Nt Examples}{examples repository} publishes the uptime of the board and prints the commands it
    receives. Give each board a client identifier of its own with QMqttClient::setClientId(): the
    random one needs an entropy driver.

    \section1 Serial Ports

    \c CONFIG_NT_SERIALPORT, with \c CONFIG_SERIAL, builds Nt SerialPort, linked as
    \c Nt::SerialPort: QSerialPort reads and writes a UART through the interrupt-driven API of its
    driver, for a GPS receiver, a modem or a radio. A port is named by the node label of its UART
    (\c lpuart2) or by its device name; QSerialPortInfo::availablePorts() lists them all. The
    interrupt handler moves bytes into a receive buffer of \c CONFIG_NT_SERIALPORT_BUFFER_SIZE bytes,
    and bytes that arrive while it is full are lost. Leave the UART of the console alone: an open
    port takes its interrupts over. \c zephyr/serial in the \l{Nt Examples}{examples repository}
    puts LPUART2 of the MIMXRT1170-EVKB, on D0 and D1 of the Arduino header, in loopback mode and
    writes a line every second, which it reads back.

    \section1 CAN Bus

    \c CONFIG_NT_SERIALBUS, with \c CONFIG_CAN, builds Nt SerialBus, linked as \c Nt::SerialBus: a
    QCanBusDevice of the backend \c zephyrcan sends and receives CAN and CAN FD frames on a controller
    of the board, named \c canbus for the one that \c zephyr,canbus chooses (FlexCAN3 on the
    MIMXRT1170-EVKB); see \l{Using Nt SerialBus}. \c zephyr/can in the \l{Nt Examples}{examples
    repository} sends a frame every second in loopback mode, which needs no transceiver, and prints
    what comes back.

    \section1 Sensors

    \c CONFIG_NT_SENSORS, with \c CONFIG_SENSOR and the driver of the sensor, builds Nt Sensors, linked
    as \c Nt::Sensors. QSensor and its subclasses read the sensors that the devicetree aliases
    \c accel0, \c gyro0, \c magn0, \c press0, \c ambient-temp0, \c die-temp0, \c humidity0 and
    \c light0 name, or any sensor device by its device name; see \l{Using Nt Sensors}. The
    MIMXRT1170-EVKB has no such sensor of its own, so one goes on its Arduino header or I2C bus.
    \c zephyr/sensors in the \l{Nt Examples}{examples repository} lists the sensors and prints a
    reading of each every second.

    \section1 Audio and Headsets

    \c CONFIG_NT_MULTIMEDIA builds the audio part of Nt Multimedia, linked as \c Nt::Multimedia:
    QAudioSink, QAudioSource, QAudioInput, QAudioOutput, QSoundEffect, QAudioDecoder, QMediaDevices
    and QAudioHeadset. It needs \c CONFIG_I2S, \c CONFIG_AUDIO_CODEC (under \c CONFIG_AUDIO) and
    \c CONFIG_I2C. Cameras, video, QMediaPlayer, QMediaRecorder and QMediaCaptureSession need Nt GUI
    and are left out (\c QT_NO_CAMERA).

    Sound goes through the I2S controller named by the devicetree alias \c i2s-codec-tx and the codec
    labelled \c audio_codec, which the board files of the MIMXRT1170-EVKB provide: SAI1 and the
    WM8962. QMediaDevices lists one output and one input, named after the codec's node, such as
    \c wm8962@1a. They play and record one or two channels of \c Int16 or \c Int32 samples at 8 to
    48 kHz; a stream with one channel is copied to both headphones, or recorded from the channel that
    QAudioHeadset::microphoneChannel() names. The output and the input can be open together, at the
    same sample rate and sample size.

    \code
    # prj.conf
    CONFIG_I2C=y
    CONFIG_I2S=y
    CONFIG_AUDIO=y
    CONFIG_AUDIO_CODEC=y
    CONFIG_NT_MULTIMEDIA=y
    CONFIG_HEAP_MEM_POOL_SIZE=131072
    \endcode

    \code
    QAudioHeadset headset;
    headset.setMicrophoneImpedance(150);     // a dynamic microphone: bias off, +36 dB
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    QAudioSource source(QMediaDevices::defaultAudioInput(), format);
    QIODevice *mic = source.start();
    \endcode

    The SAI of the i.MX RT receives in step with its transmitter, so one thread runs both
    directions: it sends silence while nothing plays, which keeps recording going. The blocks that
    its DMA moves hold 10 ms each and are kept in memory that the data cache does not cover
    (\c CONFIG_NOCACHE_MEMORY); \c CONFIG_NT_AUDIO_BLOCK_SIZE and \c CONFIG_NT_AUDIO_BLOCK_COUNT
    size them. The thread, and one for each open stream, take \c CONFIG_NT_THREAD_STACK_SIZE each
    from the kernel heap.

    QAudioHeadset controls the WM8962: the bias of the microphone and its level, the input pin, and
    the gain of the input PGA and its boost, from -23.25 to +54 dB. While the bias is on it reports
    the microphone being plugged in and the button of the headset, which the codec senses on the
    microphone contact. Zephyr's driver for the codec resets it when it sets it up for a stream,
    and the settings are applied again after it. With another codec QAudioHeadset::isAvailable()
    is \c false.

    \c zephyr/headset in the \l{Nt Examples}{examples repository} sets up the microphone for the
    impedance in its configuration, prints its level and the button, and plays it back on the
    headphones:

    \code
    west build -b mimxrt1170_evk/mimxrt1176/cm7 zephyr/headset -- -DNT_ROOT=/path/to/Nt \
        -DCONFIG_APP_MIC_IMPEDANCE=150
    west flash
    \endcode

    \section1 FM Synthesis

    \c CONFIG_NT_FM builds Nt FM, linked as \c Nt::FM: QtFM::Engine synthesises sound effects and
    plays MIDI files and OPL music in real time; see \l{Using Nt FM}. With \c CONFIG_NT_MULTIMEDIA,
    Engine::start() plays through the audio codec described above, in stereo with 16-bit samples
    at the engine's sample rate, which has to be one the codec runs at (the default, 48 kHz, is).
    A thread of Nt FM renders a block of \c EngineConfig::blockSize frames at a time and hands it to
    the codec's engine, so the latency is the four blocks of the stream's ring and the
    \c CONFIG_NT_AUDIO_BLOCK_COUNT blocks of the bus, about 80 ms with the defaults. The codec
    plays one stream at a time, so an engine and a QAudioSink cannot both play.

    The synthesiser works in single-precision floating point, so the option implies
    \c CONFIG_FPU, which the Cortex-M7 of the i.MX RT1170 has. An engine
    whose configuration has no seed draws one from QRandomGenerator::global(), which needs the
    entropy driver (\c CONFIG_ENTROPY_GENERATOR, also implied). VGZ files (gzipped VGM) are not
    read, as Zephyr has no zlib.

    \code
    CONFIG_I2C=y
    CONFIG_I2S=y
    CONFIG_AUDIO=y
    CONFIG_AUDIO_CODEC=y
    CONFIG_NT_MULTIMEDIA=y
    CONFIG_NT_FM=y
    \endcode

    \c zephyr/fm in the \l{Nt Examples}{examples repository} prints how fast the board renders eight
    voices, then plays a sound effect of each category every second over an engine hum:

    \code
    west build -b mimxrt1170_evk/mimxrt1176/cm7 zephyr/fm -- -DNT_ROOT=/path/to/Nt
    west flash
    \endcode

    \section1 Supported Hardware

    \table
    \header \li Board \li Status
    \row \li NXP MIMXRT1170-EVKB (\c mimxrt1170_evk/mimxrt1176/cm7, revision B) \li The primary
         target. Nt GPIO (its LEDs and user button) and Nt Multimedia audio (headphones and microphone
         on its WM8962, with QAudioHeadset) run on it; Nt Core and Nt Network build for it.
    \row \li NXP MIMXRT1170-EVKB, FM synthesis \li Nt FM builds for it and plays through its WM8962;
         not yet run on the board.
    \row \li NXP MIMXRT1170-EVKB, CAN \li Nt SerialBus builds for its FlexCAN controllers (CAN FD);
         not yet run on the board.
    \row \li NXP MIMXRT1170-EVKB, UART \li Nt SerialPort builds for its LPUARTs; not yet run on the
         board.
    \row \li NXP MIMXRT1170-EVKB, flash \li \c CONFIG_NT_STORAGE builds for its \c storage_partition;
         not yet run on the board.
    \row \li NXP MIMXRT1170-EVK (\c mimxrt1170_evk@A/mimxrt1176/cm7, revision A) \li Nt Multimedia
         audio builds for its WM8960, untested, without QAudioHeadset.
    \row \li Arm MPS2 AN385 in QEMU (\c mps2/an385) \li Runs the self-checking sample, without audio;
         QSerialPort on its second UART, Nt Mqtt over its emulated Ethernet,
         \c CONFIG_NT_STORAGE on a simulated flash partition and QWatchdog resetting a hung event loop
         have run on it too.
    \endtable

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
        \li \b Speed. A handshake computes elliptic curves, which takes noticeable time on a core
            with no help from hardware, a few seconds for the sample under QEMU on a desktop, so
            the sample gives QSslServer::setHandshakeTimeout() a generous time.
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
        \li QLibrary and QPluginLoader (\c QT_NO_LIBRARY), and static plugins
        \li QSharedMemory (\c QT_NO_SHAREDMEMORY) and QSystemSemaphore (\c QT_NO_SYSTEMSEMAPHORE)
        \li QFileSystemWatcher (\c QT_NO_FILESYSTEMWATCHER)
        \li QStorageInfo (\c QT_NO_STORAGEINFO)
        \li QLocalSocket (\c QT_NO_LOCALSOCKET) and QLocalServer (\c QT_NO_LOCALSERVER)
        \li QSslSocket and \c https: (\c QT_NO_SSL), unless \c CONFIG_NT_TLS is set
        \li Nt HTTP Server and Nt WebSockets, unless \c CONFIG_NT_HTTPSERVER is set; they serve
            on TCP and, with TLS, QSslServer, but QAbstractHttpServer::bind() takes no
            QLocalServer there
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
            exists is readable, writable and owned by user 0. QDir::homePath() is
            \c CONFIG_NT_HOME_PATH, below which QStandardPaths and QSettings keep their files; see
            \l{Files and Settings}.
        \li \b Time. Local time is UTC, as the C library has no time zones. The steady clock of
            the C++ library follows the system clock, so setting the time of day moves the
            timers that are running.
        \li \b Network. QNetworkInterface lists the interfaces of the Zephyr network stack, with
            their addresses, netmasks and hardware address. Host names are resolved with
            \c getaddrinfo(), which needs \c CONFIG_DNS_RESOLVER.
        \li \b Numbers. Nt writes a number with decimals through \c vsnprintf(), and the picolibc of
            Zephyr prints \c{*float*} in its place unless its printf supports floating point.
            \c CONFIG_NT selects \c CONFIG_REQUIRES_FLOAT_PRINTF for that, which costs some flash.
        \li \b Randomness. QRandomGenerator::system() reads \c getentropy(), which needs
            an entropy driver on the board.
    \endlist

    \sa {Installing Nt}, {Thread Support in Nt}
*/
