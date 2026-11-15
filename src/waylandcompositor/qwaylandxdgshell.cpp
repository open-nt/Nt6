// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandxdgshell.h>

#include "qwaylandcompositor_p.h"

#include <QtCore/qdebug.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandoutput.h>
#include <QtWaylandCompositor/qwaylandseat.h>
#include <QtWaylandCompositor/qwaylandsurface.h>

#include <algorithm>
#include <deque>
#include <map>

using namespace QtWaylandPrivate;

namespace {

QWaylandSurfaceRole g_xdgSurfaceRole(QByteArray("xdg_surface"));
QWaylandSurfaceRole g_toplevelRole(QByteArray("xdg_toplevel"));
QWaylandSurfaceRole g_popupRole(QByteArray("xdg_popup"));

constexpr std::uint32_t kXdgSurfaceErrorNotConstructed = 1;
constexpr std::uint32_t kXdgSurfaceErrorAlreadyConstructed = 2;
constexpr std::uint32_t kXdgSurfaceErrorInvalidSerial = 4;

Qt::Edges edgesFromXdg(std::uint32_t xdgEdges) {
    Qt::Edges edges;
    if (xdgEdges & 1)
        edges |= Qt::TopEdge;
    if (xdgEdges & 2)
        edges |= Qt::BottomEdge;
    if (xdgEdges & 4)
        edges |= Qt::LeftEdge;
    if (xdgEdges & 8)
        edges |= Qt::RightEdge;
    return edges;
}

// xdg_positioner anchor and gravity values: none, top, bottom, left, right, top_left, bottom_left, top_right,
// bottom_right.
Qt::Edges edgesFromAnchor(std::uint32_t anchor) {
    switch (anchor) {
    case 1:
        return Qt::TopEdge;
    case 2:
        return Qt::BottomEdge;
    case 3:
        return Qt::LeftEdge;
    case 4:
        return Qt::RightEdge;
    case 5:
        return Qt::TopEdge | Qt::LeftEdge;
    case 6:
        return Qt::BottomEdge | Qt::LeftEdge;
    case 7:
        return Qt::TopEdge | Qt::RightEdge;
    case 8:
        return Qt::BottomEdge | Qt::RightEdge;
    default:
        return Qt::Edges();
    }
}

struct Configure {
    std::uint32_t serial;
    QSize size;
    QList<QWaylandXdgToplevel::State> states;
    QRect popupGeometry;
};

// xdg_positioner objects.
class PositionerHandler : public ResourceHandler {
public:
    std::map<Resource *, PositionerData> positioners;
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        PositionerData &p = positioners[resource];
        switch (opcode) {
        case kPositionerDestroy:
            resource->destroy();
            break;
        case kPositionerSetSize:
            p.size = QSize(args[0].i, args[1].i);
            break;
        case kPositionerSetAnchorRect:
            p.anchorRect = QRect(args[0].i, args[1].i, args[2].i, args[3].i);
            break;
        case kPositionerSetAnchor:
            p.anchor = args[0].u;
            break;
        case kPositionerSetGravity:
            p.gravity = args[0].u;
            break;
        case kPositionerSetConstraintAdjustment:
            p.constraintAdjustment = args[0].u;
            break;
        case kPositionerSetOffset:
            p.offset = QPoint(args[0].i, args[1].i);
            break;
        case kPositionerSetReactive:
            p.reactive = true;
            break;
        case kPositionerSetParentSize:
            p.parentSize = QSize(args[0].i, args[1].i);
            break;
        case kPositionerSetParentConfigure:
            p.parentConfigure = args[0].u;
            break;
        default:
            break;
        }
    }
    void resourceDestroyed(Resource *resource) override { positioners.erase(resource); }
};

} // namespace

struct QWaylandXdgShell::Private {
    QWaylandCompositor *compositor = nullptr;
    std::uint32_t global = 0;
    class BaseHandler;
    std::unique_ptr<BaseHandler> baseHandler;
    PositionerHandler positioners;
    std::map<std::uint32_t, Resource *> pings; // serial -> xdg_wm_base object
    QList<QWaylandXdgSurface *> xdgSurfaces;
};

struct QWaylandXdgSurface::Private : public ResourceHandler {
    explicit Private(QWaylandXdgSurface *owner) : q(owner) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override;
    void resourceDestroyed(Resource *resource) override;
    uint sendConfigure(const Configure &configure);

    QWaylandXdgSurface *q;
    QWaylandXdgShell *shell = nullptr;
    QWaylandSurface *surface = nullptr;
    Resource *resource = nullptr;
    QWaylandXdgToplevel *toplevel = nullptr;
    QWaylandXdgPopup *popup = nullptr;
    QRect windowGeometry;
    std::deque<Configure> pendingConfigures;
    bool configured = false;
};

struct QWaylandXdgToplevel::Private : public ResourceHandler {
    explicit Private(QWaylandXdgToplevel *owner) : q(owner) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override;
    void resourceDestroyed(Resource *resource) override;
    void applyConfigure(const Configure &configure);

    QWaylandXdgToplevel *q;
    QWaylandXdgSurface *xdgSurface = nullptr;
    Resource *resource = nullptr;
    QWaylandXdgToplevel *parent = nullptr;
    QString title;
    QString appId;
    QSize maxSize;
    QSize minSize;
    QList<State> states;
    QSize lastSentSize;
    QList<State> lastSentStates;
};

struct QWaylandXdgPopup::Private : public ResourceHandler {
    explicit Private(QWaylandXdgPopup *owner) : q(owner) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override;
    void resourceDestroyed(Resource *resource) override;

    QWaylandXdgPopup *q;
    QWaylandXdgSurface *xdgSurface = nullptr;
    QWaylandXdgSurface *parent = nullptr;
    Resource *resource = nullptr;
    PositionerData positioner;
    QRect configuredGeometry;
};

// xdg_wm_base objects: one per client that bound the global.
class QWaylandXdgShell::Private::BaseHandler : public ResourceHandler {
public:
    explicit BaseHandler(QWaylandXdgShell *shell) : q(shell) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        Private *d = q->d.get();
        switch (opcode) {
        case kWmBaseDestroy:
            resource->destroy();
            break;
        case kWmBaseCreatePositioner:
            if (Resource *r = resource->connection()->resource(args[0].u)) {
                r->setHandler(&d->positioners);
                d->positioners.positioners[r] = PositionerData();
            }
            break;
        case kWmBaseGetXdgSurface: {
            Resource *r = resource->connection()->resource(args[0].u);
            QWaylandSurface *surface =
                QWaylandSurfacePrivateAccess::fromResource(resource->connection()->resource(args[1].u));
            if (!r || !surface)
                return;
            // One xdg_surface per wl_surface; the role itself comes with the toplevel or popup.
            for (QWaylandXdgSurface *existing : std::as_const(d->xdgSurfaces)) {
                if (existing->surface() == surface) {
                    resource->postError(kWmBaseErrorRole,
                                        QString::fromLatin1("the surface already has an xdg_surface"));
                    return;
                }
            }
            if (surface->role() && surface->role() != &g_toplevelRole && surface->role() != &g_popupRole) {
                resource->postError(kWmBaseErrorRole, QString::fromLatin1("the surface has another role"));
                return;
            }
            auto *xdgSurface = new QWaylandXdgSurface();
            xdgSurface->setParent(q);
            xdgSurface->d->shell = q;
            xdgSurface->d->surface = surface;
            xdgSurface->d->resource = r;
            r->setHandler(xdgSurface->d.get());
            d->xdgSurfaces.append(xdgSurface);
            QObject::connect(surface, &QWaylandSurface::surfaceDestroyed, xdgSurface, [xdgSurface] {
                xdgSurface->d->surface = nullptr;
                emit xdgSurface->surfaceChanged();
            });
            emit q->xdgSurfaceCreated(xdgSurface);
            break;
        }
        case kWmBasePong: {
            const std::uint32_t serial = args[0].u;
            if (d->pings.erase(serial) != 0)
                emit q->pong(serial);
            break;
        }
        default:
            break;
        }
    }
    void resourceDestroyed(Resource *resource) override {
        Private *d = q->d.get();
        for (auto it = d->pings.begin(); it != d->pings.end();) {
            if (it->second == resource)
                it = d->pings.erase(it);
            else
                ++it;
        }
    }
    QWaylandXdgShell *q;
};

// ---- QWaylandXdgShell ----------------------------------------------------------------------------------------

/*!
    \class QWaylandXdgShell
    \brief The QWaylandXdgShell class implements the xdg-shell protocol, which gives surfaces window roles.

    Attach it to the compositor before create():

    \code
    QWaylandCompositor compositor;
    QWaylandXdgShell shell(&compositor);
    QObject::connect(&shell, &QWaylandXdgShell::toplevelCreated,
                     [](QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *surface) {
        toplevel->sendConfigure(QSize(800, 600), {QWaylandXdgToplevel::ActivatedState});
    });
    \endcode

    A client that wants a window creates an xdg_surface for a wl_surface, then a toplevel or a popup from it;
    the shell emits xdgSurfaceCreated(), then toplevelCreated() or popupCreated(). The compositor answers with
    sendConfigure() calls, which the client acknowledges before it commits a buffer.

    \differences
    Version 6 of xdg-shell is offered. There is no xdg-decoration, so decorationMode() is always
    ClientSideDecoration, and no QML types.
    \enddifferences

    \sa QWaylandXdgSurface, QWaylandXdgToplevel, QWaylandXdgPopup
*/

/*!
    Constructs a shell extension without a compositor; call setExtensionContainer().
*/
QWaylandXdgShell::QWaylandXdgShell() : d(std::make_unique<Private>()) {
    d->baseHandler = std::make_unique<Private::BaseHandler>(this);
}

/*!
    Constructs the shell extension of \a compositor.
*/
QWaylandXdgShell::QWaylandXdgShell(QWaylandCompositor *compositor)
    : QWaylandCompositorExtensionTemplate<QWaylandXdgShell>(compositor), d(std::make_unique<Private>()) {
    d->baseHandler = std::make_unique<Private::BaseHandler>(this);
    d->compositor = compositor;
}

/*!
    Destroys the shell; the global is withdrawn from the clients.
*/
QWaylandXdgShell::~QWaylandXdgShell() {
    if (d->compositor && d->global)
        QWaylandCompositorPrivateAccess::display(d->compositor)->removeGlobal(d->global);
}

/*!
    \reimp
*/
void QWaylandXdgShell::initialize() {
    QWaylandCompositorExtensionTemplate<QWaylandXdgShell>::initialize();
    auto *compositor = qobject_cast<QWaylandCompositor *>(extensionContainer());
    if (!compositor) {
        qWarning("QWaylandXdgShell: the extension container is not a QWaylandCompositor");
        return;
    }
    d->compositor = compositor;
    if (!compositor->isCreated())
        return; // the compositor initializes its extensions in create()
    if (d->global)
        return;
    Display *display = QWaylandCompositorPrivateAccess::display(compositor);
    display->registerInterface(&kPositionerInterface);
    display->registerInterface(&kXdgSurfaceInterface);
    display->registerInterface(&kToplevelInterface);
    display->registerInterface(&kPopupInterface);
    d->global = display->addGlobal(&kWmBaseInterface, kWmBaseInterface.version,
                                   [this](Resource *r) { r->setHandler(d->baseHandler.get()); });
}

/*!
    Returns \c "xdg_wm_base".
*/
QByteArray QWaylandXdgShell::interfaceName() { return QByteArray("xdg_wm_base"); }

/*!
    Pings \a client; pong() is emitted with the returned serial when it answers.
*/
uint QWaylandXdgShell::ping(QWaylandClient *client) {
    auto *connection = reinterpret_cast<Connection *>(client ? client->client() : nullptr);
    if (!connection)
        return 0;
    const std::uint32_t serial = d->compositor->nextSerial();
    for (Resource *r : connection->resourcesOf(&kWmBaseInterface)) {
        d->pings[serial] = r;
        r->send(kWmBasePing, {Arg::Uint(serial)});
    }
    return serial;
}

// ---- QWaylandShellSurface ------------------------------------------------------------------------------------

/*!
    \class QWaylandShellSurface
    \brief The QWaylandShellSurface class is the base of the window roles a shell gives a surface.

    \differences
    There is no QML item for shell surfaces.
    \enddifferences
*/

/*!
    Constructs a shell surface without a container.
*/
QWaylandShellSurface::QWaylandShellSurface() = default;

/*!
    Constructs a shell surface of \a container.
*/
QWaylandShellSurface::QWaylandShellSurface(QWaylandObject *container) : QWaylandCompositorExtension(container) {}

// ---- QWaylandXdgSurface --------------------------------------------------------------------------------------

/*!
    \class QWaylandXdgSurface
    \brief The QWaylandXdgSurface class is a surface that xdg-shell manages, before and after it gets a role.

    The client makes it a toplevel() or a popup(); windowGeometry() is the part of the surface that is the
    window, without shadows.

    \differences
    The extension interface name is \c "xdg_surface"; there is no QML item.
    \enddifferences
*/

/*!
    Constructs an xdg surface; the shell binds it to the client's object.
*/
QWaylandXdgSurface::QWaylandXdgSurface() : d(std::make_unique<Private>(this)) {}

/*!
    Destroys the xdg surface and its role object.
*/
QWaylandXdgSurface::~QWaylandXdgSurface() {
    if (d->shell)
        d->shell->d->xdgSurfaces.removeAll(this);
    if (d->resource) {
        d->resource->setHandler(nullptr);
        d->resource = nullptr;
    }
    delete d->toplevel;
    delete d->popup;
}

/*!
    Returns the shell that made the surface.
*/
QWaylandXdgShell *QWaylandXdgSurface::shell() const { return d->shell; }

/*!
    \reimp
*/
QWaylandSurface *QWaylandXdgSurface::surface() const { return d->surface; }

/*!
    Returns the toplevel role, or \c nullptr if the surface has none.
*/
QWaylandXdgToplevel *QWaylandXdgSurface::toplevel() const { return d->toplevel; }

/*!
    Returns the popup role, or \c nullptr if the surface has none.
*/
QWaylandXdgPopup *QWaylandXdgSurface::popup() const { return d->popup; }

/*!
    Returns the window geometry the client set, or the whole surface.
*/
QRect QWaylandXdgSurface::windowGeometry() const {
    if (d->windowGeometry.isValid())
        return d->windowGeometry;
    return d->surface ? QRect(QPoint(0, 0), d->surface->destinationSize()) : QRect();
}

/*!
    \reimp

    Returns Qt::Popup for a popup, Qt::Window otherwise.
*/
Qt::WindowType QWaylandXdgSurface::windowType() const { return d->popup ? Qt::Popup : Qt::Window; }

/*!
    Returns \c "xdg_surface".
*/
QByteArray QWaylandXdgSurface::interfaceName() { return QByteArray("xdg_surface"); }

/*!
    Returns the role that xdg surfaces give their wl_surface.
*/
QWaylandSurfaceRole *QWaylandXdgSurface::role() { return &g_xdgSurfaceRole; }

void QWaylandXdgSurface::Private::request(Resource *res, std::uint32_t opcode, const Args &args) {
    switch (opcode) {
    case kXdgSurfaceDestroy:
        res->destroy();
        break;
    case kXdgSurfaceGetToplevel: {
        Resource *r = res->connection()->resource(args[0].u);
        if (!r || !surface)
            return;
        if (toplevel || popup) {
            res->postError(kXdgSurfaceErrorAlreadyConstructed, QString::fromLatin1("the xdg_surface has a role"));
            return;
        }
        if (!surface->setRole(&g_toplevelRole, reinterpret_cast<wl_resource *>(res), kWmBaseErrorRole))
            return;
        toplevel = new QWaylandXdgToplevel(q, r);
        emit q->toplevelCreated();
        emit shell->toplevelCreated(toplevel, q);
        break;
    }
    case kXdgSurfaceGetPopup: {
        Resource *r = res->connection()->resource(args[0].u);
        if (!r || !surface)
            return;
        if (toplevel || popup) {
            res->postError(kXdgSurfaceErrorAlreadyConstructed, QString::fromLatin1("the xdg_surface has a role"));
            return;
        }
        QWaylandXdgSurface *parentSurface = nullptr;
        if (!args[1].null) {
            Resource *parentResource = res->connection()->resource(args[1].u);
            if (parentResource && parentResource->interface() == &kXdgSurfaceInterface && parentResource->handler())
                parentSurface = static_cast<Private *>(parentResource->handler())->q;
        }
        Resource *positioner = res->connection()->resource(args[2].u);
        if (!positioner || positioner->interface() != &kPositionerInterface) {
            res->postError(kWmBaseErrorInvalidPositioner, QString::fromLatin1("invalid positioner"));
            return;
        }
        if (!surface->setRole(&g_popupRole, reinterpret_cast<wl_resource *>(res), kWmBaseErrorRole))
            return;
        popup = new QWaylandXdgPopup(q, parentSurface, positioner, r);
        emit q->popupCreated();
        emit shell->popupCreated(popup, q);
        emit q->windowTypeChanged();
        break;
    }
    case kXdgSurfaceSetWindowGeometry:
        windowGeometry = QRect(args[0].i, args[1].i, args[2].i, args[3].i);
        emit q->windowGeometryChanged();
        break;
    case kXdgSurfaceAckConfigure: {
        const std::uint32_t serial = args[0].u;
        bool found = false;
        while (!pendingConfigures.empty()) {
            const Configure c = pendingConfigures.front();
            pendingConfigures.pop_front();
            if (c.serial == serial) {
                found = true;
                configured = true;
                if (toplevel)
                    toplevel->d->applyConfigure(c);
                if (popup && popup->d->configuredGeometry != c.popupGeometry) {
                    popup->d->configuredGeometry = c.popupGeometry;
                    emit popup->configuredGeometryChanged();
                }
                break;
            }
        }
        if (!found)
            res->postError(kXdgSurfaceErrorInvalidSerial, QString::fromLatin1("unknown configure serial"));
        break;
    }
    default:
        break;
    }
}

void QWaylandXdgSurface::Private::resourceDestroyed(Resource *) {
    resource = nullptr;
    q->deleteLater();
}

uint QWaylandXdgSurface::Private::sendConfigure(const Configure &configure) {
    if (!resource)
        return 0;
    pendingConfigures.push_back(configure);
    resource->send(kXdgSurfaceConfigure, {Arg::Uint(configure.serial)});
    return configure.serial;
}

// ---- QWaylandXdgToplevel -------------------------------------------------------------------------------------

/*!
    \class QWaylandXdgToplevel
    \brief The QWaylandXdgToplevel class is a top-level window of xdg-shell.

    The client sets the title() and appId() and asks for states with the setMaximized(), setFullscreen() and
    similar signals; the compositor decides and answers with sendConfigure() or its shorthands sendMaximized(),
    sendFullscreen(), sendUnmaximized() and sendResizing(). The states() are those the client has acknowledged.

    \differences
    decorationMode() is always ClientSideDecoration, as there is no xdg-decoration yet.
    \enddifferences
*/

/*!
    \enum QWaylandXdgToplevel::State

    The states of a toplevel, as xdg-shell numbers them.

    \value MaximizedState The window is maximized.
    \value FullscreenState The window is full screen.
    \value ResizingState The window is being resized.
    \value ActivatedState The window is active.
    \value TiledLeftState The window is tiled to the left edge.
    \value TiledRightState The window is tiled to the right edge.
    \value TiledTopState The window is tiled to the top edge.
    \value TiledBottomState The window is tiled to the bottom edge.
*/

/*!
    \enum QWaylandXdgToplevel::DecorationMode

    Who draws the window decorations.

    \value ClientSideDecoration The client.
    \value ServerSideDecoration The compositor.
*/

/*!
    Constructs the toplevel role of \a xdgSurface for the client's object \a resource. The shell does this.
*/
QWaylandXdgToplevel::QWaylandXdgToplevel(QWaylandXdgSurface *xdgSurface, Resource *resource)
    : QObject(xdgSurface), d(std::make_unique<Private>(this)) {
    d->xdgSurface = xdgSurface;
    d->resource = resource;
    resource->setHandler(d.get());
}

/*!
    Destroys the toplevel.
*/
QWaylandXdgToplevel::~QWaylandXdgToplevel() {
    if (d->xdgSurface && d->xdgSurface->d->toplevel == this)
        d->xdgSurface->d->toplevel = nullptr;
    if (d->resource) {
        d->resource->setHandler(nullptr);
        d->resource = nullptr;
    }
}

/*!
    Returns the xdg surface the toplevel is the role of.
*/
QWaylandXdgSurface *QWaylandXdgToplevel::xdgSurface() const { return d->xdgSurface; }

/*!
    Returns the toplevel the client set as parent, or \c nullptr.
*/
QWaylandXdgToplevel *QWaylandXdgToplevel::parentToplevel() const { return d->parent; }

/*!
    Returns the title the client set.
*/
QString QWaylandXdgToplevel::title() const { return d->title; }

/*!
    Returns the application id the client set.
*/
QString QWaylandXdgToplevel::appId() const { return d->appId; }

/*!
    Returns the maximum size the client set; a dimension of 0 is no limit.
*/
QSize QWaylandXdgToplevel::maxSize() const { return d->maxSize; }

/*!
    Returns the minimum size the client set; a dimension of 0 is no limit.
*/
QSize QWaylandXdgToplevel::minSize() const { return d->minSize; }

/*!
    Returns the states the client has acknowledged.
*/
QList<QWaylandXdgToplevel::State> QWaylandXdgToplevel::states() const { return d->states; }

/*!
    Returns \c true if MaximizedState is among states().
*/
bool QWaylandXdgToplevel::maximized() const { return d->states.contains(MaximizedState); }

/*!
    Returns \c true if FullscreenState is among states().
*/
bool QWaylandXdgToplevel::fullscreen() const { return d->states.contains(FullscreenState); }

/*!
    Returns \c true if ResizingState is among states().
*/
bool QWaylandXdgToplevel::resizing() const { return d->states.contains(ResizingState); }

/*!
    Returns \c true if ActivatedState is among states().
*/
bool QWaylandXdgToplevel::activated() const { return d->states.contains(ActivatedState); }

/*!
    Returns who decorates the window: always ClientSideDecoration.
*/
QWaylandXdgToplevel::DecorationMode QWaylandXdgToplevel::decorationMode() const { return ClientSideDecoration; }

/*!
    Returns the size a window of \a size becomes when its \a edges are dragged by \a delta, kept within
    minSize() and maxSize().
*/
QSize QWaylandXdgToplevel::sizeForResize(const QSizeF &size, const QPointF &delta, Qt::Edges edges) const {
    qreal width = size.width();
    qreal height = size.height();
    if (edges & Qt::LeftEdge)
        width -= delta.x();
    else if (edges & Qt::RightEdge)
        width += delta.x();
    if (edges & Qt::TopEdge)
        height -= delta.y();
    else if (edges & Qt::BottomEdge)
        height += delta.y();
    QSize result(qRound(width), qRound(height));
    if (d->minSize.width() > 0)
        result.setWidth(std::max(result.width(), d->minSize.width()));
    if (d->minSize.height() > 0)
        result.setHeight(std::max(result.height(), d->minSize.height()));
    if (d->maxSize.width() > 0)
        result.setWidth(std::min(result.width(), d->maxSize.width()));
    if (d->maxSize.height() > 0)
        result.setHeight(std::min(result.height(), d->maxSize.height()));
    return QSize(std::max(1, result.width()), std::max(1, result.height()));
}

/*!
    Asks the client to take \a size (a dimension of 0 lets it choose) and \a states. Returns the serial the
    client acknowledges.
*/
uint QWaylandXdgToplevel::sendConfigure(const QSize &size, const QList<State> &states) {
    if (!d->resource)
        return 0;
    QByteArray array;
    for (State s : states) {
        const auto v = static_cast<std::uint32_t>(s);
        array.append(reinterpret_cast<const char *>(&v), 4);
    }
    d->resource->send(kToplevelConfigure, {Arg::Int(size.width()), Arg::Int(size.height()), Arg::Array(array)});
    d->lastSentSize = size;
    d->lastSentStates = states;
    Configure configure;
    configure.serial = d->xdgSurface->shell()->d->compositor->nextSerial();
    configure.size = size;
    configure.states = states;
    return d->xdgSurface->d->sendConfigure(configure);
}

/*!
    \overload

    Asks the client to take \a size and \a states, given as State values.
*/
uint QWaylandXdgToplevel::sendConfigure(const QSize &size, const QList<int> &states) {
    QList<State> list;
    for (int s : states)
        list.append(static_cast<State>(s));
    return sendConfigure(size, list);
}

/*!
    Asks the client to close the window.
*/
void QWaylandXdgToplevel::sendClose() {
    if (d->resource)
        d->resource->send(kToplevelClose);
}

/*!
    Asks the client to be maximized at \a size. Returns the serial.
*/
uint QWaylandXdgToplevel::sendMaximized(const QSize &size) {
    QList<State> states = d->lastSentStates;
    states.removeAll(MaximizedState);
    states.removeAll(FullscreenState);
    states.append(MaximizedState);
    return sendConfigure(size, states);
}

/*!
    Asks the client to leave the maximized state at \a size, or at a size of its choice. Returns the serial.
*/
uint QWaylandXdgToplevel::sendUnmaximized(const QSize &size) {
    QList<State> states = d->lastSentStates;
    states.removeAll(MaximizedState);
    states.removeAll(FullscreenState);
    return sendConfigure(size, states);
}

/*!
    Asks the client to go full screen at \a size. Returns the serial.
*/
uint QWaylandXdgToplevel::sendFullscreen(const QSize &size) {
    QList<State> states = d->lastSentStates;
    states.removeAll(MaximizedState);
    states.removeAll(FullscreenState);
    states.append(FullscreenState);
    return sendConfigure(size, states);
}

/*!
    Tells the client it is being resized, to at most \a maxSize. Returns the serial.
*/
uint QWaylandXdgToplevel::sendResizing(const QSize &maxSize) {
    QList<State> states = d->lastSentStates;
    if (!states.contains(ResizingState))
        states.append(ResizingState);
    return sendConfigure(maxSize, states);
}

/*!
    Returns the role that toplevels give their wl_surface.
*/
QWaylandSurfaceRole *QWaylandXdgToplevel::role() { return &g_toplevelRole; }

void QWaylandXdgToplevel::Private::request(Resource *res, std::uint32_t opcode, const Args &args) {
    QWaylandCompositor *compositor = xdgSurface->shell()->d->compositor;
    const auto seatOf = [&](const Arg &a) -> QWaylandSeat * {
        return a.null ? nullptr : QWaylandSeat::fromSeatResource(res->connection()->resource(a.u));
    };
    switch (opcode) {
    case kToplevelDestroy:
        res->destroy();
        break;
    case kToplevelSetParent: {
        QWaylandXdgToplevel *newParent = nullptr;
        if (!args[0].null) {
            Resource *pr = res->connection()->resource(args[0].u);
            if (pr && pr->interface() == &kToplevelInterface && pr->handler())
                newParent = static_cast<Private *>(pr->handler())->q;
        }
        if (parent != newParent) {
            parent = newParent;
            emit q->parentToplevelChanged();
        }
        break;
    }
    case kToplevelSetTitle:
        if (title != QString::fromUtf8(args[0].bytes)) {
            title = QString::fromUtf8(args[0].bytes);
            emit q->titleChanged();
        }
        break;
    case kToplevelSetAppId:
        if (appId != QString::fromUtf8(args[0].bytes)) {
            appId = QString::fromUtf8(args[0].bytes);
            emit q->appIdChanged();
        }
        break;
    case kToplevelShowWindowMenu:
        emit q->showWindowMenu(seatOf(args[0]), QPoint(args[2].i, args[3].i));
        break;
    case kToplevelMove:
        emit q->startMove(seatOf(args[0]));
        break;
    case kToplevelResize:
        emit q->startResize(seatOf(args[0]), edgesFromXdg(args[2].u));
        break;
    case kToplevelSetMaxSize:
        if (maxSize != QSize(args[0].i, args[1].i)) {
            maxSize = QSize(args[0].i, args[1].i);
            emit q->maxSizeChanged();
        }
        break;
    case kToplevelSetMinSize:
        if (minSize != QSize(args[0].i, args[1].i)) {
            minSize = QSize(args[0].i, args[1].i);
            emit q->minSizeChanged();
        }
        break;
    case kToplevelSetMaximized:
        emit q->setMaximized();
        break;
    case kToplevelUnsetMaximized:
        emit q->unsetMaximized();
        break;
    case kToplevelSetFullscreen: {
        QWaylandOutput *output = nullptr;
        if (!args[0].null)
            output = QWaylandOutput::fromResource(res->connection()->resource(args[0].u));
        (void)compositor;
        emit q->setFullscreen(output);
        break;
    }
    case kToplevelUnsetFullscreen:
        emit q->unsetFullscreen();
        break;
    case kToplevelSetMinimized:
        emit q->setMinimized();
        break;
    default:
        break;
    }
}

void QWaylandXdgToplevel::Private::resourceDestroyed(Resource *) {
    resource = nullptr;
    if (xdgSurface && xdgSurface->d->toplevel == q)
        xdgSurface->d->toplevel = nullptr;
    q->deleteLater();
}

void QWaylandXdgToplevel::Private::applyConfigure(const Configure &configure) {
    if (states == configure.states)
        return;
    const QList<State> old = states;
    states = configure.states;
    emit q->statesChanged();
    if (old.contains(MaximizedState) != states.contains(MaximizedState))
        emit q->maximizedChanged();
    if (old.contains(FullscreenState) != states.contains(FullscreenState))
        emit q->fullscreenChanged();
    if (old.contains(ResizingState) != states.contains(ResizingState))
        emit q->resizingChanged();
    if (old.contains(ActivatedState) != states.contains(ActivatedState))
        emit q->activatedChanged();
}

// ---- QWaylandXdgPopup ----------------------------------------------------------------------------------------

/*!
    \class QWaylandXdgPopup
    \brief The QWaylandXdgPopup class is a popup window of xdg-shell: a menu or a tooltip next to its parent.

    The client's positioner says where the popup wants to be: anchorRect() in the parent, the anchorEdges() of
    that rectangle, the gravityEdges() the popup extends to, and the offset(). unconstrainedPosition() combines
    them; the compositor may move the popup to keep it on screen, then calls sendConfigure() with the geometry.

    \differences
    Grabs are accepted but not enforced; there is no \c sendPopupDone() on a lost grab unless the application
    calls it. Repositioning is not supported.
    \enddifferences
*/

/*!
    Constructs the popup role of \a xdgSurface under \a parentXdgSurface, placed by \a positioner, for the
    client's object \a resource. The shell does this.
*/
QWaylandXdgPopup::QWaylandXdgPopup(QWaylandXdgSurface *xdgSurface, QWaylandXdgSurface *parentXdgSurface,
                                   Resource *positioner, Resource *resource)
    : QObject(xdgSurface), d(std::make_unique<Private>(this)) {
    d->xdgSurface = xdgSurface;
    d->parent = parentXdgSurface;
    d->resource = resource;
    auto *handler = static_cast<PositionerHandler *>(positioner->handler());
    if (handler)
        d->positioner = handler->positioners[positioner];
    resource->setHandler(d.get());
}

/*!
    Destroys the popup.
*/
QWaylandXdgPopup::~QWaylandXdgPopup() {
    if (d->xdgSurface && d->xdgSurface->d->popup == this)
        d->xdgSurface->d->popup = nullptr;
    if (d->resource) {
        d->resource->setHandler(nullptr);
        d->resource = nullptr;
    }
}

/*!
    Returns the xdg surface the popup is the role of.
*/
QWaylandXdgSurface *QWaylandXdgPopup::xdgSurface() const { return d->xdgSurface; }

/*!
    Returns the xdg surface the popup belongs to, or \c nullptr.
*/
QWaylandXdgSurface *QWaylandXdgPopup::parentXdgSurface() const { return d->parent; }

/*!
    Returns the geometry the last acknowledged sendConfigure() gave, relative to the parent.
*/
QRect QWaylandXdgPopup::configuredGeometry() const { return d->configuredGeometry; }

/*!
    Returns the rectangle in the parent the popup is anchored to.
*/
QRect QWaylandXdgPopup::anchorRect() const { return d->positioner.anchorRect; }

/*!
    Returns the edges of anchorRect() the popup is anchored at; none means its center.
*/
Qt::Edges QWaylandXdgPopup::anchorEdges() const { return edgesFromAnchor(d->positioner.anchor); }

/*!
    Returns the edges the popup extends towards from the anchor; none means centered on it.
*/
Qt::Edges QWaylandXdgPopup::gravityEdges() const { return edgesFromAnchor(d->positioner.gravity); }

/*!
    Returns the orientations in which the popup may be slid to fit the screen.
*/
Qt::Orientations QWaylandXdgPopup::slideConstraints() const {
    Qt::Orientations o;
    if (d->positioner.constraintAdjustment & 1)
        o |= Qt::Horizontal;
    if (d->positioner.constraintAdjustment & 2)
        o |= Qt::Vertical;
    return o;
}

/*!
    Returns the orientations in which the popup may be flipped to fit the screen.
*/
Qt::Orientations QWaylandXdgPopup::flipConstraints() const {
    Qt::Orientations o;
    if (d->positioner.constraintAdjustment & 4)
        o |= Qt::Horizontal;
    if (d->positioner.constraintAdjustment & 8)
        o |= Qt::Vertical;
    return o;
}

/*!
    Returns the orientations in which the popup may be shrunk to fit the screen.
*/
Qt::Orientations QWaylandXdgPopup::resizeConstraints() const {
    Qt::Orientations o;
    if (d->positioner.constraintAdjustment & 16)
        o |= Qt::Horizontal;
    if (d->positioner.constraintAdjustment & 32)
        o |= Qt::Vertical;
    return o;
}

/*!
    Returns the offset from the anchor.
*/
QPoint QWaylandXdgPopup::offset() const { return d->positioner.offset; }

/*!
    Returns the size the client asked for.
*/
QSize QWaylandXdgPopup::positionerSize() const { return d->positioner.size; }

/*!
    Returns where the popup wants to be in the parent, from the anchor, gravity and offset, before any
    adjustment for the screen edges.
*/
QPoint QWaylandXdgPopup::unconstrainedPosition() const {
    const QRect anchor = d->positioner.anchorRect;
    const Qt::Edges a = anchorEdges();
    QPoint p = anchor.center();
    if (a & Qt::LeftEdge)
        p.setX(anchor.left());
    else if (a & Qt::RightEdge)
        p.setX(anchor.left() + anchor.width());
    if (a & Qt::TopEdge)
        p.setY(anchor.top());
    else if (a & Qt::BottomEdge)
        p.setY(anchor.top() + anchor.height());
    const Qt::Edges g = gravityEdges();
    const QSize size = d->positioner.size;
    if (g & Qt::LeftEdge)
        p.rx() -= size.width();
    else if (!(g & Qt::RightEdge))
        p.rx() -= size.width() / 2;
    if (g & Qt::TopEdge)
        p.ry() -= size.height();
    else if (!(g & Qt::BottomEdge))
        p.ry() -= size.height() / 2;
    return p + d->positioner.offset;
}

/*!
    Tells the client where the popup is: \a geometry relative to the parent. Returns the serial.
*/
uint QWaylandXdgPopup::sendConfigure(const QRect &geometry) {
    if (!d->resource)
        return 0;
    d->resource->send(kPopupConfigure, {Arg::Int(geometry.x()), Arg::Int(geometry.y()), Arg::Int(geometry.width()),
                                        Arg::Int(geometry.height())});
    Configure configure;
    configure.serial = d->xdgSurface->shell()->d->compositor->nextSerial();
    configure.popupGeometry = geometry;
    return d->xdgSurface->d->sendConfigure(configure);
}

/*!
    Tells the client the popup was dismissed.
*/
void QWaylandXdgPopup::sendPopupDone() {
    if (d->resource)
        d->resource->send(kPopupDone);
}

/*!
    Returns the role that popups give their wl_surface.
*/
QWaylandSurfaceRole *QWaylandXdgPopup::role() { return &g_popupRole; }

void QWaylandXdgPopup::Private::request(Resource *res, std::uint32_t opcode, const Args &) {
    if (opcode == kPopupDestroy)
        res->destroy();
    // grab and reposition are accepted and ignored
}

void QWaylandXdgPopup::Private::resourceDestroyed(Resource *) {
    resource = nullptr;
    if (xdgSurface && xdgSurface->d->popup == q)
        xdgSurface->d->popup = nullptr;
    q->deleteLater();
}

#include "moc_qwaylandxdgshell.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
