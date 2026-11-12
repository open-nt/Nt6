// MIT License - see LICENSE
//
// The QML types of Nt FM, in documents run offline: the engine is not started, and the tests pull
// its samples while the event loop runs.
#include <QtCore>
#include <QtFMQml>
#include <QtQml>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace QtFM;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Renders msecs of audio and runs the event loop for as long, so that polls and signals arrive.
static float pump(int msecs) {
    float peak = 0;
    std::vector<float> buffer(2 * 480);
    for (int t = 0; t < msecs; t += 10) {
        FmEngine::instance()->engine()->render(buffer.data(), 480);
        for (float v : buffer)
            peak = std::max(peak, std::fabs(v));
        QEventLoop loop;
        QTimer::singleShot(10, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return peak;
}

struct Document {
    QQmlEngine engine;
    QQmlComponent component{&engine};
    QObject *root = nullptr;
    Document(const QByteArray &qml, const QUrl &base) {
        component.setData(qml, base);
        root = component.create();
        if (!root)
            std::printf("QML: %s\n", qPrintable(component.errorString()));
    }
    ~Document() { delete root; }
    QObject *object(const char *name) const { return root ? root->property(name).value<QObject *>() : nullptr; }
};

// A two-second IMF file: one OPL2 note.
static QByteArray imfFile() {
    const int writes[][2] = {{0x01, 0x20}, {0x20, 0x21}, {0x40, 0x3f}, {0x60, 0xf0}, {0x80, 0x0f}, {0x23, 0x21},
                             {0x43, 0x00}, {0x63, 0xf0}, {0x83, 0x0f}, {0xa0, 0x44}, {0xb0, 0x32}};
    QByteArray body;
    for (const auto &w : writes)
        body.append(char(w[0])).append(char(w[1])).append(char(0)).append(char(0));
    body[qsizetype(body.size() - 2)] = char(0x30); // 560 ticks after the key-on: one second at 560 Hz
    body[qsizetype(body.size() - 1)] = char(0x02);
    body.append(char(0xb0)).append(char(0x12)).append(char(0)).append(char(0));
    QByteArray imf;
    imf.append(char(body.size() & 0xff)).append(char(body.size() >> 8));
    return imf + body;
}

static void testSounds(const QTemporaryDir &dir) {
    const QUrl base = QUrl::fromLocalFile(dir.filePath(QStringLiteral("game.qml")));
    Document doc(R"QML(
import QtQml
import QtFM
QtObject {
    id: root
    property real speed: 0
    property int finishedCount: 0
    property FmSound coin: FmSound {
        patch: FmPatch.generate(FmPatch.Pickup, 3)
        pitchVariance: 1
        maxInstances: 2
        onFinished: root.finishedCount++
    }
    property FmSound hum: FmSound {
        patch: FmPatch.generate(FmPatch.Engine, 42)
        pitch: root.speed / 10
    }
    property FmSound fromFile: FmSound { source: "laser.json"; loop: true }
    property FmSound organ: FmSound { source: "organ.sbi"; chip: FmChip.OPL2 }
    property FmSound missing: FmSound { source: "missing.json" }
    property bool laserValid: FmPatch.generate(FmPatch.Laser, 4, FmChip.OPL2).valid
    property bool laserFits: FmPatch.fitsChip(FmPatch.generate(FmPatch.Laser, 4, FmChip.OPL2), FmChip.OPL2)
    property bool badJson: FmPatch.fromJson("{nope").valid
    property string category: FmPatch.generate(FmPatch.Explosion, 1).category
    property bool mutatedValid: FmPatch.mutated(FmPatch.generate(FmPatch.Jump, 1), 0.2, 5).valid
}
)QML",
                 base);
    CHECK(doc.root);
    if (!doc.root)
        return;
    CHECK(doc.root->property("laserValid").toBool());
    CHECK(doc.root->property("laserFits").toBool());
    CHECK(!doc.root->property("badJson").toBool());
    CHECK(doc.root->property("category").toString() == QLatin1String("explosion"));
    CHECK(doc.root->property("mutatedValid").toBool());

    auto *coin = qobject_cast<FmSound *>(doc.object("coin"));
    auto *hum = qobject_cast<FmSound *>(doc.object("hum"));
    auto *fromFile = qobject_cast<FmSound *>(doc.object("fromFile"));
    auto *organ = qobject_cast<FmSound *>(doc.object("organ"));
    auto *missing = qobject_cast<FmSound *>(doc.object("missing"));
    CHECK(coin && hum && fromFile && organ && missing);
    if (!coin || !hum || !fromFile || !organ || !missing)
        return;

    // Files, relative to the document; errors are reported, not thrown.
    CHECK(fromFile->error().isEmpty());
    CHECK(fromFile->currentPatch().isValid());
    CHECK(fromFile->currentPatch().isLoop()); // loop: true overrides the patch
    CHECK(fromFile->loop());
    CHECK(organ->currentPatch().chip() == ChipType::OPL2);
    CHECK(!missing->error().isEmpty());
    CHECK(!missing->currentPatch().isValid());
    missing->play(); // nothing to play: no effect

    // The engine is made on first use, with the chip that a sound asked for.
    Engine *engine = FmEngine::instance()->engine();
    CHECK(engine && !engine->isStarted());
    CHECK(engine->chip(ChipType::OPL2) != nullptr);

    int playing = 0;
    QObject::connect(coin, &FmSound::playingChanged, [&playing] { ++playing; });
    QMetaObject::invokeMethod(coin, "play");
    CHECK(coin->isPlaying());
    CHECK(pump(100) > 0.02f);
    CHECK(FmEngine::instance()->activeVoices() == 1);
    // maxInstances: a third play stops the oldest.
    coin->play();
    coin->play();
    pump(50);
    CHECK(FmEngine::instance()->activeVoices() == 2);
    pump(1500);
    CHECK(!coin->isPlaying());
    CHECK(playing >= 2);
    CHECK(doc.root->property("finishedCount").toInt() >= 1);

    // A loop follows its binding, and stops when told.
    hum->play();
    pump(100);
    CHECK(hum->isPlaying());
    doc.root->setProperty("speed", 120);
    CHECK(hum->pitch() == 12.0);
    pump(100);
    hum->stop();
    pump(1000);
    CHECK(!hum->isPlaying());

    // The OPL2 sound plays on the chip.
    organ->play();
    CHECK(pump(200) > 0.01f);
    organ->stop(true);
    pump(500);
    CHECK(!organ->isPlaying());

    FmEngine::instance()->setMasterGain(-6);
    CHECK(FmEngine::instance()->masterGain() == -6.0);
    FmEngine::instance()->setBusGain(1, -12);
    CHECK(FmEngine::instance()->busGain(1) == -12.0);
    FmEngine::instance()->setMasterGain(0);
}

static void testBankSequenceMusic(const QTemporaryDir &dir) {
    const QUrl base = QUrl::fromLocalFile(dir.filePath(QStringLiteral("game.qml")));
    Document doc(R"QML(
import QtQml
import QtFM
QtObject {
    id: root
    property int sequenceDone: 0
    property int musicDone: 0
    property FmSoundBank bank: FmSoundBank { source: "sfx.json" }
    property FmSound blip: FmSound { patch: root.bank.sound("blip") }
    property FmSequence jingle: FmSequence {
        bank: root.bank
        tempo: 240
        notes: [ { beat: 0, sound: "coin" }, { beat: 1, sound: "coin", pitch: 7, length: 0.5 },
                 { beat: 2, patch: FmPatch.generate(FmPatch.Blip, 2), gain: -3 } ]
        onFinished: root.sequenceDone++
    }
    property FmMusic music: FmMusic { source: "song.imf"; loops: false; onFinished: root.musicDone++ }
    property FmSoundBank badBank: FmSoundBank { source: "nothing.json" }
}
)QML",
                 base);
    CHECK(doc.root);
    if (!doc.root)
        return;
    auto *bank = qobject_cast<FmSoundBank *>(doc.object("bank"));
    auto *blip = qobject_cast<FmSound *>(doc.object("blip"));
    auto *jingle = qobject_cast<FmSequence *>(doc.object("jingle"));
    auto *music = qobject_cast<FmMusic *>(doc.object("music"));
    auto *badBank = qobject_cast<FmSoundBank *>(doc.object("badBank"));
    CHECK(bank && blip && jingle && music && badBank);
    if (!bank || !blip || !jingle || !music || !badBank)
        return;
    CHECK(bank->count() == 2);
    CHECK(bank->names() == QStringList({QStringLiteral("coin"), QStringLiteral("blip")}));
    CHECK(bank->error().isEmpty());
    CHECK(!badBank->error().isEmpty());
    CHECK(blip->currentPatch().name() == QLatin1String("blip"));

    const Sequence seq = jingle->sequence();
    CHECK(seq.noteCount() == 3);
    CHECK(seq.tempo() == 240.0f);
    CHECK(seq.notes().at(1).pitch == 7.0f);
    CHECK(seq.notes().at(1).length == 0.5f);
    CHECK(seq.notes().at(2).gain == -3.0f);
    jingle->play();
    CHECK(jingle->isPlaying());
    pump(300);
    CHECK(FmEngine::instance()->activeVoices() >= 1);
    pump(1200);
    CHECK(!jingle->isPlaying());
    CHECK(doc.root->property("sequenceDone").toInt() == 1);

    CHECK(music->error().isEmpty());
    music->play();
    CHECK(music->duration() > 0.99 && music->duration() < 1.01);
    CHECK(pump(300) > 0.02f);
    CHECK(music->isPlaying());
    CHECK(music->position() > 0.1);
    music->pause();
    pump(200);
    CHECK(!music->isPlaying());
    music->play();
    pump(1500);
    CHECK(!music->isPlaying());
    CHECK(doc.root->property("musicDone").toInt() == 1);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    registerQmlTypes();
    FmEngine::instance()->setAutoStart(false);

    QTemporaryDir dir;
    auto write = [&dir](const char *name, const QByteArray &data) {
        QFile f(dir.filePath(QString::fromLatin1(name)));
        return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
    };
    PatchBank bank;
    bank.insert(QStringLiteral("coin"), Patch::generate(Category::Pickup, 3));
    bank.insert(QStringLiteral("blip"), Patch::generate(Category::Blip, 8));
    QByteArray sbi("SBI\x1a", 4);
    sbi += QByteArray(32, '\0');
    const char regs[16] = {0x21, 0x21, 0x10, 0x00, char(0xf2), char(0xf4), 0x45, 0x37, 0, 0, 0x06, 0, 0, 0, 0, 0};
    sbi.append(regs, 16);
    CHECK(dir.isValid());
    CHECK(write("laser.json", Patch::generate(Category::Laser, 2).toJson()));
    CHECK(write("organ.sbi", sbi));
    CHECK(write("sfx.json", bank.toJson()));
    CHECK(write("song.imf", imfFile()));

    testSounds(dir);
    testBankSequenceMusic(dir);
    FmEngine::reset();

    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("All Nt FM QML tests passed\n");
    return 0;
}
