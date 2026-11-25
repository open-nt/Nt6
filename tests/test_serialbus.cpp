// MIT License - see LICENSE
//
// Nt SerialBus: QCanBusFrame, QCanBusDevice on the virtual bus inside the process, and SocketCAN on vcan0 when the
// machine has that interface (sudo modprobe vcan && sudo ip link add vcan0 type vcan && sudo ip link set up vcan0).
#include <QtCore>
#include <QtSerialBus>

#include <cstdio>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> bool waitFor(F cond, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(5);
    return cond();
}

void testFrames() {
    QCanBusFrame empty;
    CHECK(empty.frameType() == QCanBusFrame::DataFrame && empty.isValid() && empty.frameId() == 0);

    QCanBusFrame base(0x123, QByteArray::fromHex("01020304"));
    CHECK(base.isValid() && !base.hasExtendedFrameFormat());
    CHECK(base.toString() == QStringLiteral("     123   [4]  01 02 03 04"));

    QCanBusFrame extended(0x12345678, QByteArray::fromHex("0102030405060708"));
    CHECK(extended.hasExtendedFrameFormat() && extended.isValid());
    CHECK(extended.toString() == QStringLiteral("12345678   [8]  01 02 03 04 05 06 07 08"));
    QCanBusFrame small(0x234, QByteArray());
    small.setExtendedFrameFormat(true);
    CHECK(small.toString() == QStringLiteral("00000234   [0]"));

    // An identifier that needs 29 bits in the base format, or more than 29 bits, is invalid.
    QCanBusFrame wrongFormat(0x800, QByteArray());
    wrongFormat.setExtendedFrameFormat(false);
    CHECK(!wrongFormat.isValid());
    QCanBusFrame tooBig(0x20000000, QByteArray());
    CHECK(!tooBig.isValid());
    tooBig.setFrameId(0x7ff);
    CHECK(tooBig.isValid());

    // Payload lengths: up to 8 on classic CAN; the lengths of CAN FD up to 64.
    QCanBusFrame long9(0x1, QByteArray(9, 'x'));
    CHECK(!long9.isValid());
    long9.setFlexibleDataRateFormat(true);
    CHECK(!long9.isValid());
    long9.setPayload(QByteArray(12, 'x'));
    CHECK(long9.isValid());
    long9.setPayload(QByteArray(64, 'x'));
    CHECK(long9.isValid());
    long9.setPayload(QByteArray(65, 'x'));
    CHECK(!long9.isValid());
    QCanBusFrame fd(0x10, QByteArray(10, '\x0a'));
    fd.setFlexibleDataRateFormat(true);
    CHECK(fd.toString() == QStringLiteral("      10  [10]  0A 0A 0A 0A 0A 0A 0A 0A 0A 0A"));
    // The bit rate switch and error state indicator belong to CAN FD.
    QCanBusFrame brs;
    brs.setBitrateSwitch(true);
    CHECK(brs.hasFlexibleDataRateFormat());
    brs.setErrorStateIndicator(true);
    brs.setFlexibleDataRateFormat(false);
    CHECK(!brs.hasBitrateSwitch() && !brs.hasErrorStateIndicator());

    // Remote requests.
    QCanBusFrame remote(0x123, QByteArray(5, '\0'));
    remote.setFrameType(QCanBusFrame::RemoteRequestFrame);
    CHECK(remote.isValid() && remote.toString() == QStringLiteral("     123   [5]  Remote Request"));
    remote.setFlexibleDataRateFormat(true);
    CHECK(!remote.isValid()); // CAN FD has no remote frames

    // Error frames keep their errors where the identifier would be.
    QCanBusFrame error(QCanBusFrame::ErrorFrame);
    error.setError(QCanBusFrame::BusOffError | QCanBusFrame::ControllerError);
    CHECK(error.error() == (QCanBusFrame::BusOffError | QCanBusFrame::ControllerError));
    CHECK(error.frameId() == 0 && error.toString() == QStringLiteral("(Error)"));
    base.setError(QCanBusFrame::BusOffError);
    CHECK(base.error() == QCanBusFrame::NoError);
    CHECK(QCanBusFrame(QCanBusFrame::InvalidFrame).toString() == QStringLiteral("(Invalid)"));
    CHECK(!QCanBusFrame(QCanBusFrame::InvalidFrame).isValid());

    // Time stamps and the data stream.
    const QCanBusFrame::TimeStamp ts = QCanBusFrame::TimeStamp::fromMicroSeconds(3000123);
    CHECK(ts.seconds() == 3 && ts.microSeconds() == 123);
    extended.setTimeStamp(ts);
    extended.setLocalEcho(true);
    QByteArray bytes;
    {
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out << extended << fd << error;
    }
    QDataStream in(bytes);
    QCanBusFrame a, b, c;
    in >> a >> b >> c;
    CHECK(a.frameId() == 0x12345678 && a.payload() == extended.payload() && a.hasExtendedFrameFormat());
    CHECK(a.timeStamp().seconds() == 3 && a.hasLocalEcho());
    CHECK(b.hasFlexibleDataRateFormat() && b.payload().size() == 10);
    CHECK(c.frameType() == QCanBusFrame::ErrorFrame && c.error() == error.error());
}

std::unique_ptr<QCanBusDevice> makeDevice(const QString &plugin, const QString &name) {
    QString error;
    std::unique_ptr<QCanBusDevice> device(QCanBus::instance()->createDevice(plugin, name, &error));
    if (!device)
        std::printf("  cannot create %s %s: %s\n", qPrintable(plugin), qPrintable(name), qPrintable(error));
    return device;
}

void testBus() {
    QCanBus *bus = QCanBus::instance();
    CHECK(bus->plugins().contains(QStringLiteral("virtualcan")));
    QString error;
    CHECK(bus->createDevice(QStringLiteral("nosuchcan"), QStringLiteral("can0"), &error) == nullptr);
    CHECK(!error.isEmpty());
    CHECK(bus->availableDevices(QStringLiteral("nosuchcan"), &error).isEmpty() && !error.isEmpty());
    const QList<QCanBusDeviceInfo> virtuals = bus->availableDevices(QStringLiteral("virtualcan"));
    CHECK(virtuals.size() == 2 && virtuals.at(0).name() == QStringLiteral("can0"));
    CHECK(virtuals.at(0).isVirtual() && virtuals.at(0).hasFlexibleDataRate());
    CHECK(virtuals.at(0).plugin() == QStringLiteral("virtualcan"));
    CHECK(bus->availableDevices().size() >= 2);
}

void testVirtualDevices() {
    auto a = makeDevice(QStringLiteral("virtualcan"), QStringLiteral("testbus"));
    auto b = makeDevice(QStringLiteral("virtualcan"), QStringLiteral("testbus"));
    auto other = makeDevice(QStringLiteral("virtualcan"), QStringLiteral("otherbus"));
    if (!a || !b || !other)
        return;
    CHECK(a->deviceInfo().name() == QStringLiteral("testbus"));

    // Not connected: nothing can be written or read.
    CHECK(a->state() == QCanBusDevice::UnconnectedState);
    CHECK(!a->writeFrame(QCanBusFrame(0x1, QByteArray("x"))));
    CHECK(a->error() == QCanBusDevice::WriteError);
    CHECK(a->readFrame().frameType() == QCanBusFrame::InvalidFrame);
    a->clear();
    CHECK(a->error() == QCanBusDevice::OperationError);

    QList<QCanBusDevice::CanBusDeviceState> states;
    QObject::connect(a.get(), &QCanBusDevice::stateChanged,
                     [&](QCanBusDevice::CanBusDeviceState s) { states.append(s); });
    CHECK(a->connectDevice() && b->connectDevice() && other->connectDevice());
    CHECK((states ==
           QList<QCanBusDevice::CanBusDeviceState>{QCanBusDevice::ConnectingState, QCanBusDevice::ConnectedState}));
    CHECK(a->error() == QCanBusDevice::NoError);
    CHECK(!a->connectDevice() && a->error() == QCanBusDevice::ConnectionError);

    int received = 0;
    qint64 written = 0;
    QObject::connect(b.get(), &QCanBusDevice::framesReceived, [&] { ++received; });
    QObject::connect(a.get(), &QCanBusDevice::framesWritten, [&](qint64 n) { written += n; });
    CHECK(a->writeFrame(QCanBusFrame(0x123, QByteArray::fromHex("cafe"))));
    CHECK(waitFor([&] { return b->framesAvailable() == 1; }));
    CHECK(received == 1 && written == 1);
    const QCanBusFrame got = b->readFrame();
    CHECK(got.frameId() == 0x123 && got.payload() == QByteArray::fromHex("cafe") && !got.hasLocalEcho());
    CHECK(got.timeStamp().seconds() > 0 || got.timeStamp().microSeconds() > 0);
    // The sender does not get its own frame unless asked, and another bus gets nothing.
    spinEvents(20);
    CHECK(a->framesAvailable() == 0 && other->framesAvailable() == 0);
    a->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, true);
    CHECK(a->writeFrame(QCanBusFrame(0x124, QByteArray("own"))));
    CHECK(waitFor([&] { return a->framesAvailable() == 1; }));
    CHECK(a->readFrame().hasLocalEcho());
    b->readAllFrames();

    // CAN FD needs CanFdKey on both ends.
    QCanBusFrame fd(0x200, QByteArray(32, 'f'));
    fd.setFlexibleDataRateFormat(true);
    CHECK(!a->writeFrame(fd) && a->error() == QCanBusDevice::WriteError);
    a->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
    CHECK(a->writeFrame(fd));
    spinEvents(20);
    CHECK(b->framesAvailable() == 0);
    b->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
    CHECK(a->writeFrame(fd));
    CHECK(waitFor([&] { return b->framesAvailable() == 1; }));
    CHECK(b->readFrame().payload().size() == 32);
    // Invalid frames and error frames are refused.
    CHECK(!a->writeFrame(QCanBusFrame(0x1, QByteArray(9, 'x'))));
    CHECK(!a->writeFrame(QCanBusFrame(QCanBusFrame::ErrorFrame)));

    // Filters: 0x100 to 0x1FF in the base format, or extended remote requests.
    QCanBusDevice::Filter range;
    range.frameId = 0x100;
    range.frameIdMask = 0x700;
    range.format = QCanBusDevice::Filter::MatchBaseFormat;
    QCanBusDevice::Filter remote;
    remote.type = QCanBusFrame::RemoteRequestFrame;
    remote.format = QCanBusDevice::Filter::MatchExtendedFormat;
    b->setConfigurationParameter(QCanBusDevice::RawFilterKey,
                                 QVariant::fromValue(QList<QCanBusDevice::Filter>{range, remote}));
    CHECK(b->configurationKeys().contains(QCanBusDevice::RawFilterKey));
    a->writeFrame(QCanBusFrame(0x1ab, QByteArray("in")));
    a->writeFrame(QCanBusFrame(0x2ab, QByteArray("out")));
    QCanBusFrame extendedData(0x10000, QByteArray("out"));
    a->writeFrame(extendedData);
    QCanBusFrame extendedRemote(0x10000, QByteArray(2, '\0'));
    extendedRemote.setFrameType(QCanBusFrame::RemoteRequestFrame);
    a->writeFrame(extendedRemote);
    CHECK(waitFor([&] { return b->framesAvailable() == 2; }));
    spinEvents(20);
    const QList<QCanBusFrame> filtered = b->readAllFrames();
    CHECK(filtered.size() == 2);
    CHECK(filtered.size() == 2 && filtered.at(0).frameId() == 0x1ab);
    CHECK(filtered.size() == 2 && filtered.at(1).frameType() == QCanBusFrame::RemoteRequestFrame);
    b->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant());
    CHECK(!b->configurationParameter(QCanBusDevice::RawFilterKey).isValid());

    // Waiting.
    QTimer::singleShot(10, [&] { a->writeFrame(QCanBusFrame(0x7, QByteArray("w"))); });
    CHECK(b->waitForFramesReceived(2000));
    CHECK(!b->waitForFramesReceived(30)); // nothing more comes
    CHECK(a->waitForFramesWritten(100));  // nothing waits to be written
    b->clear(QCanBusDevice::Input);
    CHECK(b->framesAvailable() == 0);

    // The virtual bus has no controller to reset or report on.
    CHECK(!a->hasBusStatus() && a->busStatus() == QCanBusDevice::CanBusStatus::Unknown);
    a->resetController();
    CHECK(a->error() == QCanBusDevice::OperationError);

    // Disconnecting throws away what was not read, and a disconnected device receives nothing.
    a->writeFrame(QCanBusFrame(0x8, QByteArray("x")));
    CHECK(waitFor([&] { return b->framesAvailable() == 1; }));
    b->disconnectDevice();
    CHECK(b->state() == QCanBusDevice::UnconnectedState && b->framesAvailable() == 0);
    a->writeFrame(QCanBusFrame(0x9, QByteArray("x")));
    spinEvents(20);
    CHECK(b->framesAvailable() == 0);
    // A device that goes away while a frame is on its way does not crash the sender.
    a->writeFrame(QCanBusFrame(0xa, QByteArray("x")));
    other.reset();
    a->disconnectDevice();
    CHECK(a->state() == QCanBusDevice::UnconnectedState);
}

// A backend of one's own, registered with QCanBus::registerFactory().
class LoopbackDevice : public QCanBusDevice {
public:
    bool writeFrame(const QCanBusFrame &frame) override {
        enqueueReceivedFrames({frame});
        emit framesWritten(1);
        return true;
    }
    QString interpretErrorFrame(const QCanBusFrame &) override { return QString(); }

protected:
    bool open() override { return true; }
    void close() override {}
};

class LoopbackFactory : public QCanBusFactory {
public:
    QCanBusDevice *createDevice(const QString &, QString *) const override { return new LoopbackDevice; }
    QList<QCanBusDeviceInfo> availableDevices(QString *) const override { return {}; }
};

void testOwnBackend() {
    static LoopbackFactory factory;
    QCanBus::instance()->registerFactory(QStringLiteral("loopbackcan"), &factory);
    CHECK(QCanBus::instance()->plugins().contains(QStringLiteral("loopbackcan")));
    auto device = makeDevice(QStringLiteral("loopbackcan"), QStringLiteral("x"));
    CHECK(device && device->connectDevice());
    if (device) {
        device->writeFrame(QCanBusFrame(0x42, QByteArray("echo")));
        CHECK(device->readFrame().payload() == QByteArray("echo"));
        device->disconnectDevice();
    }
    QCanBus::instance()->registerFactory(QStringLiteral("loopbackcan"), nullptr);
    CHECK(!QCanBus::instance()->plugins().contains(QStringLiteral("loopbackcan")));
}

void testSocketCan() {
    if (!QCanBus::instance()->plugins().contains(QStringLiteral("socketcan")))
        return;
    // Listing works on any machine.
    for (const QCanBusDeviceInfo &info : QCanBus::instance()->availableDevices(QStringLiteral("socketcan")))
        CHECK(info.plugin() == QStringLiteral("socketcan") && !info.name().isEmpty());
    const QString name = qEnvironmentVariable("NT_TEST_CAN_INTERFACE", QStringLiteral("vcan0"));
    if (!QFile::exists(QStringLiteral("/sys/class/net/") + name)) {
        std::printf("no %s: SocketCAN not tested on a bus\n", qPrintable(name));
        auto missing = makeDevice(QStringLiteral("socketcan"), QStringLiteral("nosuchcan9"));
        CHECK(missing && !missing->connectDevice() && missing->error() == QCanBusDevice::ConnectionError);
        return;
    }
    auto a = makeDevice(QStringLiteral("socketcan"), name);
    auto b = makeDevice(QStringLiteral("socketcan"), name);
    if (!a || !b)
        return;
    CHECK(a->connectDevice() && b->connectDevice());
    CHECK(a->writeFrame(QCanBusFrame(0x321, QByteArray::fromHex("0102"))));
    CHECK(waitFor([&] { return b->framesAvailable() >= 1; }));
    const QCanBusFrame got = b->readFrame();
    CHECK(got.frameId() == 0x321 && got.payload() == QByteArray::fromHex("0102"));
    CHECK(got.timeStamp().seconds() > 0);
    QCanBusFrame ext(0x1abcdef, QByteArray("ext"));
    QCanBusFrame rtr(0x55, QByteArray(3, '\0'));
    rtr.setFrameType(QCanBusFrame::RemoteRequestFrame);
    a->writeFrame(ext);
    a->writeFrame(rtr);
    CHECK(waitFor([&] { return b->framesAvailable() >= 2; }));
    const QList<QCanBusFrame> two = b->readAllFrames();
    CHECK(two.size() == 2 && two.at(0).hasExtendedFrameFormat() && two.at(0).frameId() == 0x1abcdef);
    CHECK(two.size() == 2 && two.at(1).frameType() == QCanBusFrame::RemoteRequestFrame &&
          two.at(1).payload().size() == 3);
    // Receive own frames, CAN FD and a filter.
    a->setConfigurationParameter(QCanBusDevice::ReceiveOwnKey, true);
    a->writeFrame(QCanBusFrame(0x1, QByteArray("me")));
    CHECK(waitFor([&] { return a->framesAvailable() >= 1; }));
    CHECK(a->readFrame().hasLocalEcho());
    b->readAllFrames();
    if (a->deviceInfo().hasFlexibleDataRate()) {
        a->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
        b->setConfigurationParameter(QCanBusDevice::CanFdKey, true);
        QCanBusFrame fd(0x2, QByteArray(48, 'z'));
        fd.setBitrateSwitch(true);
        CHECK(a->writeFrame(fd));
        CHECK(waitFor([&] { return b->framesAvailable() >= 1; }));
        const QCanBusFrame fdGot = b->readFrame();
        CHECK(fdGot.hasFlexibleDataRateFormat() && fdGot.payload().size() == 48 && fdGot.hasBitrateSwitch());
    }
    QCanBusDevice::Filter only;
    only.frameId = 0x77;
    only.frameIdMask = 0x7ff;
    only.format = QCanBusDevice::Filter::MatchBaseFormat;
    b->setConfigurationParameter(QCanBusDevice::RawFilterKey, QVariant::fromValue(QList<QCanBusDevice::Filter>{only}));
    a->writeFrame(QCanBusFrame(0x76, QByteArray("no")));
    a->writeFrame(QCanBusFrame(0x77, QByteArray("yes")));
    CHECK(waitFor([&] { return b->framesAvailable() >= 1; }));
    spinEvents(30);
    const QList<QCanBusFrame> only77 = b->readAllFrames();
    CHECK(only77.size() == 1 && only77.at(0).frameId() == 0x77);
    std::printf("checked SocketCAN on %s\n", qPrintable(name));
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testFrames();
    testBus();
    testVirtualDevices();
    testOwnBackend();
    testSocketCan();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all serialbus tests passed\n");
    return 0;
}
