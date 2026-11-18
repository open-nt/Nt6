// MIT License - see LICENSE
//
// QBarcode: the value class, dispatch to the encoder of each symbology, and the renderer shared by
// toImage(), toSvg() and paint(). Every symbol is drawn as horizontal bands of dark runs, so the
// three outputs agree module for module.
#include "qbarcode_p.h"

#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpainter.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

using namespace QtBarcodePrivate;

namespace {

// Height of the human-readable text band under a linear symbol, how far guard bars extend into
// it, and the height of the digits, in modules.
constexpr int kTextBand = 10;
constexpr int kGuardExtension = 5;
constexpr double kTextSize = 9.0;
constexpr double kSmallTextSize = 7.0;

struct Band {
    int top = 0;
    int bottom = 0;
    std::vector<std::pair<int, int>> runs; // [begin, end) columns of dark modules
    bool fullWidth = false;                // a bearer bar, drawn across the quiet zones too
};

bool hasText(const QBarcodePrivate &d) { return d.linear && d.textVisible && !d.textItems.isEmpty(); }

int totalHeight(const QBarcodePrivate &d) { return d.height + (hasText(d) ? kTextBand : 0); }

void appendRuns(Band &band, int width, const std::function<bool(int)> &dark) {
    for (int x = 0; x < width;) {
        if (!dark(x)) {
            ++x;
            continue;
        }
        int end = x + 1;
        while (end < width && dark(end))
            ++end;
        band.runs.emplace_back(x, end);
        x = end;
    }
}

std::vector<Band> bands(const QBarcodePrivate &d) {
    std::vector<Band> result;
    if (!d.linear) {
        for (int y = 0; y < d.height; ++y) {
            Band band{y, y + 1, {}, false};
            appendRuns(band, d.width, [&d, y](int x) { return d.modules.at(qsizetype(y) * d.width + x) != 0; });
            result.push_back(std::move(band));
        }
        return result;
    }
    if (d.bearer > 0) {
        result.push_back(Band{0, d.bearer, {{0, d.width}}, true});
        result.push_back(Band{d.height - d.bearer, d.height, {{0, d.width}}, true});
    }
    Band bars{d.bearer, d.height - d.bearer, {}, false};
    appendRuns(bars, d.width, [&d](int x) { return d.modules.at(x) != 0; });
    result.push_back(std::move(bars));
    if (hasText(d) && !d.guards.isEmpty()) {
        Band guards{d.height, d.height + kGuardExtension, {}, false};
        appendRuns(guards, d.width, [&d](int x) { return d.modules.at(x) != 0 && d.guards.at(x) != 0; });
        result.push_back(std::move(guards));
    }
    return result;
}

QFont textFont(double pixelSize) {
    QFont font(QStringLiteral("OCR-B"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(std::max(1, int(std::lround(pixelSize))));
    return font;
}

// Draws the human-readable text; origin is the top-left corner of the bars, and moduleWidth and
// moduleHeight the size of one module in pixels.
void drawText(const QBarcodePrivate &d, QPainter *painter, double originX, double originY, double moduleWidth,
              double moduleHeight, const QColor &color) {
    painter->setPen(color);
    const double top = originY + (d.height + 0.5) * moduleHeight;
    for (const QBarcodeTextItem &item : d.textItems) {
        double size = (item.small ? kSmallTextSize : kTextSize) * moduleHeight;
        QFont font = textFont(size);
        // Text centred under the whole symbol shrinks to fit its width.
        const bool single = d.textItems.size() == 1;
        const double room = d.width * moduleWidth;
        const int advance = QFontMetrics(font).horizontalAdvance(item.text);
        if (single && advance > room && advance > 0) {
            size = size * room / advance;
            font = textFont(size);
        }
        painter->setFont(font);
        const double centre = originX + item.x * moduleWidth;
        const double half = std::max(room, double(advance)) + 4 * moduleWidth;
        painter->drawText(QRectF(centre - half, top, 2 * half, kTextBand * moduleHeight),
                          int(Qt::AlignHCenter | Qt::AlignTop), item.text);
    }
}

QByteArray colorName(const QColor &color) { return color.name().toLatin1(); }

QByteArray fillAttributes(const QColor &color) {
    QByteArray result = "fill=\"" + colorName(color) + '"';
    if (color.alpha() < 255)
        result += " fill-opacity=\"" + QByteArray::number(color.alphaF(), 'g', 3) + '"';
    return result;
}

QByteArray xmlEscaped(const QString &text) {
    QByteArray result;
    for (char c : text.toUtf8()) {
        if (c == '&')
            result += "&amp;";
        else if (c == '<')
            result += "&lt;";
        else if (c == '>')
            result += "&gt;";
        else
            result += c;
    }
    return result;
}

QByteArray number(double value) { return QByteArray::number(value, 'g', 6); }

} // namespace

// ---------------------------------------------------------------------------------------------
// Shared helpers

void QBarcodePrivate::fail(const QString &message) {
    error = message;
    modules.clear();
    guards.clear();
    textItems.clear();
    text.clear();
    width = 0;
    height = 0;
    version = 0;
    mask = -1;
    bearer = 0;
}

void QBarcodePrivate::setMatrix(int size, const std::vector<quint8> &dark) { setMatrix(size, size, dark); }

void QBarcodePrivate::setMatrix(int columns, int rows, const std::vector<quint8> &dark) {
    modules = QByteArray(qsizetype(dark.size()), 0);
    for (std::size_t i = 0; i < dark.size(); ++i)
        modules[qsizetype(i)] = char(dark[i] != 0 ? 1 : 0);
    width = columns;
    height = rows;
    linear = false;
}

void QBarcodePrivate::setBars(const QByteArray &bars, int barHeight) {
    modules = bars;
    width = int(bars.size());
    height = barHeight;
    linear = true;
}

namespace QtBarcodePrivate {

GaloisField::GaloisField(int bits, int primitive)
    : m_size(1 << bits), m_exp(std::size_t(m_size), 0), m_log(std::size_t(m_size), 0) {
    int x = 1;
    for (int i = 0; i < m_size - 1; ++i) {
        m_exp[std::size_t(i)] = x;
        m_log[std::size_t(x)] = i;
        x <<= 1;
        if (x >= m_size)
            x ^= primitive;
    }
}

int GaloisField::multiply(int a, int b) const {
    if (a == 0 || b == 0)
        return 0;
    return exp(m_log[std::size_t(a)] + m_log[std::size_t(b)]);
}

std::vector<int> reedSolomon(const GaloisField &field, const std::vector<int> &data, int count, int base) {
    // The generator, highest power first: (x - a^base)(x - a^(base+1))...
    std::vector<int> generator{1};
    for (int i = 0; i < count; ++i) {
        const int root = field.exp(base + i);
        std::vector<int> next(generator.size() + 1, 0);
        for (std::size_t j = 0; j < generator.size(); ++j) {
            next[j] ^= generator[j];
            next[j + 1] ^= field.multiply(generator[j], root);
        }
        generator = std::move(next);
    }
    std::vector<int> remainder(static_cast<std::size_t>(count), 0);
    for (int word : data) {
        const int factor = word ^ remainder.front();
        remainder.erase(remainder.begin());
        remainder.push_back(0);
        for (std::size_t i = 0; i < remainder.size(); ++i)
            remainder[i] ^= field.multiply(generator[i + 1], factor);
    }
    return remainder;
}

bool allDigits(const QByteArray &data) {
    return std::all_of(data.cbegin(), data.cend(), [](char c) { return c >= '0' && c <= '9'; });
}

int mod10CheckDigit(const QByteArray &digits) {
    // Weights alternate 3, 1, ... from the rightmost digit.
    int sum = 0;
    int weight = 3;
    for (qsizetype i = digits.size() - 1; i >= 0; --i) {
        sum += (digits.at(i) - '0') * weight;
        weight = 4 - weight;
    }
    return (10 - sum % 10) % 10;
}

} // namespace QtBarcodePrivate

/*!
    \module NtBarcode
    \title Nt Barcode C++ Classes
    \brief Encodes QR Code, Data Matrix, Aztec and linear barcodes as images, SVG or painter output.
    \cmaketarget Nt::Barcode

    Nt Barcode turns text into a scannable symbol: a QR Code, Data Matrix or Aztec Code for URLs,
    tickets, part marks and pairing data, or a linear Code 128, GS1-128, Code 39, ITF, EAN or UPC
    symbol for labels, cartons and products. QBarcode encodes the data once and draws the result as
    a QImage, an SVG document or with a QPainter. Qt has no such module. It needs Nt Core and
    Nt GUI. See \l{Using Nt Barcode} for an overview.
*/

/*!
    \class QBarcode
    \inmodule NtBarcode
    \brief The QBarcode class encodes data as a two-dimensional or linear barcode.
    \since 6.0

    Construct a QBarcode with the data and the symbology. The constructor encodes the data, and
    the result can be drawn at any size:

    \code
    QBarcode qr(QStringLiteral("https://open-nt.org"));
    qr.toImage(8).save(QStringLiteral("site.png"));

    QBarcode label("NT-6.0.0", QBarcode::Code128);
    label.setTextVisible(true);
    QFile svg(QStringLiteral("label.svg"));
    if (svg.open(QIODevice::WriteOnly))
        svg.write(label.toSvg(2));
    \endcode

    If the data cannot be encoded, for example because it is too long for the symbology, holds a
    character the symbology lacks, or an EAN number has the wrong check digit, isNull() returns
    \c true and errorString() says why.

    The symbol is a grid of modules: size() is its width and height in modules, and isDark()
    returns the color of one module. toImage(), toSvg() and paint() add a light quiet zone
    around it, quietZone() modules wide by default, which scanners need to find the symbol.

    \section1 Two-Dimensional Symbols

    A QR Code holds up to 2953 bytes (at LevelL). The encoder uses numeric mode when the data is
    only digits, alphanumeric mode when it uses only \c{0-9 A-Z}, space and \c{$%*+-./:}, and
    byte mode otherwise; a QString is encoded as UTF-8, which phone scanners expect. It picks the
    smallest version (1 to 40) that holds the data and the mask pattern with the lowest penalty.

    A Data Matrix (ECC 200) symbol is square, from 10 x 10 to 144 x 144 modules, or with
    RectangularSymbol one of six rectangles from 8 x 18 to 16 x 48. It holds up to 3116 digits,
    2335 upper case letters or 1555 bytes of binary data. QBarcode mixes the ASCII, C40, Text, X12
    and Base 256 encodations so that the data takes the fewest codewords. Error correction is fixed
    by the size, so the level is ignored. GS1 DataMatrix takes Application Identifiers like GS1-128
    and starts with FNC1.

    An Aztec Code is square too, 15 x 15 to 151 x 151 modules, with a bullseye in the middle and
    no quiet zone. The level sets the share of the symbol given to error correction: 10%, 23%,
    36% or 50%. Letters, digits and punctuation take 4 or 5 bits each; other bytes are encoded in
    binary.

    \section1 Linear Symbols

    Code 128 encodes ASCII (bytes 0 to 127), switching between code sets A, B and C to keep the
    symbol short. GS1-128 is Code 128 with GS1 Application Identifiers, written as
    \c{(01)09501101530003(17)250101(10)AB12}: QBarcode inserts the FNC1 codes, and checks the
    lengths of the fixed-length fields and the check digit of an SSCC or GTIN.

    Code 39 encodes \c{0-9 A-Z}, space and \c{-.$/+%}. Interleaved 2 of 5 encodes an even number of
    digits (a leading zero is added to an odd number), and ITF-14 is the GTIN-14 form of it for
    outer cartons, with bearer bars above and below.

    EAN-13 takes 12 digits, EAN-8 7 and UPC-A 11, and appends the check digit; given one more
    digit, the last is checked. UPC-E takes the number system (0 or 1) and 6 digits, and appends
    the check digit of the UPC-A number they stand for; 6 digits alone get number system 0.

    A linear symbol is one row of bars: isDark() ignores \c y (apart from the bearer bars of
    ITF-14), and the height in size() is a nominal bar height in modules. Call setTextVisible() to
    print text() under the bars; EAN and UPC then extend their guard bars and place the digits in
    the usual groups, with the first (and for UPC the last) digit in the quiet zone.

    \differences
    Qt has no barcode class; QBarcode is an Nt addition. A QR Code is encoded in a single segment
    without ECI, Kanji mode or structured append, and Micro QR is not supported. Data Matrix is
    encoded without EDIFACT, ECI, structured append, reader programming or the DMRE rectangular
    extension sizes. Aztec Codes have no ECI, structured append or Aztec Runes. Code 128 rejects
    bytes above 127, Code 39 has no Full ASCII mode or check character, and EAN and UPC have no
    2- or 5-digit add-on. PDF417 is not supported.
    \enddifferences

    \sa {Using Nt Barcode}
*/

/*!
    \enum QBarcode::Symbology

    This enum describes the kind of symbol.

    \value QrCode A two-dimensional QR Code (Model 2), versions 1 to 40.
    \value Code128 A linear Code 128 symbol of ASCII text.
    \value Ean13 A linear EAN-13 symbol of 13 digits, the last a check digit.
    \value Ean8 A linear EAN-8 symbol of 8 digits, the last a check digit.
    \value UpcA A linear UPC-A symbol of 12 digits, the last a check digit.
    \value UpcE A linear UPC-E symbol: the number system, 6 digits and a check digit.
    \value Gs1Code128 A linear GS1-128 symbol of Application Identifiers and their values.
    \value Code39 A linear Code 39 symbol of digits, upper case letters, space and \c{-.$/+%}.
    \value Interleaved2Of5 A linear Interleaved 2 of 5 symbol of an even number of digits.
    \value Itf14 A linear ITF-14 symbol of a GTIN-14, with bearer bars.
    \value DataMatrix A two-dimensional Data Matrix (ECC 200) symbol.
    \value Aztec A two-dimensional Aztec Code, compact or full-range.
    \value Gs1DataMatrix A Data Matrix symbol of GS1 Application Identifiers and their values.
*/

/*!
    \enum QBarcode::Option

    This enum describes options that change how a symbol is encoded.

    \value NoOption The default encoding.
    \value RectangularSymbol Data Matrix uses the smallest of the six rectangular sizes (8 x 18 to
        16 x 48 modules) that holds the data, for narrow labels, and a square size if none does.
        Other symbologies ignore it.
*/

/*!
    \enum QBarcode::ErrorCorrectionLevel

    This enum describes how much of a QR Code or Aztec Code can be damaged or covered and still be
    read. A higher level makes the symbol larger. Data Matrix and linear symbols ignore it.

    \value LevelL About 7% of a QR Code can be restored; 10% of an Aztec Code is error correction.
    \value LevelM About 15% of a QR Code can be restored; 23% of an Aztec Code is error correction.
    \value LevelQ About 25% of a QR Code can be restored; 36% of an Aztec Code is error correction.
    \value LevelH About 30% of a QR Code can be restored; 50% of an Aztec Code is error correction.
*/

/*!
    Constructs a null barcode.

    \sa isNull()
*/
QBarcode::QBarcode() : d(new QBarcodePrivate) {}

/*!
    Constructs a barcode of the given \a symbology that encodes the bytes of \a data. For a QR
    Code or an Aztec Code, \a level is the error correction level; \a options change how some
    symbologies are encoded.

    \sa isNull(), errorString()
*/
QBarcode::QBarcode(const QByteArray &data, Symbology symbology, ErrorCorrectionLevel level, Options options)
    : d(new QBarcodePrivate) {
    d->symbology = symbology;
    d->level = level;
    d->options = options;
    d->data = data;
    switch (symbology) {
    case QrCode:
        encodeQr(d.data());
        break;
    case DataMatrix:
    case Gs1DataMatrix:
        encodeDataMatrix(d.data());
        break;
    case Aztec:
        encodeAztec(d.data());
        break;
    case Code128:
        encodeCode128(d.data());
        break;
    case Gs1Code128:
        encodeGs1Code128(d.data());
        break;
    case Ean13:
    case Ean8:
    case UpcA:
    case UpcE:
        encodeEanUpc(d.data());
        break;
    case Code39:
        encodeCode39(d.data());
        break;
    case Interleaved2Of5:
    case Itf14:
        encodeInterleaved2Of5(d.data());
        break;
    }
}

/*!
    \overload

    Constructs a barcode of the given \a symbology that encodes \a text as UTF-8, with the error
    correction \a level for a QR Code or an Aztec Code and the given \a options.
*/
QBarcode::QBarcode(const QString &text, Symbology symbology, ErrorCorrectionLevel level, Options options)
    : QBarcode(text.toUtf8(), symbology, level, options) {}

/*!
    \overload

    Constructs a barcode of the given \a symbology that encodes the UTF-8 string \a text, with the
    error correction \a level for a QR Code or an Aztec Code and the given \a options.
*/
QBarcode::QBarcode(const char *text, Symbology symbology, ErrorCorrectionLevel level, Options options)
    : QBarcode(QByteArray(text), symbology, level, options) {}

/*!
    Constructs a copy of \a other. QBarcode is implicitly shared, so the copy is cheap.
*/
QBarcode::QBarcode(const QBarcode &other) = default;

/*!
    Move-constructs a barcode from \a other.
*/
QBarcode::QBarcode(QBarcode &&other) noexcept = default;

/*!
    Destroys the barcode.
*/
QBarcode::~QBarcode() = default;

/*!
    Assigns \a other to this barcode and returns a reference to it.
*/
QBarcode &QBarcode::operator=(const QBarcode &other) = default;

/*!
    Move-assigns \a other to this barcode and returns a reference to it.
*/
QBarcode &QBarcode::operator=(QBarcode &&other) noexcept = default;

/*!
    \fn void QBarcode::swap(QBarcode &other)

    Swaps this barcode with \a other.
*/

/*!
    Returns \c true if the barcode holds no symbol, because it was default-constructed or its data
    could not be encoded; otherwise returns \c false.

    \sa errorString()
*/
bool QBarcode::isNull() const { return d->width == 0; }

/*!
    Returns why the data could not be encoded, or an empty string if it was.

    \sa isNull()
*/
QString QBarcode::errorString() const { return d->error; }

/*!
    Returns the data the barcode was constructed with. For EAN and UPC the check digit is included,
    and for Interleaved 2 of 5 the leading zero that makes the number of digits even.
*/
QByteArray QBarcode::data() const { return d->data; }

/*!
    Returns the human-readable text of a linear symbol: the digits of EAN, UPC and ITF, the
    Application Identifiers of GS1-128 in parentheses, or the encoded text. For GS1 DataMatrix,
    returns the Application Identifiers in parentheses. Returns an empty string for other
    two-dimensional symbols and a null barcode.

    \sa setTextVisible()
*/
QString QBarcode::text() const { return d->text; }

/*!
    Returns the kind of symbol.
*/
QBarcode::Symbology QBarcode::symbology() const { return d->symbology; }

/*!
    Returns the error correction level the barcode was constructed with.
*/
QBarcode::ErrorCorrectionLevel QBarcode::errorCorrectionLevel() const { return d->level; }

/*!
    Returns the options the barcode was constructed with.
*/
QBarcode::Options QBarcode::options() const { return d->options; }

/*!
    Returns the version (1 to 40) of a QR Code, which sets its size: a version \e v symbol is
    4\e v + 17 modules wide. For an Aztec Code, returns the number of data layers (1 to 4 for a
    compact symbol, 1 to 32 for a full-range one). Returns 0 for other symbologies and a null
    barcode.
*/
int QBarcode::version() const { return d->version; }

/*!
    Returns the mask pattern (0 to 7) of a QR Code, or -1 for other symbologies and a null barcode.
*/
int QBarcode::maskPattern() const { return d->mask; }

/*!
    Returns \c true if the barcode is a linear symbol (one row of bars), or \c false if it is
    two-dimensional or null.
*/
bool QBarcode::isLinear() const { return !isNull() && d->linear; }

/*!
    Returns \c true if toImage(), toSvg() and paint() print the human-readable text under a linear
    symbol. The default is \c false.

    \sa setTextVisible(), text()
*/
bool QBarcode::isTextVisible() const { return d->textVisible; }

/*!
    Sets whether toImage(), toSvg() and paint() print the human-readable text under a linear
    symbol to \a visible. The text adds 10 modules to the height; EAN and UPC also extend their
    guard bars into it. Two-dimensional symbols ignore it.

    \sa isTextVisible(), imageSize()
*/
void QBarcode::setTextVisible(bool visible) {
    if (d->textVisible != visible)
        d->textVisible = visible;
}

/*!
    Returns the width and height of the symbol in modules, without the quiet zone or text. For a
    linear symbol the height is the nominal bar height.

    \sa isDark(), quietZone(), imageSize()
*/
QSize QBarcode::size() const { return QSize(d->width, d->height); }

/*!
    Returns \c true if the module at column \a x and row \a y is dark. Positions outside the
    symbol are light. For a linear symbol, \a y only has to lie within size().
*/
bool QBarcode::isDark(int x, int y) const {
    if (x < 0 || y < 0 || x >= d->width || y >= d->height)
        return false;
    if (!d->linear)
        return d->modules.at(qsizetype(y) * d->width + x) != 0;
    if (y < d->bearer || y >= d->height - d->bearer)
        return true;
    return d->modules.at(x) != 0;
}

/*!
    Returns the width of the light margin, in modules, that scanners need around the symbol: 4 for
    a QR Code, 1 for Data Matrix and GS1 DataMatrix, none for Aztec, 10 for Code 128, GS1-128, Code 39, ITF and
    ITF-14, 9 for UPC-E and 11 for EAN and UPC-A.
*/
int QBarcode::quietZone() const {
    switch (d->symbology) {
    case QrCode:
        return 4;
    case DataMatrix:
    case Gs1DataMatrix:
        return 1;
    case Aztec:
        return 0;
    case Code128:
    case Gs1Code128:
    case Code39:
    case Interleaved2Of5:
    case Itf14:
        return 10;
    case UpcE:
        return 9;
    case Ean13:
    case Ean8:
    case UpcA:
        break;
    }
    return 11;
}

/*!
    Returns the size in pixels of the image that toImage() makes with \a moduleSize pixels per
    module and a margin of \a quietZone modules (quietZone() if negative), including the text when
    it is visible. Returns an empty size for a null barcode.
*/
QSize QBarcode::imageSize(int moduleSize, int quietZone) const {
    if (isNull() || moduleSize <= 0)
        return QSize();
    const int margin = quietZone < 0 ? this->quietZone() : quietZone;
    return QSize((d->width + 2 * margin) * moduleSize, (totalHeight(*d) + 2 * margin) * moduleSize);
}

/*!
    Returns the symbol as an image with each module \a moduleSize pixels square and a margin of
    \a quietZone modules, or of quietZone() modules if \a quietZone is negative. Dark modules are
    painted \a dark and the rest \a light; pass a transparent color as \a light for a symbol
    without a background. Returns a null image for a null barcode.

    \sa imageSize()
*/
QImage QBarcode::toImage(int moduleSize, int quietZone, const QColor &dark, const QColor &light) const {
    const QSize size = imageSize(moduleSize, quietZone);
    if (size.isEmpty())
        return QImage();
    const int margin = quietZone < 0 ? this->quietZone() : quietZone;
    QImage image(size, QImage::Format_ARGB32);
    image.fill(light.rgba());
    const QRgb darkPixel = dark.rgba();
    for (const Band &band : bands(*d)) {
        for (const auto &[begin, end] : band.runs) {
            const int left = band.fullWidth ? 0 : (begin + margin) * moduleSize;
            const int right = band.fullWidth ? size.width() : (end + margin) * moduleSize;
            for (int py = (band.top + margin) * moduleSize; py < (band.bottom + margin) * moduleSize; ++py) {
                auto *line = reinterpret_cast<QRgb *>(image.scanLine(py));
                std::fill(line + left, line + right, darkPixel);
            }
        }
    }
    if (hasText(*d)) {
        QPainter painter(&image);
        drawText(*d, &painter, margin * moduleSize, margin * moduleSize, moduleSize, moduleSize, dark);
    }
    return image;
}

/*!
    Returns the symbol as an SVG document whose dark modules are \a dark rectangles on a \a light
    background (none if \a light is transparent), with a margin of \a quietZone modules, or of
    quietZone() modules if \a quietZone is negative. The document scales freely; its nominal size
    is \a moduleSize pixels per module. Returns an empty byte array for a null barcode.
*/
QByteArray QBarcode::toSvg(int moduleSize, int quietZone, const QColor &dark, const QColor &light) const {
    if (isNull() || moduleSize <= 0)
        return QByteArray();
    const int margin = quietZone < 0 ? this->quietZone() : quietZone;
    const int width = d->width + 2 * margin;
    const int height = totalHeight(*d) + 2 * margin;
    const QByteArray w = QByteArray::number(width);
    const QByteArray h = QByteArray::number(height);
    QByteArray svg = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                     "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" +
                     QByteArray::number(width * moduleSize) + "\" height=\"" + QByteArray::number(height * moduleSize) +
                     "\" viewBox=\"0 0 " + w + ' ' + h + "\" shape-rendering=\"crispEdges\">\n";
    if (light.alpha() > 0)
        svg += "<rect width=\"" + w + "\" height=\"" + h + "\" " + fillAttributes(light) + "/>\n";
    svg += "<path " + fillAttributes(dark) + " d=\"";
    for (const Band &band : bands(*d)) {
        for (const auto &[begin, end] : band.runs) {
            const int left = band.fullWidth ? 0 : begin + margin;
            const int run = band.fullWidth ? width : end - begin;
            svg += 'M' + QByteArray::number(left) + ' ' + QByteArray::number(band.top + margin) + 'h' +
                   QByteArray::number(run) + 'v' + QByteArray::number(band.bottom - band.top) + 'h' +
                   QByteArray::number(-run) + 'z';
        }
    }
    svg += "\"/>\n";
    if (hasText(*d)) {
        const double baseline = margin + d->height + 0.5 + kTextSize * 0.8;
        for (const QBarcodeTextItem &item : d->textItems) {
            double size = item.small ? kSmallTextSize : kTextSize;
            if (d->textItems.size() == 1)
                size = std::min(size, 1.6 * d->width / double(std::max<qsizetype>(1, item.text.size())));
            svg += "<text x=\"" + number(margin + item.x) + "\" y=\"" + number(baseline) + "\" font-size=\"" +
                   number(size) + "\" font-family=\"OCR-B, 'OCR B', monospace\" text-anchor=\"middle\" " +
                   fillAttributes(dark) + '>' + xmlEscaped(item.text) + "</text>\n";
        }
    }
    svg += "</svg>\n";
    return svg;
}

/*!
    Paints the symbol with \a painter, scaled to fill \a target, in \a dark on whatever is already
    there. No quiet zone is added; leave a light margin around \a target. Module edges are rounded
    to whole pixels, so modules differ in size by at most one pixel and no seams appear. When the
    text is visible it takes the bottom of \a target, and the digits that EAN and UPC print in the
    quiet zone are drawn outside \a target.
*/
void QBarcode::paint(QPainter *painter, const QRect &target, const QColor &dark) const {
    if (painter == nullptr || isNull() || target.isEmpty())
        return;
    const int rows = totalHeight(*d);
    auto edge = [](int origin, int extent, int i, int count) {
        return origin + int((qint64(extent) * i + count / 2) / count);
    };
    for (const Band &band : bands(*d)) {
        const int top = edge(target.top(), target.height(), band.top, rows);
        const int bottom = edge(target.top(), target.height(), band.bottom, rows);
        for (const auto &[begin, end] : band.runs) {
            const int left = edge(target.left(), target.width(), begin, d->width);
            const int right = edge(target.left(), target.width(), end, d->width);
            painter->fillRect(QRect(left, top, right - left, bottom - top), dark);
        }
    }
    if (hasText(*d)) {
        painter->save();
        drawText(*d, painter, target.left(), target.top(), double(target.width()) / d->width,
                 double(target.height()) / rows, dark);
        painter->restore();
    }
}

/*!
    Returns the EAN/UPC check digit (0 to 9) of \a digits, which holds the digits without the
    check digit (12 for EAN-13, 7 for EAN-8, 11 for UPC-A, 13 for a GTIN-14, 17 for an SSCC), or -1
    if \a digits is empty or holds a character that is not a digit.
*/
int QBarcode::checkDigit(const QByteArray &digits) {
    if (digits.isEmpty() || !allDigits(digits))
        return -1;
    return mod10CheckDigit(digits);
}
