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
    There is no \c matches() and no \c keyCombination(). key() reports only
    the keys of Qt::Key. The native codes are those of X11 on the \c xcb
    platform and 0 elsewhere.
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

/*!
    Constructs a key event as the other constructor does, with \a type, \a key, \a modifiers, \a text, \a autorep
    and \a count, and the codes of the platform: \a nativeScanCode, \a nativeVirtualKey and \a nativeModifiers.
*/
QKeyEvent::QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, quint32 nativeScanCode,
                     quint32 nativeVirtualKey, quint32 nativeModifiers, const QString &text, bool autorep, ushort count)
    : QInputEvent(type, modifiers), m_key(key), m_text(text), m_autorep(autorep), m_count(count),
      m_nativeScanCode(nativeScanCode), m_nativeVirtualKey(nativeVirtualKey), m_nativeModifiers(nativeModifiers) {}

QKeyEvent::~QKeyEvent() = default;

/*!
    \class QContextMenuEvent
    \inmodule NtGui
    \since 6.0
    \brief The QContextMenuEvent class contains parameters that describe a context menu event.

    A widget gets it, as a QEvent::ContextMenu event, when the user presses the right mouse button on it, or the
    Menu key while it has the focus. QWidget::contextMenuEvent() handles it, as the widget's
    Qt::ContextMenuPolicy says.

    \differences
    The event is sent when the right button is pressed, as on Linux and macOS in Qt; there is no setting to send it
    on the release.
    \enddifferences

    \sa QWidget::contextMenuEvent(), QWidget::setContextMenuPolicy()
*/

/*!
    Constructs an event for a menu asked for by \a reason at \a pos in the widget and \a globalPos on the screen,
    with the keyboard \a modifiers held.
*/
QContextMenuEvent::QContextMenuEvent(Reason reason, const QPoint &pos, const QPoint &globalPos,
                                     Qt::KeyboardModifiers modifiers)
    : QInputEvent(ContextMenu, modifiers), m_pos(pos), m_globalPos(globalPos), m_reason(reason) {}

/*!
    \overload

    Constructs an event for a menu asked for by \a reason at \a pos, which is also used as the global position.
*/
QContextMenuEvent::QContextMenuEvent(Reason reason, const QPoint &pos) : QContextMenuEvent(reason, pos, pos) {}

/*!
    Destroys the event.
*/
QContextMenuEvent::~QContextMenuEvent() = default;

/*!
    \class QDropEvent
    \inmodule NtGui
    \since 6.0
    \brief The QDropEvent class provides an event which is sent when a drag and drop action is completed.

    A widget that accepted the drag in its QWidget::dragEnterEvent() gets this event in QWidget::dropEvent() when the
    drag is dropped on it. Read the data from mimeData(), take dropAction() or set another of possibleActions() with
    setDropAction(), and accept the event; QDrag::exec() then returns the action.

    \differences
    Drags work within one program: a drag cannot leave it for another program, nor come from one. position() is a
    QPointF, as in Qt 6; there is no \c posF().
    \enddifferences

    \sa QDrag, QDragEnterEvent, QDragMoveEvent
*/

/*!
    Constructs a drop event at \a pos, of the given \a type, for \a data with the allowed \a actions, while
    \a buttons and \a modifiers are held. The proposed action is the first of \a actions among move, copy and link.
*/
QDropEvent::QDropEvent(const QPointF &pos, Qt::DropActions actions, const QMimeData *data, Qt::MouseButtons buttons,
                       Qt::KeyboardModifiers modifiers, Type type)
    : QEvent(type), m_pos(pos), m_actions(actions), m_data(data), m_buttons(buttons), m_modifiers(modifiers) {
    for (Qt::DropAction a : {Qt::MoveAction, Qt::CopyAction, Qt::LinkAction})
        if (actions & a) {
            m_proposed = a;
            break;
        }
    m_action = m_proposed;
    ignore();
}

/*!
    Destroys the event.
*/
QDropEvent::~QDropEvent() = default;

/*!
    \class QDragMoveEvent
    \inmodule NtGui
    \since 6.0
    \brief The QDragMoveEvent class provides an event which is sent while a drag and drop action is in progress.

    A widget that accepted the drag gets this event in QWidget::dragMoveEvent() each time the drag moves over it;
    accepting it, or ignoring it, says whether a drop there would be taken.

    \differences
    answerRect() is the point of the drag: the events come for every move.
    \enddifferences
*/

/*!
    Constructs a drag move event at \a pos, of the given \a type, for \a data with the allowed \a actions, while
    \a buttons and \a modifiers are held.
*/
QDragMoveEvent::QDragMoveEvent(const QPoint &pos, Qt::DropActions actions, const QMimeData *data,
                               Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, Type type)
    : QDropEvent(QPointF(pos), actions, data, buttons, modifiers, type), m_rect(pos, QSize(1, 1)) {}

/*!
    Destroys the event.
*/
QDragMoveEvent::~QDragMoveEvent() = default;

/*!
    \class QDragEnterEvent
    \inmodule NtGui
    \since 6.0
    \brief The QDragEnterEvent class provides an event which is sent to a widget when a drag and drop action enters it.

    A widget that accepts drops gets this event in QWidget::dragEnterEvent() when a drag comes over it. It accepts
    the event, usually after looking at the formats of mimeData(), to get the move and drop events that follow.

    \differences
    None, apart from those of QDropEvent.
    \enddifferences
*/

/*!
    Constructs a drag enter event at \a pos for \a data with the allowed \a actions, while \a buttons and
    \a modifiers are held.
*/
QDragEnterEvent::QDragEnterEvent(const QPoint &pos, Qt::DropActions actions, const QMimeData *data,
                                 Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers)
    : QDragMoveEvent(pos, actions, data, buttons, modifiers, DragEnter) {}

/*!
    Destroys the event.
*/
QDragEnterEvent::~QDragEnterEvent() = default;

/*!
    \class QDragLeaveEvent
    \inmodule NtGui
    \since 6.0
    \brief The QDragLeaveEvent class provides an event that is sent to a widget when a drag and drop action leaves it.

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a drag leave event.
*/
QDragLeaveEvent::QDragLeaveEvent() : QEvent(DragLeave) {}

/*!
    Destroys the event.
*/
QDragLeaveEvent::~QDragLeaveEvent() = default;

/*!
    \since 6.0

    Returns \c true if the key and modifiers of the event are one of the bindings of the standard key \a key,
    such as QKeySequence::Copy for Ctrl+C.

    \sa QKeySequence::keyBindings()
*/
bool QKeyEvent::matches(QKeySequence::StandardKey key) const {
    const int combination = (m_key | modifiers().toInt()) & ~static_cast<int>(Qt::KeypadModifier);
    const QList<QKeySequence> bindings = QKeySequence::keyBindings(key);
    for (const QKeySequence &binding : bindings)
        if (binding.count() == 1 && binding[0] == combination)
            return true;
    return false;
}

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
    \class QActionEvent
    \brief The QActionEvent class provides an event that is sent when an action of a widget is added, removed or changed.

    A widget that shows its actions, such as a menu, handles it in QWidget::actionEvent().

    \sa QAction, QWidget::actionEvent()
*/

/*!
    Constructs the event of the given \a type, which is \c ActionAdded, \c ActionRemoved or \c ActionChanged, for the
    \a action. For \c ActionAdded, \a before is the action that the new one is in front of; \c nullptr if it is last.
*/
QActionEvent::QActionEvent(int type, QAction *action, QAction *before)
    : QEvent(static_cast<Type>(type)), m_action(action), m_before(before) {}

QActionEvent::~QActionEvent() = default;

/*!
    \fn QAction *QActionEvent::action() const

    Returns the action that was added, removed or changed.
*/

/*!
    \fn QAction *QActionEvent::before() const

    Returns the action that the added action is in front of, or \c nullptr.
*/

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

/*!
    \class QEventPoint
    \brief The QEventPoint class is one finger of a touch event.

    \differences
    There is no device, velocity, ellipse diameters, rotation or time stamp.
    \enddifferences
*/

/*!
    \enum QEventPoint::State

    What a touch point did.

    \value Unknown Nothing is known.
    \value Stationary It stayed where it was.
    \value Pressed It went down.
    \value Updated It moved.
    \value Released It came up.
*/

/*!
    Constructs a point with the identifier \a pointId in \a state at \a scenePosition (also its position and press
    position) and \a globalPosition.
*/
QEventPoint::QEventPoint(int pointId, State state, const QPointF &scenePosition, const QPointF &globalPosition)
    : m_id(pointId), m_state(state), m_position(scenePosition), m_scenePosition(scenePosition),
      m_globalPosition(globalPosition), m_pressPosition(scenePosition) {}

/*!
    \class QTouchEvent
    \brief The QTouchEvent class describes the fingers on a touch screen.

    \differences
    The android platform delivers touch input: every finger, to the window on top. A widget gets the events if it
    sets Qt::WA_AcceptTouchEvents and accepts the TouchBegin event in event(); other widgets get the first finger as
    the mouse. The other platforms have no touch screens, so there the events come only from the application itself.
    There is no device(), target() or QPointerEvent base, and no gesture recognition (QGesture, QPinchGesture).
    \enddifferences
*/

/*!
    Constructs a touch event of \a eventType with \a modifiers and \a touchPoints.
*/
QTouchEvent::QTouchEvent(Type eventType, Qt::KeyboardModifiers modifiers, const QList<QEventPoint> &touchPoints)
    : QInputEvent(eventType, modifiers), m_points(touchPoints) {}

/*!
    Destroys the event.
*/
QTouchEvent::~QTouchEvent() = default;

/*!
    Returns the states of all the points, combined.
*/
Qt::TouchPointStates QTouchEvent::touchPointStates() const {
    Qt::TouchPointStates states;
    for (const QEventPoint &p : m_points)
        states |= static_cast<Qt::TouchPointState>(p.state());
    return states;
}
/*!
    \class QShortcutEvent
    \inmodule NtGui
    \since 6.0
    \brief The QShortcutEvent class tells a widget that a shortcut it grabbed was pressed.

    QWidget::grabShortcut() grabs a key sequence; when it is pressed in the window, the widget gets this event with
    the id that grabShortcut() returned.

    \differences
    A shortcut is never ambiguous: the first widget that has it gets it.
    \enddifferences
*/

/*!
    Constructs a shortcut event for \a key and the shortcut \a id; \a ambiguous says whether other widgets have it.
*/
QShortcutEvent::QShortcutEvent(const QKeySequence &key, int id, bool ambiguous)
    : QEvent(QEvent::Shortcut), m_key(key), m_id(id), m_ambiguous(ambiguous) {}

/*!
    Destroys the event.
*/
QShortcutEvent::~QShortcutEvent() = default;

/*!
    \class QInputMethodEvent
    \since 6.0
    \brief The QInputMethodEvent class carries text from an input method to a widget.

    An input method, such as one for Chinese or a virtual keyboard, sends the text it is composing as the preedit
    string, and the text the user settled on as the commit string, which may replace text before the cursor.
    QWidget::inputMethodEvent() receives it.

    \code
    void Editor::inputMethodEvent(QInputMethodEvent *event)
    {
        insert(event->commitString());
        event->accept();
    }
    \endcode

    \differences
    Nt has no input method of the platform that sends these events; an application or a test sends them.
    \enddifferences
*/

/*!
    \enum QInputMethodEvent::AttributeType

    What an attribute of the preedit string says.

    \value TextFormat The format of a part of it.
    \value Cursor Where the cursor is, and whether it is shown.
    \value Language The language of a part of it.
    \value Ruby A ruby annotation.
    \value Selection A selection to make in the text of the widget.
    \value MimeData Data for a part of it.
*/

/*!
    \class QInputMethodEvent::Attribute
    \brief The Attribute class is something an input method says about its preedit string.

    \differences
    The attributes are kept for the widget; Nt editors do not draw preedit strings.
    \enddifferences
*/

/*!
    \fn QInputMethodEvent::Attribute::Attribute(AttributeType typ, int s, int l, QVariant val)

    Constructs an attribute of type \a typ for the \a l characters from \a s, with the value \a val.
*/

/*!
    \fn QInputMethodEvent::Attribute::Attribute(AttributeType typ, int s, int l)
    \overload

    Constructs an attribute of type \a typ for the \a l characters from \a s, without a value.
*/

/*!
    \variable QInputMethodEvent::Attribute::type
    \brief What the attribute says.
*/

/*!
    \variable QInputMethodEvent::Attribute::start
    \brief Where in the preedit string the attribute starts.
*/

/*!
    \variable QInputMethodEvent::Attribute::length
    \brief How many characters the attribute covers.
*/

/*!
    \variable QInputMethodEvent::Attribute::value
    \brief The value of the attribute, such as a QTextCharFormat.
*/

/*!
    Constructs an event with no preedit string and no commit string.
*/
QInputMethodEvent::QInputMethodEvent() : QEvent(QEvent::InputMethod) {}

/*!
    Constructs an event with the preedit string \a preeditText and its \a attributes.
*/
QInputMethodEvent::QInputMethodEvent(const QString &preeditText, const QList<Attribute> &attributes)
    : QEvent(QEvent::InputMethod), m_preedit(preeditText), m_attributes(attributes) {}

/*!
    Destroys the event.
*/
QInputMethodEvent::~QInputMethodEvent() = default;

/*!
    Sets the text to insert to \a commitString, in the place of the \a replaceLength characters that start
    \a replaceFrom characters from the cursor; \a replaceFrom may be negative.
*/
void QInputMethodEvent::setCommitString(const QString &commitString, int replaceFrom, int replaceLength) {
    m_commit = commitString;
    m_replaceFrom = replaceFrom;
    m_replaceLength = replaceLength;
}

/*!
    \fn const QList<QInputMethodEvent::Attribute> &QInputMethodEvent::attributes() const

    Returns the attributes of the preedit string.
*/

/*!
    \fn const QString &QInputMethodEvent::preeditString() const

    Returns the text the input method is composing.
*/

/*!
    \fn const QString &QInputMethodEvent::commitString() const

    Returns the text to insert.
*/

/*!
    \fn int QInputMethodEvent::replacementStart() const

    Returns where the text to replace starts, relative to the cursor.
*/

/*!
    \fn int QInputMethodEvent::replacementLength() const

    Returns how many characters of the text the commit string replaces.
*/
