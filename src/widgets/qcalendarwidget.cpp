// MIT License - see LICENSE
#include <QtWidgets/qcalendarwidget.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qtoolbutton.h>

#include <algorithm>

namespace {

constexpr int kRows = 6;
constexpr int kColumns = 7;

const char *const kMonths[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};
const char *const kDays[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

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

    \differences
    The names of the months and the days are English and the first day of the week is Monday, as there is no
    \c QLocale; \c setFirstDayOfWeek() changes it. There are no date text formats (\c setDateTextFormat()), no
    \c horizontalHeaderFormat(), no month and year menus in the navigation bar, and no \c dateEditEnabled().
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
    m_navigation = new QWidget(this);
    m_navigation->setAutoFillBackground(true);
    auto *row = new QHBoxLayout(m_navigation);
    row->setContentsMargins(2, 2, 2, 2);
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
bool QCalendarWidget::isNavigationBarVisible() const { return !m_navigation->isHidden(); }

/*!
    Shows the navigation bar if \a visible is \c true, and hides it otherwise.
*/
void QCalendarWidget::setNavigationBarVisible(bool visible) {
    m_navigation->setVisible(visible);
    resizeEvent(nullptr);
    updateGeometry();
    update();
}

/*!
    \reimp
*/
QSize QCalendarWidget::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    const int cell = std::max(fm.horizontalAdvance(QStringLiteral("Wed")) + 8, fm.height() + 8);
    const int navigation = isNavigationBarVisible() ? m_navigation->sizeHint().height() : 0;
    return QSize((kColumns + headerColumns()) * cell, navigation + (kRows + 1) * (fm.height() + 8));
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

void QCalendarWidget::updateTitle() {
    m_title->setText(QString::fromUtf8(kMonths[m_month - 1]) + QLatin1Char(' ') + QString::number(m_year));
    m_previous->setEnabled(QDate(m_year, m_month, 1) > m_minimum);
    m_next->setEnabled(QDate(m_year, m_month, 1).addMonths(1) <= m_maximum);
}

// The date in the first cell: the first day of the week on or before the first of the month.
QDate QCalendarWidget::firstShown() const {
    const QDate first(m_year, m_month, 1);
    int back = (first.dayOfWeek() - static_cast<int>(m_firstDay) + 7) % 7;
    if (back == 0)
        back = 7; // a whole week of the month before, as Qt does
    return first.addDays(-back);
}

QRect QCalendarWidget::gridRect() const {
    const int top = isNavigationBarVisible() ? m_navigation->height() : 0;
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
    \reimp
*/
void QCalendarWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    painter.fillRect(gridRect(), palette().color(group, QPalette::Base));
    // The names of the days.
    QFont bold = font();
    bold.setBold(true);
    painter.setFont(bold);
    painter.setPen(palette().color(group, QPalette::Text));
    for (int column = 0; column < kColumns; ++column) {
        const int day = (static_cast<int>(m_firstDay) - 1 + column) % 7;
        painter.drawText(cellRect(-1, column), Qt::AlignCenter, QString::fromUtf8(kDays[day]));
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
            const bool inMonth = date.month() == m_month;
            const bool inRange = date >= m_minimum && date <= m_maximum;
            if (date == m_selected && inMonth) {
                painter.fillRect(cell.adjusted(1, 1, -1, -1), palette().color(group, QPalette::Highlight));
                painter.setPen(palette().color(group, QPalette::HighlightedText));
            } else if (!inMonth || !inRange) {
                painter.setPen(palette().color(QPalette::Disabled, QPalette::Text));
            } else {
                painter.setPen(palette().color(group, QPalette::Text));
            }
            painter.drawText(cell, Qt::AlignCenter, QString::number(date.day()));
            if (m_grid) {
                painter.setPen(palette().color(group, QPalette::Midlight));
                painter.drawLine(cell.right(), cell.top(), cell.right(), cell.bottom());
                painter.drawLine(cell.left(), cell.bottom(), cell.right(), cell.bottom());
            }
        }
    }
    if (hasFocus()) {
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
    m_navigation->setGeometry(0, 0, width(), m_navigation->sizeHint().height());
}

/*!
    \reimp

    A click on a date in the range selects it and emits clicked(); a date of another month shows that month.
*/
void QCalendarWidget::mousePressEvent(QMouseEvent *event) {
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
    const QDate date = dateAt(event->pos());
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
    if (notches == 0) {
        event->ignore();
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
    update();
    QWidget::changeEvent(event);
}

#include "moc_qcalendarwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
