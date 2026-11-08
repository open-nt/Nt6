// MIT License - see LICENSE
#include <QtWidgets/qabstractbutton.h>

#include <QtGui/qevent.h>

/*!
    \class QAbstractButton
    \brief The QAbstractButton class is the abstract base class of button widgets.

    A button has a text and can be pressed. It is clicked when the mouse button is released inside it,
    or when Space is pressed and released while the button has the focus, or when click() is called.
    A checkable button toggles on each click.

    \code
    QPushButton *button = new QPushButton("Subscribe");
    button->setCheckable(true);
    connect(button, &QPushButton::toggled, [](bool on) { qDebug() << on; });
    \endcode

    \differences
    There are no icons, shortcuts (\c setShortcut()), auto-repeat, \c animateClick(), \c QButtonGroup, \c setIcon() or
    \c setIconSize(). The only exclusive buttons are the ones that are auto-exclusive, as radio buttons are.
    \enddifferences

    \sa QPushButton, QCheckBox
*/

/*!
    Constructs a button with the given \a parent.
*/
QAbstractButton::QAbstractButton(QWidget *parent) : QWidget(parent) { setFocusPolicy(Qt::StrongFocus); }

/*!
    Destroys the button.
*/
QAbstractButton::~QAbstractButton() = default;

/*!
    \fn QString QAbstractButton::text() const

    Returns the text of the button.

    \sa setText()
*/

/*!
    Sets the text of the button to \a text.

    \sa text()
*/
void QAbstractButton::setText(const QString &text) {
    if (m_text == text)
        return;
    m_text = text;
    updateGeometry();
    update();
}

/*!
    \fn bool QAbstractButton::isCheckable() const

    Returns whether the button is checkable: it toggles when it is clicked. The default is \c false.

    \sa setCheckable()
*/

/*!
    Sets whether the button is checkable to \a checkable.

    \sa isCheckable()
*/
void QAbstractButton::setCheckable(bool checkable) {
    m_checkable = checkable;
    update();
}

/*!
    \fn bool QAbstractButton::isChecked() const

    Returns whether a checkable button is checked.

    \sa setChecked()
*/

/*!
    Checks a checkable button if \a checked is \c true, and unchecks it otherwise. Emits toggled() if the state
    changes. Nothing happens if the button is not checkable.

    \sa isChecked()
*/
void QAbstractButton::setChecked(bool checked) {
    if (!m_checkable || m_checked == checked)
        return;
    // The checked button of an exclusive group cannot be unchecked; check another one instead.
    if (!checked && m_autoExclusive && parentWidget())
        return;
    if (checked && m_autoExclusive && parentWidget()) {
        for (QObject *child : parentWidget()->children()) {
            auto *other = dynamic_cast<QAbstractButton *>(child);
            if (other && other != this && other->m_autoExclusive && other->m_checked) {
                other->m_checked = false;
                other->update();
                emit other->toggled(false);
            }
        }
    }
    m_checked = checked;
    update();
    emit toggled(checked);
}

/*!
    \fn bool QAbstractButton::autoExclusive() const

    Returns whether the button is auto-exclusive: of all the auto-exclusive buttons that have the same parent
    widget, only one is checked at a time, and checking one unchecks the others. A checked button of such a group
    cannot be unchecked, only another one can be checked. The default is \c false, and radio buttons set it.

    \sa setAutoExclusive()
*/

/*!
    \fn void QAbstractButton::setAutoExclusive(bool exclusive)

    Sets whether the button is auto-exclusive to \a exclusive.

    \sa autoExclusive()
*/

/*!
    \fn bool QAbstractButton::isDown() const

    Returns whether the button is pressed down.

    \sa setDown()
*/

/*!
    Sets whether the button looks pressed down to \a down. Mouse and key events do this
    by themselves.

    \sa isDown()
*/
void QAbstractButton::setDown(bool down) {
    if (m_down == down)
        return;
    m_down = down;
    update();
}

void QAbstractButton::activate() {
    if (m_checkable)
        setChecked(!m_checked);
    emit clicked(m_checked);
}

/*!
    Clicks the button as if the user did: it is pressed and released, so pressed(), released() and clicked() are emitted, and
    a checkable button toggles. Nothing happens if the button is disabled.
*/
void QAbstractButton::click() {
    if (!isEnabled())
        return;
    setDown(true);
    emit pressed();
    setDown(false);
    emit released();
    activate();
}

/*!
    Toggles a checkable button.

    \sa setChecked()
*/
void QAbstractButton::toggle() { setChecked(!m_checked); }

/*!
    Returns \c true if \a pos, in the coordinates of the button, is on the part that reacts to the mouse. The default
    is the whole rectangle.
*/
bool QAbstractButton::hitButton(const QPoint &pos) const { return rect().contains(pos); }

/*!
    \reimp
*/
void QAbstractButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !hitButton(event->pos())) {
        event->ignore();
        return;
    }
    setDown(true);
    emit pressed();
    event->accept();
}

/*!
    \reimp
*/
void QAbstractButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !m_down) {
        event->ignore();
        return;
    }
    const bool inside = hitButton(event->pos());
    setDown(false);
    emit released();
    if (inside)
        activate();
    event->accept();
}

/*!
    \reimp
*/
void QAbstractButton::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }
    // Moving out of the button while pressed raises it, and moving back presses it again.
    const bool inside = hitButton(event->pos());
    if (inside != m_down && !m_keyDown) {
        setDown(inside);
        if (inside)
            emit pressed();
        else
            emit released();
    }
    event->accept();
}

/*!
    \reimp
*/
void QAbstractButton::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_keyDown = true;
        setDown(true);
        emit pressed();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        event->accept();
        return;
    }
    event->ignore();
}

/*!
    \reimp
*/
void QAbstractButton::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat() && m_keyDown) {
        m_keyDown = false;
        setDown(false);
        emit released();
        activate();
        event->accept();
        return;
    }
    event->ignore();
}

/*!
    \reimp
*/
void QAbstractButton::focusOutEvent(QFocusEvent *event) {
    (void)event;
    if (m_keyDown) {
        m_keyDown = false;
        setDown(false);
        emit released();
    }
}

/*!
    \reimp
*/
void QAbstractButton::changeEvent(QEvent *event) {
    if (event->type() == QEvent::EnabledChange && !isEnabled() && m_down) {
        m_keyDown = false;
        setDown(false);
    }
    QWidget::changeEvent(event);
}

/*!
    \reimp
*/
void QAbstractButton::enterEvent(QEnterEvent *event) { QWidget::enterEvent(event); }

/*!
    \reimp
*/
void QAbstractButton::leaveEvent(QEvent *event) { QWidget::leaveEvent(event); }

#include "moc_qabstractbutton.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
