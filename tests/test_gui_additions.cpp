// MIT License - see LICENSE
//
// The Nt GUI functions added for Qt applications that need them: font families, hints and stretch, application
// fonts, fractional metrics, scaled images, text outlines, the image reader, shortcuts and native key codes. Runs on
// the offscreen platform.
#include <QtTest>

#include <QtCore/qbuffer.h>
#include <QtCore/qtemporarydir.h>
#include <QtCore/qurl.h>
#include <QtCore/qurlquery.h>
#include <QtGui/qevent.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontdatabase.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qimage.h>
#include <QtGui/qimagereader.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>
#include <QtGui/qpixmap.h>
#include <QtGui/qshortcut.h>
#include <QtGui/qwindow.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <cmath>
#include <memory>

namespace {

const char *const kDejaVu = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";

} // namespace

class TestGuiAdditions : public QObject {
    Q_OBJECT
private slots:
    void windowFlagsScreenStacking();
    void letterAndWordSpacing();
    void rgba8888Images();
    void overloadOfAndLogging();
    void normalMatrix();
    void fontFamiliesHintsStretch();
    void fontMetricsF();
    void styleHintChoosesGenericFamily();
    void applicationFonts();
    void imageScaled();
    void pixmapScaled();
    void painterPathAddText();
    void urlSetQuery();
    void imageReader();
    void shortcutOnWindow();
    void shortcutContexts();
    void keyEventNativeCodes();
};

void TestGuiAdditions::fontFamiliesHintsStretch() {
    QFont font(QString::fromLatin1("Noto Sans"), 12);
    QCOMPARE(font.families(), QStringList{QString::fromLatin1("Noto Sans")});
    QCOMPARE(font.styleHint(), QFont::AnyStyle);
    QCOMPARE(font.styleStrategy(), QFont::PreferDefault);
    QCOMPARE(font.stretch(), int(QFont::AnyStretch));

    font.setFamilies({QString::fromLatin1("Nasalization"), QString::fromLatin1("DejaVu Sans")});
    QCOMPARE(font.family(), QString::fromLatin1("Nasalization"));
    QCOMPARE(font.families().size(), 2);
    font.setFamily(QString::fromLatin1("DejaVu Serif"));
    QCOMPARE(font.families(), QStringList{QString::fromLatin1("DejaVu Serif")});

    font.setStyleHint(QFont::Monospace, QFont::PreferQuality);
    QCOMPARE(font.styleHint(), QFont::Monospace);
    QCOMPARE(font.styleStrategy(), QFont::PreferQuality);
    QCOMPARE(QFont::TypeWriter, QFont::Courier);
    font.setStretch(QFont::Condensed);
    QCOMPARE(font.stretch(), 75);
    font.setStretch(5000); // out of range: ignored
    QCOMPARE(font.stretch(), 75);

    QFont other = font;
    QVERIFY(other == font);
    other.setStretch(QFont::Expanded);
    QVERIFY(other != font);
}

void TestGuiAdditions::fontMetricsF() {
    QFont font(QString::fromLatin1("DejaVu Sans"));
    font.setPixelSize(20);
    const QFontMetrics fm(font);
    const QFontMetricsF fmf(font);
    QCOMPARE(fmf.ascent(), qreal(fm.ascent()));
    QCOMPARE(fmf.height(), qreal(fm.height()));
    QCOMPARE(fmf.lineSpacing(), qreal(fm.lineSpacing()));
    const QString text = QString::fromLatin1("Hello, Nt");
    QVERIFY(std::abs(fmf.horizontalAdvance(text) - fm.horizontalAdvance(text)) <= 1.0);
    QVERIFY(fmf.horizontalAdvance(QChar(u'W')) > fmf.horizontalAdvance(QChar(u'i')) ||
            fmf.horizontalAdvance(text) == 0);
    QCOMPARE(fmf.horizontalAdvance(text, 0), qreal(0));
    QCOMPARE(QFontMetricsF(fm).ascent(), fmf.ascent());
    QFontMetricsF assigned(QFont(QString::fromLatin1("DejaVu Serif")));
    assigned = fm;
    QCOMPARE(assigned.height(), fmf.height());
    QCOMPARE(fmf.size(0, text).height(), qreal(fm.size(0, text).height()));
    QVERIFY(fmf.elidedText(text, Qt::ElideRight, 1000) == text);
}

void TestGuiAdditions::styleHintChoosesGenericFamily() {
    // A family that does not exist, with the Monospace hint: the fallback is the monospace generic family, so all
    // characters are as wide.
    QFont font(QString::fromLatin1("No Such Family Here"));
    font.setPixelSize(16);
    font.setStyleHint(QFont::Monospace);
    const QFontMetricsF fm(font);
    if (fm.horizontalAdvance(QString::fromLatin1("M")) == 0)
        QSKIP("no fonts available");
    QCOMPARE(fm.horizontalAdvance(QString::fromLatin1("iiii")), fm.horizontalAdvance(QString::fromLatin1("MMMM")));
}

void TestGuiAdditions::applicationFonts() {
    if (!QFile::exists(QString::fromLatin1(kDejaVu)))
        QSKIP("DejaVu Sans is not installed");
    QCOMPARE(QFontDatabase::addApplicationFont(QString::fromLatin1("/no/such/font.ttf")), -1);
    QCOMPARE(QFontDatabase::addApplicationFontFromData(QByteArray("not a font")), -1);
    const int id = QFontDatabase::addApplicationFont(QString::fromLatin1(kDejaVu));
    if (id < 0)
        QSKIP("Nt GUI was built without FreeType");
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    QCOMPARE(families, QStringList{QString::fromLatin1("DejaVu Sans")});
    QVERIFY(QFontDatabase::families().contains(QString::fromLatin1("DejaVu Sans")));
    QVERIFY(QFontDatabase::hasFamily(QString::fromLatin1("dejavu sans")));
    QFont font(families.first());
    font.setPixelSize(18);
    QVERIFY(QFontMetricsF(font).horizontalAdvance(QString::fromLatin1("Nt")) > 0);
    QVERIFY(QFontDatabase::removeApplicationFont(id));
    QVERIFY(!QFontDatabase::removeApplicationFont(id));
    QVERIFY(QFontDatabase::applicationFontFamilies(id).isEmpty());
    QVERIFY(!QFontDatabase::removeAllApplicationFonts());
    QVERIFY(QFontDatabase::isFixedPitch(QString::fromLatin1("DejaVu Sans Mono")) ||
            !QFontDatabase::hasFamily(QString::fromLatin1("DejaVu Sans Mono")));
    QCOMPARE(QFontDatabase::systemFont(QFontDatabase::FixedFont).family(), QString::fromLatin1("monospace"));
}

void TestGuiAdditions::imageScaled() {
    QImage image(40, 20, QImage::Format_ARGB32_Premultiplied);
    image.fill(qRgb(200, 50, 10));
    QCOMPARE(image.scaled(10, 10).size(), QSize(10, 10));
    QCOMPARE(image.scaled(10, 10, Qt::KeepAspectRatio).size(), QSize(10, 5));
    QCOMPARE(image.scaled(10, 10, Qt::KeepAspectRatioByExpanding).size(), QSize(20, 10));
    QCOMPARE(image.scaledToWidth(80).size(), QSize(80, 40));
    QCOMPARE(image.scaledToHeight(5).size(), QSize(10, 5));
    QVERIFY(image.scaled(0, 10).isNull());
    QVERIFY(QImage().scaled(10, 10).isNull());
    const QImage smooth = image.scaled(QSize(100, 50), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QCOMPARE(smooth.size(), QSize(100, 50));
    QCOMPARE(smooth.pixel(50, 25), qRgb(200, 50, 10));
    QCOMPARE(smooth.pixel(0, 0), qRgb(200, 50, 10));

    // A two-colour image keeps its halves, nearest or smooth, away from the seam.
    QImage halves(4, 2, QImage::Format_RGB32);
    halves.fill(Qt::black);
    for (int y = 0; y < 2; ++y)
        for (int x = 2; x < 4; ++x)
            halves.setPixel(x, y, qRgb(255, 255, 255));
    const QImage big = halves.scaled(40, 20);
    QCOMPARE(big.format(), QImage::Format_RGB32);
    QCOMPARE(big.pixel(5, 10), qRgb(0, 0, 0));
    QCOMPARE(big.pixel(35, 10), qRgb(255, 255, 255));
}

void TestGuiAdditions::pixmapScaled() {
    QPixmap pixmap(30, 10);
    pixmap.fill(Qt::red);
    QCOMPARE(pixmap.scaled(QSize(15, 15), Qt::KeepAspectRatio, Qt::SmoothTransformation).size(), QSize(15, 5));
    QCOMPARE(pixmap.scaledToWidth(60).size(), QSize(60, 20));
    QCOMPARE(pixmap.scaledToHeight(5).size(), QSize(15, 5));
    QCOMPARE(pixmap.scaled(6, 2).toImage().pixel(3, 1), qRgb(255, 0, 0));
}

void TestGuiAdditions::painterPathAddText() {
    QFont font(QString::fromLatin1("DejaVu Sans"));
    font.setPixelSize(40);
    QPainterPath path;
    path.addText(QPointF(10, 60), font, QString::fromLatin1("Nt"));
    if (path.isEmpty())
        QSKIP("Nt GUI was built without FreeType");
    const QRectF bounds = path.boundingRect();
    QVERIFY(bounds.left() >= 10.0 && bounds.left() < 20.0);
    QVERIFY(bounds.bottom() <= 61.0 && bounds.bottom() > 50.0); // sits on the baseline
    QVERIFY(bounds.top() < 40.0);                               // rises above it
    QVERIFY(bounds.width() > 30.0);
    QVERIFY(path.contains(QPointF(bounds.left() + 3, bounds.center().y()))); // the stem of the N

    // Filled with a painter, the outline paints pixels where the text is.
    QImage image(120, 80, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.fillPath(path, Qt::black);
    painter.end();
    QCOMPARE(image.pixel(bounds.toAlignedRect().left() + 3, qRound(bounds.center().y())), qRgb(0, 0, 0));

    QPainterPath empty;
    empty.addText(0, 0, font, QString());
    QVERIFY(empty.isEmpty());
}

void TestGuiAdditions::urlSetQuery() {
    QUrl url(QString::fromLatin1("https://tiles.example.org/map"));
    QUrlQuery query;
    query.addQueryItem(QString::fromLatin1("z"), QString::fromLatin1("12"));
    query.addQueryItem(QString::fromLatin1("name"), QString::fromLatin1("a b&c"));
    url.setQuery(query);
    QVERIFY(url.hasQuery());
    QCOMPARE(QUrlQuery(url).queryItemValue(QString::fromLatin1("name")), QString::fromLatin1("a b&c"));
    QCOMPARE(QUrlQuery(url).queryItemValue(QString::fromLatin1("z")), QString::fromLatin1("12"));
    url.setQuery(QUrlQuery());
    QVERIFY(!url.hasQuery());
}

void TestGuiAdditions::imageReader() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(qRgba(10, 20, 30, 255));
    const QString path = dir.filePath(QString::fromLatin1("picture.png"));
    QVERIFY(image.save(path));

    QImageReader reader(path);
    QVERIFY(reader.canRead());
    QCOMPARE(reader.format(), QByteArray("png"));
    QCOMPARE(reader.size(), QSize(32, 16));
    reader.setScaledSize(QSize(8, 4));
    const QImage read = reader.read();
    QCOMPARE(read.size(), QSize(8, 4));
    QCOMPARE(read.pixel(4, 2), qRgb(10, 20, 30));
    QCOMPARE(reader.error(), QImageReader::UnknownError);
    QCOMPARE(QImageReader::imageFormat(path), QByteArray("png"));
    QVERIFY(QImageReader::supportedImageFormats().contains(QByteArray("png")));

    QImageReader missing(dir.filePath(QString::fromLatin1("missing.png")));
    QVERIFY(missing.read().isNull());
    QCOMPARE(missing.error(), QImageReader::FileNotFoundError);
    QVERIFY(!missing.errorString().isEmpty());

    QFile junk(dir.filePath(QString::fromLatin1("junk.png")));
    QVERIFY(junk.open(QIODevice::WriteOnly));
    junk.write("this is not an image");
    junk.close();
    QImageReader bad(junk.fileName());
    QVERIFY(!bad.canRead());
    QCOMPARE(bad.error(), QImageReader::UnsupportedFormatError);
}

void TestGuiAdditions::shortcutOnWindow() {
    QWindow window;
    window.resize(100, 100);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QShortcut help(QKeySequence(Qt::Key_F1), &window);
    QSignalSpy activated(&help, &QShortcut::activated);
    QTest::keyClick(&window, Qt::Key_F1);
    QCOMPARE(activated.size(), 1);

    // A letter shortcut matches without regard to case of the event's key.
    QShortcut save(QKeySequence(QString::fromLatin1("Ctrl+S")), &window);
    QSignalSpy saved(&save, &QShortcut::activated);
    QTest::keyClick(&window, Qt::Key_S, Qt::ControlModifier);
    QCOMPARE(saved.size(), 1);
    QTest::keyClick(&window, Qt::Key_S); // without Ctrl
    QCOMPARE(saved.size(), 1);

    help.setEnabled(false);
    QTest::keyClick(&window, Qt::Key_F1);
    QCOMPARE(activated.size(), 1);
    help.setEnabled(true);

    // Auto-repeat can be turned off.
    help.setAutoRepeat(false);
    QWindowSystemInterface::handleKeyEvent(&window, QEvent::KeyPress, Qt::Key_F1, Qt::NoModifier, QString(), true);
    QCOMPARE(activated.size(), 1);

    // Two shortcuts with the same key: ambiguous.
    QShortcut second(QKeySequence(Qt::Key_F1), &window);
    QSignalSpy ambiguous(&help, &QShortcut::activatedAmbiguously);
    QTest::keyClick(&window, Qt::Key_F1);
    QCOMPARE(ambiguous.size(), 1);
    QCOMPARE(activated.size(), 1);

    // The functor constructor.
    int called = 0;
    QShortcut functor(QKeySequence(Qt::Key_F5), &window, [&called] { ++called; });
    QTest::keyClick(&window, Qt::Key_F5);
    QCOMPARE(called, 1);
    QCOMPARE(functor.key(), QKeySequence(Qt::Key_F5));
    QCOMPARE(functor.context(), Qt::WindowShortcut);
}

void TestGuiAdditions::shortcutContexts() {
    QWindow first;
    first.resize(50, 50);
    first.show();
    QWindow second;
    second.resize(50, 50);
    second.show();
    QVERIFY(QTest::qWaitForWindowExposed(&first));
    QVERIFY(QTest::qWaitForWindowExposed(&second));

    QShortcut local(QKeySequence(Qt::Key_F7), &first);
    QShortcut global(QKeySequence(Qt::Key_F8), &first);
    global.setContext(Qt::ApplicationShortcut);
    QSignalSpy localSpy(&local, &QShortcut::activated);
    QSignalSpy globalSpy(&global, &QShortcut::activated);

    QTest::keyClick(&second, Qt::Key_F7); // the other window: not active there
    QCOMPARE(localSpy.size(), 0);
    QTest::keyClick(&second, Qt::Key_F8); // application-wide
    QCOMPARE(globalSpy.size(), 1);
    QTest::keyClick(&first, Qt::Key_F7);
    QCOMPARE(localSpy.size(), 1);

    QShortcut standard(QKeySequence::Copy, &first);
    QVERIFY(!standard.keys().isEmpty());
    standard.setKeys({QKeySequence(Qt::Key_F9), QKeySequence(Qt::Key_F10)});
    QSignalSpy standardSpy(&standard, &QShortcut::activated);
    QTest::keyClick(&first, Qt::Key_F10);
    QCOMPARE(standardSpy.size(), 1);
}

void TestGuiAdditions::keyEventNativeCodes() {
    const QKeyEvent plain(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, QString::fromLatin1("a"));
    QCOMPARE(plain.nativeScanCode(), 0u);
    QCOMPARE(plain.nativeVirtualKey(), 0u);
    QCOMPARE(plain.nativeModifiers(), 0u);
    const QKeyEvent native(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier, 105, 0xffe4, 4);
    QCOMPARE(native.nativeScanCode(), 105u);
    QCOMPARE(native.nativeVirtualKey(), 0xffe4u);
    QCOMPARE(native.nativeModifiers(), 4u);
    QCOMPARE(native.key(), int(Qt::Key_Control));
    std::unique_ptr<QKeyEvent> copy(native.clone());
    QCOMPARE(copy->nativeVirtualKey(), 0xffe4u);
}

void TestGuiAdditions::normalMatrix() {
    QMatrix3x3 identity;
    QVERIFY(identity.isIdentity());
    const float values[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    QMatrix3x3 m(values);
    QCOMPARE(m(0, 1), 2.0f);
    QCOMPARE(m.constData()[1], 4.0f); // column by column
    QCOMPARE(m.transposed()(0, 1), 4.0f);

    // Rotation and translation: the normal matrix is the rotation itself.
    QMatrix4x4 rigid;
    rigid.translate(5, 6, 7);
    rigid.rotate(90, 0, 0, 1);
    QMatrix3x3 n = rigid.normalMatrix();
    QVERIFY(std::abs(n(0, 1) + 1.0f) < 1e-5f && std::abs(n(1, 0) - 1.0f) < 1e-5f);
    // Non-uniform scale: the normal matrix scales by the inverse.
    QMatrix4x4 scale;
    scale.scale(2, 4, 1);
    n = scale.normalMatrix();
    QCOMPARE(n(0, 0), 0.5f);
    QCOMPARE(n(1, 1), 0.25f);
    QCOMPARE(n(2, 2), 1.0f);
    // A singular matrix gives the identity.
    QMatrix4x4 flat;
    flat.scale(1, 1, 0);
    QVERIFY(flat.normalMatrix().isIdentity());
}

void TestGuiAdditions::letterAndWordSpacing() {
    QFont font(QStringLiteral("DejaVu Sans"));
    font.setPixelSize(20);
    QCOMPARE(font.letterSpacingType(), QFont::PercentageSpacing);
    QCOMPARE(font.letterSpacing(), 100.0);
    QCOMPARE(font.wordSpacing(), 0.0);
    const qreal plain = QFontMetricsF(font).horizontalAdvance(QStringLiteral("abcd"));
    const qreal plainSpace = QFontMetricsF(font).horizontalAdvance(QStringLiteral(" "));

    QFont spaced = font;
    spaced.setLetterSpacing(QFont::AbsoluteSpacing, 3);
    QVERIFY(spaced != font);
    QCOMPARE(spaced.letterSpacingType(), QFont::AbsoluteSpacing);
    QCOMPARE(QFontMetricsF(spaced).horizontalAdvance(QStringLiteral("abcd")), plain + 12);

    QFont wide = font;
    wide.setLetterSpacing(QFont::PercentageSpacing, 200);
    QVERIFY(std::abs(QFontMetricsF(wide).horizontalAdvance(QStringLiteral("abcd")) - 2 * plain) < 0.1);

    QFont words = font;
    words.setWordSpacing(5);
    QCOMPARE(QFontMetricsF(words).horizontalAdvance(QStringLiteral(" ")), plainSpace + 5);
    QCOMPARE(QFontMetricsF(words).horizontalAdvance(QStringLiteral("abcd")), plain);
}

void TestGuiAdditions::rgba8888Images() {
    QImage argb(2, 1, QImage::Format_ARGB32);
    argb.setPixel(0, 0, qRgba(10, 20, 30, 255));
    argb.setPixel(1, 0, qRgba(200, 100, 50, 128));
    const QImage rgba = argb.convertToFormat(QImage::Format_RGBA8888);
    QCOMPARE(rgba.format(), QImage::Format_RGBA8888);
    QVERIFY(rgba.hasAlphaChannel());
    // The bytes are R, G, B, A in memory, whatever the byte order of the machine.
    const uchar *bytes = rgba.constBits();
    QCOMPARE(int(bytes[0]), 10);
    QCOMPARE(int(bytes[1]), 20);
    QCOMPARE(int(bytes[2]), 30);
    QCOMPARE(int(bytes[3]), 255);
    QCOMPARE(int(bytes[7]), 128);
    QCOMPARE(rgba.pixel(1, 0), qRgba(200, 100, 50, 128));
    QCOMPARE(rgba.convertToFormat(QImage::Format_ARGB32), argb);

    // RGB888 data is copied into a 32-bit image.
    const uchar rgb[] = {1, 2, 3, 4, 5, 6, 0, 0, 7, 8, 9, 10, 11, 12, 0, 0};
    const QImage fromRgb(rgb, 2, 2, 8, QImage::Format_RGB888);
    QCOMPARE(fromRgb.format(), QImage::Format_RGB32);
    QCOMPARE(fromRgb.pixel(1, 0), qRgb(4, 5, 6));
    QCOMPARE(fromRgb.pixel(0, 1), qRgb(7, 8, 9));
    QCOMPARE(QImage(3, 3, QImage::Format_RGB888).format(), QImage::Format_RGB32);
    QCOMPARE(argb.convertToFormat(QImage::Format_RGB888).pixel(0, 0), qRgb(10, 20, 30));

    QImage rgbx(1, 1, QImage::Format_RGBX8888);
    QVERIFY(!rgbx.hasAlphaChannel());
    rgbx.fill(QColor(1, 2, 3, 40));
    QCOMPARE(rgbx.pixel(0, 0), qRgb(1, 2, 3));
    QCOMPARE(int(rgbx.constBits()[3]), 255);

    QImage premultiplied = argb.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QCOMPARE(int(premultiplied.constBits()[4]), 100); // 200 * 128 / 255
    QCOMPARE(qAlpha(premultiplied.pixel(1, 0)), 128);

    // Painting into and from the byte-ordered formats.
    QImage canvas(4, 4, QImage::Format_RGBA8888);
    canvas.fill(Qt::transparent);
    {
        QPainter painter(&canvas);
        painter.fillRect(QRect(0, 0, 2, 4), QColor(0, 0, 255));
        painter.drawImage(QPoint(2, 0), rgba);
    }
    QCOMPARE(canvas.pixel(1, 1), qRgb(0, 0, 255));
    QCOMPARE(canvas.constBits()[2], uchar(255));
    QCOMPARE(canvas.pixel(2, 0), qRgb(10, 20, 30));
    QCOMPARE(canvas.pixel(3, 3), 0u);
    QCOMPARE(canvas.scaled(8, 8).format(), QImage::Format_RGBA8888);
    QCOMPARE(canvas.scaled(8, 8).pixel(1, 1), qRgb(0, 0, 255));

    // Saving converts to a format the writers know.
    QByteArray png;
    QBuffer buffer(&png);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(canvas.save(&buffer, "PNG"));
    QImage back;
    QVERIFY(back.loadFromData(png));
    QCOMPARE(back.pixel(2, 0), qRgb(10, 20, 30));
    QCOMPARE(back.devicePixelRatio(), 1.0);
    QCOMPARE(canvas.devicePixelRatioF(), canvas.devicePixelRatio());
}

namespace {
class Overloaded : public QObject {
public:
    void value(int v) { last = v; }
    void value(const QString &) { last = -1; }
    int read() const { return last; }
    int read(int) const { return 0; }
    int last = 0;
};
} // namespace

void TestGuiAdditions::overloadOfAndLogging() {
    Overloaded o;
    (o.*QOverload<int>::of(&Overloaded::value))(7);
    QCOMPARE(o.last, 7);
    (o.*QOverload<const QString &>::of(&Overloaded::value))(QString());
    QCOMPARE(o.last, -1);
    QCOMPARE((o.*QOverload<>::of(&Overloaded::read))(), -1);
    // QObject brings qWarning() and QDebug with it, as in Qt.
    QTest::ignoreMessage(QtWarningMsg, "logged 3");
    qWarning() << "logged" << 3;
}

void TestGuiAdditions::windowFlagsScreenStacking() {
    QWindow window;
    QCOMPARE(window.flags(), Qt::WindowFlags(Qt::Window));
    QCOMPARE(window.type(), Qt::Window);
    window.setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    QCOMPARE(window.flags(), Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint); // type added
    window.setFlag(Qt::WindowDoesNotAcceptFocus);
    window.setFlag(Qt::WindowStaysOnTopHint, false);
    QCOMPARE(window.flags(), Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

    // The flags reach the platform window when it is made, and after.
    window.resize(50, 50);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *platform = dynamic_cast<QOffscreenWindow *>(QtPrivate::GuiAccess::platformWindow(&window));
    QVERIFY(platform);
    QCOMPARE(platform->flags(), window.flags());
    window.setFlags(Qt::Tool | Qt::WindowStaysOnBottomHint);
    QCOMPARE(window.type(), Qt::Tool);
    QCOMPARE(platform->flags(), Qt::Tool | Qt::WindowStaysOnBottomHint);
    QVERIFY(window.winId() != 0);

    window.lower();
    QCOMPARE(platform->stacking(), -1);
    window.raise();
    QCOMPARE(platform->stacking(), 1);

    QSignalSpy screens(&window, &QWindow::screenChanged);
    window.setScreen(nullptr); // the primary screen, which it is on already
    QCOMPARE(screens.size(), 0);
    QCOMPARE(window.screen(), QGuiApplication::primaryScreen());
}

QTEST_MAIN(TestGuiAdditions)
#include "test_gui_additions.moc"
