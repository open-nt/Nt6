// MIT License - see LICENSE
#include <QtGpio/qgpioline.h>
#include <QtGpio/qspidevice.h>
#include <QtGpio/qssd1322display.h>

#include <QtCore/qthread.h>

#include <algorithm>

namespace {

// SSD1322 commands. Their arguments follow as data bytes, with D/C high.
constexpr quint8 CmdSetColumns = 0x15;
constexpr quint8 CmdWriteRam = 0x5c;
constexpr quint8 CmdSetRows = 0x75;
constexpr quint8 CmdRemap = 0xa0;
constexpr quint8 CmdStartLine = 0xa1;
constexpr quint8 CmdDisplayOffset = 0xa2;
constexpr quint8 CmdNormal = 0xa6;
constexpr quint8 CmdInverse = 0xa7;
constexpr quint8 CmdExitPartial = 0xa9;
constexpr quint8 CmdFunctionSelect = 0xab;
constexpr quint8 CmdSleepOn = 0xae;  // display off
constexpr quint8 CmdSleepOff = 0xaf; // display on
constexpr quint8 CmdPhaseLength = 0xb1;
constexpr quint8 CmdClockDivider = 0xb3;
constexpr quint8 CmdEnhancementA = 0xb4;
constexpr quint8 CmdGpio = 0xb5;
constexpr quint8 CmdSecondPrecharge = 0xb6;
constexpr quint8 CmdDefaultGrayTable = 0xb9;
constexpr quint8 CmdPrechargeVoltage = 0xbb;
constexpr quint8 CmdVcomh = 0xbe;
constexpr quint8 CmdContrastCurrent = 0xc1;
constexpr quint8 CmdMasterCurrent = 0xc7;
constexpr quint8 CmdMuxRatio = 0xca;
constexpr quint8 CmdEnhancementB = 0xd1;
constexpr quint8 CmdLock = 0xfd;

// The controller has 480 columns of RAM, addressed in groups of four pixels (two bytes).
constexpr int RamColumns = 480;
constexpr int RamRows = 128;
// spidev on Linux takes at most 4096 bytes per transfer unless its bufsiz parameter is raised.
// Splitting the RAM data is harmless: D/C stays high and the controller goes on where it was.
constexpr qsizetype MaxTransfer = 4096;

QByteArray bytes(std::initializer_list<int> values) {
    QByteArray out;
    out.reserve(static_cast<qsizetype>(values.size()));
    for (const int v : values)
        out.append(static_cast<char>(v));
    return out;
}

} // namespace

class QSsd1322DisplayPrivate {
public:
    explicit QSsd1322DisplayPrivate(QSsd1322Display *owner) : q(owner) {}

    void setError(QSsd1322Display::DisplayError e, const QString &message) {
        error = e;
        errorString = message;
        if (e != QSsd1322Display::NoError)
            emit q->errorOccurred(e);
    }

    bool ready() {
        if (open)
            return true;
        setError(QSsd1322Display::NotOpenError, QStringLiteral("The display is not open"));
        return false;
    }

    bool setDc(bool high) {
        if (dcState == static_cast<int>(high))
            return true;
        if (!dc.setValue(high)) {
            setError(QSsd1322Display::DeviceError, dc.errorString());
            dcState = -1;
            return false;
        }
        dcState = high ? 1 : 0;
        return true;
    }

    bool send(const QByteArray &data) {
        for (qsizetype at = 0; at < data.size(); at += MaxTransfer) {
            if (!spi.write(data.mid(at, MaxTransfer))) {
                setError(QSsd1322Display::DeviceError, spi.errorString());
                return false;
            }
        }
        return true;
    }

    bool command(quint8 cmd, const QByteArray &arguments = QByteArray()) {
        if (!setDc(false) || !send(QByteArray(1, static_cast<char>(cmd))))
            return false;
        return arguments.isEmpty() || (setDc(true) && send(arguments));
    }

    // The remap of the datasheet's reference panels (Newhaven, Winstar, EastRising): horizontal
    // address increment, nibble remap, dual COM, COM scan reversed; rotated by 180 degrees with the
    // column order reversed instead of the COM scan.
    QByteArray remap() const { return bytes({rotated ? 0x06 : 0x14, 0x11}); }

    bool initialize() {
        const bool ok = command(CmdLock, bytes({0x12})) && command(CmdSleepOn) &&
                        command(CmdClockDivider, bytes({0x91})) && command(CmdMuxRatio, bytes({height - 1})) &&
                        command(CmdDisplayOffset, bytes({0x00})) && command(CmdStartLine, bytes({0x00})) &&
                        command(CmdRemap, remap()) && command(CmdGpio, bytes({0x00})) &&
                        command(CmdFunctionSelect, bytes({0x01})) && command(CmdEnhancementA, bytes({0xa0, 0xfd})) &&
                        command(CmdContrastCurrent, bytes({contrast})) && command(CmdMasterCurrent, bytes({0x0f})) &&
                        command(CmdDefaultGrayTable) && command(CmdPhaseLength, bytes({0xe2})) &&
                        command(CmdEnhancementB, bytes({0x82, 0x20})) && command(CmdPrechargeVoltage, bytes({0x1f})) &&
                        command(CmdSecondPrecharge, bytes({0x08})) && command(CmdVcomh, bytes({0x07})) &&
                        command(CmdNormal) && command(CmdExitPartial);
        return ok && q->clear() && command(CmdSleepOff);
    }

    QSsd1322Display *q;
    QSpiDevice spi;
    QGpioLine dc;
    QGpioLine reset;
    int dcState = -1; // the level D/C was last set to, or -1 if unknown
    bool open = false;
    int width = 256;
    int height = 64;
    int columnOffset = 28;
    bool rotated = false;
    int contrast = 0x9f;
    QSsd1322Display::DisplayError error = QSsd1322Display::NoError;
    QString errorString;
};

/*!
    \class QSsd1322Display
    \inmodule NtGpio
    \brief The QSsd1322Display class drives an OLED display with a Solomon Systech SSD1322
    controller over SPI.
    \since 6.0

    The SSD1322 drives graphic OLED panels of up to 480 by 128 pixels with 16 levels of gray,
    most often 256 by 64 pixels, as on the 3.12 inch modules of Newhaven, Winstar and EastRising.
    QSsd1322Display speaks the controller's 4-wire SPI protocol: an SPI device for the data, a
    GPIO line for D/C (data or command), and optionally a GPIO line for RES (reset):

    \code
    QSsd1322Display oled;
    oled.setSpiDevice(QStringLiteral("spidev0.0"));
    oled.setDataCommandLine(QStringLiteral("gpiochip0:25"));
    oled.setResetLine(QStringLiteral("gpiochip0:24"));
    if (!oled.open())
        qFatal("%s", qPrintable(oled.errorString()));

    QImage frame(oled.width(), oled.height(), QImage::Format_Grayscale8);
    frame.fill(0);
    QPainter p(&frame);
    p.setPen(Qt::white);
    p.drawText(frame.rect(), Qt::AlignCenter, QStringLiteral("Hello, SSD1322"));
    p.end();
    oled.writePixels(frame.constBits(), frame.bytesPerLine());
    \endcode

    writePixels() takes 8-bit gray pixels and sends the top four bits of each; it does not need
    Nt GUI, so a headless device draws with whatever it likes. To run Nt GUI and Nt Widgets
    applications on the panel, start them on the \c ssd1322 platform, which uses this class (see
    \l{Using Nt GUI}).

    The bus classes it uses, QSpiDevice and QGpioLine, work on Linux and on Zephyr, so this class
    does too. A full 256 by 64 frame is 8 KB, which takes about 7 ms at the default 10 MHz.

    \differences
    Qt has no such class; QSsd1322Display is an Nt extension. Only the 4-wire SPI interface is
    supported: not 3-wire SPI (9-bit words) and not the 6800 or 8080 parallel buses. The gray
    scale table is the controller's linear default, and the analog settings are those of the
    common 256 by 64 modules; writeCommand() changes them for other panels.
    \enddifferences

    \sa QCharacterDisplay, QSpiDevice, QGpioLine
*/

/*!
    \enum QSsd1322Display::DisplayError

    This enum describes the errors of a display.

    \value NoError No error occurred.
    \value DeviceError The SPI device or a GPIO line failed; errorString() says which and why.
    \value NotOpenError The operation needs an open display.
    \value UnsupportedOperationError The settings or arguments are not possible.
*/

/*!
    Constructs a display object with the given \a parent. Set the SPI device and the D/C line, and
    open() it.
*/
QSsd1322Display::QSsd1322Display(QObject *parent) : QObject(parent), d(std::make_unique<QSsd1322DisplayPrivate>(this)) {
    d->spi.setSpeed(10000000);
}

/*!
    Destroys the display object. The panel keeps showing what it showed.
*/
QSsd1322Display::~QSsd1322Display() = default;

/*!
    Sets the SPI device to \a name. It takes effect at the next open().
*/
void QSsd1322Display::setSpiDevice(const QString &name) { d->spi.setDeviceName(name); }

/*!
    Returns the name of the SPI device.
*/
QString QSsd1322Display::spiDevice() const { return d->spi.deviceName(); }

/*!
    Sets the SPI clock to \a hertz. On an open display it takes effect at once.
*/
void QSsd1322Display::setSpeed(quint32 hertz) { d->spi.setSpeed(hertz); }

/*!
    Returns the SPI clock in hertz.
*/
quint32 QSsd1322Display::speed() const { return d->spi.speed(); }

/*!
    Sets the GPIO line wired to the D/C pin to \a line. It takes effect at the next open().
*/
void QSsd1322Display::setDataCommandLine(const QString &line) { d->dc.setLineName(line); }

/*!
    Returns the name of the GPIO line wired to the D/C pin.
*/
QString QSsd1322Display::dataCommandLine() const { return d->dc.lineName(); }

/*!
    Sets the GPIO line wired to the RES pin to \a line, or to none if \a line is empty. open()
    resets the controller through it. It takes effect at the next open().
*/
void QSsd1322Display::setResetLine(const QString &line) { d->reset.setLineName(line); }

/*!
    Returns the name of the GPIO line wired to the RES pin.
*/
QString QSsd1322Display::resetLine() const { return d->reset.lineName(); }

/*!
    Sets the size of the panel to \a width by \a height pixels. The width must be a multiple of
    4; sizes the controller cannot drive are ignored. It takes effect at the next open().

    \sa setColumnOffset()
*/
void QSsd1322Display::setSize(int width, int height) {
    if (width < 4 || width % 4 != 0 || height < 16 || height > RamRows)
        return;
    d->width = width;
    d->height = height;
}

/*!
    Returns the width of the panel in pixels.
*/
int QSsd1322Display::width() const { return d->width; }

/*!
    Returns the height of the panel in pixels.
*/
int QSsd1322Display::height() const { return d->height; }

/*!
    Sets the first column group of the controller's RAM that the panel shows to \a offset. A group
    is four pixels. Panels 256 pixels wide are wired to the middle of the 480 columns, which makes
    the offset 28; a panel that shows the picture shifted needs another value.
*/
void QSsd1322Display::setColumnOffset(int offset) {
    if (offset >= 0)
        d->columnOffset = offset;
}

/*!
    Returns the first column group of the RAM that the panel shows.
*/
int QSsd1322Display::columnOffset() const { return d->columnOffset; }

/*!
    Turns the picture by 180 degrees if \a rotated is \c true, for a panel mounted upside down. On
    an open display it takes effect at once for what is written next.
*/
void QSsd1322Display::setRotated(bool rotated) {
    d->rotated = rotated;
    if (d->open)
        d->command(CmdRemap, d->remap());
}

/*!
    Returns \c true if the picture is turned by 180 degrees.
*/
bool QSsd1322Display::isRotated() const { return d->rotated; }

/*!
    Opens the SPI device and the GPIO lines, resets the controller, sets it up, clears the panel
    and switches it on. Returns \c true on success; otherwise sets error() and returns \c false. An
    open display is closed first.
*/
bool QSsd1322Display::open() {
    close();
    clearError();
    if (d->spi.deviceName().isEmpty() || d->dc.lineName().isEmpty()) {
        d->setError(UnsupportedOperationError, QStringLiteral("The SPI device and the D/C line must be set"));
        return false;
    }
    if (d->columnOffset * 4 + d->width > RamColumns) {
        d->setError(UnsupportedOperationError, QStringLiteral("The panel does not fit in the controller's RAM"));
        return false;
    }
    d->spi.setMode(QSpiDevice::Mode0);
    if (!d->spi.open()) {
        d->setError(DeviceError, d->spi.errorString());
        return false;
    }
    d->dc.setDirection(QGpioLine::Output);
    if (!d->dc.open()) {
        d->setError(DeviceError, d->dc.errorString());
        d->spi.close();
        return false;
    }
    d->dcState = 0; // an output line starts low
    if (!d->reset.lineName().isEmpty()) {
        d->reset.setDirection(QGpioLine::Output);
        // RES is active low: hold it low for 1 ms (100 us is the minimum), then wait for the
        // controller to come out of reset.
        const bool ok = d->reset.open() && d->reset.setValue(false);
        if (ok)
            QThread::msleep(1);
        if (!ok || !d->reset.setValue(true)) {
            d->setError(DeviceError, d->reset.errorString());
            close();
            return false;
        }
        QThread::msleep(2);
    }
    d->open = true;
    if (!d->initialize()) {
        const DisplayError e = d->error;
        const QString message = d->errorString;
        close();
        d->error = e;
        d->errorString = message;
        return false;
    }
    return true;
}

/*!
    Releases the SPI device and the GPIO lines. The panel keeps showing what it showed.
*/
void QSsd1322Display::close() {
    d->open = false;
    d->dcState = -1;
    d->spi.close();
    d->dc.close();
    d->reset.close();
}

/*!
    Returns \c true if the display is open.
*/
bool QSsd1322Display::isOpen() const { return d->open; }

/*!
    Sets the segment current to \a contrast, from 0 to 255. Higher is brighter and wears the panel
    faster. On an open display it takes effect at once. Returns \c true on success.
*/
bool QSsd1322Display::setContrast(int contrast) {
    if (contrast < 0 || contrast > 255) {
        d->setError(UnsupportedOperationError, QStringLiteral("The contrast is from 0 to 255"));
        return false;
    }
    d->contrast = contrast;
    return !d->open || d->command(CmdContrastCurrent, bytes({contrast}));
}

/*!
    Returns the segment current.
*/
int QSsd1322Display::contrast() const { return d->contrast; }

/*!
    Switches the panel on if \a enabled is \c true, or puts the controller to sleep; the RAM is
    kept either way. Returns \c true on success.
*/
bool QSsd1322Display::setDisplayEnabled(bool enabled) {
    return d->ready() && d->command(enabled ? CmdSleepOff : CmdSleepOn);
}

/*!
    Shows the picture in inverse gray levels if \a inverted is \c true. Returns \c true on
    success.
*/
bool QSsd1322Display::setInverted(bool inverted) { return d->ready() && d->command(inverted ? CmdInverse : CmdNormal); }

/*!
    Sets every pixel of the panel to black. Returns \c true on success.
*/
bool QSsd1322Display::clear() {
    if (!d->ready())
        return false;
    const QByteArray black(static_cast<qsizetype>(d->width) * d->height / 2, '\0');
    const int first = d->columnOffset;
    const int last = d->columnOffset + d->width / 4 - 1;
    return d->command(CmdSetColumns, bytes({first, last})) && d->command(CmdSetRows, bytes({0, d->height - 1})) &&
           d->command(CmdWriteRam) && d->setDc(true) && d->send(black);
}

/*!
    Sends the pixels of \a area to the panel; a null \a area means all of it. \a pixels points at
    the top left pixel of a whole frame with one byte of gray per pixel (0 is black, 255 white)
    and \a bytesPerLine bytes from one row to the next, as in a QImage of format
    \c Format_Grayscale8. The top four bits of each byte are shown.

    The area is clipped to the panel and widened to a multiple of four pixels, since the
    controller addresses its RAM in groups of four. Returns \c true on success.
*/
bool QSsd1322Display::writePixels(const uchar *pixels, qsizetype bytesPerLine, const QRect &area) {
    if (!d->ready())
        return false;
    if (!pixels || bytesPerLine < d->width) {
        d->setError(UnsupportedOperationError, QStringLiteral("The pixels do not cover the panel"));
        return false;
    }
    QRect r = area.isNull() ? QRect(0, 0, d->width, d->height) : area.intersected(QRect(0, 0, d->width, d->height));
    if (r.isEmpty())
        return true;
    const int left = r.left() & ~3;
    const int right = std::min(d->width, (r.right() + 4) & ~3); // one past the last column
    const int top = r.top();
    const int rows = r.height();
    const int groups = (right - left) / 4;

    QByteArray data(static_cast<qsizetype>(groups) * 2 * rows, '\0');
    char *out = data.data();
    for (int y = top; y < top + rows; ++y) {
        const uchar *line = pixels + static_cast<qsizetype>(y) * bytesPerLine;
        for (int x = left; x < right; x += 2)
            *out++ = static_cast<char>((line[x] & 0xf0) | (line[x + 1] >> 4));
    }
    const int first = d->columnOffset + left / 4;
    return d->command(CmdSetColumns, bytes({first, first + groups - 1})) &&
           d->command(CmdSetRows, bytes({top, top + rows - 1})) && d->command(CmdWriteRam) && d->setDc(true) &&
           d->send(data);
}

/*!
    Sends \a command to the controller, followed by \a arguments as data bytes, for settings this
    class does not have, such as the gray scale table (\c 0xb8) or the precharge period. Returns
    \c true on success.
*/
bool QSsd1322Display::writeCommand(quint8 command, const QByteArray &arguments) {
    return d->ready() && d->command(command, arguments);
}

/*!
    Returns the last error that occurred.
*/
QSsd1322Display::DisplayError QSsd1322Display::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QSsd1322Display::errorString() const { return d->errorString; }

/*!
    Resets error() to \c NoError.
*/
void QSsd1322Display::clearError() {
    d->error = NoError;
    d->errorString.clear();
}

#include "moc_qssd1322display.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
