// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>

class QObject;

class QEvent {
public:
    /*!
        \enum QEvent::Type

        This enum describes the kinds of event Nt delivers.

        \value None Not an event.
        \value Timer A timer fired; delivered as a QTimerEvent.
        \value MetaCall A queued signal-slot invocation, handled by QObject::event().
        \value ThreadChange The object is about to move to another thread; see QObject::moveToThread().
        \value SockAct A socket notifier became active; see QSocketNotifier.
        \value DynamicPropertyChange A dynamic property was added, changed or removed; delivered as a QDynamicPropertyChangeEvent.
        \value LanguageChange The translations changed; QCoreApplication posts it to the application object when a translator is installed or removed.
        \value LocaleChange The default locale changed.
        \value DeferredDelete The receiver should be deleted; posted by QObject::deleteLater().
        \value UpdateRequest A request to update a deferred state; QSettings posts it to write its changes back from the event loop, and QWindow::requestUpdate() uses it to ask for a repaint.
        \value LayoutRequest A widget's layout has to be done again; posted by QLayout::invalidate().
        \value EnabledChange A widget was enabled or disabled.
        \value FontChange The font of a widget changed.
        \value PaletteChange The palette of a widget changed.
        \value StyleChange The style of a widget changed.
        \value WindowTitleChange The window title of a widget changed.
        \value ParentChange The parent of a widget changed.
        \value WindowActivate The window of a widget became the active window.
        \value WindowDeactivate The window of a widget stopped being the active window.
        \value MouseTrackingChange The mouse tracking setting of a widget changed.
        \value ModifiedChange The window of a widget got, or lost, unsaved changes.
        \value CursorChange The cursor of a widget changed.
        \value MouseButtonPress A mouse button was pressed; delivered as a QMouseEvent.
        \value MouseButtonRelease A mouse button was released; delivered as a QMouseEvent.
        \value MouseButtonDblClick A mouse button was double-clicked; delivered as a QMouseEvent.
        \value MouseMove The mouse moved; delivered as a QMouseEvent.
        \value KeyPress A key was pressed; delivered as a QKeyEvent.
        \value KeyRelease A key was released; delivered as a QKeyEvent.
        \value ContextMenu The user asked for a context menu; delivered as a QContextMenuEvent.
        \value DragEnter A drag entered a widget; delivered as a QDragEnterEvent.
        \value DragMove A drag moved over a widget; delivered as a QDragMoveEvent.
        \value DragLeave A drag left a widget; delivered as a QDragLeaveEvent.
        \value Drop A drag was dropped on a widget; delivered as a QDropEvent.
        \value GraphicsSceneDragEnter A drag entered an item; delivered as a QGraphicsSceneDragDropEvent.
        \value GraphicsSceneDragMove A drag moved over an item; delivered as a QGraphicsSceneDragDropEvent.
        \value GraphicsSceneDragLeave A drag left an item; delivered as a QGraphicsSceneDragDropEvent.
        \value GraphicsSceneDrop A drag was dropped on an item; delivered as a QGraphicsSceneDragDropEvent.
        \value GraphicsSceneContextMenu The user asked for a context menu over a scene; delivered as a
               QGraphicsSceneContextMenuEvent.
        \value ShortcutOverride A key is about to activate a shortcut; delivered as a QKeyEvent to the widget with the
               focus first, which accepts it to get the key press itself instead.
        \value Shortcut A shortcut that QWidget::grabShortcut() grabbed was pressed; a QShortcutEvent.
        \value LayoutDirectionChange The layout direction of a widget changed.
        \value WindowIconChange The icon of a window changed.
        \value IconTextChange The icon text of a window changed.
        \value WindowStateChange The state of a window changed: minimized, maximized or full screen.
        \value ZOrderChange A widget was raised or lowered among its siblings.
        \value ApplicationLayoutDirectionChange The layout direction of the application changed.
        \value FocusIn A window gained the keyboard focus; delivered as a QFocusEvent.
        \value FocusOut A window lost the keyboard focus; delivered as a QFocusEvent.
        \value Enter The mouse entered a window; delivered as a QEnterEvent.
        \value Leave The mouse left a window.
        \value Paint A window needs painting; delivered as a QPaintEvent.
        \value Move A window moved; delivered as a QMoveEvent.
        \value Resize A window was resized; delivered as a QResizeEvent.
        \value Show A window was shown; delivered as a QShowEvent.
        \value Hide A window was hidden; delivered as a QHideEvent.
        \value Close A window was asked to close; delivered as a QCloseEvent.
        \value Wheel The mouse wheel turned; delivered as a QWheelEvent.
        \value ToolTip A widget is asked for its tool tip; delivered as a QHelpEvent.
        \value ActionChanged An action of a widget has changed; delivered as a QActionEvent.
        \value ActionAdded An action has been added to a widget; delivered as a QActionEvent.
        \value ActionRemoved An action has been removed from a widget; delivered as a QActionEvent.
        \value Expose Part of a window became visible and needs painting; delivered as a QExposeEvent.
        \value GraphicsSceneMouseMove The mouse moved over a scene; delivered as a QGraphicsSceneMouseEvent.
        \value GraphicsSceneMousePress A mouse button was pressed in a scene; delivered as a QGraphicsSceneMouseEvent.
        \value GraphicsSceneMouseRelease A mouse button was released in a scene; delivered as a
            QGraphicsSceneMouseEvent.
        \value GraphicsSceneMouseDoubleClick A mouse button was double-clicked in a scene; delivered as a
            QGraphicsSceneMouseEvent.
        \value GraphicsSceneHoverEnter The mouse entered an item; delivered as a QGraphicsSceneHoverEvent.
        \value GraphicsSceneHoverMove The mouse moved over an item; delivered as a QGraphicsSceneHoverEvent.
        \value GraphicsSceneHoverLeave The mouse left an item; delivered as a QGraphicsSceneHoverEvent.
        \value GraphicsSceneWheel The mouse wheel turned over a scene; delivered as a QGraphicsSceneWheelEvent.
        \value GraphicsSceneHelp Help, such as a tool tip, was asked for over a scene; a QGraphicsSceneHelpEvent.
        \value GraphicsSceneResize A QGraphicsWidget was resized; a QGraphicsSceneResizeEvent.
        \value GraphicsSceneMove A QGraphicsWidget moved; a QGraphicsSceneMoveEvent.
        \value TouchBegin A touch sequence began; delivered as a QTouchEvent.
        \value TouchUpdate Touch points moved; delivered as a QTouchEvent.
        \value TouchEnd A touch sequence ended; delivered as a QTouchEvent.
        \value TouchCancel A touch sequence was cancelled; delivered as a QTouchEvent.
        \value ChildAdded An object gained a child; delivered as a QChildEvent.
        \value ChildPolished A widget child was polished; delivered as a QChildEvent.
        \value Polish A widget was polished by its style, before it is shown for the first time.
        \value PolishRequest A widget should be polished.
        \value ChildRemoved An object lost a child; delivered as a QChildEvent.
        \value User The first value available for application-defined events.
        \value MaxUser The last value available for application-defined events.
    */
    enum Type {
        None = 0,
        Timer = 1,
        ThreadChange = 22,
        MetaCall = 43,
        SockAct = 50,
        DynamicPropertyChange = 170,
        LanguageChange = 89,
        LocaleChange = 88,
        DeferredDelete = 52,
        UpdateRequest = 77,
        LayoutRequest = 76,
        Polish = 75,
        PolishRequest = 74,
        EnabledChange = 98,
        FontChange = 97,
        PaletteChange = 39,
        StyleChange = 100,
        WindowTitleChange = 33,
        ParentChange = 21,
        WindowActivate = 24,
        WindowDeactivate = 25,
        MouseTrackingChange = 109,
        ModifiedChange = 102,
        CursorChange = 183,
        MouseButtonPress = 2,
        MouseButtonRelease = 3,
        MouseButtonDblClick = 4,
        MouseMove = 5,
        KeyPress = 6,
        KeyRelease = 7,
        ShortcutOverride = 51,
        Shortcut = 117,
        LayoutDirectionChange = 90,
        WindowIconChange = 34,
        IconTextChange = 101,
        WindowStateChange = 105,
        ZOrderChange = 126,
        ApplicationLayoutDirectionChange = 37,
        ContextMenu = 82,
        DragEnter = 60,
        DragMove = 61,
        DragLeave = 62,
        Drop = 63,
        GraphicsSceneDragEnter = 164,
        GraphicsSceneDragMove = 165,
        GraphicsSceneDragLeave = 166,
        GraphicsSceneDrop = 167,
        GraphicsSceneContextMenu = 159,
        FocusIn = 8,
        FocusOut = 9,
        Enter = 10,
        Leave = 11,
        Paint = 12,
        Move = 13,
        Resize = 14,
        Show = 17,
        Hide = 18,
        Close = 19,
        Wheel = 31,
        ToolTip = 110,
        ActionChanged = 113,
        ActionAdded = 114,
        ActionRemoved = 115,
        Expose = 206,
        GraphicsSceneMouseMove = 155,
        GraphicsSceneMousePress = 156,
        GraphicsSceneMouseRelease = 157,
        GraphicsSceneMouseDoubleClick = 158,
        GraphicsSceneHoverEnter = 160,
        GraphicsSceneHoverMove = 161,
        GraphicsSceneHoverLeave = 162,
        GraphicsSceneWheel = 168,
        GraphicsSceneHelp = 163,
        GraphicsSceneResize = 181,
        GraphicsSceneMove = 182,
        TouchBegin = 194,
        TouchUpdate = 195,
        TouchEnd = 196,
        TouchCancel = 209,
        ChildAdded = 68,
        ChildPolished = 69,
        ChildRemoved = 71,
        User = 1000,
        MaxUser = 65535
    };

    /*!
        Constructs an event of the given \a type.
    */
    explicit QEvent(Type type) : t(type) {}
    virtual ~QEvent();
    virtual QEvent *clone() const;

    /*!
        Returns the event type.
    */
    Type type() const { return t; }
    /*!
        Returns \c true if the event has been accepted; otherwise returns
        \c false. Events are accepted when constructed.

        \sa setAccepted(), accept(), ignore()
    */
    bool isAccepted() const { return accepted; }
    /*!
        Sets the accept flag to \a a.

        \sa isAccepted()
    */
    void setAccepted(bool a) { accepted = a; }
    /*!
        Marks the event as accepted. This is the same as
        \c{setAccepted(true)}.

        \sa ignore()
    */
    void accept() { accepted = true; }
    /*!
        Marks the event as ignored, so the receiver does not handle it. This
        is the same as \c{setAccepted(false)}.

        \sa accept()
    */
    void ignore() { accepted = false; }

    /*!
        \since 6.0
        Returns \c true if the event came from outside the application, such
        as from the window system; otherwise returns \c false. Nt delivers
        every event from within the application, so this returns \c false.
    */
    bool spontaneous() const { return false; }
    bool isInputEvent() const noexcept;
    bool isPointerEvent() const noexcept;
    bool isSinglePointEvent() const noexcept;

    static int registerEventType(int hint = -1);

protected:
    /*!
        Constructs a copy of \a other. Only subclasses copy events, in clone().
    */
    QEvent(const QEvent &other) = default;
    /*!
        Assigns \a other to this event and returns a reference to it.
    */
    QEvent &operator=(const QEvent &other) = default;

private:
    Type t;
    bool accepted = true;
};

class QBasicTimer;

class QTimerEvent : public QEvent {
public:
    /*!
        Constructs a timer event for the timer with the identifier
        \a timerId.
    */
    explicit QTimerEvent(int timerId) : QEvent(Timer), m_id(timerId) {}
    ~QTimerEvent() override;
    /*!
        Returns the identifier of the timer that fired, as returned by
        QObject::startTimer().
    */
    int timerId() const { return m_id; }
    /*!
        \since 6.8
        Returns the identifier of the timer that fired as a Qt::TimerId.

        \sa timerId(), matches()
    */
    Qt::TimerId id() const { return static_cast<Qt::TimerId>(m_id); }
    /*!
        \since 6.9
        Returns \c true if this event comes from \a timer; otherwise returns
        \c false.
    */
    bool matches(const QBasicTimer &timer) const noexcept;
    /*!
        \reimp
    */
    QTimerEvent *clone() const override { return new QTimerEvent(*this); }

private:
    int m_id;
};

class QDynamicPropertyChangeEvent : public QEvent {
public:
    /*!
        Constructs a dynamic property change event for the property called \a name.
    */
    explicit QDynamicPropertyChangeEvent(const QByteArray &name) : QEvent(DynamicPropertyChange), m_name(name) {}
    ~QDynamicPropertyChangeEvent() override;
    /*!
        Returns the name of the dynamic property that was added, changed or removed.

        \sa QObject::setProperty()
    */
    QByteArray propertyName() const { return m_name; }
    /*!
        \reimp
    */
    QDynamicPropertyChangeEvent *clone() const override { return new QDynamicPropertyChangeEvent(*this); }

private:
    QByteArray m_name;
};

class QChildEvent : public QEvent {
public:
    /*!
        Constructs a child event of the given \a type (\c ChildAdded,
        \c ChildPolished or \c ChildRemoved) for \a child.
    */
    QChildEvent(Type type, QObject *child) : QEvent(type), c(child) {}
    ~QChildEvent() override;
    /*!
        Returns the child object that was added or removed.
    */
    QObject *child() const { return c; }
    /*!
        Returns \c true if the event is \c ChildAdded; otherwise returns \c false.
    */
    bool added() const { return type() == ChildAdded; }
    /*!
        Returns \c true if the event is \c ChildPolished; otherwise returns \c false.
    */
    bool polished() const { return type() == ChildPolished; }
    /*!
        Returns \c true if the event is \c ChildRemoved; otherwise returns \c false.
    */
    bool removed() const { return type() == ChildRemoved; }
    /*!
        \reimp
    */
    QChildEvent *clone() const override { return new QChildEvent(*this); }

private:
    QObject *c;
};
