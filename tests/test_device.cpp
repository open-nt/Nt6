// MIT License - see LICENSE
//
// Nt Device: QWatchdog against a fake backend (arming, auto-feeding from the event loop, a watchdog that cannot be
// stopped), the MCUboot header parser, and QFirmwareUpdate writing an image to a file with the Linux backend and
// booting it with a fake one.
#include <QtCore>
#include <QtDevice>

#include "qdevice_p.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

using namespace QtDevicePrivate;

void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

struct FakeWatchdog {
    int starts = 0;
    int feeds = 0;
    int stops = 0;
    int timeout = 0;
    bool stoppable = true;
    bool fail = false;
    bool resetByWatchdog = false;
    QString name;
};
FakeWatchdog g_dog;

class FakeWatchdogBackend final : public WatchdogBackend {
public:
    bool start(WatchdogConfig &config, Failure &failure) override {
        if (g_dog.fail) {
            setFailure(failure, ENOENT, QStringLiteral("/dev/nowhere"));
            return false;
        }
        ++g_dog.starts;
        g_dog.name = config.name;
        config.timeout = (config.timeout + 999) / 1000 * 1000; // whole seconds, as Linux has them
        g_dog.timeout = config.timeout;
        return true;
    }
    bool feed(Failure &) override {
        ++g_dog.feeds;
        return true;
    }
    bool stop(Failure &failure) override {
        if (!g_dog.stoppable) {
            setFailure(failure, Error::Unsupported, QStringLiteral("cannot stop"));
            return false;
        }
        ++g_dog.stops;
        return true;
    }
    bool causedLastReset(const QString &) override { return g_dog.resetByWatchdog; }
};

void testWatchdog() {
    setWatchdogBackendFactory(
        []() -> std::unique_ptr<WatchdogBackend> { return std::make_unique<FakeWatchdogBackend>(); });
    {
        QWatchdog dog(QStringLiteral("watchdog1"));
        CHECK(!dog.isActive() && dog.timeout() == 10000);
        CHECK(!dog.feed() && dog.error() == QWatchdog::NotOpenError);
        dog.clearError();
        dog.setTimeout(1500);
        QList<bool> active;
        QObject::connect(&dog, &QWatchdog::activeChanged, [&](bool a) { active.append(a); });
        CHECK(dog.start());
        CHECK(dog.isActive() && g_dog.starts == 1 && g_dog.name == QStringLiteral("watchdog1"));
        CHECK(dog.timeout() == 2000); // what the hardware uses
        CHECK(dog.feed() && g_dog.feeds == 1);
        CHECK(dog.start() && g_dog.starts == 1); // already active

        // Auto-feeding from the event loop, a third of the timeout apart.
        dog.setAutoFeed(true);
        const int before = g_dog.feeds;
        spinEvents(2100);
        CHECK(g_dog.feeds - before >= 2 && g_dog.feeds - before <= 4);
        CHECK(dog.stop() && !dog.isActive() && g_dog.stops == 1);
        const int afterStop = g_dog.feeds;
        spinEvents(800);
        CHECK(g_dog.feeds == afterStop); // no feeding once stopped
        CHECK(active == (QList<bool>{true, false}));
    }

    // A watchdog that cannot be stopped stays active.
    g_dog.stoppable = false;
    {
        QWatchdog dog;
        dog.setOptions(QWatchdog::PauseInDebugger);
        CHECK(dog.options() == QWatchdog::PauseInDebugger);
        CHECK(dog.start());
        CHECK(!dog.stop() && dog.isActive() && dog.error() == QWatchdog::UnsupportedOperationError);
    }
    g_dog.stoppable = true;

    // A device that does not exist.
    g_dog.fail = true;
    {
        QWatchdog dog;
        QList<QWatchdog::WatchdogError> errors;
        QObject::connect(&dog, &QWatchdog::errorOccurred, [&](QWatchdog::WatchdogError e) { errors.append(e); });
        CHECK(!dog.start() && !dog.isActive());
        CHECK(errors == (QList<QWatchdog::WatchdogError>{QWatchdog::DeviceNotFoundError}));
        CHECK(dog.errorString().contains(QStringLiteral("/dev/nowhere")));
    }
    g_dog.fail = false;

    g_dog.resetByWatchdog = true;
    CHECK(QWatchdog().causedLastReset());
    setWatchdogBackendFactory(nullptr);

    // The real Linux backend: a device that does not exist fails cleanly, without root.
    QWatchdog missing(QStringLiteral("/nonexistent/watchdog"));
    CHECK(!missing.start() && missing.error() == QWatchdog::DeviceNotFoundError);
    CHECK(!missing.causedLastReset());
}

// An MCUboot image: the 32-byte header, then imageSize bytes of payload (and, in a real image, the TLVs).
QByteArray makeImage(int major, int minor, int revision, int build, quint32 imageSize, quint32 headerSize = 0x200) {
    QByteArray image(static_cast<qsizetype>(headerSize + imageSize), '\x5a');
    const auto put32 = [&](int at, quint32 v) {
        for (int i = 0; i < 4; ++i)
            image[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
    };
    put32(0, kImageMagic);
    put32(4, 0);
    image[8] = static_cast<char>(headerSize & 0xff);
    image[9] = static_cast<char>(headerSize >> 8);
    image[10] = image[11] = 0;
    put32(12, imageSize);
    put32(16, 0);
    image[20] = static_cast<char>(major);
    image[21] = static_cast<char>(minor);
    image[22] = static_cast<char>(revision & 0xff);
    image[23] = static_cast<char>(revision >> 8);
    put32(24, static_cast<quint32>(build));
    put32(28, 0);
    for (quint32 i = 32; i < headerSize; ++i)
        image[static_cast<qsizetype>(i)] = 0;
    return image;
}

void testHeader() {
    ImageInfo info;
    CHECK(parseImageHeader(makeImage(1, 2, 300, 0, 100), info));
    CHECK(info.version == QVersionNumber(1, 2, 300) && info.imageSize == 100 && info.headerSize == 0x200);
    CHECK(parseImageHeader(makeImage(2, 0, 1, 77, 10), info));
    CHECK(info.version == QVersionNumber({2, 0, 1, 77}));
    QByteArray bad = makeImage(1, 0, 0, 0, 10);
    bad[0] = 0;
    CHECK(!parseImageHeader(bad, info));
    CHECK(!parseImageHeader(QByteArray(10, '\0'), info));
}

void testFirmwareFile() {
    QTemporaryDir dir;
    const QString target = dir.filePath(QStringLiteral("slot1.bin"));
    const QByteArray image = makeImage(1, 4, 2, 9, 5000);

    QFirmwareUpdate update;
    update.setTarget(target);
    QList<QFirmwareUpdate::State> states;
    QObject::connect(&update, &QFirmwareUpdate::stateChanged, [&](QFirmwareUpdate::State s) { states.append(s); });
    qint64 lastProgress = 0;
    qint64 lastTotal = 0;
    QObject::connect(&update, &QFirmwareUpdate::progress, [&](qint64 done, qint64 total) {
        lastProgress = done;
        lastTotal = total;
    });
    CHECK(!update.write("x", 1) && update.error() == QFirmwareUpdate::NotOpenError);
    CHECK(update.state() == QFirmwareUpdate::Idle);

    // Written through a QIODevice in pieces, as a download arrives.
    CHECK(update.begin(image.size()));
    QBuffer source;
    source.setData(image);
    source.open(QIODevice::ReadOnly);
    CHECK(update.writeFrom(&source));
    CHECK(update.bytesWritten() == image.size() && lastProgress == image.size() && lastTotal == image.size());
    CHECK(update.finish());
    CHECK(update.state() == QFirmwareUpdate::Written && update.imageVersion() == QVersionNumber({1, 4, 2, 9}));
    QFile written(target);
    CHECK(written.open(QIODevice::ReadOnly) && written.readAll() == image);
    // Booting it is left to the system on Linux.
    CHECK(!update.requestUpgrade() && update.error() == QFirmwareUpdate::UnsupportedOperationError);
    CHECK(update.state() == QFirmwareUpdate::Written);
    CHECK(states == (QList<QFirmwareUpdate::State>{QFirmwareUpdate::Writing, QFirmwareUpdate::Written}));
    CHECK(QFirmwareUpdate::isRunningImageConfirmed() && QFirmwareUpdate::runningVersion().isNull());
    CHECK(!QFirmwareUpdate::reboot());

    // Not an image, too long, too short.
    CHECK(update.begin());
    CHECK(update.write(QByteArray(100, 'x')));
    CHECK(!update.finish() && update.error() == QFirmwareUpdate::InvalidImageError);
    CHECK(update.state() == QFirmwareUpdate::Failed);
    CHECK(update.begin(10));
    CHECK(!update.write(QByteArray(11, 'x')) && update.error() == QFirmwareUpdate::InvalidImageError);
    CHECK(update.begin(image.size()));
    CHECK(update.write(image.left(1000)));
    CHECK(!update.finish() && update.error() == QFirmwareUpdate::InvalidImageError);
    const QByteArray truncated = makeImage(1, 0, 0, 0, 5000).left(3000); // the header promises more
    CHECK(update.begin());
    CHECK(update.write(truncated));
    CHECK(!update.finish() && update.error() == QFirmwareUpdate::InvalidImageError);

    // A target that cannot be written.
    QFirmwareUpdate nowhere;
    nowhere.setTarget(QStringLiteral("/nonexistent/dir/slot.bin"));
    CHECK(!nowhere.begin() && nowhere.error() == QFirmwareUpdate::DeviceNotFoundError);
    QFirmwareUpdate untargeted;
    CHECK(!untargeted.begin() && untargeted.error() == QFirmwareUpdate::NotOpenError);

    update.abort();
    CHECK(update.state() == QFirmwareUpdate::Idle);
}

// A boot loader in memory, standing in for MCUboot.
struct FakeBoot {
    QByteArray slot1;
    QByteArray running;
    bool confirmed = false;
    int upgrades = 0;
    bool permanent = false;
};
FakeBoot g_boot;

class FakeFirmwareBackend final : public FirmwareBackend {
public:
    bool begin(const QString &, qint64, Failure &) override {
        g_boot.slot1.clear();
        return true;
    }
    bool write(const char *data, qsizetype size, Failure &) override {
        g_boot.slot1.append(data, size);
        return true;
    }
    bool finish(QByteArray &header, Failure &) override {
        header = g_boot.slot1.left(kImageHeaderSize);
        return true;
    }
    void abort() override {}
    bool requestUpgrade(bool permanent, Failure &) override {
        ++g_boot.upgrades;
        g_boot.permanent = permanent;
        return true;
    }
    bool isConfirmed() override { return g_boot.confirmed; }
    bool confirm(Failure &) override {
        g_boot.confirmed = true;
        return true;
    }
    QByteArray runningHeader() override { return g_boot.running.left(kImageHeaderSize); }
    bool reboot(Failure &) override {
        g_boot.running = g_boot.slot1; // MCUboot swaps the slots
        g_boot.confirmed = g_boot.permanent;
        return true;
    }
};

void testFirmwareBoot() {
    setFirmwareBackendFactory(
        []() -> std::unique_ptr<FirmwareBackend> { return std::make_unique<FakeFirmwareBackend>(); });
    g_boot.running = makeImage(1, 0, 0, 0, 64);
    g_boot.confirmed = true;
    CHECK(QFirmwareUpdate::runningVersion() == QVersionNumber(1, 0, 0));

    QFirmwareUpdate update;
    CHECK(!update.requestUpgrade() && update.error() == QFirmwareUpdate::NotOpenError);
    const QByteArray image = makeImage(1, 1, 0, 0, 2048);
    CHECK(update.begin(image.size()));
    for (qsizetype at = 0; at < image.size(); at += 700)
        CHECK(update.write(image.mid(at, 700)));
    CHECK(update.finish());
    CHECK(update.requestUpgrade());
    CHECK(update.state() == QFirmwareUpdate::Pending && g_boot.upgrades == 1 && !g_boot.permanent);
    CHECK(QFirmwareUpdate::reboot());
    CHECK(QFirmwareUpdate::runningVersion() == QVersionNumber(1, 1, 0));
    CHECK(!QFirmwareUpdate::isRunningImageConfirmed());
    CHECK(QFirmwareUpdate::confirmRunningImage() && QFirmwareUpdate::isRunningImageConfirmed());
    setFirmwareBackendFactory(nullptr);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testWatchdog();
    testHeader();
    testFirmwareFile();
    testFirmwareBoot();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all device tests passed\n");
    return 0;
}
