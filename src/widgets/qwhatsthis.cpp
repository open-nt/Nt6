// MIT License - see LICENSE
#include <QtWidgets/qwhatsthis.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qtooltip.h>
#include <QtWidgets/qwidget.h>

namespace {

// While What's This mode is on: takes the next click and shows the help of the widget under it.
class WhatsThisFilter : public QObject {
public:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            QWhatsThis::leaveWhatsThisMode();
            return true;
        }
        if (event->type() != QEvent::MouseButtonPress)
            return event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::MouseButtonDblClick;
        auto *widget = qobject_cast<QWidget *>(watched);
        if (!widget)
            return false; // the window passes it to its widgets
        const auto *mouse = static_cast<QMouseEvent *>(event);
        QString text;
        for (QWidget *w = widget; w && text.isEmpty(); w = w->isWindow() ? nullptr : w->parentWidget())
            text = w->whatsThis();
        QWhatsThis::leaveWhatsThisMode();
        if (!text.isEmpty())
            QWhatsThis::showText(mouse->globalPosition().toPoint(), text, widget);
        return true;
    }
};

QPointer<WhatsThisFilter> &filter() {
    static QPointer<WhatsThisFilter> instance;
    return instance;
}

} // namespace

/*!
    \class QWhatsThis
    \inmodule NtWidgets
    \since 6.0
    \brief The QWhatsThis class shows the "What's This?" help of widgets.

    In What's This mode the cursor is a question mark, and a click on a widget shows its QWidget::whatsThis() text,
    or that of its nearest parent that has one; Escape leaves the mode. createAction() makes an action that turns it
    on, for a Help menu:

    \code
    helpMenu->addAction(QWhatsThis::createAction(this));
    portEdit->setWhatsThis("The UDP port that the radio listens on.");
    \endcode

    \differences
    The text is shown as a tool tip, so it goes away when the mouse moves. Shift+F1 turns the mode on only through
    the action of createAction().
    \enddifferences

    \sa QWidget::setWhatsThis()
*/

/*!
    Turns What's This mode on: the next click on a widget shows its help.
*/
void QWhatsThis::enterWhatsThisMode() {
    if (inWhatsThisMode() || !qApp)
        return;
    filter() = new WhatsThisFilter;
    filter()->setParent(qApp);
    qApp->installEventFilter(filter().data());
    QGuiApplication::setOverrideCursor(QCursor(Qt::WhatsThisCursor));
}

/*!
    Returns \c true while What's This mode is on.
*/
bool QWhatsThis::inWhatsThisMode() { return !filter().isNull(); }

/*!
    Turns What's This mode off.
*/
void QWhatsThis::leaveWhatsThisMode() {
    if (!inWhatsThisMode())
        return;
    filter()->deleteLater();
    if (qApp)
        qApp->removeEventFilter(filter().data());
    filter() = nullptr;
    QGuiApplication::restoreOverrideCursor();
}

/*!
    Shows \a text at \a pos on the screen, as the help of the widget \a w.
*/
void QWhatsThis::showText(const QPoint &pos, const QString &text, QWidget *w) { QToolTip::showText(pos, text, w); }

/*!
    Hides the help that showText() shows.
*/
void QWhatsThis::hideText() { QToolTip::hideText(); }

/*!
    Returns a new action, a child of \a parent, that turns What's This mode on when it is triggered.
*/
QAction *QWhatsThis::createAction(QObject *parent) {
    auto *action = new QAction(QStringLiteral("What's This?"), parent);
    action->setShortcut(QKeySequence(QKeyCombination(Qt::ShiftModifier, Qt::Key_F1)));
    QObject::connect(action, &QAction::triggered, action, [] { enterWhatsThisMode(); });
    return action;
}
