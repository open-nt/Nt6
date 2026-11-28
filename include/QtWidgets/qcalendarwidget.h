// MIT License - see LICENSE
#pragma once

#include <QtCore/qcalendar.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmap.h>
#include <QtGui/qtextformat.h>
#include <QtWidgets/qwidget.h>

class QLabel;
class QLineEdit;
class QPainter;
class QToolButton;

class QCalendarWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QDate selectedDate READ selectedDate WRITE setSelectedDate)
    Q_PROPERTY(QDate minimumDate READ minimumDate WRITE setMinimumDate)
    Q_PROPERTY(QDate maximumDate READ maximumDate WRITE setMaximumDate)
    Q_PROPERTY(Qt::DayOfWeek firstDayOfWeek READ firstDayOfWeek WRITE setFirstDayOfWeek)
    Q_PROPERTY(bool gridVisible READ isGridVisible WRITE setGridVisible)
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode)
    Q_PROPERTY(VerticalHeaderFormat verticalHeaderFormat READ verticalHeaderFormat WRITE setVerticalHeaderFormat)
    Q_PROPERTY(bool navigationBarVisible READ isNavigationBarVisible WRITE setNavigationBarVisible)
    Q_PROPERTY(
        HorizontalHeaderFormat horizontalHeaderFormat READ horizontalHeaderFormat WRITE setHorizontalHeaderFormat)
    Q_PROPERTY(bool dateEditEnabled READ isDateEditEnabled WRITE setDateEditEnabled)
    Q_PROPERTY(int dateEditAcceptDelay READ dateEditAcceptDelay WRITE setDateEditAcceptDelay)
public:
    enum SelectionMode { NoSelection, SingleSelection };
    Q_ENUM(SelectionMode)
    enum VerticalHeaderFormat { NoVerticalHeader, ISOWeekNumbers };
    Q_ENUM(VerticalHeaderFormat)
    enum HorizontalHeaderFormat { SingleLetterDayNames, ShortDayNames, LongDayNames, NoHorizontalHeader };
    Q_ENUM(HorizontalHeaderFormat)

    explicit QCalendarWidget(QWidget *parent = nullptr);
    ~QCalendarWidget() override;

    QDate selectedDate() const { return m_selected; }
    int yearShown() const { return m_year; }
    int monthShown() const { return m_month; }
    QDate minimumDate() const { return m_minimum; }
    void setMinimumDate(QDate date);
    QDate maximumDate() const { return m_maximum; }
    void setMaximumDate(QDate date);
    Qt::DayOfWeek firstDayOfWeek() const { return m_firstDay; }
    void setFirstDayOfWeek(Qt::DayOfWeek dayOfWeek);
    bool isGridVisible() const { return m_grid; }
    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode) { m_selectionMode = mode; }
    VerticalHeaderFormat verticalHeaderFormat() const { return m_vertical; }
    void setVerticalHeaderFormat(VerticalHeaderFormat format);
    bool isNavigationBarVisible() const;
    HorizontalHeaderFormat horizontalHeaderFormat() const { return m_horizontal; }
    void setHorizontalHeaderFormat(HorizontalHeaderFormat format);
    QCalendar calendar() const { return m_calendar; }
    void setCalendar(QCalendar calendar);
    void clearMinimumDate() { setMinimumDate(QDate(1752, 9, 14)); }
    void clearMaximumDate() { setMaximumDate(QDate(9999, 12, 31)); }
    QTextCharFormat headerTextFormat() const { return m_headerFormat; }
    void setHeaderTextFormat(const QTextCharFormat &format);
    QTextCharFormat weekdayTextFormat(Qt::DayOfWeek dayOfWeek) const;
    void setWeekdayTextFormat(Qt::DayOfWeek dayOfWeek, const QTextCharFormat &format);
    QMap<QDate, QTextCharFormat> dateTextFormat() const { return m_dateFormats; }
    QTextCharFormat dateTextFormat(QDate date) const { return m_dateFormats.value(date); }
    void setDateTextFormat(QDate date, const QTextCharFormat &format);
    bool isDateEditEnabled() const { return m_dateEditEnabled; }
    void setDateEditEnabled(bool enable) { m_dateEditEnabled = enable; }
    int dateEditAcceptDelay() const { return m_dateEditAcceptDelay; }
    void setDateEditAcceptDelay(int delay) { m_dateEditAcceptDelay = delay; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setSelectedDate(QDate date);
    void setDateRange(QDate min, QDate max);
    void setCurrentPage(int year, int month);
    void setGridVisible(bool show);
    void setNavigationBarVisible(bool visible);
    void showNextMonth();
    void showPreviousMonth();
    void showNextYear();
    void showPreviousYear();
    void showSelectedDate();
    void showToday();
    void updateCell(QDate date);
    void updateCells();

signals:
    /*!
        This signal is emitted when the selected date changes, by the user or by setSelectedDate().
    */
    void selectionChanged() { emitSignal(&QCalendarWidget::selectionChanged); }
    /*!
        This signal is emitted when the user clicks the \a date, if it is in the range.
    */
    void clicked(QDate date) { emitSignal(&QCalendarWidget::clicked, date); }
    /*!
        This signal is emitted when the user double-clicks the \a date or presses Return or Enter on it.
    */
    void activated(QDate date) { emitSignal(&QCalendarWidget::activated, date); }
    /*!
        This signal is emitted when the calendar shows another month: the \a month of the \a year.
    */
    void currentPageChanged(int year, int month) { emitSignal(&QCalendarWidget::currentPageChanged, year, month); }

protected:
    virtual void paintCell(QPainter *painter, const QRect &rect, QDate date) const;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QDate firstShown() const;
    QRect gridRect() const;
    QRect cellRect(int row, int column) const;
    QDate dateAt(const QPoint &pos) const;
    void select(QDate date, bool byUser);
    void updateTitle();
    int headerColumns() const { return m_vertical == ISOWeekNumbers ? 1 : 0; }
    int navigationHeight() const;
    int headlineHeight() const;
    // The grid of years that Material's date picker shows in the place of the days, from the year of minimumDate()
    // to that of maximumDate(), three to a row: its rows scroll, and one year is where the keyboard is.
    void setChoosingYear(bool choosing);
    void chooseYear(int year);
    int yearRows() const;
    int visibleYearRows() const;
    void scrollYears(int row);
    void moveYearCursor(int year);
    QRect yearRect(int year) const;
    int yearAt(const QPoint &pos) const; // 0 for none
    // The input mode of Material's modal date picker: a text field in the place of the month, where a date is typed
    // in the short format of the locale; the icon button at the end of the headline switches to it and back.
    void setInputMode(bool input);
    QRect modeButtonRect() const;
    void layoutModeButton();
    void fitWindowToMode(int before); // a dialog that held the picker's height before grows or shrinks with it
    void layoutDateField();
    QString dateFieldFormat() const;
    QDate dateFieldDate(QString *error) const; // an invalid date and why, for text that is not a date of the range
    void dateFieldEdited();
    void dateFieldFinished();

    QToolButton *m_modeButton = nullptr; // the icon button of the headline, shown with it
    QWidget *m_navigation = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
    QLabel *m_title = nullptr;
    QDate m_selected;
    QDate m_minimum;
    QDate m_maximum;
    int m_year = 2000;
    int m_month = 1;
    Qt::DayOfWeek m_firstDay = Qt::Monday;
    SelectionMode m_selectionMode = SingleSelection;
    VerticalHeaderFormat m_vertical = ISOWeekNumbers;
    bool m_grid = false;
    HorizontalHeaderFormat m_horizontal = ShortDayNames;
    QCalendar m_calendar;
    QTextCharFormat m_headerFormat;
    QMap<Qt::DayOfWeek, QTextCharFormat> m_weekdayFormats;
    QMap<QDate, QTextCharFormat> m_dateFormats;
    bool m_dateEditEnabled = true;
    int m_dateEditAcceptDelay = 1500;
    bool m_choosingYear = false;
    int m_yearRow = 0;    // the first row of years that is shown
    int m_yearCursor = 0; // the year that the keyboard is on
    bool m_navigationBar = true; // whether the program wants the navigation bar, which the input mode hides
    bool m_inputMode = false;
    bool m_typing = false;            // while the field selects what was typed, its text is left alone
    QLineEdit *m_dateField = nullptr; // made when the input mode is first shown
};
