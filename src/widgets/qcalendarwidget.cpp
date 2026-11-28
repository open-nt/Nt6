// MIT License - see LICENSE
#include <QtWidgets/qcalendarwidget.h>

#include "qpaintediconbutton_p.h"

#include <QtGui/qevent.h>
#include <QtCore/qlocale.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qtoolbutton.h>

#include <algorithm>
#include <functional>

namespace {

constexpr int kRows = 6;
constexpr int kColumns = 7;
constexpr int kNavigationMargin = 2;
constexpr int kYearColumns = 3; // of Material's grid of years,
constexpr int kYearRow = 52;    // a pill of 36 and 16 between two
constexpr int kMonthRow = 48;   // the rows of the menus of months and years of the docked picker
constexpr int kChoiceTop = 8;   // above the first row of either

const QMaterialStyle *materialStyle(const QWidget *widget) {
    return qobject_cast<const QMaterialStyle *>(widget->style());
}

// The font with the properties that the format sets.
QFont formatted(QFont font, const QTextCharFormat &format) {
    if (format.hasProperty(QTextFormat::FontWeight))
        font.setBold(format.fontWeight() > QFont::Medium);
    if (format.hasProperty(QTextFormat::FontItalic))
        font.setItalic(format.fontItalic());
    if (format.hasProperty(QTextFormat::FontUnderline))
        font.setUnderline(format.fontUnderline());
    if (format.hasProperty(QTextFormat::FontStrikeOut))
        font.setStrikeOut(format.fontStrikeOut());
    return font;
}

} // namespace

/*!
    \class QCalendarWidget
    \brief The QCalendarWidget class shows a month and lets the user pick a date.

    A calendar shows the days of one month in a grid of weeks, with the days before and after it in gray, and a bar
    above it with the month and the year and buttons for the previous and the next month:

    \code
    auto *calendar = new QCalendarWidget(window);
    calendar->setDateRange(QDate::currentDate(), QDate::currentDate().addYears(1));
    connect(calendar, &QCalendarWidget::clicked, [](QDate date) { book(date); });
    \endcode

    The user clicks a day, turns the mouse wheel to change the month, or uses the arrow keys (a day or a week), Page Up
    and Page Down (a month), Home and End (the first and last day of the month) and Return. Dates outside the range
    cannot be selected.

    A calendar showing November 2026, with the 13th selected and the week numbers at the side.

    \image widgets/qcalendarwidget.png QCalendarWidget in the light color scheme

    \differences
    The names of the months and the days are those of locale(), but the first day of the week is Monday whatever the
    locale; \c setFirstDayOfWeek() changes it. The month and year menus of the navigation bar are missing (under
    QMaterialStyle a click on the month and the year shows a grid of years, as Material's date picker does, and with
    the Nt-only property \c materialDatePicker set to \c "docked" the month and the year each have their arrows and
    open a menu of the months or of the years, one to a row, in the place of the days), and \c dateEditEnabled() is stored but has no
    effect. Under QMaterialStyle, a calendar with the Nt-only property
    \c materialDatePicker set to \c "modal" has Material's headline, whose pencil button switches to a text field
    where the date is typed in the short format of the locale; a valid date in the range selects it at once, and Return
    in the field emits activated(). In that mode the size hint is only as high as the headline and the field. The
    \c calendar() is stored but dates are always shown in the Gregorian calendar.
    \enddifferences

    \sa QDateEdit, QDate
*/

/*!
    \enum QCalendarWidget::SelectionMode

    Whether the user can select a date.

    \value NoSelection The selected date cannot be changed by the user.
    \value SingleSelection The user can select one date.
*/

/*!
    \enum QCalendarWidget::HorizontalHeaderFormat

    What is shown above the days.

    \value SingleLetterDayNames The first letter of the name of the day.
    \value ShortDayNames The first three letters of the name of the day.
    \value LongDayNames The whole name of the day.
    \value NoHorizontalHeader Nothing.
*/

/*!
    \enum QCalendarWidget::VerticalHeaderFormat

    What is shown at the left of the weeks.

    \value NoVerticalHeader Nothing.
    \value ISOWeekNumbers The ISO 8601 numbers of the weeks.
*/

/*!
    Constructs a calendar with the given \a parent. Today is selected and its month is shown; the range is from
    1752-09-14 to 9999-12-31, as in Qt.
*/
QCalendarWidget::QCalendarWidget(QWidget *parent) : QWidget(parent), m_minimum(1752, 9, 14), m_maximum(9999, 12, 31) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // the year under the pointer has a state layer
    // Before the navigation bar, so that Tab comes to it first, as it reads.
    m_modeButton = new QtPrivate::PaintedIconButton(this, [this](QPainter *painter) {
        if (const QMaterialStyle *material = materialStyle(this))
            material->drawDatePickerModeIcon(painter, m_modeButton->rect(), m_modeButton, m_inputMode);
    });
    m_modeButton->setAccessibleName(QMaterialStyle::tr("Switch to text input mode"));
    m_modeButton->hide();
    QObject::connect(m_modeButton, &QToolButton::clicked, this, [this] { setInputMode(!m_inputMode); });
    m_navigation = new QWidget(this);
    m_navigation->setAutoFillBackground(true);
    auto *row = new QHBoxLayout(m_navigation);
    row->setContentsMargins(kNavigationMargin, kNavigationMargin, kNavigationMargin, kNavigationMargin);
    m_previous = new QToolButton(m_navigation);
    m_previous->setArrowType(Qt::LeftArrow);
    m_previous->setAutoRaise(true);
    m_next = new QToolButton(m_navigation);
    m_next->setArrowType(Qt::RightArrow);
    m_next->setAutoRaise(true);
    m_title = new QLabel(m_navigation);
    m_title->setAlignment(Qt::AlignCenter);
    row->addWidget(m_previous);
    row->addWidget(m_title, 1);
    row->addWidget(m_next);
    QObject::connect(m_previous, &QToolButton::clicked, this, &QCalendarWidget::showPreviousMonth);
    QObject::connect(m_next, &QToolButton::clicked, this, &QCalendarWidget::showNextMonth);
    m_title->installEventFilter(this); // under QMaterialStyle, the month and the year open the years
    const QDate today = QDate::currentDate();
    m_selected = today.isValid() ? today : QDate(2000, 1, 1);
    m_year = m_selected.year();
    m_month = m_selected.month();
    updateTitle();
}

/*!
    Destroys the calendar.
*/
QCalendarWidget::~QCalendarWidget() = default;

/*!
    \fn QDate QCalendarWidget::selectedDate() const

    Returns the selected date.

    \sa setSelectedDate()
*/

/*!
    \fn int QCalendarWidget::yearShown() const

    Returns the year of the month that is shown.
*/

/*!
    \fn int QCalendarWidget::monthShown() const

    Returns the month that is shown, from 1 to 12.
*/

/*!
    \fn QDate QCalendarWidget::minimumDate() const

    Returns the first date that can be selected.

    \sa setDateRange()
*/

/*!
    Sets the first date that can be selected to \a date. The last date grows to it if it was earlier, and the selected
    date is brought into the range.
*/
void QCalendarWidget::setMinimumDate(QDate date) {
    if (date.isValid())
        setDateRange(date, std::max(date, m_maximum));
}

/*!
    \fn QDate QCalendarWidget::maximumDate() const

    Returns the last date that can be selected.

    \sa setDateRange()
*/

/*!
    Sets the last date that can be selected to \a date. The first date falls to it if it was later, and the selected
    date is brought into the range.
*/
void QCalendarWidget::setMaximumDate(QDate date) {
    if (date.isValid())
        setDateRange(std::min(m_minimum, date), date);
}

/*!
    Sets the range of dates that can be selected to \a min to \a max, and brings the selected date into it.
*/
void QCalendarWidget::setDateRange(QDate min, QDate max) {
    if (!min.isValid() || !max.isValid())
        return;
    m_minimum = min;
    m_maximum = std::max(min, max);
    const QDate clamped = std::clamp(m_selected, m_minimum, m_maximum);
    if (clamped != m_selected)
        select(clamped, false);
    if (choosing()) { // the grid has other years now, and the year other months
        scrollChoices(m_choiceRow);
        moveChoiceCursor(m_choiceCursor);
    }
    updateTitle();
    update();
}

/*!
    \fn Qt::DayOfWeek QCalendarWidget::firstDayOfWeek() const

    Returns the day in the first column, Qt::Monday by default.
*/

/*!
    Sets the day in the first column to \a dayOfWeek.
*/
void QCalendarWidget::setFirstDayOfWeek(Qt::DayOfWeek dayOfWeek) {
    m_firstDay = dayOfWeek;
    update();
}

/*!
    \fn bool QCalendarWidget::isGridVisible() const

    Returns whether lines are drawn between the days. The default is \c false.
*/

/*!
    Sets whether lines are drawn between the days to \a show.
*/
void QCalendarWidget::setGridVisible(bool show) {
    m_grid = show;
    update();
}

/*!
    \fn QCalendarWidget::SelectionMode QCalendarWidget::selectionMode() const

    Returns whether the user can select a date, SingleSelection by default.
*/

/*!
    \fn void QCalendarWidget::setSelectionMode(SelectionMode mode)

    Sets whether the user can select a date to \a mode.
*/

/*!
    \fn QCalendarWidget::VerticalHeaderFormat QCalendarWidget::verticalHeaderFormat() const

    Returns what is shown at the left of the weeks, ISOWeekNumbers by default.
*/

/*!
    Sets what is shown at the left of the weeks to \a format.
*/
void QCalendarWidget::setVerticalHeaderFormat(VerticalHeaderFormat format) {
    m_vertical = format;
    updateGeometry();
    update();
}

/*!
    Returns whether the navigation bar is shown. The default is \c true.
*/
bool QCalendarWidget::isNavigationBarVisible() const { return m_navigationBar; }

/*!
    Shows the navigation bar if \a visible is \c true, and hides it otherwise.
*/
void QCalendarWidget::setNavigationBarVisible(bool visible) {
    m_navigationBar = visible;
    m_navigation->setVisible(visible && !m_inputMode);
    resizeEvent(nullptr);
    updateGeometry();
    update();
}

/*!
    \reimp
*/
QSize QCalendarWidget::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    int cell = std::max(fm.horizontalAdvance(QStringLiteral("Wed")) + 8, fm.height() + 8);
    int row = fm.height() + 8;
    if (qobject_cast<const QMaterialStyle *>(style())) {
        // A day of Material's date picker is a circle of 40, which the names of the days nearly fill.
        cell = std::max(fm.horizontalAdvance(QStringLiteral("Wed")) + 4, 40);
        row = std::max(row, 40);
    }
    if (m_inputMode) // the headline and the field, as Material's date input dialog has
        return QSize((kColumns + headerColumns()) * cell,
                     QMaterialStyle::datePickerInputHeight(m_dateField->sizeHint().height()));
    const int navigation = isNavigationBarVisible() ? navigationHeight() : 0;
    return QSize((kColumns + headerColumns()) * cell, headlineHeight() + navigation + (kRows + 1) * row);
}

/*!
    \reimp
*/
QSize QCalendarWidget::minimumSizeHint() const { return sizeHint(); }

/*!
    Selects \a date if it is in the range, shows its month, and emits selectionChanged() if it changes.

    \sa selectedDate()
*/
void QCalendarWidget::setSelectedDate(QDate date) {
    if (!date.isValid() || date < m_minimum || date > m_maximum)
        return;
    select(date, false);
}

void QCalendarWidget::select(QDate date, bool byUser) {
    (void)byUser;
    if (date.year() != m_year || date.month() != m_month)
        setCurrentPage(date.year(), date.month());
    if (date == m_selected)
        return;
    m_selected = date;
    update();
    if (m_inputMode && !m_typing) { // the field shows the date that was selected some other way
        m_dateField->setText(locale().toString(date, dateFieldFormat()));
        m_dateField->setProperty("materialError", QString());
    }
    emit selectionChanged();
}

/*!
    Shows the \a month of the \a year, without changing the selected date, and emits currentPageChanged().
*/
void QCalendarWidget::setCurrentPage(int year, int month) {
    if (month < 1 || month > 12 || !QDate::isValid(year, month, 1) || (year == m_year && month == m_month))
        return;
    m_year = year;
    m_month = month;
    updateTitle();
    update();
    emit currentPageChanged(year, month);
}

/*!
    Shows the month after the one that is shown.
*/
void QCalendarWidget::showNextMonth() {
    const QDate next = QDate(m_year, m_month, 1).addMonths(1);
    setCurrentPage(next.year(), next.month());
}

/*!
    Shows the month before the one that is shown.
*/
void QCalendarWidget::showPreviousMonth() {
    const QDate previous = QDate(m_year, m_month, 1).addMonths(-1);
    setCurrentPage(previous.year(), previous.month());
}

/*!
    Shows the month of the next year.
*/
void QCalendarWidget::showNextYear() { setCurrentPage(m_year + 1, m_month); }

/*!
    Shows the month of the previous year.
*/
void QCalendarWidget::showPreviousYear() { setCurrentPage(m_year - 1, m_month); }

/*!
    Shows the month of the selected date.
*/
void QCalendarWidget::showSelectedDate() { setCurrentPage(m_selected.year(), m_selected.month()); }

/*!
    Shows the month of today.
*/
void QCalendarWidget::showToday() {
    const QDate today = QDate::currentDate();
    setCurrentPage(today.year(), today.month());
}

/*!
    Sets what is shown above the days to \a format.
*/
void QCalendarWidget::setHorizontalHeaderFormat(HorizontalHeaderFormat format) {
    m_horizontal = format;
    updateGeometry();
    update();
}

/*!
    \fn QCalendarWidget::HorizontalHeaderFormat QCalendarWidget::horizontalHeaderFormat() const

    Returns what is shown above the days, ShortDayNames by default.
*/

/*!
    \fn QCalendar QCalendarWidget::calendar() const

    Returns the calendar of the widget.
*/

/*!
    Sets the calendar of the widget to \a calendar. Dates are still shown in the Gregorian calendar.
*/
void QCalendarWidget::setCalendar(QCalendar calendar) {
    m_calendar = calendar;
    update();
}

/*!
    \fn void QCalendarWidget::clearMinimumDate()

    Resets the first date that can be selected to 1752-09-14.
*/

/*!
    \fn void QCalendarWidget::clearMaximumDate()

    Resets the last date that can be selected to 9999-12-31.
*/

/*!
    \fn QTextCharFormat QCalendarWidget::headerTextFormat() const

    Returns the format of the names of the days above the dates.
*/

/*!
    Sets the format of the names of the days above the dates to \a format.
*/
void QCalendarWidget::setHeaderTextFormat(const QTextCharFormat &format) {
    m_headerFormat = format;
    update();
}

/*!
    Returns the format of the \a dayOfWeek in the dates; an empty format if none was set.
*/
QTextCharFormat QCalendarWidget::weekdayTextFormat(Qt::DayOfWeek dayOfWeek) const {
    return m_weekdayFormats.value(dayOfWeek);
}

/*!
    Sets the format of the \a dayOfWeek in the dates to \a format.
*/
void QCalendarWidget::setWeekdayTextFormat(Qt::DayOfWeek dayOfWeek, const QTextCharFormat &format) {
    m_weekdayFormats.insert(dayOfWeek, format);
    update();
}

/*!
    \fn QMap<QDate, QTextCharFormat> QCalendarWidget::dateTextFormat() const

    Returns the formats of all the dates that have one. A date format wins over the format of its day of the week.
*/

/*!
    \fn QTextCharFormat QCalendarWidget::dateTextFormat(QDate date) const
    \overload

    Returns the format of the \a date; an empty format if none was set.
*/

/*!
    Sets the format of the \a date to \a format. An empty format removes it.
*/
void QCalendarWidget::setDateTextFormat(QDate date, const QTextCharFormat &format) {
    if (!date.isValid()) {
        m_dateFormats.clear();
    } else if (format.isEmpty()) {
        m_dateFormats.remove(date);
    } else {
        m_dateFormats.insert(date, format);
    }
    update();
}

/*!
    \fn bool QCalendarWidget::isDateEditEnabled() const

    Returns whether the user may type a date. The value is stored; typing is not implemented.
*/

/*!
    \fn void QCalendarWidget::setDateEditEnabled(bool enable)

    Sets whether the user may type a date to \a enable.
*/

/*!
    \fn int QCalendarWidget::dateEditAcceptDelay() const

    Returns the time in milliseconds after which a typed date is accepted, 1500 by default.
*/

/*!
    \fn void QCalendarWidget::setDateEditAcceptDelay(int delay)

    Sets the time in milliseconds after which a typed date is accepted to \a delay.
*/

/*!
    Repaints the cell of the \a date if it is shown.
*/
void QCalendarWidget::updateCell(QDate date) {
    if (!date.isValid())
        return;
    const qint64 offset = firstShown().daysTo(date);
    if (offset < 0 || offset >= kRows * kColumns)
        return;
    update(cellRect(static_cast<int>(offset / kColumns), static_cast<int>(offset % kColumns)));
}

/*!
    Repaints all the cells.
*/
void QCalendarWidget::updateCells() { update(); }

void QCalendarWidget::updateTitle() {
    const QLocale names = locale();
    const QDate first(m_year, m_month, 1);
    if (m_yearTitle && isDocked()) {
        m_title->setText(names.standaloneMonthName(m_month, QLocale::ShortFormat));
        m_yearTitle->setText(QString::number(m_year));
    } else {
        m_title->setText(names.standaloneMonthName(m_month) + QLatin1Char(' ') + QString::number(m_year));
    }
    m_previous->setEnabled(first > m_minimum);
    m_next->setEnabled(first.addMonths(1) <= m_maximum);
    if (m_yearTitle) {
        m_previousYear->setEnabled(first.addYears(-1).addMonths(1) > m_minimum);
        m_nextYear->setEnabled(first.addYears(1) <= m_maximum);
    }
}

bool QCalendarWidget::isDocked() const {
    return materialStyle(this) && property("materialDatePicker").toString() == QLatin1String("docked");
}

// The bar of the docked picker: the month between its arrows at the start, the year between its arrows at the end,
// each opening its own choice. The widgets of the year are made when the picker is first docked.
void QCalendarWidget::layoutNavigation() {
    const bool docked = isDocked();
    if (docked && !m_yearTitle) {
        auto *row = static_cast<QHBoxLayout *>(m_navigation->layout());
        m_previousYear = new QToolButton(m_navigation);
        m_previousYear->setArrowType(Qt::LeftArrow);
        m_previousYear->setAutoRaise(true);
        m_previousYear->setAccessibleName(QMaterialStyle::tr("Previous year"));
        m_nextYear = new QToolButton(m_navigation);
        m_nextYear->setArrowType(Qt::RightArrow);
        m_nextYear->setAutoRaise(true);
        m_nextYear->setAccessibleName(QMaterialStyle::tr("Next year"));
        m_yearTitle = new QLabel(m_navigation);
        m_yearTitle->setAlignment(Qt::AlignCenter);
        m_yearTitle->installEventFilter(this);
        row->addWidget(m_previousYear);
        row->addWidget(m_yearTitle, 1);
        row->addWidget(m_nextYear);
        QObject::connect(m_previousYear, &QToolButton::clicked, this, &QCalendarWidget::showPreviousYear);
        QObject::connect(m_nextYear, &QToolButton::clicked, this, &QCalendarWidget::showNextYear);
    }
    if (!docked)
        setChoosingMonth(false);
    if (m_choosingYear) // the years are in a grid or a list now: the cursor's row is the second again
        scrollChoices((m_choiceCursor - m_minimum.year()) / choiceColumns() - 1);
    if (m_yearTitle)
        m_yearTitle->setVisible(docked);
    updateTitle();
    updateChooserControls();
}

// The date in the first cell: the first day of the week on or before the first of the month.
QDate QCalendarWidget::firstShown() const {
    const QDate first(m_year, m_month, 1);
    int back = (first.dayOfWeek() - static_cast<int>(m_firstDay) + 7) % 7;
    if (back == 0)
        back = 7; // a whole week of the month before, as Qt does
    return first.addDays(-back);
}

// As high as the arrows make the bar, while they are hidden too.
int QCalendarWidget::navigationHeight() const {
    return std::max(m_navigation->sizeHint().height(), m_previous->sizeHint().height() + 2 * kNavigationMargin);
}

int QCalendarWidget::headlineHeight() const {
    return materialStyle(this) ? QMaterialStyle::datePickerHeadlineHeight(this) : 0;
}

QRect QCalendarWidget::gridRect() const {
    const int top = headlineHeight() + (isNavigationBarVisible() && !m_inputMode ? m_navigation->height() : 0);
    return QRect(0, top, width(), height() - top);
}

// Row -1 is the header of the days, column -1 that of the weeks.
QRect QCalendarWidget::cellRect(int row, int column) const {
    const QRect grid = gridRect();
    const int columns = kColumns + headerColumns();
    const int c = column + headerColumns();
    const int x0 = grid.left() + grid.width() * c / columns;
    const int x1 = grid.left() + grid.width() * (c + 1) / columns;
    const int y0 = grid.top() + grid.height() * (row + 1) / (kRows + 1);
    const int y1 = grid.top() + grid.height() * (row + 2) / (kRows + 1);
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

QDate QCalendarWidget::dateAt(const QPoint &pos) const {
    for (int row = 0; row < kRows; ++row)
        for (int column = 0; column < kColumns; ++column)
            if (cellRect(row, column).contains(pos))
                return firstShown().addDays(row * kColumns + column);
    return QDate();
}

/*!
    Paints the cell of the \a date in \a rect with the \a painter: the highlight of the selected date and the day
    number, with the format of the date or of its day of the week. Reimplement it to draw cells differently.
*/
void QCalendarWidget::paintCell(QPainter *painter, const QRect &rect, QDate date) const {
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const bool inMonth = date.month() == m_month;
    const bool inRange = date >= m_minimum && date <= m_maximum;
    QTextCharFormat format = m_weekdayFormats.value(static_cast<Qt::DayOfWeek>(date.dayOfWeek()));
    if (m_dateFormats.contains(date))
        format = m_dateFormats.value(date);
    painter->save();
    if (format.background().style() != Qt::NoBrush)
        painter->fillRect(rect, format.background());
    if (const auto *material = qobject_cast<const QMaterialStyle *>(style())) {
        if (!format.isEmpty())
            painter->setFont(formatted(font(), format));
        material->drawCalendarDay(painter, rect, this, date.day(), date == m_selected && inMonth,
                                  date == QDate::currentDate() && inMonth, isEnabled() && inMonth && inRange,
                                  format.foreground().style() != Qt::NoBrush ? format.foreground().color() : QColor());
        painter->restore();
        return;
    }
    if (date == m_selected && inMonth) {
        painter->fillRect(rect.adjusted(1, 1, -1, -1), palette().color(group, QPalette::Highlight));
        painter->setPen(palette().color(group, QPalette::HighlightedText));
    } else if (!inMonth || !inRange) {
        painter->setPen(palette().color(QPalette::Disabled, QPalette::Text));
    } else if (format.foreground().style() != Qt::NoBrush) {
        painter->setPen(format.foreground().color());
    } else {
        painter->setPen(palette().color(group, QPalette::Text));
    }
    if (!format.isEmpty())
        painter->setFont(formatted(font(), format));
    painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
    painter->restore();
}

/*!
    \reimp
*/
void QCalendarWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QMaterialStyle *material = materialStyle(this);
    // The popup of a date edit lies on a container with round corners, which shows the window at them.
    const bool popup = QMaterialStyle::isDatePickerPopup(this);
    if (popup)
        material->drawDatePickerPopup(&painter, rect(), this);
    if (const int headline = headlineHeight())
        material->drawDatePickerHeadline(&painter, QRect(0, 0, width(), headline), this, m_inputMode);
    if (!popup)
        painter.fillRect(gridRect(), palette().color(group, QPalette::Base));
    if (m_inputMode)
        return; // the text field is in the place of the month
    // Under QMaterialStyle the years or the months and the days cross-fade as one gives way to the other.
    const double choices = material ? material->datePickerYearsShown(this, choosing()) : 0.0;
    if (choices > 0.0) {
        painter.save();
        painter.setClipRect(gridRect());
        painter.setOpacity(choices);
        const bool months = showsMonths();
        const int columns = choiceColumns();
        const int first = firstChoice() + m_choiceRow * columns;
        const int last = std::min(lastChoice(), first + (visibleChoiceRows() + 1) * columns - 1);
        const int today = QDate::currentDate().year();
        for (int value = first; value <= last; ++value) {
            const bool cursor = value == m_choiceCursor && hasFocus();
            if (months)
                material->drawCalendarMonth(&painter, choiceRect(value), this, locale().standaloneMonthName(value),
                                            value == m_month, monthUsable(value), cursor, value == m_hoveredChoice,
                                            value == m_pressedChoice);
            else if (columns == 1) // the docked picker's menu of years
                material->drawCalendarMonth(&painter, choiceRect(value), this, QString::number(value), value == m_year,
                                            true, cursor, value == m_hoveredChoice, value == m_pressedChoice);
            else
                material->drawCalendarYear(&painter, choiceRect(value), this, value, value == m_year, value == today,
                                           cursor, value == m_hoveredChoice, value == m_pressedChoice);
        }
        painter.restore();
        if (choices >= 1.0)
            return;
        painter.setOpacity(1.0 - choices);
    }
    // The names of the days.
    if (m_horizontal != NoHorizontalHeader) {
        QFont bold = font();
        bold.setBold(material == nullptr); // Material's names of the days are in the type of the days
        painter.setFont(formatted(bold, m_headerFormat));
        const QBrush headerColor = m_headerFormat.foreground();
        painter.setPen(headerColor.style() != Qt::NoBrush ? headerColor.color()
                                                          : palette().color(group, QPalette::Text));
        const QLocale names = locale();
        for (int column = 0; column < kColumns; ++column) {
            const int day = (static_cast<int>(m_firstDay) - 1 + column) % 7 + 1;
            const QString name =
                m_horizontal == SingleLetterDayNames
                    ? names.standaloneDayName(day, QLocale::NarrowFormat)
                    : names.dayName(day, m_horizontal == LongDayNames ? QLocale::LongFormat : QLocale::ShortFormat);
            painter.drawText(cellRect(-1, column), Qt::AlignCenter, name);
        }
    }
    painter.setFont(font());
    const QDate first = firstShown();
    for (int row = 0; row < kRows; ++row) {
        if (headerColumns()) {
            painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
            const QDate weekDay = first.addDays(row * kColumns);
            painter.drawText(cellRect(row, -1), Qt::AlignCenter, QString::number(weekDay.addDays(3).weekNumber()));
        }
        for (int column = 0; column < kColumns; ++column) {
            const QDate date = first.addDays(row * kColumns + column);
            const QRect cell = cellRect(row, column);
            paintCell(&painter, cell, date);
            if (m_grid) {
                painter.setPen(palette().color(group, QPalette::Midlight));
                painter.drawLine(cell.right(), cell.top(), cell.right(), cell.bottom());
                painter.drawLine(cell.left(), cell.bottom(), cell.right(), cell.bottom());
            }
        }
    }
    if (hasFocus() && material == nullptr) { // Material rings the day that the keyboard is on
        painter.setPen(palette().color(group, QPalette::Highlight));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(gridRect().adjusted(0, 0, -1, -1));
    }
}

/*!
    \reimp
*/
void QCalendarWidget::resizeEvent(QResizeEvent *event) {
    if (event)
        QWidget::resizeEvent(event);
    m_navigation->setGeometry(0, headlineHeight(), width(), navigationHeight());
    if (choosing())
        scrollChoices(m_choiceRow);
    layoutDateField();
    layoutModeButton();
}

QRect QCalendarWidget::modeButtonRect() const {
    const int headline = headlineHeight();
    return headline ? QMaterialStyle::datePickerModeButton(QRect(0, 0, width(), headline)) : QRect();
}

void QCalendarWidget::layoutModeButton() {
    const QRect rect = modeButtonRect();
    if (rect.isValid())
        m_modeButton->setGeometry(rect);
    m_modeButton->setVisible(rect.isValid());
}

void QCalendarWidget::fitWindowToMode(int before) {
    updateGeometry();
    auto *dialog = qobject_cast<QDialog *>(window());
    const int change = sizeHint().height() - before;
    if (!dialog || !dialog->isVisible() || !dialog->layout() || change == 0)
        return;
    const int height = dialog->height() + change;
    dialog->layout()->activate(); // its least height follows the picker's first, which may already grow it
    dialog->resize(dialog->width(), std::max(dialog->minimumSizeHint().height(), height));
}

void QCalendarWidget::layoutDateField() {
    if (m_dateField)
        m_dateField->setGeometry(QMaterialStyle::datePickerField(gridRect(), m_dateField->sizeHint().height()));
}

// The short date format of the locale, with days and months of two digits and years of four, as Material's hint
// shows it ("MM/dd/yyyy"); a format with names of days or months becomes MM/dd/yyyy, which can be typed with digits.
QString QCalendarWidget::dateFieldFormat() const {
    const QString format = locale().dateFormat(QLocale::ShortFormat);
    QString result;
    qsizetype i = 0;
    while (i < format.size()) {
        const QChar c = format.at(i);
        qsizetype run = 1;
        while (i + run < format.size() && format.at(i + run) == c)
            ++run;
        if (c == QLatin1Char('\'') || ((c == QLatin1Char('d') || c == QLatin1Char('M')) && run > 2))
            return QStringLiteral("MM/dd/yyyy"); // quoted text or names
        if (c == QLatin1Char('d'))
            result += QStringLiteral("dd");
        else if (c == QLatin1Char('M'))
            result += QStringLiteral("MM");
        else if (c == QLatin1Char('y'))
            result += QStringLiteral("yyyy");
        else
            result += QString(run, c);
        i += run;
    }
    return result.contains(QLatin1Char('d')) && result.contains(QLatin1Char('M')) && result.contains(QLatin1Char('y'))
               ? result
               : QStringLiteral("MM/dd/yyyy");
}

QDate QCalendarWidget::dateFieldDate(QString *error) const {
    const QString format = dateFieldFormat();
    const QString text = m_dateField->text().trimmed();
    QDate date = locale().toDate(text, format);
    if (!date.isValid()) { // days and months may be typed with one digit
        QString relaxed = format;
        relaxed.replace(QStringLiteral("dd"), QStringLiteral("d")).replace(QStringLiteral("MM"), QStringLiteral("M"));
        date = locale().toDate(text, relaxed);
    }
    if (!date.isValid()) {
        *error = QMaterialStyle::tr("Date does not match the pattern %1").arg(format.toUpper());
        return QDate();
    }
    if (date < m_minimum || date > m_maximum) {
        *error = QMaterialStyle::tr("Date out of range: %1 to %2")
                     .arg(locale().toString(m_minimum, format), locale().toString(m_maximum, format));
        return QDate();
    }
    error->clear();
    return date;
}

// A date that is typed whole selects itself. While the text is shorter than the pattern it may still become another
// date (where the day comes last, its first figure is already a date), so nothing is selected and there is no error
// yet; Return, or going back, also reads a day or a month of one figure.
void QCalendarWidget::dateFieldEdited() {
    if (m_dateField->text().trimmed().size() < dateFieldFormat().size()) {
        m_dateField->setProperty("materialError", QString());
        return;
    }
    QString error;
    const QDate date = dateFieldDate(&error);
    if (date.isValid() && m_selectionMode == SingleSelection) {
        m_typing = true;
        select(date, true);
        m_typing = false;
    }
    m_dateField->setProperty("materialError", error);
}

// Return, or the field losing the focus: text that is not a date of the range shows why, and the selection stays.
void QCalendarWidget::dateFieldFinished() {
    if (!m_inputMode || m_dateField->text().trimmed().isEmpty())
        return;
    QString error;
    const QDate date = dateFieldDate(&error);
    m_dateField->setProperty("materialError", error);
    if (date.isValid() && m_selectionMode == SingleSelection) {
        m_typing = true;
        select(date, true);
        m_typing = false;
    }
}

void QCalendarWidget::setInputMode(bool input) {
    if (input == m_inputMode || (input && !headlineHeight()))
        return;
    const int before = sizeHint().height();
    if (input) {
        setChoosingYear(false);
        setChoosingMonth(false);
        if (!m_dateField) {
            m_dateField = new QLineEdit(this);
            m_dateField->setProperty("materialLabel", QMaterialStyle::tr("Date"));
            // On a phone, a keyboard of digits with the separators of a date; ImhPreferNumbers would give one of
            // digits alone, without the slash.
            m_dateField->setInputMethodHints(Qt::ImhDate);
            QObject::connect(m_dateField, &QLineEdit::textEdited, this, [this] { dateFieldEdited(); });
            QObject::connect(m_dateField, &QLineEdit::editingFinished, this, [this] { dateFieldFinished(); });
            QObject::connect(m_dateField, &QLineEdit::returnPressed, this, [this] {
                if (m_dateField->property("materialError").toString().isEmpty() &&
                    !m_dateField->text().trimmed().isEmpty())
                    emit activated(m_selected);
            });
        }
        const QString format = dateFieldFormat();
        m_dateField->setProperty("materialSupportingText", format.toUpper());
        m_dateField->setPlaceholderText(format.toUpper());
        m_dateField->setProperty("materialError", QString()); // its room is kept, so an error does not move it
        m_dateField->setText(locale().toString(m_selected, format));
        m_inputMode = true;
        m_navigation->setVisible(false);
        layoutDateField();
        m_dateField->show();
        m_dateField->setFocus();
    } else {
        // What was typed counts if it is a date of the range; otherwise the selection stays as it was.
        if (m_selectionMode == SingleSelection) {
            QString error;
            const QDate date = dateFieldDate(&error);
            if (date.isValid()) {
                m_typing = true;
                select(date, true);
                m_typing = false;
            }
        }
        const bool focused = m_dateField->hasFocus();
        m_inputMode = false;
        m_dateField->hide();
        m_navigation->setVisible(m_navigationBar);
        if (focused)
            setFocus();
    }
    m_modeButton->setAccessibleName(input ? QMaterialStyle::tr("Switch to calendar input mode")
                                          : QMaterialStyle::tr("Switch to text input mode"));
    m_modeButton->update();
    fitWindowToMode(before);
    update();
}

void QCalendarWidget::setChoosingYear(bool choosing) {
    if (choosing == m_choosingYear)
        return;
    if (choosing)
        m_choosingMonth = false;
    m_choosingYear = choosing;
    m_hoveredChoice = m_pressedChoice = 0;
    if (choosing) {
        // The year that is shown is in the second row, or as near to it as the rows go.
        m_fadingMonths = false;
        m_choiceCursor = std::clamp(m_year, m_minimum.year(), m_maximum.year());
        scrollChoices((m_choiceCursor - m_minimum.year()) / choiceColumns() - 1);
    }
    updateChooserControls();
    update();
}

void QCalendarWidget::setChoosingMonth(bool choosing) {
    if (choosing == m_choosingMonth || (choosing && !isDocked()))
        return;
    if (choosing)
        m_choosingYear = false;
    m_choosingMonth = choosing;
    m_hoveredChoice = m_pressedChoice = 0;
    if (choosing) {
        // As the years: the month that is shown in the second row.
        m_fadingMonths = true;
        m_choiceCursor = std::clamp(m_month, firstUsableChoice(), lastUsableChoice());
        scrollChoices(m_choiceCursor - 2);
    }
    updateChooserControls();
    update();
}

void QCalendarWidget::updateChooserControls() {
    const bool docked = m_yearTitle && isDocked();
    m_previous->setVisible(!choosing());
    m_next->setVisible(!choosing());
    if (m_yearTitle) {
        m_previousYear->setVisible(docked && !choosing());
        m_nextYear->setVisible(docked && !choosing());
        // While one of the docked picker's menus is open, the other is out of use.
        m_yearTitle->setEnabled(!m_choosingMonth);
    }
    m_title->setEnabled(!docked || !m_choosingYear);
    m_title->update();
    if (m_yearTitle)
        m_yearTitle->update();
}

// Shows the days of the year, in the month that was shown, or the nearest one in the range.
void QCalendarWidget::chooseYear(int year) {
    const QDate first(m_minimum.year(), m_minimum.month(), 1);
    const QDate page = std::clamp(QDate(year, m_month, 1), first, m_maximum);
    setChoosingYear(false);
    if (page.isValid())
        setCurrentPage(page.year(), page.month());
}

// Shows the days of the month in the year that is shown.
void QCalendarWidget::chooseMonth(int month) {
    if (!monthUsable(month))
        return;
    setChoosingMonth(false);
    setCurrentPage(m_year, month);
}

void QCalendarWidget::choose(int value) {
    if (m_choosingMonth)
        chooseMonth(value);
    else
        chooseYear(value);
}

bool QCalendarWidget::monthUsable(int month) const {
    const QDate first(m_year, month, 1);
    return first.isValid() && first <= m_maximum && first.addMonths(1) > m_minimum;
}

bool QCalendarWidget::showsMonths() const { return m_choosingMonth || (!m_choosingYear && m_fadingMonths); }

int QCalendarWidget::firstChoice() const { return showsMonths() ? 1 : m_minimum.year(); }

int QCalendarWidget::lastChoice() const { return showsMonths() ? 12 : m_maximum.year(); }

// The months of the range in the year shown, which follow each other, as the range does.
int QCalendarWidget::firstUsableChoice() const {
    if (!showsMonths())
        return firstChoice();
    for (int month = 1; month <= 12; ++month)
        if (monthUsable(month))
            return month;
    return m_month;
}

int QCalendarWidget::lastUsableChoice() const {
    if (!showsMonths())
        return lastChoice();
    for (int month = 12; month >= 1; --month)
        if (monthUsable(month))
            return month;
    return m_month;
}

// The docked picker lists the years one to a row, as it lists the months; another calendar has a grid of them.
int QCalendarWidget::choiceColumns() const { return showsMonths() || isDocked() ? 1 : kYearColumns; }

int QCalendarWidget::choiceRowHeight() const { return showsMonths() || isDocked() ? kMonthRow : kYearRow; }

int QCalendarWidget::choiceRows() const { return (lastChoice() - firstChoice()) / choiceColumns() + 1; }

int QCalendarWidget::visibleChoiceRows() const {
    return std::max(1, (gridRect().height() - kChoiceTop) / choiceRowHeight());
}

void QCalendarWidget::scrollChoices(int row) {
    m_choiceRow = std::clamp(row, 0, std::max(0, choiceRows() - visibleChoiceRows()));
    update();
}

void QCalendarWidget::moveChoiceCursor(int value) {
    m_choiceCursor = std::clamp(value, firstUsableChoice(), lastUsableChoice());
    const int row = (m_choiceCursor - firstChoice()) / choiceColumns();
    if (row < m_choiceRow)
        scrollChoices(row);
    else if (row >= m_choiceRow + visibleChoiceRows())
        scrollChoices(row - visibleChoiceRows() + 1);
    update();
}

QRect QCalendarWidget::choiceRect(int value) const {
    const QRect grid = gridRect();
    const int columns = choiceColumns();
    const int index = value - firstChoice();
    const int column = index % columns;
    const int x0 = grid.left() + grid.width() * column / columns;
    const int x1 = grid.left() + grid.width() * (column + 1) / columns;
    return QRect(x0, grid.top() + kChoiceTop + (index / columns - m_choiceRow) * choiceRowHeight(), x1 - x0,
                 choiceRowHeight());
}

int QCalendarWidget::choiceAt(const QPoint &pos) const {
    const QRect grid = gridRect();
    if (!choosing() || !grid.contains(pos) || pos.y() < grid.top() + kChoiceTop || grid.width() <= 0)
        return 0;
    const int columns = choiceColumns();
    const int column = std::min(columns - 1, (pos.x() - grid.left()) * columns / grid.width());
    const int row = (pos.y() - grid.top() - kChoiceTop) / choiceRowHeight() + m_choiceRow;
    const int value = firstChoice() + row * columns + column;
    if (value > lastChoice() || (m_choosingMonth && !monthUsable(value)))
        return 0;
    return value;
}

/*!
    \reimp

    A click on a date in the range selects it and emits clicked(); a date of another month shows that month.
*/
void QCalendarWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && modeButtonRect().contains(event->pos())) {
        setInputMode(!m_inputMode);
        event->accept();
        return;
    }
    if (m_inputMode) {
        event->ignore();
        return;
    }
    if (choosing()) {
        if (event->button() != Qt::LeftButton) {
            event->ignore();
            return;
        }
        setFocus();
        m_pressedChoice = choiceAt(event->pos()); // chosen when the button comes up on it
        update();
        event->accept();
        return;
    }
    const QDate date = dateAt(event->pos());
    if (event->button() != Qt::LeftButton || !date.isValid()) {
        event->ignore();
        return;
    }
    setFocus();
    if (date >= m_minimum && date <= m_maximum) {
        if (m_selectionMode == SingleSelection)
            select(date, true);
        else if (date.month() != m_month || date.year() != m_year)
            setCurrentPage(date.year(), date.month());
        emit clicked(date);
    }
    event->accept();
}

/*!
    \reimp
*/
void QCalendarWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    if (choosing() || modeButtonRect().contains(event->pos())) {
        event->accept();
        return;
    }
    const QDate date = m_inputMode ? QDate() : dateAt(event->pos());
    if (event->button() == Qt::LeftButton && date.isValid() && date >= m_minimum && date <= m_maximum) {
        emit activated(date);
        event->accept();
        return;
    }
    event->ignore();
}

/*!
    \reimp

    The arrow keys move the selection by a day or a week, Page Up and Page Down by a month, Home and End to the first
    and last day of the month. Return and Enter emit activated().
*/
void QCalendarWidget::keyPressEvent(QKeyEvent *event) {
    if (m_inputMode) {
        event->ignore();
        return;
    }
    if (choosing()) {
        const int columns = choiceColumns();
        switch (event->key()) {
        case Qt::Key_Escape:
            setChoosingYear(false);
            setChoosingMonth(false);
            break;
        case Qt::Key_Left:
            moveChoiceCursor(m_choiceCursor - 1);
            break;
        case Qt::Key_Right:
            moveChoiceCursor(m_choiceCursor + 1);
            break;
        case Qt::Key_Up:
            moveChoiceCursor(m_choiceCursor - columns);
            break;
        case Qt::Key_Down:
            moveChoiceCursor(m_choiceCursor + columns);
            break;
        case Qt::Key_PageUp:
            moveChoiceCursor(m_choiceCursor - visibleChoiceRows() * columns);
            break;
        case Qt::Key_PageDown:
            moveChoiceCursor(m_choiceCursor + visibleChoiceRows() * columns);
            break;
        case Qt::Key_Home:
            moveChoiceCursor(firstUsableChoice());
            break;
        case Qt::Key_End:
            moveChoiceCursor(lastUsableChoice());
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            choose(m_choiceCursor);
            break;
        default:
            event->ignore();
            return;
        }
        event->accept();
        return;
    }
    QDate target;
    switch (event->key()) {
    case Qt::Key_Left:
        target = m_selected.addDays(-1);
        break;
    case Qt::Key_Right:
        target = m_selected.addDays(1);
        break;
    case Qt::Key_Up:
        target = m_selected.addDays(-7);
        break;
    case Qt::Key_Down:
        target = m_selected.addDays(7);
        break;
    case Qt::Key_PageUp:
        target = m_selected.addMonths(-1);
        break;
    case Qt::Key_PageDown:
        target = m_selected.addMonths(1);
        break;
    case Qt::Key_Home:
        target = QDate(m_selected.year(), m_selected.month(), 1);
        break;
    case Qt::Key_End:
        target = QDate(m_selected.year(), m_selected.month(), m_selected.daysInMonth());
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit activated(m_selected);
        event->accept();
        return;
    default:
        event->ignore();
        return;
    }
    if (m_selectionMode == SingleSelection)
        select(std::clamp(target, m_minimum, m_maximum), true);
    event->accept();
}

/*!
    \reimp

    The wheel shows the previous or the next month.
*/
void QCalendarWidget::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0 || m_inputMode) {
        event->ignore();
        return;
    }
    if (choosing()) {
        scrollChoices(m_choiceRow - notches);
        event->accept();
        return;
    }
    if (notches > 0)
        showPreviousMonth();
    else
        showNextMonth();
    event->accept();
}

/*!
    \reimp
*/
void QCalendarWidget::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange)
        updateGeometry();
    if (event->type() == QEvent::LocaleChange)
        updateTitle();
    if (event->type() == QEvent::StyleChange) {
        // The headline, the years and the months are Material's; a property of the style may have changed too.
        if (!materialStyle(this))
            setChoosingYear(false);
        layoutNavigation();
        if (!headlineHeight()) // the input mode is the modal picker's
            setInputMode(false);
        resizeEvent(nullptr);
        updateGeometry();
    }
    update();
    QWidget::changeEvent(event);
}

/*!
    \reimp

    Under QMaterialStyle, while the years are shown, the year under the pointer is highlighted, and a year is chosen
    when the button is released over the year that it was pressed on.
*/
bool QCalendarWidget::event(QEvent *event) {
    // The navigation bar of a popup leaves the container under it to show, round at its corners.
    if (event->type() == QEvent::Show || event->type() == QEvent::StyleChange)
        m_navigation->setAutoFillBackground(!QMaterialStyle::isDatePickerPopup(this));
    if (!choosing())
        return QWidget::event(event);
    switch (event->type()) {
    case QEvent::MouseMove: {
        const int value = isEnabled() ? choiceAt(static_cast<QMouseEvent *>(event)->pos()) : 0;
        if (value != m_hoveredChoice) {
            m_hoveredChoice = value;
            update();
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::LeftButton || !m_pressedChoice)
            break;
        const int pressed = m_pressedChoice;
        m_pressedChoice = 0;
        update();
        if (choiceAt(mouse->pos()) == pressed)
            choose(pressed);
        event->accept();
        return true;
    }
    case QEvent::Leave:
        if (m_hoveredChoice) {
            m_hoveredChoice = 0;
            update();
        }
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

/*!
    \reimp

    Under QMaterialStyle, a click on the month and the year in the navigation bar shows the years in the place of the
    days, and the label is painted with the arrow of that menu.
*/
bool QCalendarWidget::eventFilter(QObject *watched, QEvent *event) {
    const QMaterialStyle *material = materialStyle(this);
    auto *label = qobject_cast<QLabel *>(watched);
    if (material && label && (label == m_title || label == m_yearTitle)) {
        // On the docked picker the month opens the months and the year the years; otherwise the label opens the years.
        const bool months = label == m_title && isDocked();
        if (event->type() == QEvent::Paint) {
            QPainter painter(label);
            material->drawCalendarTitle(&painter, label->rect(), label, label->text(),
                                        months ? m_choosingMonth : m_choosingYear);
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress &&
            static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton && isEnabled() && label->isEnabled()) {
            setFocus();
            if (months)
                setChoosingMonth(!m_choosingMonth);
            else
                setChoosingYear(!m_choosingYear);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

#include "moc_qcalendarwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
