// MIT License - see LICENSE
#include <QtWidgets/qdialogbuttonbox.h>

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qpushbutton.h>

#include <algorithm>

namespace {

struct StandardInfo {
    const char *text;
    QDialogButtonBox::StandardButton button;
    QDialogButtonBox::ButtonRole role;
};

const StandardInfo kStandard[] = {
    {"OK", QDialogButtonBox::Ok, QDialogButtonBox::AcceptRole},
    {"Save", QDialogButtonBox::Save, QDialogButtonBox::AcceptRole},
    {"Save All", QDialogButtonBox::SaveAll, QDialogButtonBox::AcceptRole},
    {"Open", QDialogButtonBox::Open, QDialogButtonBox::AcceptRole},
    {"Yes", QDialogButtonBox::Yes, QDialogButtonBox::YesRole},
    {"Yes to All", QDialogButtonBox::YesToAll, QDialogButtonBox::YesRole},
    {"No", QDialogButtonBox::No, QDialogButtonBox::NoRole},
    {"No to All", QDialogButtonBox::NoToAll, QDialogButtonBox::NoRole},
    {"Abort", QDialogButtonBox::Abort, QDialogButtonBox::RejectRole},
    {"Retry", QDialogButtonBox::Retry, QDialogButtonBox::AcceptRole},
    {"Ignore", QDialogButtonBox::Ignore, QDialogButtonBox::AcceptRole},
    {"Close", QDialogButtonBox::Close, QDialogButtonBox::RejectRole},
    {"Cancel", QDialogButtonBox::Cancel, QDialogButtonBox::RejectRole},
    {"Discard", QDialogButtonBox::Discard, QDialogButtonBox::DestructiveRole},
    {"Help", QDialogButtonBox::Help, QDialogButtonBox::HelpRole},
    {"Apply", QDialogButtonBox::Apply, QDialogButtonBox::ApplyRole},
    {"Reset", QDialogButtonBox::Reset, QDialogButtonBox::ResetRole},
    {"Restore Defaults", QDialogButtonBox::RestoreDefaults, QDialogButtonBox::ResetRole},
};

const StandardInfo *infoOf(QDialogButtonBox::StandardButton button) {
    for (const StandardInfo &info : kStandard)
        if (info.button == button)
            return &info;
    return nullptr;
}

// The order of the roles, with -1 for the stretch: the KDE layout of Qt.
constexpr int kHorizontalOrder[] = {
    QDialogButtonBox::HelpRole,   QDialogButtonBox::ResetRole, -1,
    QDialogButtonBox::YesRole,    QDialogButtonBox::NoRole,    QDialogButtonBox::ActionRole,
    QDialogButtonBox::AcceptRole, QDialogButtonBox::ApplyRole, QDialogButtonBox::DestructiveRole,
    QDialogButtonBox::RejectRole};
constexpr int kVerticalOrder[] = {QDialogButtonBox::AcceptRole,      QDialogButtonBox::ApplyRole,
                                  QDialogButtonBox::ActionRole,      QDialogButtonBox::YesRole,
                                  QDialogButtonBox::NoRole,          -1,
                                  QDialogButtonBox::DestructiveRole, QDialogButtonBox::RejectRole,
                                  QDialogButtonBox::ResetRole,       QDialogButtonBox::HelpRole};

} // namespace

/*!
    \class QDialogButtonBox
    \brief The QDialogButtonBox class lays out the buttons of a dialog in the order of the platform.

    A button box holds the buttons of a dialog, such as OK and Cancel, and puts them in the order that the desktop
    uses, whatever order they were added in. Each button has a role, and the box emits accepted(), rejected() or
    helpRequested() when a button of an accepting, rejecting or help role is clicked:

    \code
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    \endcode

    Standard buttons come with their text and role. Other buttons are added with addButton() and a role. In a
    QDialog the first button with the AcceptRole becomes the default button when the box is shown, unless the
    dialog already has one.

    \differences
    The buttons are always in the KDE order of Qt: help and reset buttons at the start, then a stretch, then the
    yes, no, action, accept, apply, destructive and reject buttons. The texts of the standard buttons have no
    mnemonics and are not translated, and the buttons have no icons. \c AlternateRole is not used.
    \enddifferences

    \sa QDialog, QMessageBox
*/

/*!
    \enum QDialogButtonBox::ButtonRole

    The role of a button, which says where it goes and what clicking it means.

    \value InvalidRole Not a valid role.
    \value AcceptRole Accepts the dialog, such as OK.
    \value RejectRole Rejects the dialog, such as Cancel.
    \value DestructiveRole Throws changes away and closes the dialog, such as Discard.
    \value ActionRole Does something in the dialog without closing it.
    \value HelpRole Asks for help.
    \value YesRole A Yes button.
    \value NoRole A No button.
    \value ResetRole Resets the fields of the dialog to their defaults.
    \value ApplyRole Applies the changes without closing the dialog.
    \omitvalue NRoles
*/

/*!
    \enum QDialogButtonBox::StandardButton

    The standard buttons, each with its text and role. StandardButtons is a typedef for
    \c{QFlags<StandardButton>}.

    \value NoButton No button.
    \value Ok OK, with the AcceptRole.
    \value Save Save, with the AcceptRole.
    \value SaveAll Save All, with the AcceptRole.
    \value Open Open, with the AcceptRole.
    \value Yes Yes, with the YesRole.
    \value YesToAll Yes to All, with the YesRole.
    \value No No, with the NoRole.
    \value NoToAll No to All, with the NoRole.
    \value Abort Abort, with the RejectRole.
    \value Retry Retry, with the AcceptRole.
    \value Ignore Ignore, with the AcceptRole.
    \value Close Close, with the RejectRole.
    \value Cancel Cancel, with the RejectRole.
    \value Discard Discard, with the DestructiveRole.
    \value Help Help, with the HelpRole.
    \value Apply Apply, with the ApplyRole.
    \value Reset Reset, with the ResetRole.
    \value RestoreDefaults Restore Defaults, with the ResetRole.
    \omitvalue FirstButton
    \omitvalue LastButton
*/

/*!
    \enum QDialogButtonBox::ButtonLayout

    The orders of buttons of the desktops that Qt knows.

    \value WinLayout Windows.
    \value MacLayout macOS.
    \value KdeLayout KDE, the order that Nt uses.
    \value GnomeLayout GNOME.
    \value AndroidLayout Android.
*/

/*!
    Constructs an empty horizontal button box with the given \a parent.
*/
QDialogButtonBox::QDialogButtonBox(QWidget *parent) : QDialogButtonBox(Qt::Horizontal, parent) {}

/*!
    Constructs an empty button box with the given \a orientation and \a parent.
*/
QDialogButtonBox::QDialogButtonBox(Qt::Orientation orientation, QWidget *parent)
    : QWidget(parent), m_orientation(orientation) {
    m_layout = new QBoxLayout(orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom, this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
}

/*!
    Constructs a horizontal button box with the standard \a buttons and the given \a parent.
*/
QDialogButtonBox::QDialogButtonBox(StandardButtons buttons, QWidget *parent)
    : QDialogButtonBox(buttons, Qt::Horizontal, parent) {}

/*!
    Constructs a button box with the standard \a buttons, the given \a orientation and \a parent.
*/
QDialogButtonBox::QDialogButtonBox(StandardButtons buttons, Qt::Orientation orientation, QWidget *parent)
    : QDialogButtonBox(orientation, parent) {
    setStandardButtons(buttons);
}

/*!
    Destroys the button box and its buttons.
*/
QDialogButtonBox::~QDialogButtonBox() {
    // The buttons are children and go with the box; stop following them first.
    for (const Entry &entry : m_buttons)
        QObject::disconnect(entry.button, nullptr, this, nullptr);
    m_buttons.clear();
}

/*!
    Sets the direction the buttons are laid out in to \a orientation.

    \sa orientation()
*/
void QDialogButtonBox::setOrientation(Qt::Orientation orientation) {
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    m_layout->setDirection(orientation == Qt::Horizontal ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    setSizePolicy(orientation == Qt::Horizontal ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                                : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding));
    relayout();
}

/*!
    \fn Qt::Orientation QDialogButtonBox::orientation() const

    Returns the direction the buttons are laid out in, Qt::Horizontal by default.

    \sa setOrientation()
*/

/*!
    Adds \a button to the box with the given \a role. The box takes the button as a child. A button with the
    InvalidRole is not added.

    \sa removeButton()
*/
void QDialogButtonBox::addButton(QAbstractButton *button, ButtonRole role) {
    if (!button || role <= InvalidRole || role >= NRoles)
        return;
    add(button, role, NoButton);
}

/*!
    \overload

    Creates a push button with the given \a text and \a role, adds it, and returns it.
*/
QPushButton *QDialogButtonBox::addButton(const QString &text, ButtonRole role) {
    if (role <= InvalidRole || role >= NRoles)
        return nullptr;
    auto *push = new QPushButton(text, this);
    add(push, role, NoButton);
    return push;
}

/*!
    \overload

    Adds the standard \a button, unless the box has it already, and returns it.
*/
QPushButton *QDialogButtonBox::addButton(StandardButton button) {
    if (QPushButton *existing = this->button(button))
        return existing;
    return createStandardButton(button);
}

/*!
    Removes \a button from the box without deleting it; the button no longer has a parent.

    \sa addButton(), clear()
*/
void QDialogButtonBox::removeButton(QAbstractButton *button) {
    const auto it = std::find_if(m_buttons.begin(), m_buttons.end(),
                                 [button](const Entry &entry) { return entry.button == button; });
    if (it == m_buttons.end())
        return;
    QObject::disconnect(button, nullptr, this, nullptr);
    m_buttons.erase(it);
    m_layout->removeWidget(button);
    button->setParent(nullptr);
    relayout();
}

/*!
    Deletes all the buttons of the box.

    \sa removeButton()
*/
void QDialogButtonBox::clear() {
    std::vector<Entry> old;
    old.swap(m_buttons);
    for (const Entry &entry : old) {
        QObject::disconnect(entry.button, nullptr, this, nullptr);
        delete entry.button;
    }
    relayout();
}

/*!
    Returns the buttons of the box, in the order they were added.
*/
QList<QAbstractButton *> QDialogButtonBox::buttons() const {
    QList<QAbstractButton *> list;
    for (const Entry &entry : m_buttons)
        list.append(entry.button);
    return list;
}

/*!
    Returns the role of \a button, or InvalidRole if it is not in the box.
*/
QDialogButtonBox::ButtonRole QDialogButtonBox::buttonRole(QAbstractButton *button) const {
    for (const Entry &entry : m_buttons)
        if (entry.button == button)
            return entry.role;
    return InvalidRole;
}

/*!
    Makes the standard buttons of the box \a buttons: the standard buttons it has are deleted, and the ones in
    \a buttons are created. Buttons that are not standard stay.

    \sa standardButtons()
*/
void QDialogButtonBox::setStandardButtons(StandardButtons buttons) {
    std::vector<QAbstractButton *> doomed;
    for (const Entry &entry : m_buttons)
        if (entry.standard != NoButton)
            doomed.push_back(entry.button);
    for (QAbstractButton *button : doomed) {
        QObject::disconnect(button, nullptr, this, nullptr);
        m_buttons.erase(std::find_if(m_buttons.begin(), m_buttons.end(),
                                     [button](const Entry &entry) { return entry.button == button; }));
        delete button;
    }
    for (const StandardInfo &info : kStandard)
        if (buttons.testFlag(info.button))
            createStandardButton(info.button);
    relayout();
}

/*!
    Returns the standard buttons of the box.

    \sa setStandardButtons()
*/
QDialogButtonBox::StandardButtons QDialogButtonBox::standardButtons() const {
    StandardButtons buttons;
    for (const Entry &entry : m_buttons)
        if (entry.standard != NoButton)
            buttons |= entry.standard;
    return buttons;
}

/*!
    Returns which standard button \a button is, or NoButton if it is not a standard button of the box.

    \sa button()
*/
QDialogButtonBox::StandardButton QDialogButtonBox::standardButton(QAbstractButton *button) const {
    for (const Entry &entry : m_buttons)
        if (entry.button == button)
            return entry.standard;
    return NoButton;
}

/*!
    Returns the push button of the standard button \a which, or \c nullptr if the box does not have it.

    \sa standardButton()
*/
QPushButton *QDialogButtonBox::button(StandardButton which) const {
    for (const Entry &entry : m_buttons)
        if (entry.standard == which && which != NoButton)
            return dynamic_cast<QPushButton *>(entry.button);
    return nullptr;
}

/*!
    Sets whether the buttons are centered in the box, rather than in the order of the platform with a stretch
    between the groups, to \a center.

    \sa centerButtons()
*/
void QDialogButtonBox::setCenterButtons(bool center) {
    if (m_center == center)
        return;
    m_center = center;
    relayout();
}

/*!
    \fn bool QDialogButtonBox::centerButtons() const

    Returns whether the buttons are centered in the box. The default is \c false.

    \sa setCenterButtons()
*/

/*!
    \reimp

    In a QDialog without a default button, the first push button with the AcceptRole or the YesRole becomes the
    default.
*/
void QDialogButtonBox::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    auto *dialog = dynamic_cast<QDialog *>(window());
    if (!dialog)
        return;
    for (QPushButton *push : dialog->findChildren<QPushButton *>())
        if (push->isDefault() && push->window() == dialog)
            return;
    for (const ButtonRole role : {AcceptRole, YesRole}) {
        for (const Entry &entry : m_buttons) {
            auto *push = dynamic_cast<QPushButton *>(entry.button);
            if (push && entry.role == role) {
                push->setDefault(true);
                return;
            }
        }
    }
}

void QDialogButtonBox::add(QAbstractButton *button, ButtonRole role, StandardButton standard) {
    for (Entry &entry : m_buttons) {
        if (entry.button == button) {
            entry.role = role;
            entry.standard = standard;
            relayout();
            return;
        }
    }
    if (button->parentWidget() != this)
        button->setParent(this);
    m_buttons.push_back({button, role, standard});
    if (isVisible())
        button->show();
    QObject::connect(button, &QAbstractButton::clicked, this, [this, button] { buttonClicked(button); });
    QObject::connect(button, &QObject::destroyed, this, [this](QObject *object) { forget(object); });
    relayout();
}

QPushButton *QDialogButtonBox::createStandardButton(StandardButton which) {
    const StandardInfo *info = infoOf(which);
    if (!info)
        return nullptr;
    auto *push = new QPushButton(QString::fromUtf8(info->text), this);
    add(push, info->role, which);
    return push;
}

// Puts the buttons in the layout in the order of their roles.
void QDialogButtonBox::relayout() {
    while (QLayoutItem *item = m_layout->takeAt(0))
        delete item;
    const bool horizontal = m_orientation == Qt::Horizontal;
    if (m_center)
        m_layout->addStretch();
    const auto addRole = [this](int role) {
        for (const Entry &entry : m_buttons)
            if (entry.role == role)
                m_layout->addWidget(entry.button);
    };
    if (horizontal) {
        for (const int role : kHorizontalOrder) {
            if (role < 0) {
                if (!m_center)
                    m_layout->addStretch();
            } else {
                addRole(role);
            }
        }
    } else {
        for (const int role : kVerticalOrder) {
            if (role < 0) {
                if (!m_center)
                    m_layout->addStretch();
            } else {
                addRole(role);
            }
        }
    }
    if (m_center)
        m_layout->addStretch();
    updateGeometry();
}

void QDialogButtonBox::buttonClicked(QAbstractButton *button) {
    const ButtonRole role = buttonRole(button);
    emit clicked(button);
    switch (role) {
    case AcceptRole:
    case YesRole:
        emit accepted();
        break;
    case RejectRole:
    case NoRole:
        emit rejected();
        break;
    case HelpRole:
        emit helpRequested();
        break;
    default:
        break;
    }
}

void QDialogButtonBox::forget(QObject *button) {
    m_buttons.erase(std::remove_if(m_buttons.begin(), m_buttons.end(),
                                   [button](const Entry &entry) { return entry.button == button; }),
                    m_buttons.end());
    relayout();
}

#include "moc_qdialogbuttonbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
