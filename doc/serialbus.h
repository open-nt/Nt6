// MIT License - see LICENSE
//
// Overview page about Nt SerialBus for the NDoc documentation. This file holds only a documentation comment and is
// not part of the library.

/*!
    \page nt-serialbus.html
    \title Using Nt SerialBus
    \brief Sending and receiving CAN frames on Linux SocketCAN, Zephyr and a virtual bus.

    Nt SerialBus is the CAN part of Qt Serial Bus. It depends only on Nt Core, so it also runs
    headless and on Zephyr. Link with \c Nt::SerialBus (or \c Qt6::SerialBus).

    \section1 Sending and Receiving

    QCanBus makes a QCanBusDevice for an interface of a backend. Configure it, connect it, and it
    sends frames with writeFrame() and reports frames that arrive with framesReceived():

    \code
    QString error;
    std::unique_ptr<QCanBusDevice> device(
        QCanBus::instance()->createDevice(QStringLiteral("socketcan"), QStringLiteral("can0"), &error));
    if (!device)
        qFatal("%s", qPrintable(error));

    QCanBusDevice::Filter engine;                 // receive 0x100 to 0x1FF only
    engine.frameId = 0x100;
    engine.frameIdMask = 0x700;
    engine.format = QCanBusDevice::Filter::MatchBaseFormat;
    device->setConfigurationParameter(QCanBusDevice::RawFilterKey,
                                      QVariant::fromValue(QList<QCanBusDevice::Filter>{engine}));

    QObject::connect(device.get(), &QCanBusDevice::framesReceived, [&device] {
        for (const QCanBusFrame &frame : device->readAllFrames())
            qDebug() << frame.toString();
    });
    if (!device->connectDevice())
        qFatal("%s", qPrintable(device->errorString()));

    device->writeFrame(QCanBusFrame(0x123, QByteArray::fromHex("cafe")));
    \endcode

    A frame of CAN FD carries up to 64 bytes and needs \c CanFdKey set on the device:

    \code
    device->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
    QCanBusFrame frame(0x200, QByteArray(32, '\0'));
    frame.setFlexibleDataRateFormat(true);
    frame.setBitrateSwitch(true);                 // the payload at the data bit rate
    device->writeFrame(frame);
    \endcode

    \section1 Backends

    \table
    \header \li Backend \li Platform \li Interfaces
    \row \li \c socketcan \li Linux \li The CAN interfaces of the kernel, such as \c can0 or
         \c vcan0.
    \row \li \c zephyrcan \li Zephyr \li \c canbus for the controller that \c zephyr,canbus
         chooses, the aliases \c can0 to \c can3, or the name of a device, such as
         \c can@40c3c000.
    \row \li \c virtualcan \li All \li Any name: the devices of one name inside the process form a
         bus, for tests and simulations.
    \endtable

    A backend of your own, for example for a controller on SPI, is a QCanBusFactory registered with
    QCanBus::registerFactory().

    \section2 SocketCAN

    The options of the socket carry the configuration: \c LoopbackKey (on by default) lets the
    other applications of the machine see the frames that are sent, \c ReceiveOwnKey returns them to
    the sender with QCanBusFrame::hasLocalEcho() set, and \c ErrorFilterKey selects the error frames
    (all, by default). Frames carry the time stamps of the kernel. The bit rate is a setting of the
    interface, which needs root, and is set outside the application:

    \code
    sudo ip link set can0 type can bitrate 500000 dbitrate 2000000 fd on
    sudo ip link set up can0
    \endcode

    A virtual interface for trying things out is \c vcan0:

    \code
    sudo modprobe vcan
    sudo ip link add vcan0 type vcan
    sudo ip link set up vcan0
    \endcode

    \section2 Zephyr

    Set \c CONFIG_CAN and \c CONFIG_NT_SERIALBUS. \c BitRateKey and \c DataBitRateKey set the
    bit rates, \c CanFdKey the CAN FD mode, and \c LoopbackKey the loopback mode of the controller,
    in which it receives what it sends without a transceiver or a bus. busStatus() reports the
    state of the controller, a change of state arrives as an error frame, and resetController()
    restarts a controller that went bus off.

    \section1 What Is Missing

    Modbus, the CAN frame processor and the DBC parser of Qt Serial Bus are not there, nor the
    PEAK, Vector, SysTec, TinyCAN and PassThru backends. SocketCAN cannot set the bit rate or report
    the bus status itself, and the virtual bus does not reach other processes.

    \sa {Nt SerialBus C++ Classes}, {Nt on Zephyr}
*/
