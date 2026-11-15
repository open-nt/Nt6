// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QInputEvent;
class QKeyEvent;
class QWaylandClient;
class QWaylandCompositor;
class QWaylandDrag;
class QWaylandKeyboard;
class QWaylandKeymap;
class QWaylandPointer;
class QWaylandSurface;
class QWaylandTouch;
class QWaylandView;

class QWaylandSeat : public QWaylandObject {
    Q_OBJECT
public:
    enum CapabilityFlag {
        Pointer = 0x01,
        Keyboard = 0x02,
        Touch = 0x04,
        DefaultCapabilities = Pointer | Keyboard | Touch
    };
    Q_DECLARE_FLAGS(CapabilityFlags, CapabilityFlag)
    Q_FLAG(CapabilityFlags)

    QWaylandSeat(QWaylandCompositor *compositor, CapabilityFlags capabilityFlags = DefaultCapabilities);
    ~QWaylandSeat() override;

    virtual void initialize();
    bool isInitialized() const;

    void sendMousePressEvent(Qt::MouseButton button);
    void sendMouseReleaseEvent(Qt::MouseButton button);
    void sendMouseMoveEvent(QWaylandView *surface, const QPointF &localPos, const QPointF &outputSpacePos = QPointF());
    void sendMouseWheelEvent(Qt::Orientation orientation, int delta);

    void sendKeyPressEvent(uint code);
    void sendKeyReleaseEvent(uint code);

    void sendFullKeyEvent(QKeyEvent *event);
    Q_INVOKABLE void sendKeyEvent(int qtKey, bool pressed);
    Q_INVOKABLE void sendUnicodeKeyPressEvent(uint unicode);
    Q_INVOKABLE void sendUnicodeKeyReleaseEvent(uint unicode);

    uint sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &point, Qt::TouchPointState state);
    Q_INVOKABLE uint sendTouchPointPressed(QWaylandSurface *surface, int id, const QPointF &position);
    Q_INVOKABLE uint sendTouchPointReleased(QWaylandSurface *surface, int id, const QPointF &position);
    Q_INVOKABLE uint sendTouchPointMoved(QWaylandSurface *surface, int id, const QPointF &position);
    Q_INVOKABLE void sendTouchFrameEvent(QWaylandClient *client);
    Q_INVOKABLE void sendTouchCancelEvent(QWaylandClient *client);

    QWaylandPointer *pointer() const;
    QWaylandKeyboard *keyboard() const;
    QWaylandTouch *touch() const;

    QWaylandView *mouseFocus() const;
    void setMouseFocus(QWaylandView *view);

    QWaylandSurface *keyboardFocus() const;
    bool setKeyboardFocus(QWaylandSurface *surface);

    QWaylandKeymap *keymap();

    QWaylandDrag *drag() const;

    QWaylandCompositor *compositor() const;
    CapabilityFlags capabilities() const;

    virtual bool isOwner(QInputEvent *inputEvent) const;

    static QWaylandSeat *fromSeatResource(void *resource);

protected:
    bool event(QEvent *event) override;

signals:
    /*!
        This signal is emitted when the mouse focus moves from \a oldFocus to \a newFocus.
    */
    void mouseFocusChanged(QWaylandView *newFocus, QWaylandView *oldFocus) {
        emitSignal(&QWaylandSeat::mouseFocusChanged, newFocus, oldFocus);
    }
    /*!
        This signal is emitted when the keyboard focus moves from \a oldFocus to \a newFocus.
    */
    void keyboardFocusChanged(QWaylandSurface *newFocus, QWaylandSurface *oldFocus) {
        emitSignal(&QWaylandSeat::keyboardFocusChanged, newFocus, oldFocus);
    }
    /*!
        This signal is emitted when \a client asks for \a surface to be shown as the cursor, with its hotspot at
        (\a hotspotX, \a hotspotY). A null \a surface hides the cursor.
    */
    void cursorSurfaceRequested(QWaylandSurface *surface, int hotspotX, int hotspotY, QWaylandClient *client) {
        emitSignal(&QWaylandSeat::cursorSurfaceRequested, surface, hotspotX, hotspotY, client);
    }

private:
    friend class QWaylandCompositor;
    friend class QWaylandPointer;
    friend class QWaylandKeyboard;
    friend class QWaylandTouch;
    friend struct QWaylandSeatPrivateAccess;
    struct Private;
    std::unique_ptr<Private> d;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QWaylandSeat::CapabilityFlags)
