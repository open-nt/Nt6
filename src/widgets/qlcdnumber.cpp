// MIT License - see LICENSE
#include <QtWidgets/qlcdnumber.h>

#include <QtGui/qpainter.h>

#include <algorithm>

namespace {

enum Segment { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };

// The segments that are lit for a character: a is the top bar and the others follow clockwise, with g in the middle.
int segmentsOf(char c) {
    switch (c) {
    case '0':
        return A | B | C | D | E | F;
    case '1':
        return B | C;
    case '2':
        return A | B | D | E | G;
    case '3':
        return A | B | C | D | G;
    case '4':
        return B | C | F | G;
    case '5':
        return A | C | D | F | G;
    case '6':
        return A | C | D | E | F | G;
    case '7':
        return A | B | C;
    case '8':
        return A | B | C | D | E | F | G;
    case '9':
        return A | B | C | D | F | G;
    case 'A':
        return A | B | C | E | F | G;
    case 'B':
        return C | D | E | F | G;
    case 'C':
        return A | D | E | F;
    case 'D':
        return B | C | D | E | G;
    case 'E':
        return A | D | E | F | G;
    case 'F':
        return A | E | F | G;
    case '-':
        return G;
    case 'o':
        return C | D | E | G;
    case 'r':
        return E | G;
    case 'n':
        return C | E | G;
    case 'u':
        return C | D | E;
    case 'h':
        return C | E | F | G;
    case 'l':
        return D | E | F;
    default:
        return 0; // a space
    }
}

} // namespace

/*!
    \class QLCDNumber
    \brief The QLCDNumber class shows a number with digits that look like those of a calculator.

    The number is set with display(), and shown in decimal, hexadecimal, octal or binary. The digits are made of seven
    segments, and a display has as many digits as digitCount() says; a number that needs more is an overflow:

    \code
    QLCDNumber *clock = new QLCDNumber(8, window);
    clock->display("12:30");
    QLCDNumber *count = new QLCDNumber(window);
    count->display(42);
    \endcode

    Displays with a number, a hexadecimal number, a number with a decimal point, a minus sign, and the outline style.

    \image widgets/qlcdnumber.png QLCDNumber in the light color scheme

    \differences
    The segments are rectangles, without the slanted ends of Qt's, and \c Flat is the same as \c Filled but without the space
    between the segments. The characters that can be shown are the digits, \c A to \c F, the minus sign, a space, and a few
    letters; the colon of a clock and the quotation mark are drawn as spaces. A number is formatted to fit with fewer digits
    than Qt does for numbers that are not whole.
    \enddifferences

    \sa QProgressBar, QSpinBox
*/

/*!
    \enum QLCDNumber::Mode

    This enum says how a number is written.

    \value Hex Hexadecimal, with the digits \c 0 to \c F.
    \value Dec Decimal.
    \value Oct Octal.
    \value Bin Binary.
*/

/*!
    \enum QLCDNumber::SegmentStyle

    This enum says how the segments of a digit are drawn.

    \value Outline Only the outline of the segments, in the text color.
    \value Filled The segments are filled, with a small space between them.
    \value Flat The segments are filled and touch each other.
*/

/*!
    Constructs a display with five digits and the given \a parent.
*/
QLCDNumber::QLCDNumber(QWidget *parent) : QLCDNumber(5, parent) {}

/*!
    Constructs a display with \a numDigits digits and the given \a parent.
*/
QLCDNumber::QLCDNumber(uint numDigits, QWidget *parent)
    : QFrame(parent), m_digits(static_cast<int>(std::min(numDigits, 99u))) {
    setFrameStyle(Box | Raised);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_text = "0";
}

/*!
    Destroys the display.
*/
QLCDNumber::~QLCDNumber() = default;

/*!
    \fn bool QLCDNumber::smallDecimalPoint() const

    Returns whether the decimal point is drawn small, between the digits, so that it does not use a digit. The default is
    \c false.

    \sa setSmallDecimalPoint()
*/

/*!
    Sets whether the decimal point is drawn small, and does not use a digit, to \a on.

    \sa smallDecimalPoint()
*/
void QLCDNumber::setSmallDecimalPoint(bool on) {
    if (m_smallPoint == on)
        return;
    m_smallPoint = on;
    updateGeometry();
    update();
}

/*!
    \fn int QLCDNumber::digitCount() const

    Returns the number of digits, 5 by default.

    \sa setDigitCount()
*/

/*!
    Sets the number of digits to \a numDigits, from 0 to 99. What is shown is formatted again.

    \sa digitCount()
*/
void QLCDNumber::setDigitCount(int numDigits) {
    numDigits = std::clamp(numDigits, 0, 99);
    if (m_digits == numDigits)
        return;
    m_digits = numDigits;
    updateGeometry();
    update();
}

/*!
    \fn QLCDNumber::Mode QLCDNumber::mode() const

    Returns the base of the numbers that are shown, \c Dec by default.

    \sa setMode()
*/

/*!
    \fn void QLCDNumber::setHexMode()

    Sets the base of the numbers that are shown to hexadecimal; the same as \c setMode(Hex).

    \sa setMode()
*/

/*!
    \fn void QLCDNumber::setDecMode()

    Sets the base of the numbers that are shown to decimal; the same as \c setMode(Dec).

    \sa setMode()
*/

/*!
    \fn void QLCDNumber::setOctMode()

    Sets the base of the numbers that are shown to octal; the same as \c setMode(Oct).

    \sa setMode()
*/

/*!
    \fn void QLCDNumber::setBinMode()

    Sets the base of the numbers that are shown to binary; the same as \c setMode(Bin).

    \sa setMode()
*/

/*!
    Sets the base of the numbers that are shown to \a mode, and shows the number again.

    \sa mode()
*/
void QLCDNumber::setMode(Mode mode) {
    if (m_mode == mode)
        return;
    m_mode = mode;
    display(m_value);
}

/*!
    \fn QLCDNumber::SegmentStyle QLCDNumber::segmentStyle() const

    Returns how the segments are drawn, \c Filled by default.

    \sa setSegmentStyle()
*/

/*!
    Sets how the segments are drawn to \a style.

    \sa segmentStyle()
*/
void QLCDNumber::setSegmentStyle(SegmentStyle style) {
    if (m_style == style)
        return;
    m_style = style;
    update();
}

/*!
    \fn double QLCDNumber::value() const

    Returns the number that is shown.

    \sa display(), intValue()
*/

/*!
    Returns the number that is shown, rounded to a whole number.

    \sa value()
*/
int QLCDNumber::intValue() const { return static_cast<int>(m_value < 0 ? m_value - 0.5 : m_value + 0.5); }

QString QLCDNumber::format(int num) const {
    switch (m_mode) {
    case Hex:
        return QString::number(num, 16).toUpper();
    case Oct:
        return QString::number(num, 8);
    case Bin:
        return QString::number(num, 2);
    case Dec:
        break;
    }
    return QString::number(num);
}

// The digits and decimal points of `text`: a point after a digit goes with it if the points are small.
std::vector<QLCDNumber::Digit> QLCDNumber::digitsOf(const QString &text) const {
    std::vector<Digit> digits;
    for (QChar c : text) {
        char ch = c.unicode() < 128 ? static_cast<char>(c.unicode()) : ' ';
        if (ch >= 'a' && ch <= 'f' && m_mode == Hex)
            ch = static_cast<char>(ch - 32);
        if (ch == '.' && m_smallPoint && !digits.empty() && !digits.back().point)
            digits.back().point = true;
        else
            digits.push_back(Digit{ch, false});
    }
    return digits;
}

// A number with a fraction, written with the most decimals that fit.
QString QLCDNumber::format(double num) const {
    if (m_mode != Dec)
        return format(static_cast<int>(num));
    // As many decimals as fit; a number that does not fit as a whole number is an overflow.
    for (int decimals = std::max(0, m_digits); decimals >= 0; --decimals) {
        const QString s = QString::number(num, 'f', decimals);
        if (static_cast<int>(digitsOf(s).size()) <= m_digits)
            return s;
    }
    return QString::number(num, 'f', 0);
}

/*!
    Returns \c true if \a num needs more digits than the display has.

    \sa display()
*/
bool QLCDNumber::checkOverflow(double num) const { return static_cast<int>(digitsOf(format(num)).size()) > m_digits; }

/*!
    \overload

    Returns \c true if the whole number \a num needs more digits than the display has.
*/
bool QLCDNumber::checkOverflow(int num) const { return static_cast<int>(digitsOf(format(num)).size()) > m_digits; }

void QLCDNumber::show(const QString &text) {
    m_text = text;
    const bool over = static_cast<int>(digitsOf(text).size()) > m_digits;
    update();
    if (over)
        emit overflow();
}

/*!
    Shows the text \a str: digits, \c A to \c F, minus signs, spaces and a few letters. The value() becomes the number
    that the text makes, or 0.

    \sa value()
*/
void QLCDNumber::display(const QString &str) {
    bool ok = false;
    const double parsed = str.toDouble(&ok);
    m_value = ok ? parsed : 0;
    show(str);
}

/*!
    \overload

    Shows the whole number \a num in the base of the display.
*/
void QLCDNumber::display(int num) {
    m_value = num;
    show(format(num));
}

/*!
    \overload

    Shows the number \a num, with as many digits as there is room for. In the hexadecimal, octal and binary modes, it is
    shown as a whole number.
*/
void QLCDNumber::display(double num) {
    m_value = num;
    show(format(num));
}

/*!
    \reimp
*/
QSize QLCDNumber::sizeHint() const { return QSize(10 + 9 * (m_digits + (m_smallPoint ? 0 : 1)), 23); }

/*!
    \reimp
*/
void QLCDNumber::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    drawFrame(&painter);
    const QRect area = contentsRect().adjusted(2, 2, -2, -2);
    if (m_digits <= 0 || area.width() <= 0 || area.height() <= 0)
        return;
    const std::vector<Digit> digits = digitsOf(m_text);
    const int shown = static_cast<int>(digits.size());
    if (shown > m_digits)
        return; // an overflow shows nothing
    const int cellWidth = std::max(1, area.width() / m_digits);
    const int t = std::max(1, std::min(cellWidth / 6, area.height() / 9));
    const int gap = m_style == Flat ? 0 : 1;
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor color = palette().color(group, QPalette::WindowText);
    painter.setPen(color);
    for (int i = 0; i < shown; ++i) {
        const int x = area.left() + (m_digits - shown + i) * cellWidth + (cellWidth - (cellWidth - 2 * t)) / 2;
        const int w = std::max(3 * t, cellWidth - 2 * t);
        const int y = area.top(), h = area.height();
        const int vertical = (h - 3 * t) / 2;
        auto segment = [&](int sx, int sy, int sw, int sh, bool horizontal) {
            const QRect r = horizontal ? QRect(sx + gap, sy, sw - 2 * gap, sh) : QRect(sx, sy + gap, sw, sh - 2 * gap);
            if (r.width() <= 0 || r.height() <= 0)
                return;
            if (m_style == Outline) {
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(r.adjusted(0, 0, -1, -1));
            } else {
                painter.fillRect(r, color);
            }
        };
        const int lit = segmentsOf(digits[static_cast<std::size_t>(i)].character);
        const int top = y, bottom = y + h - t, middle = y + t + vertical;
        if (lit & A)
            segment(x + t, top, w - 2 * t, t, true);
        if (lit & D)
            segment(x + t, bottom, w - 2 * t, t, true);
        if (lit & G)
            segment(x + t, middle, w - 2 * t, t, true);
        if (lit & F)
            segment(x, top + t, t, vertical + t, false);
        if (lit & B)
            segment(x + w - t, top + t, t, vertical + t, false);
        if (lit & E)
            segment(x, middle, t, vertical + t, false);
        if (lit & C)
            segment(x + w - t, middle, t, vertical + t, false);
        if (digits[static_cast<std::size_t>(i)].character == '.')
            painter.fillRect(QRect(x + (w - t) / 2, y + h - t, t, t), color);
        else if (digits[static_cast<std::size_t>(i)].point)
            painter.fillRect(QRect(x + w, y + h - t, t, t), color);
    }
}

#include "moc_qlcdnumber.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
