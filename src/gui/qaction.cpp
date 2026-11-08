// MIT License - see LICENSE
#include <QtGui/qaction.h>

#include <QtGui/qactiongroup.h>

#include <algorithm>

/*!
    \class QAction
    \brief The QAction class describes something that a user can do, which a menu, a tool button and a shortcut share.

    An action has a text, a check state, a shortcut, and a state of being enabled and visible, in one place. A widget
    that shows the action, such as a QMenu, follows it: when the action is disabled, so is the menu entry, and when the
    user chooses the entry the action is triggered. Connect to triggered() to do the work:

    \code
    QAction *open = new QAction("&Open...", this);
    open->setShortcut(QKeySequence::Open);
    connect(open, &QAction::triggered, this, &Editor::openFile);
    menu->addAction(open);
    \endcode

    A checkable action toggles each time it is triggered; toggled() reports the new state. Actions in a QActionGroup
    can be exclusive, like radio buttons. An action with a shortcut is triggered by that key combination whenever
    it has been added to a widget of the window that has the focus, with QWidget::addAction() or to a menu.

    \differences
    There are no icons (\c icon(), \c setIcon()), no \c menuRole(), \c priority(), \c shortcuts() (only one), \c
    shortcutContext() (the shortcut works in the window of the widget that has the action), \c autoRepeat(), \c
    whatsThis() or \c iconVisibleInMenu(). \c menu() is a template that takes the pointer type to cast to, as in Qt 6,
    and setMenu() takes a \c QObject so that QtGui does not depend on QtWidgets.
    \enddifferences

    \sa QActionGroup, QMenu, QKeySequence
*/

/*!
    \enum QAction::ActionEvent

    This enum says what activate() does.

    \value Trigger Triggers the action, as the user does by choosing it.
    \value Hover Reports that the user is on the action, as in a menu.
*/

/*!
    Constructs an action with the given \a parent.
*/
QAction::QAction(QObject *parent) : QObject(parent) {}

/*!
    Constructs an action with the given \a text and \a parent. A \c{&} in the text marks the mnemonic, the letter that a
    menu underlines.
*/
QAction::QAction(const QString &text, QObject *parent) : QObject(parent), m_text(text) {}

/*!
    Destroys the action, and removes it from its group.
*/
QAction::~QAction() {
    if (m_group)
        m_group->removeAction(this);
}

/*!
    \fn QString QAction::text() const

    Returns the text of the action.

    \sa setText()
*/

/*!
    Sets the text of the action to \a text.

    \sa text()
*/
void QAction::setText(const QString &text) {
    if (m_text == text)
        return;
    m_text = text;
    emit changed();
}

/*!
    Returns the text of the action without the mnemonic marks: \c{"&Open"} gives \c{"Open"}.
*/
QString QAction::iconText() const {
    QString plain;
    for (qsizetype i = 0; i < m_text.size(); ++i) {
        if (m_text.at(i) == '&' && i + 1 < m_text.size()) {
            ++i;
            if (m_text.at(i) == '&')
                plain += '&';
            else
                plain += m_text.at(i);
            continue;
        }
        plain += m_text.at(i);
    }
    return plain;
}

/*!
    Returns the tool tip of the action, or iconText() if it has none.

    \sa setToolTip()
*/
QString QAction::toolTip() const { return m_toolTip.isEmpty() ? iconText() : m_toolTip; }

/*!
    Sets the tool tip of the action to \a tip.

    \sa toolTip()
*/
void QAction::setToolTip(const QString &tip) {
    if (m_toolTip == tip)
        return;
    m_toolTip = tip;
    emit changed();
}

/*!
    \fn QString QAction::statusTip() const

    Returns the status tip of the action.

    \sa setStatusTip()
*/

/*!
    Sets the status tip of the action to \a tip.

    \sa statusTip()
*/
void QAction::setStatusTip(const QString &tip) {
    if (m_statusTip == tip)
        return;
    m_statusTip = tip;
    emit changed();
}

/*!
    \fn QFont QAction::font() const

    Returns the font that a menu uses for the action; the default font if none was set.

    \sa setFont()
*/

/*!
    Sets the font that a menu uses for the action to \a font.

    \sa font()
*/
void QAction::setFont(const QFont &font) {
    if (m_font == font)
        return;
    m_font = font;
    emit changed();
}

/*!
    \fn QKeySequence QAction::shortcut() const

    Returns the key combination that triggers the action.

    \sa setShortcut()
*/

/*!
    Sets the key combination that triggers the action to \a shortcut. It works when the action has been added to a
    widget of the window that has the focus, or to a menu of one.

    \sa shortcut()
*/
void QAction::setShortcut(const QKeySequence &shortcut) {
    if (m_shortcut == shortcut)
        return;
    m_shortcut = shortcut;
    emit changed();
}

/*!
    \fn QVariant QAction::data() const

    Returns the data that the application has put in the action; an invalid variant if none.

    \sa setData()
*/

/*!
    \fn void QAction::setData(const QVariant &data)

    Sets the data of the action to \a data. It is for the application, which can tell actions of a menu apart with it.

    \sa data()
*/

/*!
    \fn bool QAction::isCheckable() const

    Returns \c true if the action can be checked; otherwise returns \c false.

    \sa setCheckable()
*/

/*!
    Sets whether the action can be checked to \a checkable. A checkable action is checked or not, and each trigger
    toggles it.

    \sa isCheckable()
*/
void QAction::setCheckable(bool checkable) {
    if (m_checkable == checkable)
        return;
    m_checkable = checkable;
    emit changed();
    emit checkableChanged(checkable);
    if (m_checked)
        emit toggled(checkable);
}

/*!
    \fn bool QAction::isChecked() const

    Returns \c true if the action is checkable and checked; otherwise returns \c false.

    \sa setChecked()
*/

/*!
    Sets whether the action is checked to \a checked. Nothing happens on a change if the action is not checkable, though
    it is remembered. toggled() is emitted when the state changes, and in a QActionGroup that is exclusive the other
    action that was checked is unchecked first.

    \sa isChecked(), toggle()
*/
void QAction::setChecked(bool checked) {
    if (m_checked == checked)
        return;
    m_checked = checked;
    if (!m_checkable)
        return;
    emit changed();
    if (m_group)
        m_group->actionToggled(this, checked);
    emit toggled(checked);
}

/*!
    Toggles the check state of the action.

    \sa setChecked()
*/
void QAction::toggle() { setChecked(!isChecked()); }

/*!
    \fn bool QAction::isEnabled() const

    Returns \c true if the action is enabled and visible; otherwise returns \c false.

    \sa setEnabled()
*/

/*!
    Sets whether the action is enabled to \a enabled. A disabled action does not trigger, and a menu shows it dimmed.

    \sa isEnabled(), setVisible()
*/
void QAction::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    const bool was = isEnabled();
    m_enabled = enabled;
    emit changed();
    if (was != isEnabled())
        emit enabledChanged(isEnabled());
}

/*!
    \fn void QAction::setDisabled(bool disabled)

    Disables the action if \a disabled is \c true, and enables it if it is \c false.

    \sa setEnabled()
*/

/*!
    \fn bool QAction::isVisible() const

    Returns \c true if the action is shown; otherwise returns \c false.

    \sa setVisible()
*/

/*!
    Sets whether the action is shown to \a visible. A menu leaves a hidden action out, and it cannot be triggered.

    \sa isVisible()
*/
void QAction::setVisible(bool visible) {
    if (m_visible == visible)
        return;
    const bool was = isEnabled();
    m_visible = visible;
    emit changed();
    if (was != isEnabled())
        emit enabledChanged(isEnabled());
    emit visibleChanged();
}

/*!
    \fn bool QAction::isSeparator() const

    Returns \c true if the action is a separator, a line between the entries of a menu; otherwise returns \c false.

    \sa setSeparator()
*/

/*!
    Sets whether the action is a separator to \a separator. QMenu::addSeparator() makes one.

    \sa isSeparator()
*/
void QAction::setSeparator(bool separator) {
    if (m_separator == separator)
        return;
    m_separator = separator;
    emit changed();
}

/*!
    \fn QActionGroup *QAction::actionGroup() const

    Returns the group that the action is in; \c nullptr if it is in none.

    \sa setActionGroup()
*/

/*!
    Puts the action in the \a group, and takes it out of the one it was in. A \c nullptr takes it out of its group.

    \sa actionGroup(), QActionGroup::addAction()
*/
void QAction::setActionGroup(QActionGroup *group) {
    if (group == m_group)
        return;
    if (m_group)
        m_group->removeAction(this);
    if (group)
        group->addAction(this);
}

/*!
    \fn template <typename T> T QAction::menu() const

    Returns the menu of the action cast to the pointer type \c T, as in \c{action->menu<QMenu *>()}. It is \c nullptr if
    the action has no menu, or if it is not a \c T.

    \sa setMenu()
*/

/*!
    Sets the menu of the action to \a menu, a QMenu. A menu bar and a menu show the action as a submenu. The action does not
    own the menu.

    \sa menu()
*/
void QAction::setMenu(QObject *menu) {
    if (m_menu == menu)
        return;
    m_menu = menu;
    emit changed();
}

/*!
    Returns the widgets that the action has been added to, such as menus and menu bars.
*/
QList<QObject *> QAction::associatedObjects() const {
    QList<QObject *> list;
    for (const QPointer<QObject> &object : m_associated)
        if (object)
            list.append(object.data());
    return list;
}

void QAction::addAssociated(QObject *object) {
    for (const QPointer<QObject> &known : m_associated)
        if (known == object)
            return;
    m_associated.append(QPointer<QObject>(object));
}

void QAction::removeAssociated(QObject *object) {
    for (qsizetype i = 0; i < m_associated.size(); ++i)
        if (!m_associated.at(i) || m_associated.at(i) == object) {
            m_associated.removeAt(i);
            --i;
        }
}

/*!
    Does what the user does to the action, as \a event says: \c Trigger triggers it, and \c Hover reports that it is hovered. An action that is
    not enabled does not trigger, but it can be hovered. Triggering a checkable action toggles it first, except
    that the checked action of an exclusive group stays checked.

    \sa trigger(), hover()
*/
void QAction::activate(ActionEvent event) {
    QPointer<QAction> guard(this);
    QActionGroup *group = m_group;
    if (event == Hover) {
        if (group)
            emit group->hovered(this);
        if (guard)
            emit hovered();
        return;
    }
    if (!isEnabled())
        return;
    if (m_checkable) {
        if (m_checked && group && group->exclusionPolicy() == QActionGroup::ExclusionPolicy::Exclusive) {
            emit group->triggered(this);
            if (guard)
                emit triggered(true);
            return;
        }
        setChecked(!m_checked);
        if (!guard)
            return;
    }
    if (group)
        emit group->triggered(this);
    if (guard)
        emit triggered(isChecked());
}

/*!
    \fn void QAction::trigger()

    Triggers the action, as if the user had chosen it: a checkable action toggles, and triggered() is emitted. It does
    nothing if the action is not enabled.

    \sa activate(), hover()
*/

/*!
    \fn void QAction::hover()

    Emits hovered(), as if the user had moved to the action in a menu.

    \sa trigger()
*/

#include "moc_qaction.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
