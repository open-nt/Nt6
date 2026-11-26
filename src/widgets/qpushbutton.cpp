// MIT License - see LICENSE
#include <QtWidgets/qpushbutton.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qmaterialstyle.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qstyleoption.h>

namespace {

// The dialog that the button is in, or null.
QDialog *dialogOf(const QWidget *w) { return dynamic_cast<QDialog *>(w->window()); }

} // namespace

/*!
    \class QPushButton
    \brief The QPushButton class provides a command button.

    A push button is clicked to run a command. Connect to clicked():

    \code
    QPushButton *quit = new QPushButton("Quit", window);
    QObject::connect(quit, &QPushButton::clicked, &app, &QApplication::quit);
    \endcode

    The button is drawn by the style: it looks pressed while it is down, and shows the keyboard focus. Space
    clicks the button while it has the focus.

    In a QDialog, Return and Enter click the default button: the button that has the focus if it is an auto-default
    button, which the buttons of a dialog are, and otherwise the one set with setDefault():

    \code
    QPushButton *ok = new QPushButton("OK", dialog);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, dialog, &QDialog::accept);
    \endcode

    Push buttons: normal, with the keyboard focus, pressed, checked, flat, and disabled.

    \image widgets/qpushbutton.png QPushButton in the light color scheme

    \differences
    There are no menus (\c setMenu()).
    \enddifferences

    \sa QAbstractButton, QCheckBox
*/

/*!
    Constructs a button with no text and the given \a parent.
*/
QPushButton::QPushButton(QWidget *parent) : QAbstractButton(parent) {
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
}

/*!
    Constructs a button that shows \a text, with the given \a parent.
*/
QPushButton::QPushButton(const QString &text, QWidget *parent) : QAbstractButton(parent) {
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    setText(text);
}

/*!
    Destroys the button.
*/
QPushButton::~QPushButton() = default;

/*!
    Returns whether the button becomes the default button of its dialog while it has the focus. Unless it was set,
    it is \c true for a button in a QDialog and \c false otherwise.

    \sa setAutoDefault(), isDefault()
*/
bool QPushButton::autoDefault() const {
    if (m_autoDefault == AutoDefault::Auto)
        return dialogOf(this) != nullptr;
    return m_autoDefault == AutoDefault::On;
}

/*!
    Sets whether the button becomes the default button of its dialog while it has the focus to \a enable.

    \sa autoDefault()
*/
void QPushButton::setAutoDefault(bool enable) {
    m_autoDefault = enable ? AutoDefault::On : AutoDefault::Off;
    update();
}

/*!
    \fn bool QPushButton::isDefault() const

    Returns whether the button is the default button of its dialog: the one that Return clicks when no auto-default
    button has the focus. The default is \c false.

    \sa setDefault()
*/

/*!
    Makes the button the default button of its dialog if \a enable is \c true. The other push buttons of the dialog
    stop being the default. The default button is drawn with a stronger border.

    \sa isDefault(), autoDefault()
*/
void QPushButton::setDefault(bool enable) {
    if (m_default == enable)
        return;
    m_default = enable;
    if (enable) {
        if (QDialog *dialog = dialogOf(this)) {
            for (QPushButton *other : dialog->findChildren<QPushButton *>()) {
                if (other != this && other->m_default) {
                    other->m_default = false;
                    other->update();
                }
            }
        }
    }
    update();
}

/*!
    \fn bool QPushButton::isFlat() const

    Returns whether the button is flat: it has no bevel until the mouse is over it or it is pressed. The default is \c false.

    \sa setFlat()
*/

/*!
    Sets whether the button is flat to \a flat.

    \sa isFlat()
*/
void QPushButton::setFlat(bool flat) {
    m_flat = flat;
    update();
}

/*!
    \reimp
*/
QSize QPushButton::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    const QSize text = fm.size(Qt::TextSingleLine, this->text());
    QStyleOptionButton option;
    option.initFrom(this);
    option.text = this->text();
    option.icon = icon();
    option.iconSize = iconSize();
    return style()->sizeFromContents(QStyle::CT_PushButton, &option,
                                     this->text().isEmpty() ? QSize(0, fm.height()) : text, this);
}

/*!
    \reimp
*/
void QPushButton::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionButton option;
    initStyleOption(&option);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
    // A floating action button of the Material style shows its menu without an arrow.
    const bool fab =
        qobject_cast<const QMaterialStyle *>(style()) && property("materialButton").toString() == QLatin1String("fab");
    if (m_menu && !fab) {
        // The arrow that tells the button opens a menu.
        QStyleOption arrow = option;
        const int size = std::max(6, height() / 3);
        arrow.rect = QRect(width() - size - 6, (height() - size) / 2, size, size);
        style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &arrow, &painter, this);
    }
}

/*!
    \reimp
*/
void QPushButton::keyPressEvent(QKeyEvent *event) {
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && (autoDefault() || m_default)) {
        click();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

/*!
    \reimp
*/
void QPushButton::focusInEvent(QFocusEvent *event) {
    QAbstractButton::focusInEvent(event);
    update();
}

/*!
    \reimp
*/
void QPushButton::focusOutEvent(QFocusEvent *event) {
    QAbstractButton::focusOutEvent(event);
    update();
}

/*!
    \since 6.0

    Fills \a option with the state of the button, for the style to draw it.
*/
void QPushButton::initStyleOption(QStyleOptionButton *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->text = text();
    option->icon = icon();
    option->iconSize = iconSize();
    if (m_flat)
        option->features |= QStyleOptionButton::Flat;
    if (m_menu)
        option->features |= QStyleOptionButton::HasMenu;
    if (m_default || (autoDefault() && hasFocus()))
        option->features |= QStyleOptionButton::DefaultButton;
    option->state |= isDown() ? QStyle::State_Sunken : QStyle::State_Raised;
    if (isChecked())
        option->state |= QStyle::State_On;
}

/*!
    \since 6.0

    Returns the menu that the button opens, or \c nullptr.
*/
QMenu *QPushButton::menu() const { return static_cast<QMenu *>(m_menu.data()); }

/*!
    \since 6.0

    Makes the button open \a menu, below it, when it is pressed, and draws an arrow on it; \c nullptr removes the
    menu. The button does not take ownership of the menu.

    \code
    auto *menu = new QMenu(button);
    menu->addAction("Copy");
    button->setMenu(menu);
    \endcode
*/
void QPushButton::setMenu(QMenu *menu) {
    if (menu == m_menu)
        return;
    QObject::disconnect(m_menuConnection);
    m_menu = menu;
    if (menu)
        m_menuConnection = QObject::connect(this, &QAbstractButton::pressed, this, [this] { showMenu(); });
    updateGeometry();
    update();
}

/*!
    \since 6.0

    Opens the menu of the button below it, or above it if there is no room. The button stays down while the menu
    is open.
*/
void QPushButton::showMenu() {
    QMenu *m = menu();
    if (!m)
        return;
    setDown(true);
    QPointer<QPushButton> guard(this);
    QObject::connect(
        m, &QMenu::aboutToHide, this,
        [guard] {
            if (guard)
                guard->setDown(false);
        },
        Qt::SingleShotConnection);
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    if (!material || !material->popupFabMenu(this, m))
        m->popup(mapToGlobal(QPoint(0, height())));
}

#include "moc_qpushbutton.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
