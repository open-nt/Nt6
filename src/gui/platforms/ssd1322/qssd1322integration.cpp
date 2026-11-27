// MIT License - see LICENSE
#include "qssd1322integration_p.h"

#include "qpa/qwindowsysteminterface_p.h"

#include <QtCore/qhash.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>

#include <cstdio>

namespace {

// Splits "spi=spidev0.0:dc=gpiochip0:25:size=256x64" into key=value pairs. A part without '='
// continues the value before it, since GPIO line names on Linux contain a colon themselves.
QHash<QString, QString> parseOptions(const QString &options) {
    QHash<QString, QString> values;
    QString lastKey;
    for (const QString &part : options.split(QChar(':'))) {
        const qsizetype eq = part.indexOf(QChar('='));
        if (eq > 0) {
            lastKey = part.left(eq).trimmed().toLower();
            values.insert(lastKey, part.mid(eq + 1));
        } else if (!lastKey.isEmpty()) {
            values[lastKey] += QChar(':') + part;
        }
    }
    return values;
}

bool failWith(const QString &message) {
    std::fprintf(stderr, "Nt GUI: ssd1322: %s\n", message.toUtf8().constData());
    return false;
}

bool toInt(const QString &text, int &value) {
    bool ok = false;
    const int v = text.toInt(&ok, 0);
    if (ok)
        value = v;
    return ok;
}

// Applies the options to display; prints the first one that is wrong.
bool configure(QSsd1322Display &display, const QHash<QString, QString> &options) {
    static const QStringList known = {QStringLiteral("spi"),    QStringLiteral("dc"),      QStringLiteral("reset"),
                                      QStringLiteral("size"),   QStringLiteral("offset"),  QStringLiteral("speed"),
                                      QStringLiteral("rotate"), QStringLiteral("contrast")};
    for (auto it = options.cbegin(); it != options.cend(); ++it) {
        if (!known.contains(it.key()))
            return failWith(QStringLiteral("unknown option \"%1\"").arg(it.key()));
    }
    display.setSpiDevice(options.value(QStringLiteral("spi"), QStringLiteral("spidev0.0")));
    display.setDataCommandLine(options.value(QStringLiteral("dc")));
    display.setResetLine(options.value(QStringLiteral("reset")));
    if (display.dataCommandLine().isEmpty())
        return failWith(QStringLiteral("the dc option (the GPIO line of the D/C pin) is required"));
    if (options.contains(QStringLiteral("size"))) {
        const QStringList wh = options.value(QStringLiteral("size")).split(QChar('x'));
        int w = 0;
        int h = 0;
        if (wh.size() != 2 || !toInt(wh.at(0), w) || !toInt(wh.at(1), h))
            return failWith(QStringLiteral("size must be WIDTHxHEIGHT"));
        display.setSize(w, h);
        if (display.width() != w || display.height() != h)
            return failWith(QStringLiteral("the controller cannot drive a panel of %1x%2").arg(w).arg(h));
    }
    int value = 0;
    if (options.contains(QStringLiteral("offset"))) {
        if (!toInt(options.value(QStringLiteral("offset")), value) || value < 0)
            return failWith(QStringLiteral("offset must be a column group from 0"));
        display.setColumnOffset(value);
    }
    if (options.contains(QStringLiteral("speed"))) {
        if (!toInt(options.value(QStringLiteral("speed")), value) || value <= 0)
            return failWith(QStringLiteral("speed must be a clock in hertz"));
        display.setSpeed(static_cast<quint32>(value));
    }
    if (options.contains(QStringLiteral("rotate"))) {
        if (!toInt(options.value(QStringLiteral("rotate")), value) || (value != 0 && value != 180))
            return failWith(QStringLiteral("rotate must be 0 or 180"));
        display.setRotated(value == 180);
    }
    if (options.contains(QStringLiteral("contrast"))) {
        if (!toInt(options.value(QStringLiteral("contrast")), value) || !display.setContrast(value))
            return failWith(QStringLiteral("contrast must be from 0 to 255"));
    }
    return true;
}

} // namespace

void QSsd1322Window::setVisible(bool visible) {
    if (visible && !window()->transientParent() && window()->screen()) {
        const QRect target = window()->screen()->geometry();
        if (geometry() != target) {
            setGeometry(target);
            QWindowSystemInterface::handleGeometryChange(window(), target);
        }
    }
    QOffscreenWindow::setVisible(visible);
}

void QSsd1322BackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset) {
    (void)window;
    // The region is in window coordinates; the image holds the window from offset on.
    const QPlatformWindow *platform = this->window()->handle();
    const QPoint origin = platform ? platform->geometry().topLeft() : QPoint();
    m_integration->present(toImage(), origin - offset, region.translated(offset));
}

QSsd1322Integration *QSsd1322Integration::create(const QString &options) {
    auto display = std::make_unique<QSsd1322Display>();
    if (!configure(*display, parseOptions(options)))
        return nullptr;
    if (!display->open()) {
        failWith(QStringLiteral("cannot open the display: %1").arg(display->errorString()));
        return nullptr;
    }
    return new QSsd1322Integration(std::move(display));
}

QSsd1322Integration::QSsd1322Integration(std::unique_ptr<QSsd1322Display> display)
    : m_display(std::move(display)), m_screen(new QSsd1322Screen(QSize(m_display->width(), m_display->height()))),
      m_frame(m_display->width(), m_display->height(), QImage::Format_Grayscale8) {
    m_frame.fill(0);
}

QSsd1322Integration::~QSsd1322Integration() { delete m_screen; }

void QSsd1322Integration::initialize() { QWindowSystemInterface::handleScreenAdded(m_screen); }

QPlatformWindow *QSsd1322Integration::createPlatformWindow(QWindow *window) const { return new QSsd1322Window(window); }

QPlatformBackingStore *QSsd1322Integration::createPlatformBackingStore(QWindow *window) const {
    return new QSsd1322BackingStore(window, this);
}

void QSsd1322Integration::present(const QImage &image, const QPoint &origin, const QRegion &region) const {
    if (image.isNull())
        return;
    const QRect panel = m_frame.rect();
    QRect sent;
    for (const QRect &inImage : region.rects()) {
        const QRect r = inImage.intersected(image.rect()).translated(origin).intersected(panel);
        if (r.isEmpty())
            continue;
        for (int y = r.top(); y <= r.bottom(); ++y) {
            const auto *in = reinterpret_cast<const quint32 *>(image.constScanLine(y - origin.y()));
            uchar *out = m_frame.scanLine(y);
            for (int x = r.left(); x <= r.right(); ++x) {
                const quint32 p = in[x - origin.x()];
                // Luma (BT.601) of the premultiplied pixel, which is the pixel over black.
                const quint32 gray = (((p >> 16) & 0xff) * 77 + ((p >> 8) & 0xff) * 150 + (p & 0xff) * 29) >> 8;
                out[x] = static_cast<uchar>(gray);
            }
        }
        sent = sent.united(r);
    }
    if (!sent.isEmpty() && !m_display->writePixels(m_frame.constBits(), m_frame.bytesPerLine(), sent))
        failWith(m_display->errorString());
}

QImage QSsd1322Integration::frame() const { return m_frame; }
