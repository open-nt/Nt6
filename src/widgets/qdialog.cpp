// MIT License - see LICENSE
#include <QtWidgets/qdialog.h>

#include "qwidget_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qpointer.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/qpushbutton.h>

/*!
    \class QDialog
    \brief The QDialog class is the base class of dialog windows.

    A dialog is a window for a short task: asking the user something, or showing a message. A dialog with a parent
    is a window of its own that the window system keeps above the window of its parent, and it is centered over
    that window when it is first shown.

    A modal dialog blocks the input of the other windows while it is shown. exec() shows the dialog as an
    application-modal dialog and returns when it is closed, with its result:

    \code
    QDialog dialog(window);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("Delete the file?"));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::No);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted)
        deleteFile();
    \endcode

    open() shows the dialog as a window-modal dialog and returns at once; connect to finished() for the result. A
    dialog shown with show() is modeless unless setModal() was called.

    Escape and the close button of the window reject the dialog. Return and Enter click the default button (see
    QPushButton::setDefault()).

    \differences
    There is no size grip (\c setSizeGripEnabled()), no extension (\c setExtension()) and no
    \c Qt::WA_DeleteOnClose. A modal dialog blocks the mouse and the keyboard of the windows behind it, but they are
    not shown as blocked and clicking them does not raise the dialog.
    \enddifferences

    \sa QDialogButtonBox, QMessageBox, QWidget::setWindowModality()
*/

/*!
    \enum QDialog::DialogCode

    The results that accept() and reject() give a dialog.

    \value Rejected The dialog was rejected: cancelled or closed.
    \value Accepted The dialog was accepted.
*/

/*!
    Constructs a dialog with the given \a parent and window flags \a f. Without a kind of window in \a f the dialog
    is a Qt::Dialog.
*/
QDialog::QDialog(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f | ((f & Qt::WindowType_Mask) == 0 ? Qt::Dialog : Qt::Widget)) {}

/*!
    Destroys the dialog. A dialog that is destroyed while exec() runs makes exec() return Rejected.
*/
QDialog::~QDialog() {
    if (m_loop)
        m_loop->exit();
}

/*!
    \fn int QDialog::result() const

    Returns the result of the dialog: usually Accepted or Rejected, or the code given to done().

    \sa setResult()
*/

/*!
    \fn void QDialog::setResult(int result)

    Sets the result of the dialog to \a result.

    \sa result()
*/

/*!
    Makes the dialog application-modal if \a modal is \c true, and modeless otherwise. It takes effect the next time
    the dialog is shown with show().

    \sa QWidget::setWindowModality(), exec()
*/
void QDialog::setModal(bool modal) { setWindowModality(modal ? Qt::ApplicationModal : Qt::NonModal); }

/*!
    \reimp

    The first time the dialog is shown it takes its size hint, unless it was resized, and is centered over the window
    of its parent, or on the screen if it has no parent, unless it was moved. Hiding the dialog ends exec().
*/
void QDialog::setVisible(bool visible) {
    if (visible && isWindow() && !isVisible() && !m_positioned) {
        m_positioned = true;
        if (!QtPrivate::WidgetAccess::data(this).geometrySet)
            adjustSize();
        if (pos() == QPoint(0, 0)) {
            QRect area;
            if (QWidget *p = parentWidget())
                area = p->window()->geometry();
            else if (QScreen *screen = QGuiApplication::primaryScreen())
                area = screen->availableGeometry();
            if (area.isValid())
                move(area.center() - QPoint(width() / 2, height() / 2));
        }
    }
    QWidget::setVisible(visible);
    if (!visible && m_loop)
        m_loop->exit();
}

/*!
    Shows the dialog as a window-modal dialog and returns at once. The modality is reset when the dialog closes.
    Connect to finished() to get the result.

    \sa exec(), finished()
*/
void QDialog::open() {
    if (windowModality() != Qt::WindowModal) {
        setWindowModality(Qt::WindowModal);
        m_resetModality = true;
    }
    setResult(0);
    show();
}

/*!
    Shows the dialog as an application-modal dialog and blocks until it closes, running an event loop meanwhile.
    Returns the result of the dialog, or Rejected if it was destroyed. A dialog that is already in exec() returns -1.

    \sa open(), result()
*/
int QDialog::exec() {
    if (m_loop) {
        qWarning("QDialog::exec: Recursive call detected");
        return -1;
    }
    const Qt::WindowModality old = windowModality();
    setWindowModality(Qt::ApplicationModal);
    setResult(0);
    show();
    QPointer<QDialog> guard(this);
    QEventLoop loop;
    m_loop = &loop;
    if (isVisible())
        loop.exec();
    if (!guard)
        return Rejected;
    m_loop = nullptr;
    setWindowModality(old);
    return result();
}

/*!
    Closes the dialog with the result \a r: hides it, sets the result, emits accepted() or rejected() if \a r is
    Accepted or Rejected, and then finished(). exec() returns \a r.

    \sa accept(), reject()
*/
void QDialog::done(int r) {
    QPointer<QDialog> guard(this);
    hide();
    setResult(r);
    if (r == Accepted)
        emit accepted();
    else if (r == Rejected)
        emit rejected();
    if (!guard)
        return;
    emit finished(r);
    if (guard && m_resetModality) {
        m_resetModality = false;
        setWindowModality(Qt::NonModal);
    }
}

/*!
    Closes the dialog with the result Accepted.

    \sa done(), reject()
*/
void QDialog::accept() { done(Accepted); }

/*!
    Closes the dialog with the result Rejected.

    \sa done(), accept()
*/
void QDialog::reject() { done(Rejected); }

QPushButton *QDialog::defaultButton() const {
    for (QPushButton *button : findChildren<QPushButton *>())
        if (button->isDefault() && button->window() == this && button->isVisible())
            return button;
    return nullptr;
}

/*!
    \reimp

    Escape rejects the dialog. Return and Enter click the default button, if it is enabled.
*/
void QDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        reject();
        event->accept();
        return;
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier) {
        if (QPushButton *button = defaultButton()) {
            if (button->isEnabled())
                button->click();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

/*!
    \reimp

    Closing the window rejects the dialog if it is still visible.
*/
void QDialog::closeEvent(QCloseEvent *event) {
    if (isVisible()) {
        QPointer<QDialog> guard(this);
        reject();
        if (guard && isVisible())
            event->ignore();
        else
            event->accept();
        return;
    }
    event->accept();
}

#include "moc_qdialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
