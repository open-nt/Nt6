// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtWidgets/qwidget.h>

class QLabel;
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
public:
    enum SelectionMode { NoSelection, SingleSelection };
    Q_ENUM(SelectionMode)
    enum VerticalHeaderFormat { NoVerticalHeader, ISOWeekNumbers };
    Q_ENUM(VerticalHeaderFormat)

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
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    QDate firstShown() const;
    QRect gridRect() const;
    QRect cellRect(int row, int column) const;
    QDate dateAt(const QPoint &pos) const;
    void select(QDate date, bool byUser);
    void updateTitle();
    int headerColumns() const { return m_vertical == ISOWeekNumbers ? 1 : 0; }

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
};
