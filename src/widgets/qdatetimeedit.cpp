// MIT License - see LICENSE
#include <QtWidgets/qdatetimeedit.h>

#include "qpaintediconbutton_p.h"
#include "qwidget_p.h"

#include <QtGui/qevent.h>
#include <QtWidgets/qcalendarwidget.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {

const char *const kMonthLong[] = {"January", "February", "March",     "April",   "May",      "June",
                                  "July",    "August",   "September", "October", "November", "December"};
const char *const kMonthShort[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *const kDayLong[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
const char *const kDayShort[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

const QDateTime &defaultMinimum() {
    static const QDateTime value(QDate(1752, 9, 14), QTime(0, 0));
    return value;
}

const QDateTime &defaultMaximum() {
    static const QDateTime value(QDate(9999, 12, 31), QTime(23, 59, 59, 999));
    return value;
}

QString padded(int value, int width) {
    QString text = QString::number(value);
    while (text.size() < width)
        text.prepend(QLatin1Char('0'));
    return text;
}

// The calendar of a date edit as a popup: it closes when the mouse is pressed outside it. Under QMaterialStyle it is
// Material's docked date picker; the application may set materialDatePicker to something else.
class CalendarPopup : public QCalendarWidget {
public:
    explicit CalendarPopup(QWidget *parent) : QCalendarWidget(parent) {
        hide();
        setProperty("materialDatePicker", QStringLiteral("docked"));
    }

protected:
    void hideEvent(QHideEvent *event) override {
        QtPrivate::WidgetAccess::setPopup(this, nullptr);
        QCalendarWidget::hideEvent(event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        QCalendarWidget::keyPressEvent(event);
    }
};

} // namespace

/*!
    \class QDateTimeEdit
    \brief The QDateTimeEdit class lets the user edit a date and a time.

    A date time edit shows a date and a time in the displayFormat(), divided into sections such as the day, the month
    and the hour. The arrow keys, the mouse wheel and the buttons step the section that the cursor is in; typing
    changes the text, which is taken when it is a valid date and time. A number that fills its section, such as two
    digits of the hour, moves on to the next section and selects it, as does the separator that follows a section (a
    colon typed after "3" of the hour writes "03" and selects the minute). In the AM/PM section, \c a and \c p choose
    AM or PM and move on to the next section, if there is one:

    \code
    auto *start = new QDateTimeEdit(QDateTime::currentDateTime(), window);
    start->setDisplayFormat("yyyy-MM-dd HH:mm");
    start->setMinimumDateTime(QDateTime::currentDateTime());
    connect(start, &QDateTimeEdit::dateTimeChanged, [](const QDateTime &dt) { schedule(dt); });
    \endcode

    The format has the letters of QDate::toString() and QTime::toString(): \c d, \c dd, \c ddd, \c dddd, \c M, \c MM,
    \c MMM, \c MMMM, \c yy, \c yyyy, \c h, \c hh, \c H, \c HH, \c m, \c mm, \c s, \c ss, \c z, \c zzz, and \c AP or
    \c ap; text in single quotes is shown as it is. A step that goes past the end of a section stops there, or goes
    round to the other end of the section if the edit wraps; the result is kept in the range.

    With setCalendarPopup(), the buttons open a QCalendarWidget to pick the date from.

    QDateEdit and QTimeEdit edit only a date or a time.

    A date and time edit, a QDateEdit with the keyboard focus, a QTimeEdit, and a disabled date edit.

    \image widgets/qdatetimeedit.png QDateTimeEdit in the light color scheme

    \differences
    There is no \c QLocale, so the default formats are \c "yyyy-MM-dd HH:mm", \c "yyyy-MM-dd" and \c "HH:mm", the
    names are English, and \c yy is read as a year of the century of the current date. The time is always local time:
    \c setTimeZone() keeps the zone for \c timeZone() but does not convert, and there is no \c timeSpec(). Tab does not move between the sections, and the calendar popup
    opens from the step buttons, which are drawn as for any spin box. \c setCalendar() keeps the calendar and
    gives it to the popup, but dates are shown in the Gregorian calendar.
    \enddifferences

    \sa QDateEdit, QTimeEdit, QCalendarWidget
*/

/*!
    \enum QDateTimeEdit::Section

    The sections of the text. Sections is a typedef for \c{QFlags<Section>}.

    \value NoSection No section.
    \value AmPmSection AM or PM.
    \value MSecSection The milliseconds.
    \value SecondSection The seconds.
    \value MinuteSection The minutes.
    \value HourSection The hours.
    \value DaySection The day.
    \value MonthSection The month.
    \value YearSection The year.
    \value TimeSections_Mask The sections of the time.
    \value DateSections_Mask The sections of the date.
*/

/*!
    Constructs a date time edit with the given \a parent, at 2000-01-01 00:00.
*/
QDateTimeEdit::QDateTimeEdit(QWidget *parent) : QAbstractSpinBox(parent) {
    init(QDateTime(QDate(2000, 1, 1), QTime(0, 0)), QStringLiteral("yyyy-MM-dd HH:mm"));
}

/*!
    Constructs a date time edit with the given \a datetime and \a parent.
*/
QDateTimeEdit::QDateTimeEdit(const QDateTime &datetime, QWidget *parent) : QAbstractSpinBox(parent) {
    init(datetime, QStringLiteral("yyyy-MM-dd HH:mm"));
}

/*!
    Constructs a date time edit that shows the \a date, with the given \a parent.
*/
QDateTimeEdit::QDateTimeEdit(QDate date, QWidget *parent) : QAbstractSpinBox(parent) {
    init(QDateTime(date, QTime(0, 0)), QStringLiteral("yyyy-MM-dd"));
}

/*!
    Constructs a date time edit that shows the \a time, with the given \a parent.
*/
QDateTimeEdit::QDateTimeEdit(QTime time, QWidget *parent) : QAbstractSpinBox(parent) {
    init(QDateTime(QDate(2000, 1, 1), time), QStringLiteral("HH:mm"));
}

/*!
    Destroys the edit.
*/
QDateTimeEdit::~QDateTimeEdit() = default;

void QDateTimeEdit::init(const QDateTime &value, const QString &format) {
    m_minimum = defaultMinimum();
    m_maximum = defaultMaximum();
    m_value = clamped(value.isValid() ? value : QDateTime(QDate(2000, 1, 1), QTime(0, 0)));
    setDisplayFormat(format);
    // A style may show the sections itself, as the Material style's time input does, and then follows the line edit.
    const auto repaint = [this] { update(); };
    QObject::connect(lineEdit(), &QLineEdit::textChanged, this, repaint);
    QObject::connect(lineEdit(), &QLineEdit::cursorPositionChanged, this, repaint);
    QObject::connect(lineEdit(), &QLineEdit::selectionChanged, this, repaint);
    QObject::connect(lineEdit(), &QLineEdit::textEdited, this, [this] { advanceSection(); });
}

/*!
    \fn QDateTime QDateTimeEdit::dateTime() const

    Returns the date and the time.

    \sa setDateTime()
*/

/*!
    \fn QDate QDateTimeEdit::date() const

    Returns the date.

    \sa setDate()
*/

/*!
    \fn QTime QDateTimeEdit::time() const

    Returns the time.

    \sa setTime()
*/

/*!
    Sets the date and the time to \a dateTime, brought into the range. Emits dateTimeChanged(), and dateChanged() or
    timeChanged() for the part that changes.
*/
void QDateTimeEdit::setDateTime(const QDateTime &dateTime) {
    if (dateTime.isValid())
        changeValue(clamped(dateTime), true);
}

/*!
    Sets the date to \a date, keeping the time.
*/
void QDateTimeEdit::setDate(QDate date) {
    if (date.isValid())
        setDateTime(QDateTime(date, m_value.time()));
}

/*!
    Sets the time to \a time, keeping the date.
*/
void QDateTimeEdit::setTime(QTime time) {
    if (time.isValid())
        setDateTime(QDateTime(m_value.date(), time));
}

/*!
    \fn QDateTime QDateTimeEdit::minimumDateTime() const

    Returns the earliest date and time, 1752-09-14 00:00 by default.
*/

/*!
    Sets the earliest date and time back to 1752-09-14 00:00.
*/
void QDateTimeEdit::clearMinimumDateTime() { setMinimumDateTime(defaultMinimum()); }

/*!
    Sets the earliest date and time to \a dt. The latest grows to it if it was earlier.
*/
void QDateTimeEdit::setMinimumDateTime(const QDateTime &dt) {
    if (dt.isValid())
        setDateTimeRange(dt, std::max(dt, m_maximum));
}

/*!
    \fn QDateTime QDateTimeEdit::maximumDateTime() const

    Returns the latest date and time, 9999-12-31 23:59:59.999 by default.
*/

/*!
    Sets the latest date and time back to 9999-12-31 23:59:59.999.
*/
void QDateTimeEdit::clearMaximumDateTime() { setMaximumDateTime(defaultMaximum()); }

/*!
    Sets the latest date and time to \a dt. The earliest falls to it if it was later.
*/
void QDateTimeEdit::setMaximumDateTime(const QDateTime &dt) {
    if (dt.isValid())
        setDateTimeRange(std::min(m_minimum, dt), dt);
}

/*!
    Sets the range to \a min to \a max, and brings the value into it.
*/
void QDateTimeEdit::setDateTimeRange(const QDateTime &min, const QDateTime &max) {
    if (!min.isValid() || !max.isValid())
        return;
    m_minimum = min;
    m_maximum = std::max(min, max);
    changeValue(clamped(m_value), true);
    if (m_calendar)
        m_calendar->setDateRange(m_minimum.date(), m_maximum.date());
}

/*!
    \fn QDate QDateTimeEdit::minimumDate() const

    Returns the date of minimumDateTime().
*/

/*!
    Sets the earliest date to \a min, at the start of the day.
*/
void QDateTimeEdit::setMinimumDate(QDate min) {
    if (min.isValid())
        setMinimumDateTime(QDateTime(min, QTime(0, 0)));
}

/*!
    Sets the earliest date back to 1752-09-14.
*/
void QDateTimeEdit::clearMinimumDate() { setMinimumDate(defaultMinimum().date()); }

/*!
    \fn QDate QDateTimeEdit::maximumDate() const

    Returns the date of maximumDateTime().
*/

/*!
    Sets the latest date to \a max, at the end of the day.
*/
void QDateTimeEdit::setMaximumDate(QDate max) {
    if (max.isValid())
        setMaximumDateTime(QDateTime(max, QTime(23, 59, 59, 999)));
}

/*!
    Sets the latest date back to 9999-12-31.
*/
void QDateTimeEdit::clearMaximumDate() { setMaximumDate(defaultMaximum().date()); }

/*!
    Sets the range of dates to \a min to \a max, from the start of the first day to the end of the last.
*/
void QDateTimeEdit::setDateRange(QDate min, QDate max) {
    if (min.isValid() && max.isValid())
        setDateTimeRange(QDateTime(min, QTime(0, 0)), QDateTime(max, QTime(23, 59, 59, 999)));
}

/*!
    \fn QTime QDateTimeEdit::minimumTime() const

    Returns the time of minimumDateTime().
*/

/*!
    Sets the time of the earliest date and time to \a min.
*/
void QDateTimeEdit::setMinimumTime(QTime min) {
    if (min.isValid())
        setMinimumDateTime(QDateTime(m_minimum.date(), min));
}

/*!
    Sets the time of the earliest date and time back to 00:00.
*/
void QDateTimeEdit::clearMinimumTime() { setMinimumTime(QTime(0, 0)); }

/*!
    \fn QTime QDateTimeEdit::maximumTime() const

    Returns the time of maximumDateTime().
*/

/*!
    Sets the time of the latest date and time to \a max.
*/
void QDateTimeEdit::setMaximumTime(QTime max) {
    if (max.isValid())
        setMaximumDateTime(QDateTime(m_maximum.date(), max));
}

/*!
    Sets the time of the latest date and time back to 23:59:59.999.
*/
void QDateTimeEdit::clearMaximumTime() { setMaximumTime(QTime(23, 59, 59, 999)); }

/*!
    Sets the range of times to \a min to \a max, on the dates of the range.
*/
void QDateTimeEdit::setTimeRange(QTime min, QTime max) {
    if (min.isValid() && max.isValid())
        setDateTimeRange(QDateTime(m_minimum.date(), min), QDateTime(m_maximum.date(), max));
}

/*!
    Returns the sections that the display format shows.
*/
QDateTimeEdit::Sections QDateTimeEdit::displayedSections() const {
    Sections sections;
    for (const Token &token : m_tokens)
        if (!token.literal)
            sections |= token.section;
    return sections;
}

/*!
    Returns the section that the cursor is in, which the keys and the buttons step.

    \sa setCurrentSection()
*/
QDateTimeEdit::Section QDateTimeEdit::currentSection() const { return sectionAt(currentSectionIndex()); }

/*!
    Returns the section at \a index in the text, or NoSection.
*/
QDateTimeEdit::Section QDateTimeEdit::sectionAt(int index) const {
    return index >= 0 && index < sectionCount() ? m_sections[static_cast<std::size_t>(index)].section : NoSection;
}

/*!
    Moves the cursor to the first \a section of the text, if the format has it.
*/
void QDateTimeEdit::setCurrentSection(Section section) {
    for (int i = 0; i < sectionCount(); ++i)
        if (m_sections[static_cast<std::size_t>(i)].section == section)
            setCurrentSectionIndex(i);
}

/*!
    Returns the index of the section that the cursor is in.
*/
int QDateTimeEdit::currentSectionIndex() const {
    return m_sections.empty() ? -1 : sectionIndexAt(lineEdit()->cursorPosition());
}

/*!
    Moves the cursor to the section at \a index.
*/
void QDateTimeEdit::setCurrentSectionIndex(int index) {
    if (index < 0 || index >= sectionCount())
        return;
    m_currentSection = index;
    lineEdit()->setCursorPosition(m_sections[static_cast<std::size_t>(index)].start);
}

/*!
    \fn int QDateTimeEdit::sectionCount() const

    Returns the number of sections of the text.
*/

/*!
    Selects the text of the first \a section, and moves the cursor there.
*/
void QDateTimeEdit::setSelectedSection(Section section) {
    for (const Span &span : m_sections) {
        if (span.section == section) {
            lineEdit()->setSelection(span.start, span.length);
            return;
        }
    }
}

/*!
    Returns the text of the first \a section, or an empty string if the format does not have it.
*/
QString QDateTimeEdit::sectionText(Section section) const {
    const QString text = lineEdit()->text();
    for (const Span &span : m_sections)
        if (span.section == section)
            return text.mid(span.start, span.length);
    return QString();
}

/*!
    \fn QString QDateTimeEdit::displayFormat() const

    Returns the format of the text.

    \sa setDisplayFormat()
*/

/*!
    Sets the format of the text to \a format, with the letters of QDate::toString() and QTime::toString().
*/
void QDateTimeEdit::setDisplayFormat(const QString &format) {
    m_format = format;
    m_tokens.clear();
    const auto addLiteral = [this](const QString &text) {
        if (!m_tokens.empty() && m_tokens.back().literal)
            m_tokens.back().pattern += text;
        else
            m_tokens.push_back({NoSection, text, true});
    };
    for (qsizetype i = 0; i < format.size();) {
        const QChar c = format.at(i);
        if (c == QLatin1Char('\'')) {
            // Quoted text, in which '' is a quote.
            QString literal;
            ++i;
            while (i < format.size()) {
                if (format.at(i) == QLatin1Char('\'')) {
                    if (i + 1 < format.size() && format.at(i + 1) == QLatin1Char('\'')) {
                        literal += QLatin1Char('\'');
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                literal += format.at(i++);
            }
            if (literal.isEmpty())
                literal = QStringLiteral("'");
            addLiteral(literal);
            continue;
        }
        if ((c == QLatin1Char('A') || c == QLatin1Char('a'))) {
            const bool pair =
                i + 1 < format.size() && (format.at(i + 1) == QLatin1Char('P') || format.at(i + 1) == QLatin1Char('p'));
            m_tokens.push_back({AmPmSection, pair ? format.mid(i, 2) : QString(c), false});
            i += pair ? 2 : 1;
            continue;
        }
        const char letter = c.toLatin1();
        Section section = NoSection;
        qsizetype longest = 2;
        switch (letter) {
        case 'd':
            section = DaySection;
            longest = 4;
            break;
        case 'M':
            section = MonthSection;
            longest = 4;
            break;
        case 'y':
            section = YearSection;
            longest = 4;
            break;
        case 'h':
        case 'H':
            section = HourSection;
            break;
        case 'm':
            section = MinuteSection;
            break;
        case 's':
            section = SecondSection;
            break;
        case 'z':
            section = MSecSection;
            longest = 3;
            break;
        default:
            break;
        }
        if (section == NoSection) {
            addLiteral(QString(c));
            ++i;
            continue;
        }
        qsizetype run = 1;
        while (i + run < format.size() && format.at(i + run) == c && run < longest)
            ++run;
        if (letter == 'y' && run != 4)
            run = std::min<qsizetype>(run, 2);
        if (letter == 'y' && run == 1) {
            addLiteral(QString(c)); // a single y is no year
            ++i;
            continue;
        }
        if (letter == 'z' && run == 2)
            run = 1;
        m_tokens.push_back({section, format.mid(i, run), false});
        i += run;
    }
    // A format of numbers alone asks an on-screen keyboard for its digits; AM, PM and the names of months and days
    // are typed in letters.
    const bool words = std::any_of(m_tokens.begin(), m_tokens.end(), [](const Token &token) {
        return !token.literal &&
               (token.section == AmPmSection ||
                ((token.section == MonthSection || token.section == DaySection) && token.pattern.size() > 2));
    });
    lineEdit()->setInputMethodHints(words ? Qt::ImhNone : Qt::ImhPreferNumbers);
    refreshText();
    updateGeometry();
    layoutTimeToggle();
}

/*!
    \fn bool QDateTimeEdit::calendarPopup() const

    Returns whether the buttons open a calendar. The default is \c false.
*/

/*!
    Sets whether the buttons open a calendar to pick the date from to \a enable.

    \sa calendarWidget()
*/
void QDateTimeEdit::setCalendarPopup(bool enable) {
    m_calendarPopup = enable;
    update();
}

/*!
    \fn QCalendar QDateTimeEdit::calendar() const

    Returns the calendar system of the editor, the Gregorian calendar by default.
*/

/*!
    Sets the calendar system of the editor to \a calendar, and of the popup if there is one.
*/
void QDateTimeEdit::setCalendar(QCalendar calendar) {
    m_cal = calendar;
    if (m_calendar)
        m_calendar->setCalendar(calendar);
}

/*!
    \fn QTimeZone QDateTimeEdit::timeZone() const

    Returns the time zone that was set; only kept.
*/

/*!
    \fn void QDateTimeEdit::setTimeZone(const QTimeZone &zone)

    Sets the time zone to \a zone; only kept, the editor does not convert times.
*/

/*!
    Returns the calendar of the popup, creating it if there is none yet. Under QMaterialStyle the calendar that it
    creates is Material's docked date picker (the Nt-only property \c materialDatePicker is \c "docked"); a calendar
    given to setCalendarWidget() keeps its own.

    \sa setCalendarWidget()
*/
QCalendarWidget *QDateTimeEdit::calendarWidget() const {
    if (!m_calendar) {
        auto *self = const_cast<QDateTimeEdit *>(this);
        self->setCalendarWidget(new CalendarPopup(self));
    }
    return m_calendar;
}

/*!
    Makes \a calendarWidget the calendar of the popup. The edit takes it and deletes the one it had.
*/
void QDateTimeEdit::setCalendarWidget(QCalendarWidget *calendarWidget) {
    if (!calendarWidget || calendarWidget == m_calendar)
        return;
    delete m_calendar.data();
    m_calendar = calendarWidget;
    calendarWidget->hide();
    calendarWidget->setDateRange(m_minimum.date(), m_maximum.date());
    QObject::connect(calendarWidget, &QCalendarWidget::clicked, this, [this](QDate date) {
        setDate(date);
        if (m_calendar)
            m_calendar->hide();
    });
    QObject::connect(calendarWidget, &QCalendarWidget::activated, this, [this](QDate date) {
        setDate(date);
        if (m_calendar)
            m_calendar->hide();
    });
}

void QDateTimeEdit::showPopup() {
    QCalendarWidget *calendar = calendarWidget();
    QWidget *top = window();
    if (calendar->parentWidget() != top)
        calendar->setParent(top);
    calendar->setSelectedDate(m_value.date());
    // The calendar knows what opened it before it shows, so the style can draw it as a popup.
    QtPrivate::WidgetAccess::data(calendar).popupAnchor = this;
    const QSize size = calendar->sizeHint();
    // Below the field, or above it where there is no room below, inside the part of the window that the style keeps
    // menus in (8 from its edges under QMaterialStyle) along a side where the calendar fits in it.
    const QMargins margins = QMaterialStyle::menuWindowMargins(calendar);
    QRect area = top->rect();
    if (size.width() <= area.width() - margins.left() - margins.right())
        area.adjust(margins.left(), 0, -margins.right(), 0);
    if (size.height() <= area.height() - margins.top() - margins.bottom())
        area.adjust(0, margins.top(), 0, -margins.bottom());
    QPoint at = mapTo(top, QPoint(0, height()));
    if (at.y() + size.height() > area.bottom() + 1)
        at.setY(std::max(area.top(), mapTo(top, QPoint(0, 0)).y() - size.height()));
    at.setX(std::clamp(at.x(), area.left(), std::max(area.left(), area.right() + 1 - size.width())));
    calendar->setGeometry(QRect(at, size));
    calendar->raise();
    calendar->show();
    calendar->setFocus();
    QtPrivate::WidgetAccess::setPopup(calendar, calendar);
}

/*!
    \reimp

    Clears the text; the value does not change until the user types a new one.
*/
void QDateTimeEdit::clear() { lineEdit()->clear(); }

/*!
    \reimp

    Steps the section that the cursor is in by \a steps, and selects it.
*/
void QDateTimeEdit::stepBy(int steps) {
    const int index = std::max(0, currentSectionIndex());
    const Section section = sectionAt(index);
    if (section == NoSection)
        return;
    QDate date = m_value.date();
    QTime time = m_value.time();
    const auto stepped = [this, steps](int value, int low, int high) {
        const int count = high - low + 1;
        int next = value + steps;
        if (wrapping())
            next = low + (((next - low) % count) + count) % count;
        return std::clamp(next, low, high);
    };
    switch (section) {
    case YearSection:
        date = date.addYears(steps);
        break;
    case MonthSection: {
        const int month = stepped(date.month(), 1, 12);
        date = QDate(date.year(), month, std::min(date.day(), QDate(date.year(), month, 1).daysInMonth()));
        break;
    }
    case DaySection:
        date = QDate(date.year(), date.month(), stepped(date.day(), 1, date.daysInMonth()));
        break;
    case HourSection:
        time = QTime(stepped(time.hour(), 0, 23), time.minute(), time.second(), time.msec());
        break;
    case MinuteSection:
        time = QTime(time.hour(), stepped(time.minute(), 0, 59), time.second(), time.msec());
        break;
    case SecondSection:
        time = QTime(time.hour(), time.minute(), stepped(time.second(), 0, 59), time.msec());
        break;
    case MSecSection:
        time = QTime(time.hour(), time.minute(), time.second(), stepped(time.msec(), 0, 999));
        break;
    case AmPmSection:
        if (steps % 2 != 0) {
            const int hour = time.hour() < 12 ? time.hour() + 12 : time.hour() - 12;
            time = QTime(hour, time.minute(), time.second(), time.msec());
        }
        break;
    default:
        break;
    }
    if (!date.isValid() || !time.isValid())
        return;
    changeValue(clamped(QDateTime(date, time)), true);
    const Span span = m_sections[static_cast<std::size_t>(std::min(index, sectionCount() - 1))];
    lineEdit()->setSelection(span.start, span.length);
}

/*!
    Returns the date and time that \a text shows in the display format, or an invalid QDateTime. Sections that the
    format does not have keep the value of the edit.

    \sa textFromDateTime()
*/
QDateTime QDateTimeEdit::dateTimeFromText(const QString &text) const {
    QDateTime result;
    return parse(text, &result) ? result : QDateTime();
}

/*!
    Returns the text that shows \a dt in the display format.

    \sa dateTimeFromText()
*/
QString QDateTimeEdit::textFromDateTime(const QDateTime &dt) const { return format(dt, nullptr); }

/*!
    \reimp
*/
QAbstractSpinBox::StepEnabled QDateTimeEdit::stepEnabled() const {
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

/*!
    \reimp

    With calendarPopup(), the buttons open the calendar.
*/
void QDateTimeEdit::mousePressEvent(QMouseEvent *event) {
    // In the Material style's time input, a click chooses a box; in the choice of AM or PM, a half.
    if (qobject_cast<const QMaterialStyle *>(style()) && QMaterialStyle::isTimeInput(this) &&
        event->button() == Qt::LeftButton) {
        const std::vector<QRect> rects = QMaterialStyle::timeInputRects(this);
        // On the dial of the time picker, the number under the press is chosen, and a drag goes on choosing.
        if (QMaterialStyle::isTimeDial(this) && QMaterialStyle::onTimeDial(this, event->pos())) {
            const Section shown = QMaterialStyle::timeDialHours(this) ? HourSection : currentSection();
            lineEdit()->setFocus();
            if (!isReadOnly()) {
                m_dialHeld = true;
                m_dialDragged = false;
                setTime(QMaterialStyle::timeDialTime(this, event->pos()));
            }
            setSelectedSection(shown);
            update();
            event->accept();
            return;
        }
        lineEdit()->setFocus();
        for (int i = 0; i < static_cast<int>(rects.size()); ++i) {
            const QRect &box = rects[static_cast<std::size_t>(i)];
            if (!box.contains(event->pos()))
                continue;
            const Section section = sectionAt(i);
            const bool after = QMaterialStyle::timePeriodAfter(box, event->pos());
            setSelectedSection(section);
            if (section == AmPmSection && !isReadOnly() && (time().hour() >= 12) != after)
                stepBy(after ? 1 : -1);
        }
        update();
        event->accept();
        return;
    }
    if (m_calendarPopup && event->button() == Qt::LeftButton && !isReadOnly()) {
        QStyleOptionSpinBox option;
        option.initFrom(this);
        if (style()->subElementRect(QStyle::SE_SpinBoxUp, &option, this).contains(event->pos()) ||
            style()->subElementRect(QStyle::SE_SpinBoxDown, &option, this).contains(event->pos())) {
            showPopup();
            event->accept();
            return;
        }
    }
    QAbstractSpinBox::mousePressEvent(event);
}

/*!
    \reimp
*/
void QDateTimeEdit::mouseMoveEvent(QMouseEvent *event) {
    if (m_dialHeld) {
        m_dialDragged = true;
        const Section shown = QMaterialStyle::timeDialHours(this) ? HourSection : currentSection();
        setTime(QMaterialStyle::timeDialTime(this, event->pos()));
        setSelectedSection(shown);
        update();
        event->accept();
        return;
    }
    QAbstractSpinBox::mouseMoveEvent(event);
}

/*!
    \reimp
*/
void QDateTimeEdit::mouseReleaseEvent(QMouseEvent *event) {
    // An hour chosen on the dial of the Material style's time picker goes on to the minutes, as Material's does.
    if (m_dialHeld) {
        m_dialHeld = false;
        m_dialDragged = false;
        if (QMaterialStyle::timeDialHours(this))
            setSelectedSection(MinuteSection);
        update();
        event->accept();
        return;
    }
    QAbstractSpinBox::mouseReleaseEvent(event);
}

/*!
    \reimp

    Up, Down, Page Up and Page Down step the section that the cursor is in and keep it selected. With calendarPopup(),
    F4 and Alt+Down open the calendar.
*/
void QDateTimeEdit::keyPressEvent(QKeyEvent *event) {
    if (m_calendarPopup &&
        (event->key() == Qt::Key_F4 || (event->key() == Qt::Key_Down && (event->modifiers() & Qt::AltModifier)))) {
        showPopup();
        event->accept();
        return;
    }
    int steps = 0;
    switch (event->key()) {
    case Qt::Key_Up:
        steps = 1;
        break;
    case Qt::Key_Down:
        steps = -1;
        break;
    case Qt::Key_PageUp:
        steps = 10;
        break;
    case Qt::Key_PageDown:
        steps = -10;
        break;
    default:
        QAbstractSpinBox::keyPressEvent(event);
        return;
    }
    if (stepEnabled().testFlag(steps > 0 ? StepUpEnabled : StepDownEnabled))
        stepBy(steps);
    event->accept();
}

/*!
    \reimp
*/
void QDateTimeEdit::resizeEvent(QResizeEvent *event) {
    QAbstractSpinBox::resizeEvent(event);
    layoutTimeToggle();
}

/*!
    \reimp
*/
void QDateTimeEdit::changeEvent(QEvent *event) {
    QAbstractSpinBox::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange)
        layoutTimeToggle();
}

// The Material style's time picker has an icon button at its bottom start that switches between the dial and the
// input. It is made the first time that the style wants one, after the line edit, so that Tab comes to it from the
// keys, and goes where the style puts it.
void QDateTimeEdit::layoutTimeToggle() {
    const QRect rect = qobject_cast<const QMaterialStyle *>(style()) ? QMaterialStyle::timeToggleRect(this) : QRect();
    if (!rect.isValid()) {
        if (m_timeToggle)
            m_timeToggle->hide();
        return;
    }
    if (!m_timeToggle) {
        m_timeToggle = new QtPrivate::PaintedIconButton(this, [this](QPainter *painter) {
            if (const auto *material = qobject_cast<const QMaterialStyle *>(style()))
                material->drawTimeToggleIcon(painter, m_timeToggle->rect(), m_timeToggle,
                                             QMaterialStyle::isTimeDial(this));
        });
        QObject::connect(m_timeToggle, &QToolButton::clicked, this, [this] { toggleTimeForm(); });
    }
    m_timeToggle->setAccessibleName(QMaterialStyle::isTimeDial(this)
                                        ? QMaterialStyle::tr("Switch to text input mode for the time input")
                                        : QMaterialStyle::tr("Switch to clock mode for the time input"));
    m_timeToggle->setGeometry(rect);
    m_timeToggle->show();
    m_timeToggle->update();
}

// Switches the time picker from the form that materialTimeInput asks for to the other one, or back. The picker is of
// another size, which a layout is told of.
void QDateTimeEdit::toggleTimeForm() {
    m_timeToggledFrom = QMaterialStyle::timeFormToggled(this) ? QString() : property("materialTimeInput").toString();
    m_dialHeld = false;
    m_dialDragged = false;
    updateGeometry();
    layoutEdit();
    layoutTimeToggle();
    update();
}

// The text of `dt` in the display format, with the span of each section.
QString QDateTimeEdit::format(const QDateTime &dt, std::vector<Span> *spans) const {
    if (spans)
        spans->clear();
    QString text;
    const QDate date = dt.date();
    const QTime time = dt.time();
    const bool twelveHours = displayedSections().testFlag(AmPmSection);
    for (const Token &token : m_tokens) {
        if (token.literal) {
            text += token.pattern;
            continue;
        }
        const int start = static_cast<int>(text.size());
        const qsizetype n = token.pattern.size();
        switch (token.section) {
        case DaySection:
            if (n <= 2)
                text += padded(date.day(), static_cast<int>(n));
            else
                text += QString::fromUtf8(n == 3 ? kDayShort[date.dayOfWeek() - 1] : kDayLong[date.dayOfWeek() - 1]);
            break;
        case MonthSection:
            if (n <= 2)
                text += padded(date.month(), static_cast<int>(n));
            else
                text += QString::fromUtf8(n == 3 ? kMonthShort[date.month() - 1] : kMonthLong[date.month() - 1]);
            break;
        case YearSection:
            text += n == 4 ? padded(date.year(), 4) : padded(date.year() % 100, 2);
            break;
        case HourSection: {
            int hour = time.hour();
            if (token.pattern.at(0) == QLatin1Char('h') && twelveHours)
                hour = hour % 12 == 0 ? 12 : hour % 12;
            text += padded(hour, static_cast<int>(n));
            break;
        }
        case MinuteSection:
            text += padded(time.minute(), static_cast<int>(n));
            break;
        case SecondSection:
            text += padded(time.second(), static_cast<int>(n));
            break;
        case MSecSection:
            text += padded(time.msec(), n == 3 ? 3 : 1);
            break;
        case AmPmSection: {
            const bool upper = token.pattern.at(0) == QLatin1Char('A');
            const bool pm = time.hour() >= 12;
            text += upper ? QString::fromLatin1(pm ? "PM" : "AM") : QString::fromLatin1(pm ? "pm" : "am");
            break;
        }
        default:
            break;
        }
        if (spans)
            spans->push_back({token.section, start, static_cast<int>(text.size()) - start});
    }
    return text;
}

// Reads `text` in the display format; the sections that it lacks come from the value.
bool QDateTimeEdit::parse(const QString &text, QDateTime *result) const {
    int year = m_value.date().year();
    int month = m_value.date().month();
    int day = m_value.date().day();
    int hour = m_value.time().hour();
    int minute = m_value.time().minute();
    int second = m_value.time().second();
    int msec = m_value.time().msec();
    int pm = -1;
    qsizetype pos = 0;
    const auto readNumber = [&](qsizetype maxDigits, int *value) {
        qsizetype digits = 0;
        int number = 0;
        while (digits < maxDigits && pos < text.size() && text.at(pos).isDigit()) {
            number = number * 10 + text.at(pos).digitValue();
            ++pos;
            ++digits;
        }
        *value = number;
        return digits > 0;
    };
    const auto readName = [&](const char *const *names, int count, int *index) {
        int best = -1;
        qsizetype bestLength = 0;
        for (int i = 0; i < count; ++i) {
            const QString name = QString::fromUtf8(names[i]);
            if (text.mid(pos, name.size()).compare(name, Qt::CaseInsensitive) == 0 && name.size() > bestLength) {
                best = i;
                bestLength = name.size();
            }
        }
        if (best < 0)
            return false;
        pos += bestLength;
        *index = best;
        return true;
    };
    for (const Token &token : m_tokens) {
        if (token.literal) {
            if (text.mid(pos, token.pattern.size()) != token.pattern)
                return false;
            pos += token.pattern.size();
            continue;
        }
        const qsizetype n = token.pattern.size();
        int value = 0;
        switch (token.section) {
        case DaySection:
            if (n <= 2) {
                if (!readNumber(2, &day))
                    return false;
            } else if (!readName(n == 3 ? kDayShort : kDayLong, 7, &value)) {
                return false;
            }
            break;
        case MonthSection:
            if (n <= 2) {
                if (!readNumber(2, &month))
                    return false;
            } else {
                if (!readName(n == 3 ? kMonthShort : kMonthLong, 12, &value))
                    return false;
                month = value + 1;
            }
            break;
        case YearSection:
            if (!readNumber(n == 4 ? 4 : 2, &value))
                return false;
            year = n == 4 ? value : (m_value.date().year() / 100) * 100 + value;
            break;
        case HourSection:
            if (!readNumber(2, &hour))
                return false;
            break;
        case MinuteSection:
            if (!readNumber(2, &minute))
                return false;
            break;
        case SecondSection:
            if (!readNumber(2, &second))
                return false;
            break;
        case MSecSection:
            if (!readNumber(3, &msec))
                return false;
            break;
        case AmPmSection: {
            const QString two = text.mid(pos, 2).toLower();
            if (two == QStringLiteral("am"))
                pm = 0;
            else if (two == QStringLiteral("pm"))
                pm = 1;
            else
                return false;
            pos += 2;
            break;
        }
        default:
            break;
        }
    }
    if (pos != text.size())
        return false;
    if (pm >= 0) {
        if (hour < 1 || hour > 12)
            return false;
        hour = hour % 12 + (pm ? 12 : 0);
    }
    const QDate date(year, month, day);
    const QTime time(hour, minute, second, msec);
    if (!date.isValid() || !time.isValid())
        return false;
    *result = QDateTime(date, time);
    return true;
}

QDateTime QDateTimeEdit::clamped(const QDateTime &value) const { return std::clamp(value, m_minimum, m_maximum); }

// As in Qt, a number typed into a section that fills it, with as many digits as the section has or with a first digit
// that no second one could follow, moves on to the next section, which is selected so that it is typed over. The text
// is written again in the display format, so that a single digit gets its zero.
void QDateTimeEdit::advanceSection() {
    QLineEdit *edit = lineEdit();
    const QString text = edit->text();
    const int cursor = edit->cursorPosition();
    if (edit->hasSelectedText() || cursor <= 0 || cursor > text.size() || !text.at(cursor - 1).isDigit())
        return;
    const bool twelveHours = displayedSections().testFlag(AmPmSection);
    qsizetype pos = 0;
    int index = 0; // of the section that the walk is in
    int filled = -1;
    for (const Token &token : m_tokens) {
        if (token.literal) {
            if (text.mid(pos, token.pattern.size()) != token.pattern)
                return;
            pos += token.pattern.size();
            continue;
        }
        const qsizetype start = pos;
        const qsizetype n = token.pattern.size();
        const bool named =
            token.section == AmPmSection || ((token.section == DaySection || token.section == MonthSection) && n > 2);
        if (named) {
            while (pos < text.size() && text.at(pos).isLetter())
                ++pos;
        } else {
            const qsizetype width = token.section == MSecSection ? 3 : token.section == YearSection && n == 4 ? 4 : 2;
            int first = 0;
            while (pos < text.size() && pos - start < width && text.at(pos).isDigit()) {
                if (pos == start)
                    first = text.at(pos).digitValue();
                ++pos;
            }
            if (pos > start && pos == cursor) {
                int largest = 0; // that the section can hold
                switch (token.section) {
                case DaySection:
                    largest = 31;
                    break;
                case MonthSection:
                    largest = 12;
                    break;
                case HourSection:
                    largest = twelveHours ? 12 : 23;
                    break;
                case MinuteSection:
                case SecondSection:
                    largest = 59;
                    break;
                default:
                    largest = 999;
                    break;
                }
                if (pos - start == width || (pos - start == 1 && width == 2 && first * 10 > largest))
                    filled = index;
                break;
            }
        }
        if (pos == start)
            return;
        ++index;
    }
    if (filled < 0)
        return;
    QDateTime value;
    if (!parse(text, &value))
        return;
    std::vector<Span> spans;
    const QString written = format(value, &spans);
    const int following = filled + 1;
    if (following >= static_cast<int>(spans.size()))
        return;
    m_sections = spans;
    if (written != text)
        edit->setText(written);
    const Span &next = m_sections[static_cast<std::size_t>(following)];
    m_currentSection = following;
    edit->setSelection(next.start, next.length);
}

// The sections of `text` as the user has typed it, which may have fewer digits than the display format writes: a
// number takes the digits that its section can hold, a name the letters. False when the text does not follow the
// format.
bool QDateTimeEdit::textSpans(const QString &text, std::vector<Span> *spans) const {
    spans->clear();
    qsizetype pos = 0;
    for (const Token &token : m_tokens) {
        if (token.literal) {
            if (text.mid(pos, token.pattern.size()) != token.pattern)
                return false;
            pos += token.pattern.size();
            continue;
        }
        const qsizetype start = pos;
        const qsizetype n = token.pattern.size();
        const bool named =
            token.section == AmPmSection || ((token.section == DaySection || token.section == MonthSection) && n > 2);
        if (named) {
            while (pos < text.size() && text.at(pos).isLetter())
                ++pos;
        } else {
            const qsizetype width = token.section == MSecSection ? 3 : token.section == YearSection && n == 4 ? 4 : 2;
            while (pos < text.size() && pos - start < width && text.at(pos).isDigit())
                ++pos;
        }
        if (pos == start)
            return false;
        spans->push_back({token.section, static_cast<int>(start), static_cast<int>(pos - start)});
    }
    return pos == text.size();
}

// As in Qt, the separator that follows a section, typed in that section, moves on to the next one, which is selected,
// and writes the section as the format does ("3:" puts "03"); and 'a' or 'p' typed in the AM/PM section chooses it
// and moves on if a section follows.
bool QDateTimeEdit::typeCharacter(QChar c) {
    QLineEdit *edit = lineEdit();
    const QString text = edit->text();
    std::vector<Span> spans;
    if (!textSpans(text, &spans))
        return false;
    const int position = edit->hasSelectedText() ? edit->selectionStart() : edit->cursorPosition();
    int index = -1; // the section that the cursor is in, or at the end of
    for (int i = 0; i < static_cast<int>(spans.size()); ++i) {
        const Span &span = spans[static_cast<std::size_t>(i)];
        if (position >= span.start && position <= span.start + span.length) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return false;
    const auto selectSection = [this, edit](int at) {
        const Span &span = m_sections[static_cast<std::size_t>(at)];
        m_currentSection = at;
        edit->setSelection(span.start, span.length);
    };
    const int last = static_cast<int>(spans.size()) - 1;
    if (spans[static_cast<std::size_t>(index)].section == AmPmSection && c.isLetter()) {
        const QChar letter = c.toLower();
        if (letter != QLatin1Char('a') && letter != QLatin1Char('p')) {
            // Another letter is refused where AM or PM is written in full, and typed where it is being written.
            const Span &span = spans[static_cast<std::size_t>(index)];
            const QString written = text.mid(span.start, span.length).toLower();
            return written == QLatin1String("am") || written == QLatin1String("pm");
        }
        QDateTime value;
        if (!parse(text, &value))
            value = m_value;
        QTime time = value.time();
        const bool pm = letter == QLatin1Char('p');
        if ((time.hour() >= 12) != pm)
            time = QTime(pm ? time.hour() + 12 : time.hour() - 12, time.minute(), time.second(), time.msec());
        const QDateTime chosen(value.date(), time);
        changeValue(chosen >= m_minimum && chosen <= m_maximum ? chosen : m_value, true);
        selectSection(std::min(index + 1, last));
        return true;
    }
    if (index == last)
        return false;
    // The literal between this section and the next one.
    int seen = -1;
    const Token *following = nullptr;
    for (std::size_t i = 0; i < m_tokens.size(); ++i) {
        if (m_tokens[i].literal)
            continue;
        if (++seen == index) {
            if (i + 1 < m_tokens.size() && m_tokens[i + 1].literal)
                following = &m_tokens[i + 1];
            break;
        }
    }
    if (!following || !following->pattern.startsWith(c))
        return false;
    QDateTime value;
    if (!parse(text, &value))
        return true; // the separator would not make it any better
    if (value >= m_minimum && value <= m_maximum)
        changeValue(value, false);
    std::vector<Span> written;
    const QString rewritten = format(value, &written);
    m_sections = written;
    if (rewritten != text)
        edit->setText(rewritten);
    selectSection(index + 1);
    return true;
}

int QDateTimeEdit::sectionIndexAt(int position) const {
    int index = 0;
    for (int i = 0; i < sectionCount(); ++i)
        if (m_sections[static_cast<std::size_t>(i)].start <= position)
            index = i;
    return index;
}

void QDateTimeEdit::changeValue(const QDateTime &value, bool rewrite) {
    const QDateTime old = m_value;
    m_value = value;
    if (rewrite)
        refreshText();
    else
        update();
    if (old == value)
        return;
    emit dateTimeChanged(value);
    if (old.date() != value.date())
        emit dateChanged(value.date());
    if (old.time() != value.time())
        emit timeChanged(value.time());
}

QString QDateTimeEdit::valueText() const {
    if (!m_special.isEmpty() && m_value == m_minimum)
        return m_special;
    return format(m_value, &m_sections);
}

QStringList QDateTimeEdit::sizeTexts() const {
    const QDateTime wide(QDate(2000, 9, 27), QTime(23, 58, 58, 888));
    return {format(wide, nullptr), format(m_minimum, nullptr), format(m_maximum, nullptr)};
}

void QDateTimeEdit::takeText(const QString &number, bool atMinimum, bool finished) {
    if (atMinimum) {
        changeValue(m_minimum, finished);
        return;
    }
    const QDateTime value = dateTimeFromText(number);
    if (finished) {
        changeValue(value.isValid() ? clamped(value) : m_value, true);
    } else if (value.isValid() && value >= m_minimum && value <= m_maximum) {
        changeValue(value, false); // leave the text as the user types it
    }
}

bool QDateTimeEdit::acceptsCharacter(QChar c) const {
    if (c.isDigit())
        return true;
    for (const Token &token : m_tokens) {
        if (token.literal && token.pattern.contains(c))
            return true;
        if (!token.literal && c.isLetter() &&
            (token.section == AmPmSection ||
             ((token.section == DaySection || token.section == MonthSection) && token.pattern.size() > 2)))
            return true;
    }
    return false;
}

/*!
    \class QDateEdit
    \brief The QDateEdit class lets the user edit a date.

    A QDateEdit is a QDateTimeEdit with the format \c "yyyy-MM-dd":

    \code
    auto *birthday = new QDateEdit(QDate(1990, 1, 1), form);
    birthday->setCalendarPopup(true);
    \endcode

    A QDateTimeEdit, a date edit with the keyboard focus, a QTimeEdit, and a disabled date edit.

    \image widgets/qdatetimeedit.png QDateTimeEdit in the light color scheme

    \differences
    See QDateTimeEdit.
    \enddifferences

    \sa QTimeEdit, QCalendarWidget
*/

/*!
    Constructs a date edit with the given \a parent, at 2000-01-01.
*/
QDateEdit::QDateEdit(QWidget *parent) : QDateEdit(QDate(2000, 1, 1), parent) {}

/*!
    Constructs a date edit with the given \a date and \a parent.
*/
QDateEdit::QDateEdit(QDate date, QWidget *parent) : QDateTimeEdit(date, parent) {
    QObject::connect(this, &QDateTimeEdit::dateChanged, this, [this](QDate d) { emit userDateChanged(d); });
}

/*!
    Destroys the date edit.
*/
QDateEdit::~QDateEdit() = default;

/*!
    \class QTimeEdit
    \brief The QTimeEdit class lets the user edit a time.

    A QTimeEdit is a QDateTimeEdit with the format \c "HH:mm":

    \code
    auto *alarm = new QTimeEdit(QTime(7, 30), form);
    alarm->setDisplayFormat("HH:mm:ss");
    \endcode

    A QDateTimeEdit, a QDateEdit with the keyboard focus, a time edit, and a disabled QDateEdit.

    \image widgets/qdatetimeedit.png QDateTimeEdit in the light color scheme

    \differences
    See QDateTimeEdit.
    \enddifferences

    \sa QDateEdit
*/

/*!
    Constructs a time edit with the given \a parent, at 00:00.
*/
QTimeEdit::QTimeEdit(QWidget *parent) : QTimeEdit(QTime(0, 0), parent) {}

/*!
    Constructs a time edit with the given \a time and \a parent.
*/
QTimeEdit::QTimeEdit(QTime time, QWidget *parent) : QDateTimeEdit(time, parent) {
    QObject::connect(this, &QDateTimeEdit::timeChanged, this, [this](QTime t) { emit userTimeChanged(t); });
}

/*!
    Destroys the time edit.
*/
QTimeEdit::~QTimeEdit() = default;

#include "moc_qdatetimeedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
