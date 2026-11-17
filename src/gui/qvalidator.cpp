// MIT License - see LICENSE
#include <QtGui/qvalidator.h>

#include <cmath>
#include <cstdlib>
#include <limits>

namespace {

// A number split into its parts as it is typed, in the notation of `locale`. Fails on a character that cannot be part
// of the number, which makes the input invalid.
struct Number {
    bool negative = false;
    bool positive = false;
    QString integer;  // the digits before the decimal point, without group separators
    QString fraction; // the digits after it
    bool point = false;
    bool exponent = false;
    QString exponentDigits; // with its sign
};

bool isDigit(QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); }

bool split(const QString &input, const QLocale &locale, bool allowFraction, bool allowExponent, Number *number) {
    const QString minus = locale.negativeSign();
    const QString plus = locale.positiveSign();
    const QString point = locale.decimalPoint();
    const QString group = locale.groupSeparator();
    qsizetype i = 0;
    const qsizetype n = input.size();
    if (input.startsWith(minus) || input.startsWith(QLatin1Char('-'))) {
        number->negative = true;
        i = input.startsWith(minus) ? minus.size() : 1;
    } else if (input.startsWith(plus) || input.startsWith(QLatin1Char('+'))) {
        number->positive = true;
        i = input.startsWith(plus) ? plus.size() : 1;
    }
    while (i < n) {
        const QChar c = input.at(i);
        if (number->exponent) {
            if (isDigit(c) || ((c == QLatin1Char('-') || c == QLatin1Char('+')) && number->exponentDigits.isEmpty()))
                number->exponentDigits.append(c);
            else
                return false;
            ++i;
        } else if (isDigit(c)) {
            (number->point ? number->fraction : number->integer).append(c);
            ++i;
        } else if (allowFraction && !number->point && QStringView(input).mid(i).startsWith(point)) {
            number->point = true;
            i += point.size();
        } else if (!number->point && !group.isEmpty() && QStringView(input).mid(i).startsWith(group) &&
                   !number->integer.isEmpty()) {
            i += group.size();
        } else if (allowExponent && (c == QLatin1Char('e') || c == QLatin1Char('E')) &&
                   !(number->integer.isEmpty() && number->fraction.isEmpty())) {
            number->exponent = true;
            ++i;
        } else {
            return false;
        }
    }
    return true;
}

int digitCount(qint64 value) {
    int digits = 1;
    for (value = value < 0 ? -value : value; value >= 10; value /= 10)
        ++digits;
    return digits;
}

} // namespace

/*!
    \class QValidator
    \inmodule NtGui
    \since 6.0
    \brief The QValidator class checks text that the user types.

    A validator decides whether a text is \l{QValidator::State}{Acceptable}, could become acceptable as the user goes
    on typing (\c Intermediate), or is \c Invalid. QLineEdit::setValidator() makes a line edit refuse the edits that
    make its text invalid, and emit returnPressed() and editingFinished() only for acceptable text:

    \code
    QLineEdit *port = new QLineEdit(this);
    port->setValidator(new QIntValidator(1, 65535, port));
    \endcode

    Subclass it and reimplement validate(), and fixup() if text that is not acceptable can be corrected.

    \differences
    The locale is used for the signs, the decimal point and the group separators; digits are the ASCII digits.
    \enddifferences

    \sa QIntValidator, QDoubleValidator, QRegularExpressionValidator
*/

/*!
    \enum QValidator::State

    This enum says what a validator thinks of a text.

    \value Invalid The text cannot become acceptable by typing more.
    \value Intermediate The text is not acceptable yet, but can become so.
    \value Acceptable The text is acceptable.
*/

/*!
    Constructs a validator with the given \a parent.
*/
QValidator::QValidator(QObject *parent) : QObject(parent) {}

/*!
    Destroys the validator.
*/
QValidator::~QValidator() = default;

/*!
    Sets the locale that numbers are read in to \a locale. The default is QLocale().
*/
void QValidator::setLocale(const QLocale &locale) {
    m_locale = locale;
    emit changed();
}

/*!
    Returns the locale that numbers are read in.
*/
QLocale QValidator::locale() const { return m_locale; }

/*!
    \fn QValidator::State QValidator::validate(QString &input, int &pos) const

    Returns the state of \a input, with the cursor at \a pos. A validator may change both, for example to put a
    text in upper case.
*/

/*!
    Corrects \a input, if it can, so that it becomes acceptable. The default implementation does nothing.

    A line edit calls it when Return is pressed or it loses the focus while its text is not acceptable.
*/
void QValidator::fixup(QString &input) const { (void)input; }

/*!
    \class QIntValidator
    \inmodule NtGui
    \since 6.0
    \brief The QIntValidator class accepts integers in a range.

    The text must be an integer from bottom() to top(). A text that has too few digits to be in the range, such as
    \c 4 for a range of 10 to 99, is \c Intermediate, as typing more makes it acceptable.

    \differences
    Digits are the ASCII digits.
    \enddifferences

    \sa QDoubleValidator
*/

/*!
    Constructs a validator with the given \a parent that accepts every \c int.
*/
QIntValidator::QIntValidator(QObject *parent)
    : QIntValidator(std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), parent) {}

/*!
    Constructs a validator with the given \a parent that accepts the integers from \a minimum to \a maximum.
*/
QIntValidator::QIntValidator(int minimum, int maximum, QObject *parent)
    : QValidator(parent), m_bottom(minimum), m_top(maximum) {}

/*!
    Destroys the validator.
*/
QIntValidator::~QIntValidator() = default;

/*!
    \reimp
*/
QValidator::State QIntValidator::validate(QString &input, int &pos) const {
    (void)pos;
    if (input.isEmpty())
        return Intermediate;
    Number number;
    if (!split(input, locale(), false, false, &number))
        return Invalid;
    if ((number.negative && m_bottom >= 0) || (number.positive && m_top < 0))
        return Invalid;
    if (number.integer.isEmpty())
        return Intermediate; // only a sign
    if (number.integer.size() > 19)
        return Invalid;
    qint64 entered = number.integer.toLongLong();
    if (number.negative)
        entered = -entered;
    if (entered >= m_bottom && entered <= m_top)
        return Acceptable;
    // Too large a number of digits cannot become smaller by typing more; a sign typed last is still allowed.
    if (entered >= 0)
        return (entered > m_top && -entered < m_bottom && number.integer.size() > digitCount(m_top)) ? Invalid
                                                                                                     : Intermediate;
    return entered < m_bottom ? Invalid : Intermediate;
}

/*!
    \reimp

    Removes the group separators and writes the number again in the locale.
*/
void QIntValidator::fixup(QString &input) const {
    Number number;
    if (!split(input, locale(), false, false, &number) || number.integer.isEmpty() || number.integer.size() > 19)
        return;
    const qint64 value = number.integer.toLongLong();
    input = locale().toString(static_cast<qlonglong>(number.negative ? -value : value));
    input.remove(locale().groupSeparator());
}

/*!
    Sets the lowest acceptable value to \a bottom.
*/
void QIntValidator::setBottom(int bottom) { setRange(bottom, m_top); }

/*!
    Sets the highest acceptable value to \a top.
*/
void QIntValidator::setTop(int top) { setRange(m_bottom, top); }

/*!
    Sets the range of acceptable values to \a bottom up to \a top.
*/
void QIntValidator::setRange(int bottom, int top) {
    const bool bottomChange = bottom != m_bottom;
    const bool topChange = top != m_top;
    m_bottom = bottom;
    m_top = top;
    if (bottomChange)
        emit bottomChanged(m_bottom);
    if (topChange)
        emit topChanged(m_top);
    if (bottomChange || topChange)
        emit changed();
}

/*!
    \fn int QIntValidator::bottom() const

    Returns the lowest acceptable value.
*/

/*!
    \fn int QIntValidator::top() const

    Returns the highest acceptable value.
*/

/*!
    \class QDoubleValidator
    \inmodule NtGui
    \since 6.0
    \brief The QDoubleValidator class accepts floating-point numbers in a range.

    The text must be a number from bottom() to top() with at most decimals() digits after the decimal point. In
    ScientificNotation, the default, it may have an exponent, as in \c 1.5e3.

    \differences
    Digits are the ASCII digits. A number out of the range is \c Invalid in StandardNotation once its whole part is
    larger than the range allows, and is always \c Intermediate in ScientificNotation.
    \enddifferences

    \sa QIntValidator
*/

/*!
    \enum QDoubleValidator::Notation

    This enum says how a number may be written.

    \value StandardNotation As digits with an optional decimal point, such as \c 1250.5.
    \value ScientificNotation As StandardNotation, or with an exponent, such as \c 1.2505e3.
*/

/*!
    Constructs a validator with the given \a parent that accepts every number.
*/
QDoubleValidator::QDoubleValidator(QObject *parent)
    : QDoubleValidator(-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), -1, parent) {}

/*!
    Constructs a validator with the given \a parent that accepts the numbers from \a bottom to \a top with at most
    \a decimals digits after the decimal point; -1 allows any number of them.
*/
QDoubleValidator::QDoubleValidator(double bottom, double top, int decimals, QObject *parent)
    : QValidator(parent), m_bottom(bottom), m_top(top), m_decimals(decimals) {}

/*!
    Destroys the validator.
*/
QDoubleValidator::~QDoubleValidator() = default;

/*!
    \reimp
*/
QValidator::State QDoubleValidator::validate(QString &input, int &pos) const {
    (void)pos;
    if (input.isEmpty())
        return Intermediate;
    Number number;
    if (!split(input, locale(), m_decimals != 0, m_notation == ScientificNotation, &number))
        return Invalid;
    if ((number.negative && m_bottom >= 0) || (number.positive && m_top < 0))
        return Invalid;
    if (m_decimals >= 0 && number.fraction.size() > m_decimals)
        return Invalid;
    if (number.integer.isEmpty() && number.fraction.isEmpty())
        return Intermediate;
    if (number.exponent && (number.exponentDigits.isEmpty() || !isDigit(number.exponentDigits.back())))
        return Intermediate;
    QString c = number.negative ? QStringLiteral("-") : QString();
    c += number.integer.isEmpty() ? QStringLiteral("0") : number.integer;
    if (!number.fraction.isEmpty())
        c += QLatin1Char('.') + number.fraction;
    if (number.exponent)
        c += QLatin1Char('e') + number.exponentDigits;
    const QByteArray bytes = c.toLatin1();
    const double value = std::strtod(bytes.constData(), nullptr);
    if (std::isnan(value))
        return Invalid;
    if (value >= m_bottom && value <= m_top)
        return Acceptable;
    if (m_notation == ScientificNotation)
        return Intermediate;
    const double limit = std::max(std::fabs(m_bottom), std::fabs(m_top));
    return std::trunc(std::fabs(value)) > limit ? Invalid : Intermediate;
}

/*!
    \reimp

    Rounds the number to decimals() digits after the decimal point.
*/
void QDoubleValidator::fixup(QString &input) const {
    bool ok = false;
    const double value = locale().toDouble(input, &ok);
    if (!ok || m_decimals < 0)
        return;
    input = locale().toString(value, 'f', m_decimals);
    input.remove(locale().groupSeparator());
}

/*!
    Sets the range of acceptable values to \a bottom up to \a top, with at most \a decimals digits after the
    decimal point.
*/
void QDoubleValidator::setRange(double bottom, double top, int decimals) {
    const bool bottomChange = bottom != m_bottom;
    const bool topChange = top != m_top;
    const bool decimalsChange = decimals != m_decimals;
    m_bottom = bottom;
    m_top = top;
    m_decimals = decimals;
    if (bottomChange)
        emit bottomChanged(m_bottom);
    if (topChange)
        emit topChanged(m_top);
    if (decimalsChange)
        emit decimalsChanged(m_decimals);
    if (bottomChange || topChange || decimalsChange)
        emit changed();
}

/*!
    \overload

    Sets the range of acceptable values to \a bottom up to \a top, and keeps decimals().
*/
void QDoubleValidator::setRange(double bottom, double top) { setRange(bottom, top, m_decimals); }

/*!
    Sets the lowest acceptable value to \a bottom.
*/
void QDoubleValidator::setBottom(double bottom) { setRange(bottom, m_top, m_decimals); }

/*!
    Sets the highest acceptable value to \a top.
*/
void QDoubleValidator::setTop(double top) { setRange(m_bottom, top, m_decimals); }

/*!
    Sets the largest number of digits after the decimal point to \a decimals; -1 allows any number.
*/
void QDoubleValidator::setDecimals(int decimals) { setRange(m_bottom, m_top, decimals); }

/*!
    Sets the notation that numbers may be written in to \a notation.
*/
void QDoubleValidator::setNotation(Notation notation) {
    if (notation == m_notation)
        return;
    m_notation = notation;
    emit notationChanged(m_notation);
    emit changed();
}

/*!
    \fn double QDoubleValidator::bottom() const

    Returns the lowest acceptable value.
*/

/*!
    \fn double QDoubleValidator::top() const

    Returns the highest acceptable value.
*/

/*!
    \fn int QDoubleValidator::decimals() const

    Returns the largest number of digits after the decimal point, or -1 for any number.
*/

/*!
    \fn QDoubleValidator::Notation QDoubleValidator::notation() const

    Returns the notation that numbers may be written in.
*/

/*!
    \class QRegularExpressionValidator
    \inmodule NtGui
    \since 6.0
    \brief The QRegularExpressionValidator class accepts text that matches a regular expression.

    The whole text must match regularExpression(); a text that is the start of a match is \c Intermediate:

    \code
    // A call sign: two to six letters or digits.
    edit->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Z0-9]{2,6}"), edit));
    \endcode

    \differences
    None beyond those of QRegularExpression.
    \enddifferences

    \sa QIntValidator
*/

/*!
    Constructs a validator with the given \a parent that accepts any text.
*/
QRegularExpressionValidator::QRegularExpressionValidator(QObject *parent) : QValidator(parent) {}

/*!
    Constructs a validator with the given \a parent that accepts the text that matches \a re.
*/
QRegularExpressionValidator::QRegularExpressionValidator(const QRegularExpression &re, QObject *parent)
    : QValidator(parent), m_re(re) {}

/*!
    Destroys the validator.
*/
QRegularExpressionValidator::~QRegularExpressionValidator() = default;

/*!
    \reimp
*/
QValidator::State QRegularExpressionValidator::validate(QString &input, int &pos) const {
    if (m_re.pattern().isEmpty())
        return Acceptable;
    const QRegularExpression anchored(QRegularExpression::anchoredPattern(m_re.pattern()), m_re.patternOptions());
    const QRegularExpressionMatch match = anchored.match(input, 0, QRegularExpression::PartialPreferCompleteMatch);
    if (match.hasMatch())
        return Acceptable;
    if (input.isEmpty() || match.hasPartialMatch())
        return Intermediate;
    pos = static_cast<int>(input.size());
    return Invalid;
}

/*!
    \fn QRegularExpression QRegularExpressionValidator::regularExpression() const

    Returns the regular expression that the text must match.
*/

/*!
    Sets the regular expression that the text must match to \a re.
*/
void QRegularExpressionValidator::setRegularExpression(const QRegularExpression &re) {
    if (re == m_re)
        return;
    m_re = re;
    emit regularExpressionChanged(m_re);
    emit changed();
}

#include "moc_qvalidator.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
