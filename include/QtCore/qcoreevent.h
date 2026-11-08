// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>

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
        \value MouseButtonPress A mouse button was pressed; delivered as a QMouseEvent.
        \value MouseButtonRelease A mouse button was released; delivered as a QMouseEvent.
        \value MouseButtonDblClick A mouse button was double-clicked; delivered as a QMouseEvent.
        \value MouseMove The mouse moved; delivered as a QMouseEvent.
        \value KeyPress A key was pressed; delivered as a QKeyEvent.
        \value KeyRelease A key was released; delivered as a QKeyEvent.
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
        \value Expose Part of a window became visible and needs painting; delivered as a QExposeEvent.
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
        DeferredDelete = 52,
        UpdateRequest = 77,
        LayoutRequest = 76,
        EnabledChange = 98,
        FontChange = 97,
        PaletteChange = 39,
        StyleChange = 100,
        WindowTitleChange = 33,
        ParentChange = 21,
        WindowActivate = 24,
        WindowDeactivate = 25,
        MouseTrackingChange = 109,
        MouseButtonPress = 2,
        MouseButtonRelease = 3,
        MouseButtonDblClick = 4,
        MouseMove = 5,
        KeyPress = 6,
        KeyRelease = 7,
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
        Expose = 206,
        User = 1000,
        MaxUser = 65535
    };

    /*!
        Constructs an event of the given \a type.
    */
    explicit QEvent(Type type) : t(type) {}
    virtual ~QEvent();
    Q_DISABLE_COPY(QEvent)

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

    static int registerEventType(int hint = -1);

private:
    Type t;
    bool accepted = true;
};

class QTimerEvent : public QEvent {
public:
    /*!
        Constructs a timer event for the timer with the identifier
        \a timerId.
    */
    explicit QTimerEvent(int timerId) : QEvent(Timer), id(timerId) {}
    ~QTimerEvent() override;
    /*!
        Returns the identifier of the timer that fired, as returned by
        QObject::startTimer().
    */
    int timerId() const { return id; }

private:
    int id;
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

private:
    QByteArray m_name;
};
