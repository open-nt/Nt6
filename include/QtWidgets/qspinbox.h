// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QLineEdit;

class QSpinBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(int singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(bool wrapping READ wrapping WRITE setWrapping)
public:
    explicit QSpinBox(QWidget *parent = nullptr);
    ~QSpinBox() override;

    int minimum() const { return m_minimum; }
    void setMinimum(int minimum);
    int maximum() const { return m_maximum; }
    void setMaximum(int maximum);
    void setRange(int minimum, int maximum);
    int value() const { return m_value; }
    int singleStep() const { return m_step; }
    void setSingleStep(int step);
    QString prefix() const { return m_prefix; }
    void setPrefix(const QString &prefix);
    QString suffix() const { return m_suffix; }
    void setSuffix(const QString &suffix);
    bool wrapping() const { return m_wrapping; }
    void setWrapping(bool wrapping);
    QString text() const;
    QString cleanText() const;
    void stepBy(int steps);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setValue(int value);
    void stepUp();
    void stepDown();
    void selectAll();

signals:
    /*!
        This signal is emitted when the value changes to \a value, whether the user changed it by typing or with the
        buttons, or setValue() did.
    */
    void valueChanged(int value) { emitSignal(&QSpinBox::valueChanged, value); }
    /*!
        This signal is emitted when the user has finished editing the text: Return or Enter is pressed, or the spin box
        loses the focus.
    */
    void editingFinished() { emitSignal(&QSpinBox::editingFinished); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    enum Button { NoButton, UpButton, DownButton };

    QString textFromValue(int value) const;
    void layoutEdit();
    void editEdited(const QString &text);
    void editFinished();

    QLineEdit *m_edit = nullptr;
    int m_minimum = 0;
    int m_maximum = 99;
    int m_value = 0;
    int m_step = 1;
    QString m_prefix;
    QString m_suffix;
    bool m_wrapping = false;
    Button m_pressed = NoButton;
};
