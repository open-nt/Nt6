// MIT License - see LICENSE
#include <QtWidgets/qabstractbutton.h>

#include <QtCore/qpointer.h>
#include <QtGui/qevent.h>
#include <QtWidgets/qbuttongroup.h>

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
    There are no icons, shortcuts (\c setShortcut()), auto-repeat, \c animateClick(), \c setIcon() or
    \c setIconSize().
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
QAbstractButton::~QAbstractButton() {
    if (m_group)
        m_group->removeButton(this);
}

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
    \fn QButtonGroup *QAbstractButton::group() const

    Returns the group that the button belongs to, or \c nullptr if it is in none.

    \sa QButtonGroup::addButton()
*/

/*!
    Checks a checkable button if \a checked is \c true, and unchecks it otherwise. Emits toggled() if the state
    changes. Nothing happens if the button is not checkable.

    \sa isChecked()
*/
void QAbstractButton::setChecked(bool checked) {
    if (!m_checkable || m_checked == checked)
        return;
    const bool groupExclusive = m_group && m_group->exclusive();
    const bool siblingsExclusive = !m_group && m_autoExclusive && parentWidget();
    // The checked button of an exclusive group cannot be unchecked; check another one instead.
    if (!checked && (groupExclusive || siblingsExclusive))
        return;
    if (checked && groupExclusive) {
        for (QAbstractButton *other : m_group->buttons()) {
            if (other != this && other->m_checked)
                other->changeChecked(false);
        }
    } else if (checked && siblingsExclusive) {
        for (QObject *child : parentWidget()->children()) {
            auto *other = dynamic_cast<QAbstractButton *>(child);
            if (other && other != this && !other->m_group && other->m_autoExclusive && other->m_checked)
                other->changeChecked(false);
        }
    }
    changeChecked(checked);
}

void QAbstractButton::changeChecked(bool checked) {
    if (m_checked == checked)
        return;
    m_checked = checked;
    update();
    QPointer<QAbstractButton> guard(this);
    emit toggled(checked);
    if (guard && m_group) {
        emit m_group->idToggled(m_group->id(this), checked);
        if (guard && m_group)
            emit m_group->buttonToggled(this, checked);
    }
}

void QAbstractButton::emitPressed() {
    QPointer<QAbstractButton> guard(this);
    emit pressed();
    if (guard && m_group) {
        emit m_group->idPressed(m_group->id(this));
        if (guard && m_group)
            emit m_group->buttonPressed(this);
    }
}

void QAbstractButton::emitReleased() {
    QPointer<QAbstractButton> guard(this);
    emit released();
    if (guard && m_group) {
        emit m_group->idReleased(m_group->id(this));
        if (guard && m_group)
            emit m_group->buttonReleased(this);
    }
}

void QAbstractButton::emitClicked() {
    QPointer<QAbstractButton> guard(this);
    emit clicked(m_checked);
    if (guard && m_group) {
        emit m_group->idClicked(m_group->id(this));
        if (guard && m_group)
            emit m_group->buttonClicked(this);
    }
}

/*!
    \fn bool QAbstractButton::autoExclusive() const

    Returns whether the button is auto-exclusive: of all the auto-exclusive buttons that have the same parent
    widget, only one is checked at a time, and checking one unchecks the others. A checked button of such a group
    cannot be unchecked, only another one can be checked. A button in a QButtonGroup follows the group instead. The
    default is \c false, and radio buttons set it.

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
    emitClicked();
}

/*!
    Clicks the button as if the user did: it is pressed and released, so pressed(), released() and clicked() are emitted, and
    a checkable button toggles. Nothing happens if the button is disabled.
*/
void QAbstractButton::click() {
    if (!isEnabled())
        return;
    setDown(true);
    emitPressed();
    setDown(false);
    emitReleased();
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
    emitPressed();
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
    emitReleased();
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
            emitPressed();
        else
            emitReleased();
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
        emitPressed();
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
        emitReleased();
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
        emitReleased();
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
