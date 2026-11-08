// MIT License - see LICENSE
#include <QtGui/qevent.h>

/*!
    \class QInputEvent
    \brief The QInputEvent class is the base class for events that describe user input.

    It carries the keyboard modifiers that were down when the input happened.

    \differences
    There is no timestamp, device or \c clone().
    \enddifferences

    \sa QMouseEvent, QKeyEvent, QWheelEvent
*/

/*!
    \fn QInputEvent::QInputEvent(Type type, Qt::KeyboardModifiers modifiers)

    Constructs an input event of the given \a type with the keyboard \a modifiers.
*/

/*!
    \fn Qt::KeyboardModifiers QInputEvent::modifiers() const

    Returns the modifier keys that were down when the event happened.

    \sa setModifiers()
*/

/*!
    \fn void QInputEvent::setModifiers(Qt::KeyboardModifiers modifiers)

    Sets the modifiers to \a modifiers.

    \sa modifiers()
*/

QInputEvent::~QInputEvent() = default;

/*!
    \class QMouseEvent
    \brief The QMouseEvent class contains parameters that describe a mouse event.

    Mouse events arrive when a mouse button is pressed or released
    (\c MouseButtonPress, \c MouseButtonRelease, \c MouseButtonDblClick) and when
    the mouse moves (\c MouseMove). Windows receive them in
    QWindow::mousePressEvent() and the related functions.

    \differences
    Positions are in the coordinates of the window and its screen only; there is no
    scene position, source device or synthesized flag.
    \enddifferences

    \sa QWindow, QWheelEvent
*/

/*!
    Constructs a mouse event of the given \a type. \a localPos is the position in the
    receiving window and \a globalPos the position on the screen. \a button is the
    button that caused the event, \a buttons the buttons that are down, and
    \a modifiers the modifier keys.
*/

QMouseEvent::QMouseEvent(Type type, const QPointF &localPos, const QPointF &globalPos, Qt::MouseButton button,
                         Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
    : QInputEvent(type, modifiers), m_local(localPos), m_global(globalPos), m_button(button), m_buttons(buttons) {}

QMouseEvent::~QMouseEvent() = default;

/*!
    \fn Qt::MouseButton QMouseEvent::button() const

    Returns the button that caused the event, or Qt::NoButton for a move.
*/

/*!
    \fn Qt::MouseButtons QMouseEvent::buttons() const

    Returns the buttons that were down when the event happened.
*/

/*!
    \fn QPointF QMouseEvent::position() const

    Returns the position of the mouse in the receiving window.
*/

/*!
    \fn QPointF QMouseEvent::globalPosition() const

    Returns the position of the mouse on the screen.
*/

/*!
    \fn QPoint QMouseEvent::pos() const

    Returns position() as an integer point.
*/

/*!
    \fn QPoint QMouseEvent::globalPos() const

    Returns globalPosition() as an integer point.
*/

/*!
    \fn int QMouseEvent::x() const

    Returns the x coordinate of pos().
*/

/*!
    \fn int QMouseEvent::y() const

    Returns the y coordinate of pos().
*/

/*!
    \class QWheelEvent
    \brief The QWheelEvent class contains parameters that describe a wheel event.

    The angle delta is in eighths of a degree; a notch of a typical wheel is 120.

    \differences
    There is no phase, inverted flag or source; the event is a single scroll step.
    \enddifferences

    \sa QMouseEvent
*/

/*!
    Constructs a wheel event for a mouse at \a pos in the window and \a globalPos on the
    screen, with the scroll amounts \a pixelDelta and \a angleDelta, the pressed
    \a buttons and the \a modifiers.
*/

QWheelEvent::QWheelEvent(const QPointF &pos, const QPointF &globalPos, const QPoint &pixelDelta,
                         const QPoint &angleDelta, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
    : QInputEvent(Wheel, modifiers), m_local(pos), m_global(globalPos), m_pixelDelta(pixelDelta),
      m_angleDelta(angleDelta), m_buttons(buttons) {}

QWheelEvent::~QWheelEvent() = default;

/*!
    \fn QPoint QWheelEvent::pixelDelta() const

    Returns the scroll amount in pixels, or a null point if the device does not report pixels.
*/

/*!
    \fn QPoint QWheelEvent::angleDelta() const

    Returns the scroll amount in eighths of a degree.
*/

/*!
    \fn Qt::MouseButtons QWheelEvent::buttons() const

    Returns the buttons that were down.
*/

/*!
    \fn QPointF QWheelEvent::position() const

    Returns the position of the mouse in the receiving window.
*/

/*!
    \fn QPointF QWheelEvent::globalPosition() const

    Returns the position of the mouse on the screen.
*/

/*!
    \class QKeyEvent
    \brief The QKeyEvent class describes a key event.

    Key events arrive as \c KeyPress and \c KeyRelease. key() is a Qt::Key value or
    a Latin-1 character, and text() holds the text that the key produces, if any.

    \differences
    There is no native scan code or virtual key, no \c matches() and no
    \c keyCombination(). key() reports only the keys of Qt::Key.
    \enddifferences

    \sa QWindow::keyPressEvent()
*/

/*!
    Constructs a key event of the given \a type for \a key with the \a modifiers
    down. \a text is the text the key produced, \a autorep tells that the event is
    an auto repeat, and \a count is the number of keys the event stands for.
*/

QKeyEvent::QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text, bool autorep,
                     ushort count)
    : QInputEvent(type, modifiers), m_key(key), m_text(text), m_autorep(autorep), m_count(count) {}

QKeyEvent::~QKeyEvent() = default;

/*!
    \fn int QKeyEvent::key() const

    Returns the code of the key that was pressed or released, a value of Qt::Key.
*/

/*!
    \fn QString QKeyEvent::text() const

    Returns the text the key produced, which is empty for keys such as Shift.
*/

/*!
    \fn bool QKeyEvent::isAutoRepeat() const

    Returns \c true if the event comes from the key being held down.
*/

/*!
    \fn int QKeyEvent::count() const

    Returns the number of keys the event stands for.
*/

/*!
    \class QFocusEvent
    \brief The QFocusEvent class contains event parameters for widget and window focus events.

    Focus events arrive as \c FocusIn when a window gets the keyboard focus and as
    \c FocusOut when it loses it.

    \differences
    There is no focus reason.
    \enddifferences

    \sa QWindow::focusInEvent()
*/

/*!
    \fn QFocusEvent::QFocusEvent(Type type)

    Constructs a focus event of the given \a type, \c FocusIn or \c FocusOut.
*/

QFocusEvent::~QFocusEvent() = default;

/*!
    \fn bool QFocusEvent::gotFocus() const

    Returns \c true for a \c FocusIn event.
*/

/*!
    \fn bool QFocusEvent::lostFocus() const

    Returns \c true for a \c FocusOut event.
*/

/*!
    \class QHelpEvent
    \brief The QHelpEvent class asks a widget for the help it has at a position, such as a tool tip.

    A window sends a QHelpEvent of the type QEvent::ToolTip to the widget under the mouse when the mouse has rested
    there for a moment. A widget that has something to show for the position shows it and accepts the event; one that does
    not ignores it, and the event goes on to its parent. Reimplement QWidget::event() to show a tool tip that depends on
    the position, as a table does for its cells:

    \code
    bool MyWidget::event(QEvent *event)
    {
        if (event->type() == QEvent::ToolTip) {
            auto *help = static_cast<QHelpEvent *>(event);
            QToolTip::showText(help->globalPos(), describe(help->pos()), this);
            return true;
        }
        return QWidget::event(event);
    }
    \endcode

    \differences
    There is no \c WhatsThis event type, and the position is a QPoint.
    \enddifferences

    \sa QEvent, QMouseEvent
*/

/*!
    \fn QHelpEvent::QHelpEvent(Type type, const QPoint &pos, const QPoint &globalPos)

    Constructs a help event of the given \a type for a mouse at \a pos in the widget and \a globalPos on the screen.
*/

QHelpEvent::~QHelpEvent() = default;

/*!
    \fn int QHelpEvent::x() const

    Returns the x coordinate of the position in the widget.
*/

/*!
    \fn int QHelpEvent::y() const

    Returns the y coordinate of the position in the widget.
*/

/*!
    \fn int QHelpEvent::globalX() const

    Returns the x coordinate of the position on the screen.
*/

/*!
    \fn int QHelpEvent::globalY() const

    Returns the y coordinate of the position on the screen.
*/

/*!
    \fn QPoint QHelpEvent::pos() const

    Returns the position in the widget.
*/

/*!
    \fn QPoint QHelpEvent::globalPos() const

    Returns the position on the screen.
*/

/*!
    \class QEnterEvent
    \brief The QEnterEvent class contains parameters that describe an enter event.

    A window receives it when the mouse enters it.

    \differences
    There is no scene position or device.
    \enddifferences

    \sa QMouseEvent
*/

/*!
    \fn QEnterEvent::QEnterEvent(const QPointF &localPos, const QPointF &globalPos)

    Constructs an enter event for a mouse at \a localPos in the window and \a globalPos on the screen.
*/

QEnterEvent::~QEnterEvent() = default;

/*!
    \fn QPointF QEnterEvent::position() const

    Returns the position of the mouse in the window.
*/

/*!
    \fn QPointF QEnterEvent::globalPosition() const

    Returns the position of the mouse on the screen.
*/

/*!
    \fn QPoint QEnterEvent::pos() const

    Returns position() as an integer point.
*/

/*!
    \fn QPoint QEnterEvent::globalPos() const

    Returns globalPosition() as an integer point.
*/

/*!
    \class QPaintEvent
    \brief The QPaintEvent class contains event parameters for paint events.

    A paint event asks a window to paint the part of itself that the region names.
    QPaintDeviceWindow sends it.

    \sa QRasterWindow, QPainter
*/

/*!
    \fn QPaintEvent::QPaintEvent(const QRegion &paintRegion)

    Constructs a paint event for \a paintRegion.
*/

/*!
    \fn QPaintEvent::QPaintEvent(const QRect &paintRect)
    \overload

    Constructs a paint event for \a paintRect.
*/

QPaintEvent::~QPaintEvent() = default;

/*!
    \fn const QRegion &QPaintEvent::region() const

    Returns the region that needs painting.
*/

/*!
    \fn QRect QPaintEvent::rect() const

    Returns the smallest rectangle that contains the region.
*/

/*!
    \class QExposeEvent
    \brief The QExposeEvent class contains event parameters for expose events.

    The window system sends an expose event when part of a window becomes visible
    and its contents have to be drawn again.

    \sa QWindow::exposeEvent()
*/

/*!
    \fn QExposeEvent::QExposeEvent(const QRegion &exposeRegion)

    Constructs an expose event for \a exposeRegion.
*/

QExposeEvent::~QExposeEvent() = default;

/*!
    \fn const QRegion &QExposeEvent::region() const

    Returns the region that became visible.
*/

/*!
    \class QResizeEvent
    \brief The QResizeEvent class contains event parameters for resize events.

    \sa QWindow::resizeEvent()
*/

/*!
    \fn QResizeEvent::QResizeEvent(const QSize &size, const QSize &oldSize)

    Constructs a resize event for a window that changed from \a oldSize to \a size.
*/

QResizeEvent::~QResizeEvent() = default;

/*!
    \fn const QSize &QResizeEvent::size() const

    Returns the new size.
*/

/*!
    \fn const QSize &QResizeEvent::oldSize() const

    Returns the old size.
*/

/*!
    \class QMoveEvent
    \brief The QMoveEvent class contains event parameters for move events.

    \sa QWindow::moveEvent()
*/

/*!
    \fn QMoveEvent::QMoveEvent(const QPoint &pos, const QPoint &oldPos)

    Constructs a move event for a window that moved from \a oldPos to \a pos.
*/

QMoveEvent::~QMoveEvent() = default;

/*!
    \fn const QPoint &QMoveEvent::pos() const

    Returns the new position.
*/

/*!
    \fn const QPoint &QMoveEvent::oldPos() const

    Returns the old position.
*/

/*!
    \class QShowEvent
    \brief The QShowEvent class provides an event that is sent when a window is shown.

    \sa QWindow::showEvent()
*/

/*!
    \fn QShowEvent::QShowEvent()

    Constructs the event.
*/

QShowEvent::~QShowEvent() = default;

/*!
    \class QHideEvent
    \brief The QHideEvent class provides an event that is sent when a window is hidden.

    \sa QWindow::hideEvent()
*/

/*!
    \fn QHideEvent::QHideEvent()

    Constructs the event.
*/

QHideEvent::~QHideEvent() = default;

/*!
    \class QCloseEvent
    \brief The QCloseEvent class contains parameters that describe a close event.

    The event is sent when the user asks to close a window, for example with the
    close button of its frame. A window that ignores the event, with
    \c{event->ignore()}, stays open.

    \sa QWindow::closeEvent()
*/

/*!
    \fn QCloseEvent::QCloseEvent()

    Constructs a close event, which starts out accepted.
*/

QCloseEvent::~QCloseEvent() = default;
