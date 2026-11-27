// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qvalidator.h>
#include <QtWidgets/qwidget.h>

class QLineEdit;
class QStyleOptionSpinBox;

class QAbstractSpinBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool wrapping READ wrapping WRITE setWrapping)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(QString specialValueText READ specialValueText WRITE setSpecialValueText)
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(bool keyboardTracking READ keyboardTracking WRITE setKeyboardTracking)
    Q_PROPERTY(bool frame READ hasFrame WRITE setFrame)
    Q_PROPERTY(Qt::Alignment alignment READ alignment WRITE setAlignment)
    Q_PROPERTY(bool accelerated READ isAccelerated WRITE setAccelerated)
    Q_PROPERTY(CorrectionMode correctionMode READ correctionMode WRITE setCorrectionMode)
    Q_PROPERTY(bool acceptableInput READ hasAcceptableInput)
    Q_PROPERTY(bool showGroupSeparator READ isGroupSeparatorShown WRITE setGroupSeparatorShown)
public:
    enum CorrectionMode { CorrectToPreviousValue, CorrectToNearestValue };
    Q_ENUM(CorrectionMode)
    enum StepType { DefaultStepType, AdaptiveDecimalStepType };
    Q_ENUM(StepType)
    enum StepEnabledFlag { StepNone = 0x00, StepUpEnabled = 0x01, StepDownEnabled = 0x02 };
    enum ButtonSymbols { UpDownArrows, PlusMinus, NoButtons };
    Q_ENUM(ButtonSymbols)
    Q_DECLARE_FLAGS(StepEnabled, StepEnabledFlag)

    explicit QAbstractSpinBox(QWidget *parent = nullptr);
    ~QAbstractSpinBox() override;

    QString text() const;
    QString specialValueText() const { return m_special; }
    void setSpecialValueText(const QString &text);
    bool wrapping() const { return m_wrapping; }
    void setWrapping(bool wrapping);
    bool isReadOnly() const { return m_readOnly; }
    void setReadOnly(bool readOnly);
    bool keyboardTracking() const { return m_keyboardTracking; }
    void setKeyboardTracking(bool enable) { m_keyboardTracking = enable; }
    ButtonSymbols buttonSymbols() const { return m_buttonSymbols; }
    void setButtonSymbols(ButtonSymbols bs);
    void interpretText();
    bool hasFrame() const { return m_frame; }
    void setFrame(bool frame);
    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment flag);
    bool isAccelerated() const { return m_accelerated; }
    void setAccelerated(bool on) { m_accelerated = on; }
    CorrectionMode correctionMode() const { return m_correction; }
    void setCorrectionMode(CorrectionMode cm) { m_correction = cm; }
    bool hasAcceptableInput() const;
    bool isGroupSeparatorShown() const { return m_groupSeparator; }
    void setGroupSeparatorShown(bool shown);
    virtual QValidator::State validate(QString &input, int &pos) const;
    virtual void fixup(QString &input) const;
    virtual void stepBy(int steps);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void stepUp();
    void stepDown();
    void selectAll();
    virtual void clear();

signals:
    /*!
        This signal is emitted when the user has finished editing the text: Return or Enter is pressed, or the spin box
        loses the focus.
    */
    void editingFinished() { emitSignal(&QAbstractSpinBox::editingFinished); }
    /*!
        \since 6.0
        This signal is emitted when Return or Enter is pressed in the spin box.
    */
    void returnPressed() { emitSignal(&QAbstractSpinBox::returnPressed); }

protected:
    virtual StepEnabled stepEnabled() const;
    QLineEdit *lineEdit() const { return m_edit; }
    void setLineEdit(QLineEdit *edit);
    void initStyleOption(QStyleOptionSpinBox *option) const;

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void timerEvent(QTimerEvent *event) override;

private:
    friend class QSpinBox;
    friend class QDoubleSpinBox;
    friend class QDateTimeEdit;
    enum Button { NoButton, UpButton, DownButton };

    // What QSpinBox and QDoubleSpinBox provide; the defaults leave the text as the user typed it.
    // The text of the current value, with the prefix and the suffix, or the special value text.
    virtual QString valueText() const;
    // The texts that the width of the spin box must fit, such as those of the smallest and the largest value.
    virtual QStringList sizeTexts() const;
    // Takes `number`, the text between the prefix and the suffix, as the value; `atMinimum` if the special value
    // text was typed. While the user types (`finished` is false) only a valid number in the range is taken; when the
    // user is done the text is settled.
    virtual void takeText(const QString &number, bool atMinimum, bool finished);
    // Whether the user may type `c`.
    virtual bool acceptsCharacter(QChar c) const;

    void refreshText();
    void layoutEdit();
    void editEdited(const QString &text);
    void editFinished();
    void interpret(bool finished);

    QLineEdit *m_edit = nullptr;
    QString m_prefix;
    QString m_suffix;
    QString m_special;
    bool m_wrapping = false;
    bool m_readOnly = false;
    bool m_keyboardTracking = true;
    ButtonSymbols m_buttonSymbols = UpDownArrows;
    Button m_pressed = NoButton;
    bool m_frame = true;
    bool m_accelerated = false;
    bool m_groupSeparator = false;
    CorrectionMode m_correction = CorrectToPreviousValue;
    void connectEdit();
    bool numberRange(int *begin, int *end) const; // of the number between the prefix and the suffix
    void keepCursorInNumber();
    Button buttonAt(const QPoint &pos) const;
    bool stepPressed(); // steps the way of the button held down; false if it cannot
    void startRepeat(bool atRate);
    void stopRepeat();
    int m_repeatTimer = 0;    // the next step while a button is held,
    bool m_repeating = false; // which comes at the rate rather than after the threshold
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractSpinBox::StepEnabled)
