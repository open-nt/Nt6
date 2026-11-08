// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qwidget.h>

class QLineEdit;

class QAbstractSpinBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool wrapping READ wrapping WRITE setWrapping)
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)
    Q_PROPERTY(QString specialValueText READ specialValueText WRITE setSpecialValueText)
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(bool keyboardTracking READ keyboardTracking WRITE setKeyboardTracking)
public:
    enum StepEnabledFlag { StepNone = 0x00, StepUpEnabled = 0x01, StepDownEnabled = 0x02 };
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
    void interpretText();
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

protected:
    virtual StepEnabled stepEnabled() const;
    QLineEdit *lineEdit() const { return m_edit; }

    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

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
    Button m_pressed = NoButton;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractSpinBox::StepEnabled)
