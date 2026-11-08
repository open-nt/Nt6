// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qpointer.h>
#include <QtWidgets/qabstractspinbox.h>
#include <QtWidgets/qcalendarwidget.h>

#include <vector>

class QDateTimeEdit : public QAbstractSpinBox {
    Q_OBJECT
    Q_PROPERTY(QDateTime dateTime READ dateTime WRITE setDateTime NOTIFY dateTimeChanged)
    Q_PROPERTY(QDate date READ date WRITE setDate NOTIFY dateChanged)
    Q_PROPERTY(QTime time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(QDateTime maximumDateTime READ maximumDateTime WRITE setMaximumDateTime)
    Q_PROPERTY(QDateTime minimumDateTime READ minimumDateTime WRITE setMinimumDateTime)
    Q_PROPERTY(Section currentSection READ currentSection WRITE setCurrentSection)
    Q_PROPERTY(Sections displayedSections READ displayedSections)
    Q_PROPERTY(QString displayFormat READ displayFormat WRITE setDisplayFormat)
    Q_PROPERTY(bool calendarPopup READ calendarPopup WRITE setCalendarPopup)
    Q_PROPERTY(int currentSectionIndex READ currentSectionIndex WRITE setCurrentSectionIndex)
    Q_PROPERTY(int sectionCount READ sectionCount)
public:
    enum Section {
        NoSection = 0x0000,
        AmPmSection = 0x0001,
        MSecSection = 0x0002,
        SecondSection = 0x0004,
        MinuteSection = 0x0008,
        HourSection = 0x0010,
        DaySection = 0x0100,
        MonthSection = 0x0200,
        YearSection = 0x0400,
        TimeSections_Mask = AmPmSection | MSecSection | SecondSection | MinuteSection | HourSection,
        DateSections_Mask = DaySection | MonthSection | YearSection
    };
    Q_DECLARE_FLAGS(Sections, Section)

    explicit QDateTimeEdit(QWidget *parent = nullptr);
    explicit QDateTimeEdit(const QDateTime &datetime, QWidget *parent = nullptr);
    explicit QDateTimeEdit(QDate date, QWidget *parent = nullptr);
    explicit QDateTimeEdit(QTime time, QWidget *parent = nullptr);
    ~QDateTimeEdit() override;

    QDateTime dateTime() const { return m_value; }
    QDate date() const { return m_value.date(); }
    QTime time() const { return m_value.time(); }

    QDateTime minimumDateTime() const { return m_minimum; }
    void clearMinimumDateTime();
    void setMinimumDateTime(const QDateTime &dt);
    QDateTime maximumDateTime() const { return m_maximum; }
    void clearMaximumDateTime();
    void setMaximumDateTime(const QDateTime &dt);
    void setDateTimeRange(const QDateTime &min, const QDateTime &max);
    QDate minimumDate() const { return m_minimum.date(); }
    void setMinimumDate(QDate min);
    void clearMinimumDate();
    QDate maximumDate() const { return m_maximum.date(); }
    void setMaximumDate(QDate max);
    void clearMaximumDate();
    void setDateRange(QDate min, QDate max);
    QTime minimumTime() const { return m_minimum.time(); }
    void setMinimumTime(QTime min);
    void clearMinimumTime();
    QTime maximumTime() const { return m_maximum.time(); }
    void setMaximumTime(QTime max);
    void clearMaximumTime();
    void setTimeRange(QTime min, QTime max);

    Sections displayedSections() const;
    Section currentSection() const;
    Section sectionAt(int index) const;
    void setCurrentSection(Section section);
    int currentSectionIndex() const;
    void setCurrentSectionIndex(int index);
    int sectionCount() const { return static_cast<int>(m_sections.size()); }
    void setSelectedSection(Section section);
    QString sectionText(Section section) const;

    QString displayFormat() const { return m_format; }
    void setDisplayFormat(const QString &format);
    bool calendarPopup() const { return m_calendarPopup; }
    void setCalendarPopup(bool enable);
    QCalendarWidget *calendarWidget() const;
    void setCalendarWidget(QCalendarWidget *calendarWidget);

    void clear() override;
    void stepBy(int steps) override;

public slots:
    void setDateTime(const QDateTime &dateTime);
    void setDate(QDate date);
    void setTime(QTime time);

signals:
    /*!
        This signal is emitted when the date or the time changes to \a datetime.
    */
    void dateTimeChanged(const QDateTime &datetime) { emitSignal(&QDateTimeEdit::dateTimeChanged, datetime); }
    /*!
        This signal is emitted when the time changes to \a time.
    */
    void timeChanged(QTime time) { emitSignal(&QDateTimeEdit::timeChanged, time); }
    /*!
        This signal is emitted when the date changes to \a date.
    */
    void dateChanged(QDate date) { emitSignal(&QDateTimeEdit::dateChanged, date); }

protected:
    virtual QDateTime dateTimeFromText(const QString &text) const;
    virtual QString textFromDateTime(const QDateTime &dt) const;
    StepEnabled stepEnabled() const override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct Token {
        Section section;
        QString pattern; // the letters of the format, or the literal text
        bool literal;
    };
    struct Span {
        Section section;
        int start;
        int length;
    };

    void init(const QDateTime &value, const QString &format);
    QString format(const QDateTime &dt, std::vector<Span> *spans) const;
    bool parse(const QString &text, QDateTime *result) const;
    void changeValue(const QDateTime &value, bool rewrite);
    QDateTime clamped(const QDateTime &value) const;
    int sectionIndexAt(int position) const;
    void showPopup();

    QString valueText() const override;
    QStringList sizeTexts() const override;
    void takeText(const QString &number, bool atMinimum, bool finished) override;
    bool acceptsCharacter(QChar c) const override;

    QDateTime m_value;
    QDateTime m_minimum;
    QDateTime m_maximum;
    QString m_format;
    std::vector<Token> m_tokens;
    mutable std::vector<Span> m_sections; // the sections of the current text
    QPointer<QCalendarWidget> m_calendar;
    int m_currentSection = 0;
    bool m_calendarPopup = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QDateTimeEdit::Sections)

class QDateEdit : public QDateTimeEdit {
    Q_OBJECT
public:
    explicit QDateEdit(QWidget *parent = nullptr);
    explicit QDateEdit(QDate date, QWidget *parent = nullptr);
    ~QDateEdit() override;
};

class QTimeEdit : public QDateTimeEdit {
    Q_OBJECT
public:
    explicit QTimeEdit(QWidget *parent = nullptr);
    explicit QTimeEdit(QTime time, QWidget *parent = nullptr);
    ~QTimeEdit() override;
};
