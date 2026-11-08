// MIT License - see LICENSE
#include <QtWidgets/qmessagebox.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>

#include <algorithm>

namespace {

constexpr int kIconSize = 32;

// The icon of a message box, drawn rather than taken from a theme: a round sign with "i", "?" or a cross, or a
// triangle with "!".
class MessageIcon : public QWidget {
public:
    explicit MessageIcon(QWidget *parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedSize(kIconSize, kIconSize);
    }

    void setIcon(QMessageBox::Icon icon) {
        m_icon = icon;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        if (m_icon == QMessageBox::NoIcon)
            return;
        QPainter painter(this);
        QFont font = this->font();
        font.setPixelSize(kIconSize * 3 / 4);
        font.setBold(true);
        painter.setFont(font);
        const QRect r(0, 0, kIconSize, kIconSize);
        QColor fill;
        QString mark;
        switch (m_icon) {
        case QMessageBox::Information:
            fill = QColor(0x2a, 0x7a, 0xd4);
            mark = QStringLiteral("i");
            break;
        case QMessageBox::Question:
            fill = QColor(0x2a, 0x7a, 0xd4);
            mark = QStringLiteral("?");
            break;
        case QMessageBox::Critical:
            fill = QColor(0xd0, 0x30, 0x30);
            break;
        case QMessageBox::Warning: {
            // A triangle, filled line by line, with "!" in it.
            painter.setPen(QColor(0xe8, 0xa0, 0x10));
            for (int y = 1; y < kIconSize - 1; ++y) {
                const int half = y / 2;
                painter.drawLine(kIconSize / 2 - half, y, kIconSize / 2 + half - 1, y);
            }
            painter.setPen(QColor(0x20, 0x20, 0x20));
            painter.drawText(QRect(0, kIconSize / 6, kIconSize, kIconSize * 5 / 6), Qt::AlignCenter,
                             QStringLiteral("!"));
            return;
        }
        case QMessageBox::NoIcon:
            return;
        }
        painter.setPen(fill);
        painter.setBrush(fill);
        painter.drawEllipse(r.adjusted(0, 0, -1, -1));
        painter.setPen(QColor(Qt::white));
        if (m_icon == QMessageBox::Critical) {
            const int m = kIconSize / 4;
            for (int d = -1; d <= 1; ++d) {
                painter.drawLine(m + d, m, kIconSize - 1 - m + d, kIconSize - 1 - m);
                painter.drawLine(kIconSize - 1 - m + d, m, m + d, kIconSize - 1 - m);
            }
        } else {
            painter.drawText(r, Qt::AlignCenter, mark);
        }
    }

private:
    QMessageBox::Icon m_icon = QMessageBox::NoIcon;
};

QDialogButtonBox::StandardButton toBox(QMessageBox::StandardButton button) {
    return static_cast<QDialogButtonBox::StandardButton>(static_cast<int>(button));
}

// The flags of one kind of standard buttons as the flags of the other, bit by bit.
template <typename To, typename From> To convertButtons(From from) {
    To to;
    for (int bit = QMessageBox::FirstButton; bit <= QMessageBox::LastButton; bit <<= 1)
        if (static_cast<int>(from) & bit)
            to |= static_cast<typename To::enum_type>(bit);
    return to;
}

QMessageBox::StandardButton showBox(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &text,
                                    QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton) {
    QMessageBox box(icon, title, text, buttons, parent);
    if (defaultButton != QMessageBox::NoButton)
        box.setDefaultButton(defaultButton);
    box.exec();
    return box.standardButton(box.clickedButton());
}

} // namespace

/*!
    \class QMessageBox
    \brief The QMessageBox class shows a message and asks the user to answer with a button.

    A message box has an icon, a text, and buttons. The static functions show a box and wait for the answer:

    \code
    if (QMessageBox::question(window, "Quit", "Quit without saving?") == QMessageBox::Yes)
        qApp->quit();
    \endcode

    For more, set up a message box and call exec(). It returns the standard button that was clicked:

    \code
    QMessageBox box(QMessageBox::Warning, "Save", "The document has been modified.",
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, window);
    box.setInformativeText("Do you want to save your changes?");
    box.setDefaultButton(QMessageBox::Save);
    switch (box.exec()) {
    case QMessageBox::Save: save(); break;
    case QMessageBox::Discard: break;
    default: return;
    }
    \endcode

    The buttons are laid out by a QDialogButtonBox. Escape and the close button of the window click the escape
    button: the one set with setEscapeButton(), or else the button with the RejectRole, or the only button.

    \differences
    The icons are drawn rather than taken from the icon theme, and there is no \c iconPixmap(), \c textFormat()
    (the text is plain), \c setTextInteractionFlags(), \c aboutQt() or \c open() with a receiver. The texts of the
    buttons are not translated. exec() returns the index of a custom button among buttons() rather than an opaque
    value.
    \enddifferences

    \sa QDialog, QDialogButtonBox
*/

/*!
    \enum QMessageBox::Icon

    The icon shown beside the text.

    \value NoIcon No icon.
    \value Information A message that needs no answer: a blue round sign with "i".
    \value Warning A warning: a yellow triangle with "!".
    \value Critical A serious problem: a red round sign with a cross.
    \value Question A question: a blue round sign with "?".
*/

/*!
    \enum QMessageBox::ButtonRole

    The role of a button; the same as QDialogButtonBox::ButtonRole.

    \value InvalidRole Not a valid role.
    \value AcceptRole Accepts, such as OK.
    \value RejectRole Rejects, such as Cancel.
    \value DestructiveRole Throws changes away, such as Discard.
    \value ActionRole Does something else.
    \value HelpRole Asks for help.
    \value YesRole A Yes button.
    \value NoRole A No button.
    \value ResetRole Resets to defaults.
    \value ApplyRole Applies changes.
    \omitvalue NRoles
*/

/*!
    \enum QMessageBox::StandardButton

    The standard buttons; the same as QDialogButtonBox::StandardButton. StandardButtons is a typedef for
    \c{QFlags<StandardButton>}.

    \value NoButton No button.
    \value Ok OK.
    \value Save Save.
    \value SaveAll Save All.
    \value Open Open.
    \value Yes Yes.
    \value YesToAll Yes to All.
    \value No No.
    \value NoToAll No to All.
    \value Abort Abort.
    \value Retry Retry.
    \value Ignore Ignore.
    \value Close Close.
    \value Cancel Cancel.
    \value Discard Discard.
    \value Help Help.
    \value Apply Apply.
    \value Reset Reset.
    \value RestoreDefaults Restore Defaults.
    \omitvalue FirstButton
    \omitvalue LastButton
*/

/*!
    Constructs a message box with no text, no icon and no buttons, with the given \a parent.
*/
QMessageBox::QMessageBox(QWidget *parent) : QDialog(parent) { init(); }

/*!
    Constructs a message box with the given \a icon, window \a title, \a text, standard \a buttons, \a parent and
    window flags \a f.
*/
QMessageBox::QMessageBox(Icon icon, const QString &title, const QString &text, StandardButtons buttons, QWidget *parent,
                         Qt::WindowFlags f)
    : QDialog(parent, f) {
    init();
    setWindowTitle(title);
    setIcon(icon);
    setText(text);
    setStandardButtons(buttons);
}

/*!
    Destroys the message box.
*/
QMessageBox::~QMessageBox() = default;

void QMessageBox::init() {
    m_grid = new QGridLayout(this);
    m_iconWidget = new MessageIcon(this);
    m_iconWidget->hide();
    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_informative = new QLabel(this);
    m_informative->setWordWrap(true);
    m_informative->hide();
    m_box = new QDialogButtonBox(this);
    m_grid->addWidget(m_iconWidget, 0, 0, 2, 1, Qt::AlignTop);
    m_grid->addWidget(m_label, 0, 1);
    m_grid->addWidget(m_informative, 1, 1);
    m_grid->addWidget(m_box, 3, 0, 1, 2);
    m_grid->setColumnStretch(1, 1);
    QObject::connect(m_box, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) { clicked(button); });
}

/*!
    Adds \a button with the given \a role.

    \sa removeButton()
*/
void QMessageBox::addButton(QAbstractButton *button, ButtonRole role) {
    m_box->addButton(button, static_cast<QDialogButtonBox::ButtonRole>(role));
}

/*!
    \overload

    Creates a push button with the given \a text and \a role, adds it and returns it.
*/
QPushButton *QMessageBox::addButton(const QString &text, ButtonRole role) {
    return m_box->addButton(text, static_cast<QDialogButtonBox::ButtonRole>(role));
}

/*!
    \overload

    Adds the standard \a button and returns it.
*/
QPushButton *QMessageBox::addButton(StandardButton button) { return m_box->addButton(toBox(button)); }

/*!
    Removes \a button from the message box without deleting it.

    \sa addButton()
*/
void QMessageBox::removeButton(QAbstractButton *button) {
    if (m_escape == button)
        m_escape = nullptr;
    if (m_default == button)
        m_default = nullptr;
    m_box->removeButton(button);
}

/*!
    Returns the buttons of the message box.
*/
QList<QAbstractButton *> QMessageBox::buttons() const {
    QList<QAbstractButton *> list = m_box->buttons();
    list.removeAll(static_cast<QAbstractButton *>(m_detailsButton));
    return list;
}

/*!
    Returns the role of \a button, or InvalidRole if it is not in the message box.
*/
QMessageBox::ButtonRole QMessageBox::buttonRole(QAbstractButton *button) const {
    return static_cast<ButtonRole>(m_box->buttonRole(button));
}

/*!
    Makes \a buttons the standard buttons of the message box.

    \sa standardButtons()
*/
void QMessageBox::setStandardButtons(StandardButtons buttons) {
    m_box->setStandardButtons(convertButtons<QDialogButtonBox::StandardButtons>(buttons));
}

/*!
    Returns the standard buttons of the message box.
*/
QMessageBox::StandardButtons QMessageBox::standardButtons() const {
    return convertButtons<StandardButtons>(m_box->standardButtons());
}

/*!
    Returns which standard button \a button is, or NoButton.
*/
QMessageBox::StandardButton QMessageBox::standardButton(QAbstractButton *button) const {
    return static_cast<StandardButton>(static_cast<int>(m_box->standardButton(button)));
}

/*!
    Returns the button of the standard button \a which, or \c nullptr.
*/
QAbstractButton *QMessageBox::button(StandardButton which) const { return m_box->button(toBox(which)); }

/*!
    \fn QPushButton *QMessageBox::defaultButton() const

    Returns the button that was made the default with setDefaultButton(), or \c nullptr.
*/

/*!
    Makes \a button the default button: the one that Return clicks.

    \sa defaultButton()
*/
void QMessageBox::setDefaultButton(QPushButton *button) {
    m_default = button;
    if (button) {
        button->setDefault(true);
        button->setFocus();
    }
}

/*!
    \overload

    Makes the standard \a button the default button.
*/
void QMessageBox::setDefaultButton(StandardButton button) {
    setDefaultButton(dynamic_cast<QPushButton *>(this->button(button)));
}

/*!
    \fn QAbstractButton *QMessageBox::escapeButton() const

    Returns the button that was set with setEscapeButton(), or \c nullptr.
*/

/*!
    Makes \a button the button that Escape and closing the window click.

    \sa escapeButton()
*/
void QMessageBox::setEscapeButton(QAbstractButton *button) { m_escape = button; }

/*!
    \overload

    Makes the standard \a button the escape button.
*/
void QMessageBox::setEscapeButton(StandardButton button) { setEscapeButton(this->button(button)); }

/*!
    \fn QAbstractButton *QMessageBox::clickedButton() const

    Returns the button that the user clicked, or \c nullptr if none was clicked yet.
*/

/*!
    Returns the text of the message.

    \sa setText()
*/
QString QMessageBox::text() const { return m_label->text(); }

/*!
    Sets the text of the message to \a text. Lines are wrapped to the width of the box.

    \sa text()
*/
void QMessageBox::setText(const QString &text) { m_label->setText(text); }

/*!
    \fn QMessageBox::Icon QMessageBox::icon() const

    Returns the icon of the message box, NoIcon by default.

    \sa setIcon()
*/

/*!
    Sets the icon beside the text to \a icon.

    \sa icon()
*/
void QMessageBox::setIcon(Icon icon) {
    m_icon = icon;
    static_cast<MessageIcon *>(m_iconWidget)->setIcon(icon);
    m_iconWidget->setVisible(icon != NoIcon);
}

/*!
    Returns the text shown under the message.

    \sa setInformativeText()
*/
QString QMessageBox::informativeText() const { return m_informative->text(); }

/*!
    Sets the text shown under the message, such as what the user can do about it, to \a text.

    \sa informativeText()
*/
void QMessageBox::setInformativeText(const QString &text) {
    m_informative->setText(text);
    m_informative->setVisible(!text.isEmpty());
}

/*!
    Returns the detailed text, or an empty string if there is none.

    \sa setDetailedText()
*/
QString QMessageBox::detailedText() const { return m_details ? m_details->toPlainText() : QString(); }

/*!
    Sets the detailed text to \a text. A "Show Details..." button then shows it in a text area under the buttons. An
    empty text removes the button.

    \sa detailedText()
*/
void QMessageBox::setDetailedText(const QString &text) {
    if (text.isEmpty()) {
        delete m_details;
        m_details = nullptr;
        delete m_detailsButton;
        m_detailsButton = nullptr;
        return;
    }
    if (!m_details) {
        m_details = new QPlainTextEdit(this);
        m_details->setReadOnly(true);
        m_details->hide();
        m_grid->addWidget(m_details, 4, 0, 1, 2);
        m_detailsButton = new QPushButton(QStringLiteral("Show Details..."));
        m_detailsButton->setAutoDefault(false);
        m_box->addButton(m_detailsButton, QDialogButtonBox::ActionRole);
    }
    m_details->setPlainText(text);
}

/*!
    Puts the check box \a cb under the text, such as "Do not ask again", or removes the check box with \c nullptr.
    The message box takes the check box as a child.

    \sa checkBox()
*/
void QMessageBox::setCheckBox(QCheckBox *cb) {
    if (m_checkBox == cb)
        return;
    if (m_checkBox) {
        m_grid->removeWidget(m_checkBox);
        delete m_checkBox;
    }
    m_checkBox = cb;
    if (cb) {
        cb->setParent(this);
        m_grid->addWidget(cb, 2, 1);
        if (isVisible())
            cb->show();
    }
}

/*!
    \fn QCheckBox *QMessageBox::checkBox() const

    Returns the check box of the message box, or \c nullptr.

    \sa setCheckBox()
*/

/*!
    Shows a message box with the Information icon and waits for the user. Returns the standard button that was
    clicked, or NoButton. \a parent, \a title, \a text, \a buttons and \a defaultButton set up the box.
*/
QMessageBox::StandardButton QMessageBox::information(QWidget *parent, const QString &title, const QString &text,
                                                     StandardButtons buttons, StandardButton defaultButton) {
    return showBox(parent, Information, title, text, buttons, defaultButton);
}

/*!
    Shows a message box with the Question icon and waits for the user. Returns the standard button that was
    clicked, or NoButton. \a parent, \a title, \a text, \a buttons and \a defaultButton set up the box.
*/
QMessageBox::StandardButton QMessageBox::question(QWidget *parent, const QString &title, const QString &text,
                                                  StandardButtons buttons, StandardButton defaultButton) {
    return showBox(parent, Question, title, text, buttons, defaultButton);
}

/*!
    Shows a message box with the Warning icon and waits for the user. Returns the standard button that was
    clicked, or NoButton. \a parent, \a title, \a text, \a buttons and \a defaultButton set up the box.
*/
QMessageBox::StandardButton QMessageBox::warning(QWidget *parent, const QString &title, const QString &text,
                                                 StandardButtons buttons, StandardButton defaultButton) {
    return showBox(parent, Warning, title, text, buttons, defaultButton);
}

/*!
    Shows a message box with the Critical icon and waits for the user. Returns the standard button that was
    clicked, or NoButton. \a parent, \a title, \a text, \a buttons and \a defaultButton set up the box.
*/
QMessageBox::StandardButton QMessageBox::critical(QWidget *parent, const QString &title, const QString &text,
                                                  StandardButtons buttons, StandardButton defaultButton) {
    return showBox(parent, Critical, title, text, buttons, defaultButton);
}

/*!
    Shows a message box with an OK button that tells about the application, with the given \a parent, window
    \a title and \a text, and waits for the user.
*/
void QMessageBox::about(QWidget *parent, const QString &title, const QString &text) {
    QMessageBox box(NoIcon, title, text, Ok, parent);
    box.exec();
}

/*!
    \reimp

    Returns the standard button that was clicked, or the index of a custom button among buttons(), or -1 if no
    button was clicked.
*/
int QMessageBox::exec() {
    m_clicked = nullptr;
    QDialog::exec();
    return m_clicked ? returnCode(m_clicked) : -1;
}

int QMessageBox::returnCode(QAbstractButton *button) const {
    const StandardButton standard = standardButton(button);
    if (standard != NoButton)
        return standard;
    return static_cast<int>(buttons().indexOf(button));
}

void QMessageBox::clicked(QAbstractButton *button) {
    if (button == m_detailsButton) {
        toggleDetails();
        return;
    }
    m_clicked = button;
    QPointer<QMessageBox> guard(this);
    emit buttonClicked(button);
    if (!guard)
        return;
    // Close with the return code; accepted() and rejected() follow the role of the button.
    const int code = returnCode(button);
    const ButtonRole role = buttonRole(button);
    hide();
    setResult(code);
    if (role == AcceptRole || role == YesRole)
        emit accepted();
    else if (role == RejectRole || role == NoRole)
        emit rejected();
    if (guard)
        emit finished(code);
}

void QMessageBox::toggleDetails() {
    if (!m_details)
        return;
    const bool show = m_details->isHidden();
    m_details->setVisible(show);
    m_detailsButton->setText(show ? QStringLiteral("Hide Details...") : QStringLiteral("Show Details..."));
    adjustSize();
}

QAbstractButton *QMessageBox::detectedEscapeButton() const {
    if (m_escape)
        return m_escape;
    const QList<QAbstractButton *> all = buttons();
    for (QAbstractButton *b : all)
        if (buttonRole(b) == RejectRole)
            return b;
    if (all.size() == 1)
        return all.first();
    QAbstractButton *no = nullptr;
    for (QAbstractButton *b : all) {
        if (buttonRole(b) == NoRole) {
            if (no)
                return nullptr;
            no = b;
        }
    }
    return no;
}

/*!
    \reimp

    Escape clicks the escape button; without one it does nothing.
*/
void QMessageBox::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        if (QAbstractButton *escape = detectedEscapeButton())
            escape->click();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

/*!
    \reimp

    Closing the window clicks the escape button; without one the window stays open.
*/
void QMessageBox::closeEvent(QCloseEvent *event) {
    if (!isVisible()) {
        event->accept();
        return;
    }
    QAbstractButton *escape = detectedEscapeButton();
    if (!escape) {
        event->ignore();
        return;
    }
    escape->click();
    event->setAccepted(!isVisible());
}

/*!
    \reimp
*/
void QMessageBox::showEvent(QShowEvent *event) {
    // A message box without buttons gets an OK button, as in Qt.
    if (buttons().isEmpty())
        addButton(Ok);
    QDialog::showEvent(event);
}

#include "moc_qmessagebox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
