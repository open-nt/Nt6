// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QWaylandClient;
class QWaylandCompositor;
class QWaylandOutput;
class QWaylandSeat;
class QWaylandSurface;
class QWaylandSurfaceRole;
class QWaylandXdgPopup;
class QWaylandXdgSurface;
class QWaylandXdgToplevel;

namespace QtWaylandPrivate {
class Resource;
}

class QWaylandShellSurface : public QWaylandCompositorExtension {
    Q_OBJECT
    Q_PROPERTY(Qt::WindowType windowType READ windowType NOTIFY windowTypeChanged)
public:
    QWaylandShellSurface();
    explicit QWaylandShellSurface(QWaylandObject *container);

    /*!
        Returns the kind of window the shell surface is: Qt::Window by default.
    */
    virtual Qt::WindowType windowType() const { return Qt::Window; }
    /*!
        Returns the surface the shell surface is a role of, or \c nullptr.
    */
    virtual QWaylandSurface *surface() const = 0;

signals:
    /*!
        This signal is emitted when the window type changes.
    */
    void windowTypeChanged() { emitSignal(&QWaylandShellSurface::windowTypeChanged); }
};

class QWaylandXdgShell : public QWaylandCompositorExtensionTemplate<QWaylandXdgShell> {
    Q_OBJECT
public:
    QWaylandXdgShell();
    explicit QWaylandXdgShell(QWaylandCompositor *compositor);
    ~QWaylandXdgShell() override;

    void initialize() override;

    static QByteArray interfaceName();

public slots:
    uint ping(QWaylandClient *client);

signals:
    /*!
        This signal is emitted when the client has created \a xdgSurface, which has no role yet.
    */
    void xdgSurfaceCreated(QWaylandXdgSurface *xdgSurface) {
        emitSignal(&QWaylandXdgShell::xdgSurfaceCreated, xdgSurface);
    }
    /*!
        This signal is emitted when \a toplevel has been created for \a xdgSurface.
    */
    void toplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface) {
        emitSignal(&QWaylandXdgShell::toplevelCreated, toplevel, xdgSurface);
    }
    /*!
        This signal is emitted when \a popup has been created for \a xdgSurface.
    */
    void popupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface) {
        emitSignal(&QWaylandXdgShell::popupCreated, popup, xdgSurface);
    }
    /*!
        This signal is emitted when a client answers ping() with \a serial.
    */
    void pong(uint serial) { emitSignal(&QWaylandXdgShell::pong, serial); }

private:
    friend class QWaylandXdgSurface;
    friend class QWaylandXdgToplevel;
    friend class QWaylandXdgPopup;
    struct Private;
    std::unique_ptr<Private> d;
};

class QWaylandXdgSurface : public QWaylandShellSurface {
    Q_OBJECT
    Q_PROPERTY(QWaylandXdgShell *shell READ shell NOTIFY shellChanged)
    Q_PROPERTY(QWaylandSurface *surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QWaylandXdgToplevel *toplevel READ toplevel NOTIFY toplevelCreated)
    Q_PROPERTY(QWaylandXdgPopup *popup READ popup NOTIFY popupCreated)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry NOTIFY windowGeometryChanged)
public:
    QWaylandXdgSurface();
    ~QWaylandXdgSurface() override;

    QWaylandXdgShell *shell() const;
    QWaylandSurface *surface() const override;
    QWaylandXdgToplevel *toplevel() const;
    QWaylandXdgPopup *popup() const;
    QRect windowGeometry() const;

    Qt::WindowType windowType() const override;
    /*!
        \reimp
    */
    QByteArray extensionInterfaceName() const override { return interfaceName(); }

    static QByteArray interfaceName();
    static QWaylandSurfaceRole *role();

signals:
    /*!
        This signal is emitted when the shell changes.
    */
    void shellChanged() { emitSignal(&QWaylandXdgSurface::shellChanged); }
    /*!
        This signal is emitted when the surface changes.
    */
    void surfaceChanged() { emitSignal(&QWaylandXdgSurface::surfaceChanged); }
    /*!
        This signal is emitted when the client gives the surface the toplevel role.
    */
    void toplevelCreated() { emitSignal(&QWaylandXdgSurface::toplevelCreated); }
    /*!
        This signal is emitted when the client gives the surface the popup role.
    */
    void popupCreated() { emitSignal(&QWaylandXdgSurface::popupCreated); }
    /*!
        This signal is emitted when the client sets the window geometry.
    */
    void windowGeometryChanged() { emitSignal(&QWaylandXdgSurface::windowGeometryChanged); }

private:
    friend class QWaylandXdgShell;
    friend class QWaylandXdgToplevel;
    friend class QWaylandXdgPopup;
    struct Private;
    std::unique_ptr<Private> d;
};

class QWaylandXdgToplevel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWaylandXdgSurface *xdgSurface READ xdgSurface CONSTANT)
    Q_PROPERTY(QWaylandXdgToplevel *parentToplevel READ parentToplevel NOTIFY parentToplevelChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString appId READ appId NOTIFY appIdChanged)
    Q_PROPERTY(QSize maxSize READ maxSize NOTIFY maxSizeChanged)
    Q_PROPERTY(QSize minSize READ minSize NOTIFY minSizeChanged)
    Q_PROPERTY(bool maximized READ maximized NOTIFY maximizedChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizingChanged)
    Q_PROPERTY(bool activated READ activated NOTIFY activatedChanged)
    Q_PROPERTY(QWaylandXdgToplevel::DecorationMode decorationMode READ decorationMode NOTIFY decorationModeChanged)
public:
    enum State : uint {
        MaximizedState = 1,
        FullscreenState = 2,
        ResizingState = 3,
        ActivatedState = 4,
        TiledLeftState = 5,
        TiledRightState = 6,
        TiledTopState = 7,
        TiledBottomState = 8
    };
    Q_ENUM(State)

    enum DecorationMode { ClientSideDecoration = 1, ServerSideDecoration = 2 };
    Q_ENUM(DecorationMode)

    explicit QWaylandXdgToplevel(QWaylandXdgSurface *xdgSurface, QtWaylandPrivate::Resource *resource);
    ~QWaylandXdgToplevel() override;

    QWaylandXdgSurface *xdgSurface() const;
    QWaylandXdgToplevel *parentToplevel() const;

    QString title() const;
    QString appId() const;
    QSize maxSize() const;
    QSize minSize() const;
    QList<QWaylandXdgToplevel::State> states() const;
    bool maximized() const;
    bool fullscreen() const;
    bool resizing() const;
    bool activated() const;
    DecorationMode decorationMode() const;

    Q_INVOKABLE QSize sizeForResize(const QSizeF &size, const QPointF &delta, Qt::Edges edges) const;
    uint sendConfigure(const QSize &size, const QList<State> &states);
    Q_INVOKABLE uint sendConfigure(const QSize &size, const QList<int> &states);
    Q_INVOKABLE void sendClose();
    Q_INVOKABLE uint sendMaximized(const QSize &size);
    Q_INVOKABLE uint sendUnmaximized(const QSize &size = QSize(0, 0));
    Q_INVOKABLE uint sendFullscreen(const QSize &size);
    Q_INVOKABLE uint sendResizing(const QSize &maxSize);

    static QWaylandSurfaceRole *role();

signals:
    /*!
        This signal is emitted when the parent toplevel changes.
    */
    void parentToplevelChanged() { emitSignal(&QWaylandXdgToplevel::parentToplevelChanged); }
    /*!
        This signal is emitted when the title changes.
    */
    void titleChanged() { emitSignal(&QWaylandXdgToplevel::titleChanged); }
    /*!
        This signal is emitted when the application id changes.
    */
    void appIdChanged() { emitSignal(&QWaylandXdgToplevel::appIdChanged); }
    /*!
        This signal is emitted when the maximum size changes.
    */
    void maxSizeChanged() { emitSignal(&QWaylandXdgToplevel::maxSizeChanged); }
    /*!
        This signal is emitted when the minimum size changes.
    */
    void minSizeChanged() { emitSignal(&QWaylandXdgToplevel::minSizeChanged); }
    /*!
        This signal is emitted when the client asks to start an interactive move with \a seat.
    */
    void startMove(QWaylandSeat *seat) { emitSignal(&QWaylandXdgToplevel::startMove, seat); }
    /*!
        This signal is emitted when the client asks to start an interactive resize of \a edges with \a seat.
    */
    void startResize(QWaylandSeat *seat, Qt::Edges edges) {
        emitSignal(&QWaylandXdgToplevel::startResize, seat, edges);
    }
    /*!
        This signal is emitted when the states the client acknowledged change.
    */
    void statesChanged() { emitSignal(&QWaylandXdgToplevel::statesChanged); }
    /*!
        This signal is emitted when the maximized state changes.
    */
    void maximizedChanged() { emitSignal(&QWaylandXdgToplevel::maximizedChanged); }
    /*!
        This signal is emitted when the fullscreen state changes.
    */
    void fullscreenChanged() { emitSignal(&QWaylandXdgToplevel::fullscreenChanged); }
    /*!
        This signal is emitted when the resizing state changes.
    */
    void resizingChanged() { emitSignal(&QWaylandXdgToplevel::resizingChanged); }
    /*!
        This signal is emitted when the activated state changes.
    */
    void activatedChanged() { emitSignal(&QWaylandXdgToplevel::activatedChanged); }
    /*!
        This signal is emitted when the decoration mode changes.
    */
    void decorationModeChanged() { emitSignal(&QWaylandXdgToplevel::decorationModeChanged); }
    /*!
        This signal is emitted when the client asks for the window menu at \a localSurfacePosition with \a seat.
    */
    void showWindowMenu(QWaylandSeat *seat, const QPoint &localSurfacePosition) {
        emitSignal(&QWaylandXdgToplevel::showWindowMenu, seat, localSurfacePosition);
    }
    /*!
        This signal is emitted when the client asks to be maximized.
    */
    void setMaximized() { emitSignal(&QWaylandXdgToplevel::setMaximized); }
    /*!
        This signal is emitted when the client asks to leave the maximized state.
    */
    void unsetMaximized() { emitSignal(&QWaylandXdgToplevel::unsetMaximized); }
    /*!
        This signal is emitted when the client asks to go full screen, on \a output or any.
    */
    void setFullscreen(QWaylandOutput *output) { emitSignal(&QWaylandXdgToplevel::setFullscreen, output); }
    /*!
        This signal is emitted when the client asks to leave full screen.
    */
    void unsetFullscreen() { emitSignal(&QWaylandXdgToplevel::unsetFullscreen); }
    /*!
        This signal is emitted when the client asks to be minimized.
    */
    void setMinimized() { emitSignal(&QWaylandXdgToplevel::setMinimized); }

private:
    friend class QWaylandXdgShell;
    friend class QWaylandXdgSurface;
    struct Private;
    std::unique_ptr<Private> d;
};

class QWaylandXdgPopup : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWaylandXdgSurface *xdgSurface READ xdgSurface CONSTANT)
    Q_PROPERTY(QWaylandXdgSurface *parentXdgSurface READ parentXdgSurface CONSTANT)
    Q_PROPERTY(QRect configuredGeometry READ configuredGeometry NOTIFY configuredGeometryChanged)
    Q_PROPERTY(QRect anchorRect READ anchorRect CONSTANT)
    Q_PROPERTY(Qt::Edges anchorEdges READ anchorEdges CONSTANT)
    Q_PROPERTY(Qt::Edges gravityEdges READ gravityEdges CONSTANT)
    Q_PROPERTY(QPoint offset READ offset CONSTANT)
    Q_PROPERTY(QSize positionerSize READ positionerSize CONSTANT)
    Q_PROPERTY(QPoint unconstrainedPosition READ unconstrainedPosition CONSTANT)
public:
    QWaylandXdgPopup(QWaylandXdgSurface *xdgSurface, QWaylandXdgSurface *parentXdgSurface,
                     QtWaylandPrivate::Resource *positioner, QtWaylandPrivate::Resource *resource);
    ~QWaylandXdgPopup() override;

    QWaylandXdgSurface *xdgSurface() const;
    QWaylandXdgSurface *parentXdgSurface() const;
    QRect configuredGeometry() const;

    QRect anchorRect() const;
    Qt::Edges anchorEdges() const;
    Qt::Edges gravityEdges() const;
    Qt::Orientations slideConstraints() const;
    Qt::Orientations flipConstraints() const;
    Qt::Orientations resizeConstraints() const;
    QPoint offset() const;
    QSize positionerSize() const;
    QPoint unconstrainedPosition() const;

    Q_INVOKABLE uint sendConfigure(const QRect &geometry);
    Q_INVOKABLE void sendPopupDone();

    static QWaylandSurfaceRole *role();

signals:
    /*!
        This signal is emitted when the configured geometry changes.
    */
    void configuredGeometryChanged() { emitSignal(&QWaylandXdgPopup::configuredGeometryChanged); }

private:
    friend class QWaylandXdgShell;
    friend class QWaylandXdgSurface;
    struct Private;
    std::unique_ptr<Private> d;
};
