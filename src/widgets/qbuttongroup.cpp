// MIT License - see LICENSE
#include <QtWidgets/qbuttongroup.h>

#include <QtWidgets/qabstractbutton.h>

#include <algorithm>

/*!
    \class QButtonGroup
    \brief The QButtonGroup class organizes buttons that belong together, such as a choice of radio buttons.

    A button group is not a widget: the buttons stay where they are in their layouts. An exclusive group, the default,
    lets one checkable button be checked at a time, whatever the parents of the buttons are. The group gives each button
    an id and reports clicks, presses and toggles of any of its buttons:

    \code
    auto *sizes = new QButtonGroup(window);
    sizes->addButton(new QRadioButton("Small"), 1);
    sizes->addButton(new QRadioButton("Large"), 2);
    connect(sizes, &QButtonGroup::idClicked, [](int id) { qDebug() << "size" << id; });
    \endcode

    A button belongs to one group at most. A button in a group is exclusive by the rules of the group, not by
    QAbstractButton::autoExclusive().

    \differences
    None known.
    \enddifferences

    \sa QAbstractButton, QRadioButton, QGroupBox
*/

/*!
    Constructs an exclusive button group with the given \a parent.
*/
QButtonGroup::QButtonGroup(QObject *parent) : QObject(parent) {}

/*!
    Destroys the group. The buttons stay; they no longer belong to a group.
*/
QButtonGroup::~QButtonGroup() {
    for (const Entry &entry : m_buttons)
        entry.button->m_group = nullptr;
}

/*!
    \fn bool QButtonGroup::exclusive() const

    Returns whether at most one button of the group can be checked. The default is \c true.

    \sa setExclusive()
*/

/*!
    \fn void QButtonGroup::setExclusive(bool exclusive)

    Sets whether at most one button of the group can be checked to \a exclusive. In an exclusive group the checked
    button cannot be unchecked by clicking it; checking another one unchecks it.

    \sa exclusive()
*/

/*!
    Adds \a button to the group with the given \a id. With an \a id of -1 the group gives the button a negative id of
    its own, starting at -2. A button that is in another group leaves it.

    \sa removeButton(), id()
*/
void QButtonGroup::addButton(QAbstractButton *button, int id) {
    if (!button)
        return;
    if (button->m_group && button->m_group != this)
        button->m_group->removeButton(button);
    if (id == -1)
        id = m_nextAutoId--;
    for (Entry &entry : m_buttons) {
        if (entry.button == button) {
            entry.id = id;
            return;
        }
    }
    m_buttons.push_back({button, id});
    button->m_group = this;
    // An exclusive group keeps one checked button: a newly added checked one wins.
    if (m_exclusive && button->isChecked()) {
        for (const Entry &entry : m_buttons) {
            if (entry.button != button && entry.button->isChecked())
                entry.button->changeChecked(false);
        }
    }
}

/*!
    Removes \a button from the group.

    \sa addButton()
*/
void QButtonGroup::removeButton(QAbstractButton *button) {
    const auto it = std::find_if(m_buttons.begin(), m_buttons.end(),
                                 [button](const Entry &entry) { return entry.button == button; });
    if (it == m_buttons.end())
        return;
    m_buttons.erase(it);
    button->m_group = nullptr;
}

/*!
    Returns the buttons of the group, in the order they were added.
*/
QList<QAbstractButton *> QButtonGroup::buttons() const {
    QList<QAbstractButton *> list;
    for (const Entry &entry : m_buttons)
        list.append(entry.button);
    return list;
}

/*!
    Returns the checked button of an exclusive group, or the first checked one of a group that is not exclusive, or
    \c nullptr if no button is checked.

    \sa checkedId()
*/
QAbstractButton *QButtonGroup::checkedButton() const {
    for (const Entry &entry : m_buttons)
        if (entry.button->isChecked())
            return entry.button;
    return nullptr;
}

/*!
    Returns the button with the given \a id, or \c nullptr if there is none.

    \sa id()
*/
QAbstractButton *QButtonGroup::button(int id) const {
    for (const Entry &entry : m_buttons)
        if (entry.id == id)
            return entry.button;
    return nullptr;
}

/*!
    Sets the id of \a button to \a id. An \a id of -1 is not taken, as in Qt.

    \sa id()
*/
void QButtonGroup::setId(QAbstractButton *button, int id) {
    if (id == -1)
        return;
    for (Entry &entry : m_buttons)
        if (entry.button == button)
            entry.id = id;
}

/*!
    Returns the id of \a button, or -1 if it is not in the group.

    \sa setId(), button()
*/
int QButtonGroup::id(QAbstractButton *button) const {
    for (const Entry &entry : m_buttons)
        if (entry.button == button)
            return entry.id;
    return -1;
}

/*!
    Returns the id of the checked button, or -1 if no button is checked.

    \sa checkedButton()
*/
int QButtonGroup::checkedId() const {
    for (const Entry &entry : m_buttons)
        if (entry.button->isChecked())
            return entry.id;
    return -1;
}

#include "moc_qbuttongroup.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
