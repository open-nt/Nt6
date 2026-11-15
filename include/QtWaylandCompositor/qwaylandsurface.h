// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtGui/qregion.h>
#include <QtWaylandCompositor/qwaylandbufferref.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

struct wl_client;
struct wl_resource;

class QWaylandClient;
class QWaylandCompositor;
class QWaylandDrag;
class QWaylandOutput;
class QWaylandView;

namespace QtWaylandPrivate {
class Resource;
}

class QWaylandSurfaceRole {
public:
    /*!
        Constructs a role called \a name.
    */
    explicit QWaylandSurfaceRole(const QByteArray &name) : m_name(name) {}
    /*!
        Returns the name of the role, such as \c "xdg_toplevel".
    */
    const QByteArray name() const { return m_name; }

private:
    QByteArray m_name;
};

class QWaylandSurface : public QWaylandObject {
    Q_OBJECT
    Q_PROPERTY(QWaylandClient *client READ client CONSTANT)
    Q_PROPERTY(QRectF sourceGeometry READ sourceGeometry NOTIFY sourceGeometryChanged)
    Q_PROPERTY(QSize destinationSize READ destinationSize NOTIFY destinationSizeChanged)
    Q_PROPERTY(QSize bufferSize READ bufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(int bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(Qt::ScreenOrientation contentOrientation READ contentOrientation NOTIFY contentOrientationChanged)
    Q_PROPERTY(QWaylandSurface::Origin origin READ origin NOTIFY originChanged)
    Q_PROPERTY(bool hasContent READ hasContent NOTIFY hasContentChanged)
    Q_PROPERTY(bool cursorSurface READ isCursorSurface WRITE markAsCursorSurface NOTIFY cursorSurfaceChanged)
    Q_PROPERTY(bool inhibitsIdle READ inhibitsIdle NOTIFY inhibitsIdleChanged)
    Q_PROPERTY(bool isOpaque READ isOpaque NOTIFY isOpaqueChanged)
public:
    enum Origin { OriginTopLeft, OriginBottomLeft };
    Q_ENUM(Origin)

    QWaylandSurface();
    QWaylandSurface(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version);
    ~QWaylandSurface() override;

    Q_INVOKABLE void initialize(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version);
    bool isInitialized() const;

    QWaylandClient *client() const;
    wl_client *waylandClient() const;

    bool setRole(QWaylandSurfaceRole *role, wl_resource *errorResource, uint errorCode);
    QWaylandSurfaceRole *role() const;

    bool hasContent() const;
    bool isDestroyed() const;

    QRectF sourceGeometry() const;
    QSize destinationSize() const;
    QSize bufferSize() const;
    int bufferScale() const;

    Qt::ScreenOrientation contentOrientation() const;
    Origin origin() const;

    QWaylandCompositor *compositor() const;

    bool inputRegionContains(const QPoint &p) const;
    bool inputRegionContains(const QPointF &position) const;
    QRegion inputRegion() const;
    QRegion opaqueRegion() const;
    bool isOpaque() const;

    Q_INVOKABLE void destroy();
    Q_INVOKABLE void frameStarted();
    Q_INVOKABLE void sendFrameCallbacks();

    QWaylandView *primaryView() const;
    void setPrimaryView(QWaylandView *view);
    QList<QWaylandView *> views() const;

    static QWaylandSurface *fromResource(wl_resource *resource);
    wl_resource *resource() const;

    void markAsCursorSurface(bool cursorSurface);
    bool isCursorSurface() const;

    bool inhibitsIdle() const;

    QWaylandBufferRef currentBuffer() const;
    QRegion currentDamage() const;

    QPoint offset() const;

    QWaylandSurface *parentSurface() const;
    QList<QWaylandSurface *> subsurfaces() const;
    QPoint subsurfacePosition() const;

    void sendEnter(QWaylandOutput *output);
    void sendLeave(QWaylandOutput *output);

signals:
    /*!
        This signal is emitted when the surface gets or loses content.
    */
    void hasContentChanged() { emitSignal(&QWaylandSurface::hasContentChanged); }
    /*!
        This signal is emitted when the client commits damage in \a rect, in surface coordinates.
    */
    void damaged(const QRegion &rect) { emitSignal(&QWaylandSurface::damaged, rect); }
    /*!
        This signal is emitted when the parent of a subsurface changes from \a oldParent to \a newParent.
    */
    void parentChanged(QWaylandSurface *newParent, QWaylandSurface *oldParent) {
        emitSignal(&QWaylandSurface::parentChanged, newParent, oldParent);
    }
    /*!
        This signal is emitted when \a child becomes a subsurface of this surface.
    */
    void childAdded(QWaylandSurface *child) { emitSignal(&QWaylandSurface::childAdded, child); }
    /*!
        This signal is emitted when the source geometry changes.
    */
    void sourceGeometryChanged() { emitSignal(&QWaylandSurface::sourceGeometryChanged); }
    /*!
        This signal is emitted when the destination size changes.
    */
    void destinationSizeChanged() { emitSignal(&QWaylandSurface::destinationSizeChanged); }
    /*!
        This signal is emitted when the buffer size changes.
    */
    void bufferSizeChanged() { emitSignal(&QWaylandSurface::bufferSizeChanged); }
    /*!
        This signal is emitted when the buffer scale changes.
    */
    void bufferScaleChanged() { emitSignal(&QWaylandSurface::bufferScaleChanged); }
    /*!
        This signal is emitted when the content orientation changes.
    */
    void contentOrientationChanged() { emitSignal(&QWaylandSurface::contentOrientationChanged); }
    /*!
        This signal is emitted when the client destroys the surface.
    */
    void surfaceDestroyed() { emitSignal(&QWaylandSurface::surfaceDestroyed); }
    /*!
        This signal is emitted when the origin of the buffer changes.
    */
    void originChanged() { emitSignal(&QWaylandSurface::originChanged); }
    /*!
        This signal is emitted when a subsurface moves to \a position in its parent.
    */
    void subsurfacePositionChanged(const QPoint &position) {
        emitSignal(&QWaylandSurface::subsurfacePositionChanged, position);
    }
    /*!
        This signal is emitted when the subsurface asks to be placed above \a sibling.
    */
    void subsurfacePlaceAbove(QWaylandSurface *sibling) { emitSignal(&QWaylandSurface::subsurfacePlaceAbove, sibling); }
    /*!
        This signal is emitted when the subsurface asks to be placed below \a sibling.
    */
    void subsurfacePlaceBelow(QWaylandSurface *sibling) { emitSignal(&QWaylandSurface::subsurfacePlaceBelow, sibling); }
    /*!
        This signal is emitted when \a drag starts from the surface. Nt never emits it.
    */
    void dragStarted(QWaylandDrag *drag) { emitSignal(&QWaylandSurface::dragStarted, drag); }
    /*!
        This signal is emitted when the surface becomes, or stops being, a cursor surface.
    */
    void cursorSurfaceChanged() { emitSignal(&QWaylandSurface::cursorSurfaceChanged); }
    /*!
        This signal is emitted when the idle-inhibition state changes.
    */
    void inhibitsIdleChanged() { emitSignal(&QWaylandSurface::inhibitsIdleChanged); }
    /*!
        This signal is emitted when the opaque region changes.
    */
    void isOpaqueChanged() { emitSignal(&QWaylandSurface::isOpaqueChanged); }
    /*!
        This signal is emitted after a commit, to have the surface redrawn.
    */
    void redraw() { emitSignal(&QWaylandSurface::redraw); }
    /*!
        This signal is emitted when the client attached its next buffer with an \a offset.
    */
    void offsetForNextFrame(const QPoint &offset) { emitSignal(&QWaylandSurface::offsetForNextFrame, offset); }

protected:
    /*!
        Constructs the surface of \a client of \a compositor for the object \a id with protocol \a version, and
        initializes it unless \a deferInit is set, for a subclass that initializes later.
    */
    QWaylandSurface(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version, bool deferInit);

private:
    friend class QWaylandCompositor;
    friend class QWaylandView;
    friend class QWaylandXdgShell;
    friend class QWaylandXdgSurface;
    friend class QWaylandSeat;
    friend class QWaylandPointer;
    friend class QWaylandKeyboard;
    friend class QWaylandTouch;
    friend class QWaylandOutput;
    friend struct QWaylandSurfacePrivateAccess;
    struct Private;
    std::unique_ptr<Private> d;
};
