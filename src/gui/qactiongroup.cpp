// MIT License - see LICENSE
#include <QtGui/qactiongroup.h>

/*!
    \class QActionGroup
    \brief The QActionGroup class groups actions together.

    Checkable actions in an exclusive group are like radio buttons: checking one unchecks the one that was checked.
    The group also enables, disables, shows and hides all its actions at once, and reports the triggered and hovered
    action with one signal:

    \code
    QActionGroup *align = new QActionGroup(this);
    QAction *left = align->addAction("Left");
    QAction *right = align->addAction("Right");
    left->setCheckable(true);
    right->setCheckable(true);
    left->setChecked(true);
    connect(align, &QActionGroup::triggered, this, &Editor::setAlignment);
    \endcode

    \differences
    The actions are not owned by the group unless the group made them with addAction(const QString &).
    \enddifferences

    \sa QAction
*/

/*!
    \enum QActionGroup::ExclusionPolicy

    This enum says how many of the checkable actions of a group can be checked.

    \value None Any number of them.
    \value Exclusive Exactly one, once one is checked: triggering the checked one does not uncheck it.
    \value ExclusiveOptional At most one: triggering the checked one unchecks it.
*/

/*!
    Constructs an empty, exclusive action group with the given \a parent.
*/
QActionGroup::QActionGroup(QObject *parent) : QObject(parent) {}

/*!
    Destroys the group. Its actions stay, outside of any group.
*/
QActionGroup::~QActionGroup() {
    for (const QPointer<QAction> &action : m_actions)
        if (action)
            action->m_group = nullptr;
}

/*!
    Adds \a action to the group, and returns it. An action is in one group at a time. The action takes the enabled and
    visible state of the group, and if the group is exclusive and the action is checked, it is the one that stays
    checked.

    \sa removeAction()
*/
QAction *QActionGroup::addAction(QAction *action) {
    if (!action)
        return nullptr;
    if (action->m_group == this)
        return action;
    if (action->m_group)
        action->m_group->removeAction(action);
    m_actions.append(QPointer<QAction>(action));
    action->m_group = this;
    if (!m_enabled)
        action->setEnabled(false);
    if (!m_visible)
        action->setVisible(false);
    if (isExclusive() && action->isChecked())
        actionToggled(action, true);
    return action;
}

/*!
    Creates an action with the given \a text, adds it to the group, and returns it. The group owns the action.

    \sa addAction()
*/
QAction *QActionGroup::addAction(const QString &text) { return addAction(new QAction(text, this)); }

/*!
    Takes \a action out of the group.

    \sa addAction()
*/
void QActionGroup::removeAction(QAction *action) {
    if (!action || action->m_group != this)
        return;
    action->m_group = nullptr;
    for (qsizetype i = 0; i < m_actions.size(); ++i)
        if (!m_actions.at(i) || m_actions.at(i) == action) {
            m_actions.removeAt(i);
            --i;
        }
}

/*!
    Returns the actions of the group, in the order in which they were added.
*/
QList<QAction *> QActionGroup::actions() const {
    QList<QAction *> list;
    for (const QPointer<QAction> &action : m_actions)
        if (action)
            list.append(action.data());
    return list;
}

/*!
    Returns the action that is checked; \c nullptr if none is.
*/
QAction *QActionGroup::checkedAction() const {
    for (const QPointer<QAction> &action : m_actions)
        if (action && action->isChecked())
            return action.data();
    return nullptr;
}

/*!
    \fn bool QActionGroup::isExclusive() const

    Returns \c true if the group allows one checked action at most; otherwise returns \c false.

    \sa setExclusive(), exclusionPolicy()
*/

/*!
    Sets whether the group is exclusive to \a exclusive.

    \sa isExclusive()
*/
void QActionGroup::setExclusive(bool exclusive) {
    setExclusionPolicy(exclusive ? ExclusionPolicy::Exclusive : ExclusionPolicy::None);
}

/*!
    \fn QActionGroup::ExclusionPolicy QActionGroup::exclusionPolicy() const

    Returns how many checkable actions of the group can be checked, \c Exclusive by default.

    \sa setExclusionPolicy()
*/

/*!
    Sets how many checkable actions of the group can be checked to \a policy.

    \sa exclusionPolicy()
*/
void QActionGroup::setExclusionPolicy(ExclusionPolicy policy) { m_policy = policy; }

/*!
    \fn bool QActionGroup::isEnabled() const

    Returns \c false if the group has been disabled; otherwise returns \c true.

    \sa setEnabled()
*/

/*!
    Enables or disables all the actions of the group, as \a enabled says.

    \sa isEnabled()
*/
void QActionGroup::setEnabled(bool enabled) {
    m_enabled = enabled;
    for (const QPointer<QAction> &action : m_actions)
        if (action)
            action->setEnabled(enabled);
}

/*!
    \fn void QActionGroup::setDisabled(bool disabled)

    Disables all the actions of the group if \a disabled is \c true, and enables them if it is \c false.

    \sa setEnabled()
*/

/*!
    \fn bool QActionGroup::isVisible() const

    Returns \c false if the group has been hidden; otherwise returns \c true.

    \sa setVisible()
*/

/*!
    Shows or hides all the actions of the group, as \a visible says.

    \sa isVisible()
*/
void QActionGroup::setVisible(bool visible) {
    m_visible = visible;
    for (const QPointer<QAction> &action : m_actions)
        if (action)
            action->setVisible(visible);
}

// An action was checked or unchecked: in an exclusive group the others are unchecked.
void QActionGroup::actionToggled(QAction *action, bool checked) {
    if (!checked || !isExclusive())
        return;
    for (const QPointer<QAction> &other : m_actions)
        if (other && other != action && other->isChecked())
            other->setChecked(false);
}

#include "moc_qactiongroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
