// MIT License - see LICENSE
#include <QtWidgets/qspinbox.h>

#include <algorithm>
#include <cmath>

/*!
    \class QSpinBox
    \brief The QSpinBox class lets the user pick an integer by typing it or stepping with two buttons.

    A spin box shows a number in a text that the user can edit, with a button to step up and one to step down. The
    arrow keys, Page Up and Page Down, and the mouse wheel step too. A prefix and a suffix, such as a unit,
    are shown around the number:

    \code
    QSpinBox *size = new QSpinBox(window);
    size->setRange(8, 72);
    size->setSuffix(" pt");
    connect(size, &QSpinBox::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    Spin boxes: plain, with a suffix and the keyboard focus, with a prefix, at the largest value, and disabled.

    \image widgets/qspinbox.png QSpinBox in the light color scheme

    \differences
    There is no \c displayIntegerBase() or \c stepType(). textFromValue() and valueFromText() see the number without
    the prefix and the suffix, and what the user types is checked by turning it into a value and back.
    \enddifferences

    \sa QDoubleSpinBox, QSlider, QLineEdit
*/

/*!
    Constructs a spin box with the given \a parent. The range is 0 to 99 and the value 0.
*/
QSpinBox::QSpinBox(QWidget *parent) : QAbstractSpinBox(parent) { refreshText(); }

/*!
    Destroys the spin box.
*/
QSpinBox::~QSpinBox() = default;

/*!
    \fn int QSpinBox::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QSpinBox::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QSpinBox::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QSpinBox::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the spin box to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is made
    equal. The value is brought into the range.

    \sa minimum(), maximum()
*/
void QSpinBox::setRange(int minimum, int maximum) {
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    changeValue(m_value, true);
    updateGeometry();
}

/*!
    \fn int QSpinBox::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, brought into the range. Emits valueChanged() if the value changes.

    \sa value()
*/
void QSpinBox::setValue(int value) { changeValue(value, true); }

/*!
    \fn int QSpinBox::singleStep() const

    Returns how much a step changes the value by, 1 by default.

    \sa setSingleStep()
*/

/*!
    Sets the step to \a step, at least 1.

    \sa singleStep()
*/
void QSpinBox::setSingleStep(int step) { m_step = std::max(1, step); }

/*!
    \fn QString QSpinBox::prefix() const

    Returns the text shown before the number.

    \sa setPrefix(), suffix()
*/

/*!
    Sets the text shown before the number to \a prefix.

    \sa prefix()
*/
void QSpinBox::setPrefix(const QString &prefix) {
    m_prefix = prefix;
    refreshText();
    updateGeometry();
}

/*!
    \fn QString QSpinBox::suffix() const

    Returns the text shown after the number, such as a unit.

    \sa setSuffix(), prefix()
*/

/*!
    Sets the text shown after the number to \a suffix.

    \sa suffix()
*/
void QSpinBox::setSuffix(const QString &suffix) {
    m_suffix = suffix;
    refreshText();
    updateGeometry();
}

/*!
    Returns the number as text, without the prefix and the suffix.

    \sa text()
*/
QString QSpinBox::cleanText() const { return textFromValue(m_value); }

/*!
    \reimp

    The value moves by \a steps times singleStep(). Past an end it stops there, or goes round to the other end if the
    spin box wraps.
*/
void QSpinBox::stepBy(int steps) {
    qint64 step = m_step;
    if (m_stepType == AdaptiveDecimalStepType) {
        // A power of ten one digit below the leading digit of the value, at least 1.
        qint64 magnitude = 1;
        for (qint64 v = std::abs(static_cast<qint64>(m_value)) / 10; v >= 10; v /= 10)
            magnitude *= 10;
        step = std::max<qint64>(m_step, magnitude);
    }
    qint64 value = static_cast<qint64>(m_value) + static_cast<qint64>(steps) * step;
    if (wrapping() && (value > m_maximum || value < m_minimum)) {
        const qint64 range = static_cast<qint64>(m_maximum) - m_minimum + 1;
        value = m_minimum + (((value - m_minimum) % range) + range) % range;
    }
    changeValue(static_cast<int>(std::clamp<qint64>(value, m_minimum, m_maximum)), true);
}

/*!
    Returns the text for \a value, without the prefix and the suffix. The default implementation writes the number in
    decimal; reimplement it to show the value in another way, together with valueFromText().

    \sa valueFromText()
*/
QString QSpinBox::textFromValue(int value) const {
    QString text = QString::number(value, m_base);
    if (m_base != 10 || !isGroupSeparatorShown())
        return text;
    const QString separator = locale().groupSeparator();
    const qsizetype digitsStart = value < 0 ? 1 : 0;
    for (qsizetype i = text.size() - 3; i > digitsStart; i -= 3)
        text.insert(i, separator);
    return text;
}

/*!
    Returns the value for \a text, which is the text between the prefix and the suffix. The default implementation
    reads a decimal number.

    \sa textFromValue()
*/
int QSpinBox::valueFromText(const QString &text) const { return stripped(text).toInt(nullptr, m_base); }

/*!
    \reimp
*/
QAbstractSpinBox::StepEnabled QSpinBox::stepEnabled() const {
    if (isReadOnly())
        return StepNone;
    if (wrapping())
        return StepUpEnabled | StepDownEnabled;
    StepEnabled steps = StepNone;
    if (m_value < m_maximum)
        steps |= StepUpEnabled;
    if (m_value > m_minimum)
        steps |= StepDownEnabled;
    return steps;
}

QString QSpinBox::valueText() const {
    if (!m_special.isEmpty() && m_value == m_minimum)
        return m_special;
    return m_prefix + textFromValue(m_value) + m_suffix;
}

QStringList QSpinBox::sizeTexts() const {
    return {m_prefix + textFromValue(m_minimum) + m_suffix, m_prefix + textFromValue(m_maximum) + m_suffix};
}

void QSpinBox::takeText(const QString &number, bool atMinimum, bool finished) {
    if (atMinimum) {
        changeValue(m_minimum, finished);
        return;
    }
    bool ok = false;
    int value = stripped(number).toInt(&ok, m_base);
    if (!ok && !number.isEmpty()) {
        // A text of a reimplemented textFromValue(): take it if it reads back the same.
        value = valueFromText(number);
        ok = textFromValue(value) == number;
    } else if (ok) {
        value = valueFromText(number);
    }
    if (finished) {
        const bool outside = ok && (value < m_minimum || value > m_maximum);
        changeValue(ok && !(outside && correctionMode() == CorrectToPreviousValue) ? value : m_value, true);
    } else if (ok && value >= m_minimum && value <= m_maximum) {
        changeValue(value, false); // leave the text as the user types it
    }
}

bool QSpinBox::acceptsCharacter(QChar c) const {
    if (c == QChar('-') || c == QChar('+') || c.isDigit())
        return true;
    if (isGroupSeparatorShown() && locale().groupSeparator().contains(c))
        return true;
    // Letters are digits in a base above ten.
    const int letter = c.toLower().unicode() - 'a';
    return m_base > 10 && letter >= 0 && letter < m_base - 10;
}

// Sets the value, brought into the range; rewrites the text if `rewrite`.
void QSpinBox::changeValue(int value, bool rewrite) {
    value = std::clamp(value, m_minimum, m_maximum);
    const bool changed = value != m_value;
    m_value = value;
    if (rewrite)
        refreshText();
    else
        update();
    if (changed) {
        emit valueChanged(m_value);
        emit textChanged(valueText());
    }
}

/*!
    \class QDoubleSpinBox
    \brief The QDoubleSpinBox class lets the user pick a floating-point number by typing it or stepping with two
    buttons.

    A QDoubleSpinBox works like a QSpinBox, with a value that is a \c double shown with decimals() digits after the
    point. The value is rounded to those decimals:

    \code
    QDoubleSpinBox *gain = new QDoubleSpinBox(window);
    gain->setRange(0.0, 2.0);
    gain->setSingleStep(0.05);
    gain->setDecimals(2);
    connect(gain, &QDoubleSpinBox::valueChanged, [](double value) { qDebug() << value; });
    \endcode

    Double spin boxes with decimals, a suffix and a prefix; one has the keyboard focus, one is disabled.

    \image widgets/qdoublespinbox.png QDoubleSpinBox in the light color scheme

    \differences
    The number is written and read with a point, whatever the locale, and without group separators. There is no
    \c stepType(). textFromValue() and valueFromText() see the number without the prefix and the suffix.
    \enddifferences

    \sa QSpinBox
*/

/*!
    Constructs a spin box with the given \a parent. The range is 0.00 to 99.99, the value 0.00 and the single step 1.0.
*/
QDoubleSpinBox::QDoubleSpinBox(QWidget *parent) : QAbstractSpinBox(parent) { refreshText(); }

/*!
    Destroys the spin box.
*/
QDoubleSpinBox::~QDoubleSpinBox() = default;

/*!
    \fn double QDoubleSpinBox::minimum() const

    Returns the smallest value, 0.0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum, rounded to the decimals. The largest value grows to the same if it was
    smaller.

    \sa minimum()
*/
void QDoubleSpinBox::setMinimum(double minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn double QDoubleSpinBox::maximum() const

    Returns the largest value, 99.99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum, rounded to the decimals. The smallest value falls to the same if it was
    larger.

    \sa maximum()
*/
void QDoubleSpinBox::setMaximum(double maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range of the spin box to \a minimum to \a maximum, both rounded to the decimals. If \a maximum is less
    than \a minimum, it is made equal. The value is brought into the range.

    \sa minimum(), maximum()
*/
void QDoubleSpinBox::setRange(double minimum, double maximum) {
    m_minimum = round(minimum);
    m_maximum = std::max(m_minimum, round(maximum));
    changeValue(m_value, true);
    updateGeometry();
}

/*!
    \fn double QDoubleSpinBox::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, rounded to the decimals and brought into the range. Emits valueChanged() if the value
    changes.

    \sa value()
*/
void QDoubleSpinBox::setValue(double value) { changeValue(value, true); }

/*!
    \fn double QDoubleSpinBox::singleStep() const

    Returns how much a step changes the value by, 1.0 by default.

    \sa setSingleStep()
*/

/*!
    Sets the step to \a step. A step that is not positive is ignored.

    \sa singleStep()
*/
void QDoubleSpinBox::setSingleStep(double step) {
    if (step > 0)
        m_step = step;
}

/*!
    \fn int QDoubleSpinBox::decimals() const

    Returns how many digits are shown after the point, 2 by default.

    \sa setDecimals()
*/

/*!
    Sets how many digits are shown after the point to \a decimals, from 0 to 15. The range and the value are rounded
    to them.

    \sa decimals()
*/
void QDoubleSpinBox::setDecimals(int decimals) {
    m_decimals = std::clamp(decimals, 0, 15);
    setRange(m_minimum, m_maximum);
}

/*!
    \fn QString QDoubleSpinBox::prefix() const

    Returns the text shown before the number.

    \sa setPrefix(), suffix()
*/

/*!
    Sets the text shown before the number to \a prefix.

    \sa prefix()
*/
void QDoubleSpinBox::setPrefix(const QString &prefix) {
    m_prefix = prefix;
    refreshText();
    updateGeometry();
}

/*!
    \fn QString QDoubleSpinBox::suffix() const

    Returns the text shown after the number, such as a unit.

    \sa setSuffix(), prefix()
*/

/*!
    Sets the text shown after the number to \a suffix.

    \sa suffix()
*/
void QDoubleSpinBox::setSuffix(const QString &suffix) {
    m_suffix = suffix;
    refreshText();
    updateGeometry();
}

/*!
    Returns the number as text, without the prefix and the suffix.

    \sa text()
*/
QString QDoubleSpinBox::cleanText() const { return textFromValue(m_value); }

/*!
    \reimp

    The value moves by \a steps times singleStep(). Past an end it stops there, or goes round to the other end if the
    spin box wraps.
*/
void QDoubleSpinBox::stepBy(int steps) {
    double step = m_step;
    if (m_stepType == AdaptiveDecimalStepType) {
        const double magnitude = m_value == 0.0 ? 0.0 : std::pow(10.0, std::floor(std::log10(std::abs(m_value))) - 1);
        step = std::max({m_step, magnitude, std::pow(10.0, -m_decimals)});
    }
    double value = m_value + steps * step;
    if (wrapping()) {
        if (value > m_maximum + 1e-9)
            value = m_minimum;
        else if (value < m_minimum - 1e-9)
            value = m_maximum;
    }
    changeValue(value, true);
}

/*!
    Returns the text for \a value, without the prefix and the suffix: the number with decimals() digits after the
    point.

    \sa valueFromText()
*/
QString QDoubleSpinBox::textFromValue(double value) const { return QString::number(value, 'f', m_decimals); }

/*!
    Returns the value for \a text, which is the text between the prefix and the suffix.

    \sa textFromValue()
*/
double QDoubleSpinBox::valueFromText(const QString &text) const { return text.toDouble(); }

/*!
    \reimp
*/
QAbstractSpinBox::StepEnabled QDoubleSpinBox::stepEnabled() const {
    if (isReadOnly())
        return StepNone;
    if (wrapping())
        return StepUpEnabled | StepDownEnabled;
    StepEnabled steps = StepNone;
    if (m_value < m_maximum)
        steps |= StepUpEnabled;
    if (m_value > m_minimum)
        steps |= StepDownEnabled;
    return steps;
}

QString QDoubleSpinBox::valueText() const {
    if (!m_special.isEmpty() && m_value <= m_minimum)
        return m_special;
    return m_prefix + textFromValue(m_value) + m_suffix;
}

QStringList QDoubleSpinBox::sizeTexts() const {
    return {m_prefix + textFromValue(m_minimum) + m_suffix, m_prefix + textFromValue(m_maximum) + m_suffix};
}

void QDoubleSpinBox::takeText(const QString &number, bool atMinimum, bool finished) {
    if (atMinimum) {
        changeValue(m_minimum, finished);
        return;
    }
    bool ok = false;
    (void)number.toDouble(&ok);
    double value = ok ? round(valueFromText(number)) : m_value;
    if (finished && ok && (value < m_minimum || value > m_maximum) && correctionMode() == CorrectToPreviousValue)
        value = m_value;
    if (finished)
        changeValue(value, true);
    else if (ok && value >= m_minimum && value <= m_maximum)
        changeValue(value, false); // leave the text as the user types it
}

bool QDoubleSpinBox::acceptsCharacter(QChar c) const {
    return c.isDigit() || c == QChar('-') || c == QChar('+') || (c == QChar('.') && m_decimals > 0);
}

double QDoubleSpinBox::round(double value) const {
    const double scale = std::pow(10.0, m_decimals);
    return std::round(value * scale) / scale;
}

// Sets the value, rounded and brought into the range; rewrites the text if `rewrite`.
void QDoubleSpinBox::changeValue(double value, bool rewrite) {
    value = std::clamp(round(value), m_minimum, m_maximum);
    const bool changed = value != m_value; // rounded values compare exactly
    m_value = value;
    if (rewrite)
        refreshText();
    else
        update();
    if (changed) {
        emit valueChanged(m_value);
        emit textChanged(valueText());
    }
}

QString QSpinBox::stripped(const QString &number) const {
    if (!isGroupSeparatorShown())
        return number;
    QString out = number;
    out.remove(locale().groupSeparator());
    return out;
}

/*!
    \fn int QSpinBox::displayIntegerBase() const
    \since 6.0

    Returns the base the value is shown in, 10 by default.
*/

/*!
    \since 6.0

    Shows the value in \a base, from 2 to 36; letters stand for the digits above 9. Another base is ignored.
*/
void QSpinBox::setDisplayIntegerBase(int base) {
    if (base < 2 || base > 36)
        return;
    m_base = base;
    refreshText();
    updateGeometry();
}

/*!
    \fn QAbstractSpinBox::StepType QSpinBox::stepType() const
    \since 6.0

    Returns how large a step is, DefaultStepType by default.
*/

/*!
    \fn void QSpinBox::setStepType(StepType stepType)
    \since 6.0

    Sets how large a step is to \a stepType.
*/

/*!
    \reimp

    Takes a number in the range, the special value text, or, while it is typed, an empty text or a sign.
*/
QValidator::State QSpinBox::validate(QString &input, int &pos) const {
    (void)pos;
    if (!m_special.isEmpty() && input == m_special)
        return QValidator::Acceptable;
    QString number = input;
    if (number.startsWith(m_prefix))
        number.remove(0, m_prefix.size());
    if (!m_suffix.isEmpty() && number.endsWith(m_suffix))
        number.chop(m_suffix.size());
    number = stripped(number.trimmed());
    if (number.isEmpty() || number == QLatin1String("-") || number == QLatin1String("+"))
        return QValidator::Intermediate;
    bool ok = false;
    const int value = number.toInt(&ok, m_base);
    if (!ok)
        return QValidator::Invalid;
    return value >= m_minimum && value <= m_maximum ? QValidator::Acceptable : QValidator::Intermediate;
}

/*!
    \reimp

    Writes a valid number in \a input again as the spin box writes it.
*/
void QSpinBox::fixup(QString &input) const {
    bool ok = false;
    const int value = stripped(input).toInt(&ok, m_base);
    if (ok)
        input = textFromValue(value);
}

/*!
    \fn QAbstractSpinBox::StepType QDoubleSpinBox::stepType() const
    \since 6.0

    Returns how large a step is, DefaultStepType by default.
*/

/*!
    \fn void QDoubleSpinBox::setStepType(StepType stepType)
    \since 6.0

    Sets how large a step is to \a stepType.
*/

/*!
    \reimp

    Takes a number in the range, the special value text, or, while it is typed, an empty text or a sign.
*/
QValidator::State QDoubleSpinBox::validate(QString &input, int &pos) const {
    (void)pos;
    if (!m_special.isEmpty() && input == m_special)
        return QValidator::Acceptable;
    QString number = input;
    if (number.startsWith(m_prefix))
        number.remove(0, m_prefix.size());
    if (!m_suffix.isEmpty() && number.endsWith(m_suffix))
        number.chop(m_suffix.size());
    number = number.trimmed();
    if (number.isEmpty() || number == QLatin1String("-") || number == QLatin1String("+"))
        return QValidator::Intermediate;
    bool ok = false;
    const double value = number.toDouble(&ok);
    if (!ok)
        return QValidator::Invalid;
    const qsizetype point = number.indexOf(QLatin1Char('.'));
    if (point >= 0 && number.size() - point - 1 > m_decimals)
        return QValidator::Invalid;
    return value >= m_minimum && value <= m_maximum ? QValidator::Acceptable : QValidator::Intermediate;
}

/*!
    \reimp

    Writes a valid number in \a input again with decimals() digits after the point.
*/
void QDoubleSpinBox::fixup(QString &input) const {
    bool ok = false;
    const double value = input.toDouble(&ok);
    if (ok)
        input = textFromValue(value);
}

#include "moc_qspinbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
