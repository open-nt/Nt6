// MIT License - see LICENSE
#include <QtGui/qdrag.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qeventloop.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qpointer.h>
#include <QtGui/qcursor.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#include <functional>

namespace {

// The drag that is going on; QDrag::cancel() stops it.
QDrag *g_current = nullptr;

} // namespace

struct QDrag::Data {
    QObject *source = nullptr;
    QPointer<QObject> target;
    std::unique_ptr<QMimeData> data;
    QPixmap pixmap;
    QPoint hotSpot;
    Qt::DropActions supported = Qt::MoveAction;
    Qt::DropAction defaultAction = Qt::IgnoreAction;
    QEventLoop *loop = nullptr;
    bool cancelled = false;
};

namespace {

// Follows the mouse while the drag is going on: it takes the mouse and key events of the windows of the program and
// turns them into drag events for the window under the mouse.
class DragFilter : public QObject {
public:
    DragFilter(QDrag *drag, Qt::DropActions supported, Qt::DropAction preferred)
        : m_drag(drag), m_supported(supported), m_preferred(preferred) {}

    Qt::DropAction result() const { return m_result; }
    QWindow *dropped() const { return m_dropped.data(); }

    // Ends the drag without a drop.
    void abandon() {
        leave();
        finish(Qt::IgnoreAction);
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        auto *window = qobject_cast<QWindow *>(watched);
        if (!window)
            return false;
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto *e = static_cast<QMouseEvent *>(event);
            move(window, e->globalPosition().toPoint(), e->buttons(), e->modifiers());
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto *e = static_cast<QMouseEvent *>(event);
            drop(window, e->globalPosition().toPoint(), e->modifiers());
            return true;
        }
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
        case QEvent::Wheel:
            return true;
        case QEvent::KeyPress:
            if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
                abandon();
            return true;
        case QEvent::KeyRelease:
            return true;
        default:
            return false;
        }
    }

private:
    // The window of the program at `global`: the one that got the event if it is there, else a visible one.
    static QWindow *windowAt(QWindow *receiver, const QPoint &global) {
        if (receiver && receiver->isVisible() && receiver->geometry().contains(global))
            return receiver;
        QWindow *found = nullptr;
        for (QWindow *w : QGuiApplication::topLevelWindows())
            if (w->isVisible() && w->geometry().contains(global))
                found = w;
        return found;
    }

    Qt::DropAction proposal(Qt::KeyboardModifiers modifiers) const {
        if ((modifiers & Qt::ControlModifier) && (m_supported & Qt::CopyAction))
            return Qt::CopyAction;
        if ((modifiers & Qt::ShiftModifier) && (m_supported & Qt::MoveAction))
            return Qt::MoveAction;
        if (m_preferred != Qt::IgnoreAction && (m_supported & m_preferred))
            return m_preferred;
        for (Qt::DropAction a : {Qt::MoveAction, Qt::CopyAction, Qt::LinkAction})
            if (m_supported & a)
                return a;
        return Qt::IgnoreAction;
    }

    void move(QWindow *receiver, const QPoint &global, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers) {
        QWindow *target = windowAt(receiver, global);
        if (target != m_window) {
            leave();
            m_window = target;
            emit m_drag->targetChanged(target);
            if (!target)
                return showAction(Qt::IgnoreAction);
            QDragEnterEvent enter(target->mapFromGlobal(global), m_supported, m_drag->mimeData(), buttons, modifiers);
            enter.setSource(m_drag->source());
            enter.setProposedAction(proposal(modifiers));
            enter.setDropAction(proposal(modifiers));
            QCoreApplication::sendEvent(target, &enter);
            m_accepted = enter.isAccepted();
            showAction(m_accepted ? enter.dropAction() : Qt::IgnoreAction);
            return;
        }
        if (!target)
            return;
        QDragMoveEvent moveEvent(target->mapFromGlobal(global), m_supported, m_drag->mimeData(), buttons, modifiers);
        moveEvent.setSource(m_drag->source());
        moveEvent.setProposedAction(proposal(modifiers));
        moveEvent.setDropAction(proposal(modifiers));
        moveEvent.setAccepted(m_accepted);
        QCoreApplication::sendEvent(target, &moveEvent);
        m_accepted = moveEvent.isAccepted();
        showAction(m_accepted ? moveEvent.dropAction() : Qt::IgnoreAction);
    }

    void drop(QWindow *receiver, const QPoint &global, Qt::KeyboardModifiers modifiers) {
        QWindow *target = windowAt(receiver, global);
        if (!target || target != m_window || !m_accepted) {
            leave();
            return finish(Qt::IgnoreAction);
        }
        QDropEvent dropEvent(QPointF(target->mapFromGlobal(global)), m_supported, m_drag->mimeData(), Qt::NoButton,
                             modifiers);
        dropEvent.setSource(m_drag->source());
        dropEvent.setProposedAction(proposal(modifiers));
        dropEvent.setDropAction(m_action);
        QCoreApplication::sendEvent(target, &dropEvent);
        m_dropped = target;
        m_window = nullptr;
        const Qt::DropAction done = dropEvent.dropAction();
        finish(dropEvent.isAccepted() && (m_supported & done) ? done : Qt::IgnoreAction);
    }

    void leave() {
        if (!m_window)
            return;
        QDragLeaveEvent leaveEvent;
        QCoreApplication::sendEvent(m_window.data(), &leaveEvent);
        m_window = nullptr;
        m_accepted = false;
    }

    void showAction(Qt::DropAction action) {
        if (action == m_action)
            return;
        m_action = action;
        emit m_drag->actionChanged(action);
        const Qt::CursorShape shape = action == Qt::CopyAction   ? Qt::DragCopyCursor
                                      : action == Qt::MoveAction ? Qt::DragMoveCursor
                                      : action == Qt::LinkAction ? Qt::DragLinkCursor
                                                                 : Qt::ForbiddenCursor;
        QGuiApplication::changeOverrideCursor(QCursor(shape));
    }

    void finish(Qt::DropAction result) {
        m_result = result;
        m_done = true;
        quit();
    }

public:
    // The loop of exec() ends when this is emitted.
    std::function<void()> quitLoop;
    bool done() const { return m_done; }

private:
    void quit() {
        if (quitLoop)
            quitLoop();
    }

    QDrag *m_drag;
    Qt::DropActions m_supported;
    Qt::DropAction m_preferred;
    QPointer<QWindow> m_window;
    QPointer<QWindow> m_dropped;
    bool m_accepted = false;
    Qt::DropAction m_action = Qt::IgnoreAction;
    Qt::DropAction m_result = Qt::IgnoreAction;
    bool m_done = false;
};

DragFilter *g_filter = nullptr;

} // namespace

/*!
    \class QDrag
    \inmodule NtGui
    \since 6.0
    \brief The QDrag class provides support for MIME-based drag and drop data transfer.

    A widget starts a drag in its mouse move handler, once the mouse has moved far enough with a button held: make a
    QDrag with the data and call exec(), which returns when the drag is dropped or given up:

    \code
    void Palette::mouseMoveEvent(QMouseEvent *event) {
        if (!(event->buttons() & Qt::LeftButton) ||
            (event->position().toPoint() - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            return;
        auto *drag = new QDrag(this);
        auto *data = new QMimeData;
        data->setData("application/x-part", partId());
        drag->setMimeData(data);
        if (drag->exec(Qt::CopyAction | Qt::MoveAction) == Qt::MoveAction)
            removePart();
    }
    \endcode

    While the drag goes on, the widget under the mouse that accepts drops gets QDragEnterEvent, QDragMoveEvent and
    QDragLeaveEvent, and the QDropEvent when the mouse button is released. Control asks for a copy and Shift for a
    move; Escape gives the drag up. The cursor shows what a drop would do.

    \differences
    Drags work within one program: they cannot be dropped on another program, nor come from one (there is no XDND),
    and pixmap() is not shown under the mouse. There is no \c setDragCursor().
    \enddifferences

    \sa QDropEvent, QMimeData
*/

/*!
    Constructs a drag whose source is \a dragSource, which also becomes its parent.
*/
QDrag::QDrag(QObject *dragSource) : QObject(dragSource), m_d(std::make_unique<Data>()) { m_d->source = dragSource; }

/*!
    Destroys the drag, and its data.
*/
QDrag::~QDrag() {
    if (g_current == this)
        g_current = nullptr;
}

/*!
    Sets the data of the drag to \a data, and takes ownership of it.
*/
void QDrag::setMimeData(QMimeData *data) { m_d->data.reset(data); }

/*!
    Returns the data of the drag, or \c nullptr if it has none.
*/
QMimeData *QDrag::mimeData() const { return m_d->data.get(); }

/*!
    Sets the pixmap that stands for the dragged data to \a pixmap.
*/
void QDrag::setPixmap(const QPixmap &pixmap) { m_d->pixmap = pixmap; }

/*!
    Returns the pixmap that stands for the dragged data.
*/
QPixmap QDrag::pixmap() const { return m_d->pixmap; }

/*!
    Sets the point of the pixmap that is under the mouse to \a hotspot.
*/
void QDrag::setHotSpot(const QPoint &hotspot) { m_d->hotSpot = hotspot; }

/*!
    Returns the point of the pixmap that is under the mouse.
*/
QPoint QDrag::hotSpot() const { return m_d->hotSpot; }

/*!
    Returns the object that started the drag.
*/
QObject *QDrag::source() const { return m_d->source; }

/*!
    Returns the window the data was dropped on, once exec() has returned, or \c nullptr.
*/
QObject *QDrag::target() const { return m_d->target.data(); }

/*!
    Starts the drag and returns when it is dropped, with the action the target took, one of \a supportedActions, or
    Qt::IgnoreAction if nothing took the data.
*/
Qt::DropAction QDrag::exec(Qt::DropActions supportedActions) { return exec(supportedActions, Qt::IgnoreAction); }

/*!
    \overload

    Starts the drag with \a supportedActions, proposing \a defaultDropAction when no key asks for another, and
    returns the action the target took.
*/
Qt::DropAction QDrag::exec(Qt::DropActions supportedActions, Qt::DropAction defaultDropAction) {
    if (g_current || !QCoreApplication::instance() || !m_d->data)
        return Qt::IgnoreAction;
    m_d->supported = supportedActions;
    m_d->defaultAction = defaultDropAction;
    DragFilter filter(this, supportedActions, defaultDropAction);
    QEventLoop loop;
    filter.quitLoop = [&loop] { loop.quit(); };
    g_current = this;
    g_filter = &filter;
    QGuiApplication::setOverrideCursor(QCursor(Qt::ForbiddenCursor));
    QCoreApplication::instance()->installEventFilter(&filter);
    const QPointer<QDrag> self(this);
    if (!filter.done())
        loop.exec();
    QCoreApplication::instance()->removeEventFilter(&filter);
    QGuiApplication::restoreOverrideCursor();
    g_filter = nullptr;
    if (self && g_current == this)
        g_current = nullptr;
    if (self)
        m_d->target = filter.dropped();
    return filter.result();
}

/*!
    Returns the actions that the current or last exec() allowed.
*/
Qt::DropActions QDrag::supportedActions() const { return m_d->supported; }

/*!
    Returns the action that exec() was asked to propose, or Qt::IgnoreAction if none.
*/
Qt::DropAction QDrag::defaultAction() const { return m_d->defaultAction; }

/*!
    Gives up the drag that is going on, as Escape does.
*/
void QDrag::cancel() {
    if (g_filter)
        g_filter->abandon();
}

#include "moc_qdrag.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
