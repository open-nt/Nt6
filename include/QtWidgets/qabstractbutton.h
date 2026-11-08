// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QAbstractButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)
    Q_PROPERTY(bool down READ isDown WRITE setDown)
public:
    ~QAbstractButton() override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    bool isCheckable() const { return m_checkable; }
    void setCheckable(bool checkable);
    bool isChecked() const { return m_checked; }
    bool isDown() const { return m_down; }
    void setDown(bool down);

public slots:
    void setChecked(bool checked);
    void click();
    void toggle();

signals:
    /*!
        This signal is emitted when the button is pressed down.
    */
    void pressed() { emitSignal(&QAbstractButton::pressed); }
    /*!
        This signal is emitted when the button is released.
    */
    void released() { emitSignal(&QAbstractButton::released); }
    /*!
        This signal is emitted when the button is activated: pressed and released with the
        mouse, or triggered with the keyboard or click(). \a checked is the checked state of a
        checkable button.
    */
    void clicked(bool checked = false) { emitSignal(&QAbstractButton::clicked, checked); }
    /*!
        This signal is emitted when the checked state of a checkable button changes to
        \a checked.
    */
    void toggled(bool checked) { emitSignal(&QAbstractButton::toggled, checked); }

protected:
    explicit QAbstractButton(QWidget *parent = nullptr);
    virtual bool hitButton(const QPoint &pos) const;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void activate();

    QString m_text;
    bool m_checkable = false;
    bool m_checked = false;
    bool m_down = false;
    bool m_keyDown = false;
};
