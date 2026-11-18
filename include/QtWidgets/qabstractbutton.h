// MIT License - see LICENSE
#pragma once

#include <QtGui/qkeysequence.h>

#include <QtGui/qicon.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qwidget.h>

class QButtonGroup;

class QAction;

class QAbstractButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(QSize iconSize READ iconSize WRITE setIconSize)
    Q_PROPERTY(bool checkable READ isCheckable WRITE setCheckable)
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled USER true)
    Q_PROPERTY(bool down READ isDown WRITE setDown)
    Q_PROPERTY(bool autoExclusive READ autoExclusive WRITE setAutoExclusive)
    Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat)
    Q_PROPERTY(int autoRepeatDelay READ autoRepeatDelay WRITE setAutoRepeatDelay)
    Q_PROPERTY(int autoRepeatInterval READ autoRepeatInterval WRITE setAutoRepeatInterval)
public:
    ~QAbstractButton() override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon &icon);
    QSize iconSize() const;
    void setIconSize(const QSize &size);
    bool isCheckable() const { return m_checkable; }
    void setCheckable(bool checkable);
    bool isChecked() const { return m_checked; }
    bool isDown() const { return m_down; }
    void setDown(bool down);
    bool autoExclusive() const { return m_autoExclusive; }
    void setAutoExclusive(bool exclusive) { m_autoExclusive = exclusive; }
    QButtonGroup *group() const { return m_group; }
    bool autoRepeat() const { return m_autoRepeat; }
    void setAutoRepeat(bool enable);
    int autoRepeatDelay() const { return m_repeatDelay; }
    void setAutoRepeatDelay(int delay) { m_repeatDelay = delay; }
    int autoRepeatInterval() const { return m_repeatInterval; }
    void setAutoRepeatInterval(int interval) { m_repeatInterval = interval; }
    QKeySequence shortcut() const;
    void setShortcut(const QKeySequence &key);

public slots:
    void setChecked(bool checked);
    void click();
    void animateClick();
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
    virtual void checkStateSet();
    virtual void nextCheckState();
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
    friend class QButtonGroup;
    void activate();
    // Sets the checked state without the rules of exclusive groups, and emits toggled().
    void changeChecked(bool checked);
    void emitPressed();
    void emitReleased();
    void emitClicked();

    QString m_text;
    QIcon m_icon;
    QSize m_iconSize; // invalid: the size of the style
    bool m_checkable = false;
    bool m_autoExclusive = false;
    bool m_checked = false;
    bool m_down = false;
    bool m_keyDown = false;
    QButtonGroup *m_group = nullptr;
    QAction *m_shortcutAction = nullptr; // a child that holds the shortcut, made when one is set
    bool m_shortcutSet = false;          // setShortcut() was called; otherwise the mnemonic of the text is it
    void updateShortcut(const QKeySequence &key);
    void repeat();
    bool m_autoRepeat = false;
    int m_repeatDelay = 300;
    int m_repeatInterval = 100;
    QTimer m_repeatTimer;
};
