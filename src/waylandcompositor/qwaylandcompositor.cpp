// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandcompositor.h>

#include "qwaylandcompositor_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandoutput.h>
#include <QtWaylandCompositor/qwaylandseat.h>
#include <QtWaylandCompositor/qwaylandsurface.h>

#include <unistd.h>

using namespace QtWaylandPrivate;

// The wl_compositor objects of the clients: they create surfaces and regions.
class QWaylandCompositor::Private::CompositorHandler : public ResourceHandler {
public:
    explicit CompositorHandler(QWaylandCompositor *compositor) : q(compositor) {}
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        QWaylandClient *client = QWaylandCompositorPrivateAccess::clientFor(q, resource->connection());
        if (opcode == kCompositorCreateSurface) {
            Resource *surfaceResource = resource->connection()->resource(args[0].u);
            const uint id = args[0].u;
            const int version = static_cast<int>(resource->version());
            emit q->surfaceRequested(client, id, version);
            if (!surfaceResource->handler()) {
                // The surface parents itself to the compositor once initialized; one that failed to is dropped.
                std::unique_ptr<QWaylandSurface> made(q->createSurface(client, id, version));
                if (made && made->isInitialized()) {
                    [[maybe_unused]] QWaylandSurface *ownedByCompositor = made.release();
                }
            }
            if (QWaylandSurface *surface = QWaylandSurfacePrivateAccess::fromResource(surfaceResource))
                emit q->surfaceCreated(surface);
        } else if (opcode == kCompositorCreateRegion) {
            Resource *region = resource->connection()->resource(args[0].u);
            region->setHandler(&regionHandler);
        }
    }
    void resourceDestroyed(Resource *) override {}

    // wl_region objects accumulate a QRegion, read by the surface when the region is set on it.
    class RegionHandler : public ResourceHandler {
    public:
        // `region` without `rect`: each rectangle of the region is cut into the parts outside it.
        static QRegion subtracted(const QRegion &region, const QRect &rect) {
            QRegion out;
            for (const QRect &r : region.rects()) {
                const QRect cut = r.intersected(rect);
                if (cut.isEmpty()) {
                    out += r;
                    continue;
                }
                if (cut.top() > r.top())
                    out += QRect(r.left(), r.top(), r.width(), cut.top() - r.top());
                if (cut.bottom() < r.bottom())
                    out += QRect(r.left(), cut.bottom() + 1, r.width(), r.bottom() - cut.bottom());
                if (cut.left() > r.left())
                    out += QRect(r.left(), cut.top(), cut.left() - r.left(), cut.height());
                if (cut.right() < r.right())
                    out += QRect(cut.right() + 1, cut.top(), r.right() - cut.right(), cut.height());
            }
            return out;
        }
        std::map<Resource *, QRegion> regions;
        void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
            if (opcode == kRegionDestroy) {
                resource->destroy();
            } else if (opcode == kRegionAdd || opcode == kRegionSubtract) {
                const QRect rect(args[0].i, args[1].i, args[2].i, args[3].i);
                QRegion &region = regions[resource];
                region = opcode == kRegionAdd ? region.united(rect) : subtracted(region, rect);
            }
        }
        void resourceDestroyed(Resource *resource) override { regions.erase(resource); }
    } regionHandler;

    QWaylandCompositor *q;
};

// The wl_shm objects: they create pools of shared memory.
class QWaylandCompositor::Private::ShmHandler : public ResourceHandler {
public:
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override {
        if (opcode == kShmCreatePool) {
            Resource *poolResource = resource->connection()->resource(args[0].u);
            const int fd = args[1].fd;
            const std::int32_t size = args[2].i;
            if (size <= 0) {
                ::close(fd);
                resource->postError(kShmErrorInvalidStride, QString::fromLatin1("invalid pool size"));
                return;
            }
            auto pool = std::make_shared<ShmPool>(fd, size);
            if (!pool->isValid()) {
                resource->postError(kShmErrorInvalidFd, QString::fromLatin1("cannot map the pool"));
                return;
            }
            pool->self = pool;
            poolResource->setHandler(pool.get());
        } else if (opcode == kShmRelease) {
            resource->destroy();
        }
    }
    void resourceDestroyed(Resource *) override {}
};

// The QRegion that the wl_region object `region` holds.
QRegion QWaylandCompositorPrivateAccess::regionOf(QWaylandCompositor *compositor, Resource *region) {
    auto *handler = compositor->d->compositorHandler.get();
    const auto it = handler->regionHandler.regions.find(region);
    return it == handler->regionHandler.regions.end() ? QRegion() : it->second;
}

/*!
    \module NtWaylandCompositor
    \title Nt Wayland Compositor C++ Classes
    \brief Classes that make an application a Wayland display server.
    \cmaketarget Nt::WaylandCompositor

    Nt Wayland Compositor hosts Wayland clients: QWaylandCompositor accepts them, QWaylandSurface carries their
    pixels, QWaylandXdgShell gives their surfaces window roles and QWaylandSeat sends them input. See
    \l{Nt Wayland Compositor}.
*/

/*!
    \class QWaylandCompositor
    \brief The QWaylandCompositor class is the Wayland display server that clients connect to.

    A compositor listens on a socket in \c XDG_RUNTIME_DIR, gives each client its surfaces, and tells the
    application about them through surfaceCreated(). What is shown, where, and what gets input is up to the
    application: it reads the surfaces' buffers (QWaylandSurface::currentBuffer()) and sends input through a
    QWaylandSeat. Shell protocols, which give surfaces window roles, are extensions such as QWaylandXdgShell:

    \code
    QWaylandCompositor compositor;
    QWaylandOutput output(&compositor, nullptr);
    output.addMode(QWaylandOutputMode(QSize(1280, 720), 60000), true);
    QWaylandXdgShell shell(&compositor);
    QObject::connect(&shell, &QWaylandXdgShell::toplevelCreated, [](QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *) {
        toplevel->sendFullscreen(QSize(1280, 720));
    });
    compositor.create();
    \endcode

    The compositor runs in the thread of the application object and reads the clients from its event loop.

    \differences
    The protocol is spoken by Nt itself: there is no libwayland, so display() returns \c nullptr,
    addSocketDescriptor() is not available, and there is no hardware integration (no \c wl_drm, no
    \c linux-dmabuf) or QML API. Buffers are shared memory (\c wl_shm) only. There is no clipboard, no drag and
    drop (retainedSelectionEnabled() and overrideSelection() are remembered but do nothing) and no
    grabSurface(). The subcompositor protocol is not offered yet.
    \enddifferences

    \sa QWaylandSurface, QWaylandSeat, QWaylandOutput, QWaylandXdgShell
*/

/*!
    Constructs a compositor with the given \a parent. Call create() once the outputs, seats and extensions are
    set up.
*/
QWaylandCompositor::QWaylandCompositor(QObject *parent) : QWaylandObject(parent), d(std::make_unique<Private>()) {
    d->display = new Display(this);
    d->compositorHandler = std::make_unique<Private::CompositorHandler>(this);
    d->shmHandler = std::make_unique<Private::ShmHandler>();
    d->display->onConnected = [this](Connection *connection) {
        auto *client = new QWaylandClient(this, connection);
        d->clients.append(client);
    };
    d->display->onDisconnected = [this](Connection *connection) {
        auto *client = static_cast<QWaylandClient *>(connection->userData);
        if (!client)
            return;
        d->clients.removeAll(client);
        connection->userData = nullptr;
        client->m_connection = nullptr;
        client->deleteLater();
    };
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Destroys the compositor: the clients are disconnected and the socket is removed.
*/
QWaylandCompositor::~QWaylandCompositor() {
    // Everything that withdraws a global goes while the display is still there: the extensions, seats and
    // outputs that are children of the compositor would otherwise be deleted by ~QObject(), after this.
    for (QWaylandSurface *surface : std::vector<QWaylandSurface *>(d->surfaces.begin(), d->surfaces.end()))
        surface->destroy();
    for (QWaylandCompositorExtension *extension : extensions()) {
        if (extension->parent() == this)
            delete extension;
        else
            extension->setExtensionContainer(nullptr);
    }
    for (QWaylandSeat *seat : QList<QWaylandSeat *>(d->seats)) {
        if (seat->parent() == this)
            delete seat;
        else
            QWaylandSeatPrivateAccess::d(seat)->compositor = nullptr;
    }
    for (QWaylandOutput *output : QList<QWaylandOutput *>(d->outputs)) {
        if (output->parent() == this)
            delete output;
        else
            QWaylandOutputPrivateAccess::d(output)->compositor = nullptr;
    }
    d->display->close();
}

/*!
    Starts the compositor: listens on the socket named socketName() (or on the first free \c wayland-N name),
    announces the outputs, seats and extensions, and emits createdChanged(). Called from the event loop after
    construction if the application does not call it.
*/
void QWaylandCompositor::create() {
    if (d->created)
        return;
    if (!d->display->listen(d->socketName)) {
        qWarning("QWaylandCompositor: create() failed");
        return;
    }
    if (d->socketName != d->display->socketName()) {
        d->socketName = d->display->socketName();
        emit socketNameChanged(d->socketName);
    }
    d->display->registerInterface(&kSurfaceInterface);
    d->display->registerInterface(&kRegionInterface);
    d->display->registerInterface(&kShmPoolInterface);
    d->display->registerInterface(&kBufferInterface);
    d->compositorGlobal = d->display->addGlobal(&kCompositorInterface, kCompositorInterface.version,
                                                [this](Resource *r) { r->setHandler(d->compositorHandler.get()); });
    d->shmGlobal = d->display->addGlobal(&kShmInterface, kShmInterface.version, [this](Resource *r) {
        r->setHandler(d->shmHandler.get());
        r->send(kShmFormatEvent, {Arg::Uint(kShmFormatArgb8888)});
        r->send(kShmFormatEvent, {Arg::Uint(kShmFormatXrgb8888)});
    });
    d->created = true;
    if (d->seats.isEmpty()) {
        QWaylandSeat *seat = createSeat();
        if (seat && !d->seats.contains(seat))
            d->seats.append(seat);
    }
    for (QWaylandSeat *seat : std::as_const(d->seats))
        seat->initialize();
    for (QWaylandOutput *output : std::as_const(d->outputs))
        output->initialize();
    for (QWaylandCompositorExtension *extension : extensions()) {
        if (!extension->isInitialized())
            extension->initialize();
    }
    emit createdChanged();
}

/*!
    Returns \c true once create() has run.
*/
bool QWaylandCompositor::isCreated() const { return d->created; }

/*!
    Sets the name of the socket, in \c XDG_RUNTIME_DIR, to \a name. Has to be called before create().
*/
void QWaylandCompositor::setSocketName(const QByteArray &name) {
    if (d->socketName == name)
        return;
    if (d->created) {
        qWarning("QWaylandCompositor: the socket name cannot change after create()");
        return;
    }
    d->socketName = name;
    emit socketNameChanged(name);
}

/*!
    Returns the name of the socket: what was set, or the \c wayland-N name chosen by create().
*/
QByteArray QWaylandCompositor::socketName() const { return d->socketName; }

/*!
    Listens on \a fd too. Not available in Nt: prints a warning.
*/
void QWaylandCompositor::addSocketDescriptor(int fd) {
    (void)fd;
    qWarning("QWaylandCompositor::addSocketDescriptor() is not available in Nt");
}

/*!
    Returns the libwayland display, which Nt does not have: always \c nullptr.
*/
wl_display *QWaylandCompositor::display() const { return nullptr; }

/*!
    Returns the next serial number, for events that carry one.
*/
std::uint32_t QWaylandCompositor::nextSerial() { return d->display->nextSerial(); }

/*!
    Returns the connected clients.
*/
QList<QWaylandClient *> QWaylandCompositor::clients() const { return d->clients; }

/*!
    Disconnects the client that owns \a surface.
*/
void QWaylandCompositor::destroyClientForSurface(QWaylandSurface *surface) {
    if (surface && surface->client())
        destroyClient(surface->client());
}

/*!
    Disconnects \a client.
*/
void QWaylandCompositor::destroyClient(QWaylandClient *client) {
    if (client)
        client->close();
}

/*!
    Returns every surface of every client.
*/
QList<QWaylandSurface *> QWaylandCompositor::surfaces() const { return d->surfaces; }

/*!
    Returns the surfaces of \a client.
*/
QList<QWaylandSurface *> QWaylandCompositor::surfacesForClient(QWaylandClient *client) const {
    QList<QWaylandSurface *> out;
    for (QWaylandSurface *s : std::as_const(d->surfaces)) {
        if (s->client() == client)
            out.append(s);
    }
    return out;
}

/*!
    Returns the output shown in \a window, or \c nullptr.
*/
QWaylandOutput *QWaylandCompositor::outputFor(QWindow *window) const {
    for (QWaylandOutput *o : std::as_const(d->outputs)) {
        if (o->window() == window)
            return o;
    }
    return nullptr;
}

/*!
    Returns the default output: the one set with setDefaultOutput(), or the first one added.
*/
QWaylandOutput *QWaylandCompositor::defaultOutput() const { return d->defaultOutput; }

/*!
    Makes \a output the default output.
*/
void QWaylandCompositor::setDefaultOutput(QWaylandOutput *output) {
    if (d->defaultOutput == output)
        return;
    d->defaultOutput = output;
    emit defaultOutputChanged();
}

/*!
    Returns the outputs, in the order they were added.
*/
QList<QWaylandOutput *> QWaylandCompositor::outputs() const { return d->outputs; }

/*!
    Sets whether the compositor keeps the clipboard selection when its owner goes away to \a enabled. Nt has no
    clipboard protocol, so only the value is kept.
*/
void QWaylandCompositor::setRetainedSelectionEnabled(bool enabled) {
    if (d->retainedSelection == enabled)
        return;
    d->retainedSelection = enabled;
    emit retainedSelectionChanged();
}

/*!
    Returns the value set with setRetainedSelectionEnabled().
*/
bool QWaylandCompositor::retainedSelectionEnabled() const { return d->retainedSelection; }

/*!
    Offers \a data as the clipboard selection. Nt has no clipboard protocol: does nothing.
*/
void QWaylandCompositor::overrideSelection(const QMimeData *data) { (void)data; }

/*!
    Returns the default seat: the first one.
*/
QWaylandSeat *QWaylandCompositor::defaultSeat() const { return d->seats.isEmpty() ? nullptr : d->seats.first(); }

/*!
    Returns the seat that \a inputEvent belongs to: the first seat that isOwner() of it, or the default seat.
*/
QWaylandSeat *QWaylandCompositor::seatFor(QInputEvent *inputEvent) {
    for (QWaylandSeat *seat : std::as_const(d->seats)) {
        if (seat->isOwner(inputEvent))
            return seat;
    }
    return defaultSeat();
}

/*!
    Sets whether clients may use the hardware integration extension to \a use. Nt has none; the value is kept.
*/
void QWaylandCompositor::setUseHardwareIntegrationExtension(bool use) {
    if (d->useHardwareIntegration == use)
        return;
    d->useHardwareIntegration = use;
    emit useHardwareIntegrationExtensionChanged();
}

/*!
    Returns the value set with setUseHardwareIntegrationExtension().
*/
bool QWaylandCompositor::useHardwareIntegrationExtension() const { return d->useHardwareIntegration; }

/*!
    Grabs the pointer of \a seat for \a surface. Not implemented in Nt: does nothing.
*/
void QWaylandCompositor::grabSurface(QWaylandSeat *seat, const QWaylandSurface *surface) {
    (void)seat;
    (void)surface;
}

/*!
    Reads and handles what the clients have sent, and flushes what is queued for them. The event loop does this
    by itself; call it to process the clients without returning to the loop.
*/
void QWaylandCompositor::processWaylandEvents() {
    QCoreApplication::processEvents();
    d->display->flushClients();
}

/*!
    Called with the \a mimeData of a selection that the compositor retained. Nt never calls it.
*/
void QWaylandCompositor::retainedSelectionReceived(QMimeData *mimeData) { (void)mimeData; }

/*!
    Creates the default seat when create() finds none. Override it to make a subclass of QWaylandSeat.
*/
QWaylandSeat *QWaylandCompositor::createSeat() { return new QWaylandSeat(this); }

/*!
    Creates the surface that \a client asked for with the object \a id and protocol \a version, when no slot of
    surfaceRequested() did. Override it to make a subclass of QWaylandSurface.
*/
QWaylandSurface *QWaylandCompositor::createSurface(QWaylandClient *client, uint id, int version) {
    return new QWaylandSurface(this, client, id, version);
}

/*!
    \reimp
*/
bool QWaylandCompositor::event(QEvent *event) {
    if (event->type() == QEvent::User) {
        if (!d->created)
            create();
        return true;
    }
    return QWaylandObject::event(event);
}

// ---- QWaylandCompositorPrivateAccess -------------------------------------------------------------------------

QWaylandClient *QWaylandCompositorPrivateAccess::clientFor(QWaylandCompositor *c, Connection *connection) {
    (void)c;
    return static_cast<QWaylandClient *>(connection->userData);
}

void QWaylandCompositorPrivateAccess::addSurface(QWaylandCompositor *c, QWaylandSurface *surface) {
    if (!c->d->surfaces.contains(surface))
        c->d->surfaces.append(surface);
}

void QWaylandCompositorPrivateAccess::removeSurface(QWaylandCompositor *c, QWaylandSurface *surface) {
    if (c->d->surfaces.contains(surface)) {
        emit c->surfaceAboutToBeDestroyed(surface);
        c->d->surfaces.removeAll(surface);
    }
}

void QWaylandCompositorPrivateAccess::addOutput(QWaylandCompositor *c, QWaylandOutput *output) {
    if (c->d->outputs.contains(output))
        return;
    c->d->outputs.append(output);
    if (!c->d->defaultOutput)
        c->setDefaultOutput(output);
    emit c->outputAdded(output);
}

void QWaylandCompositorPrivateAccess::removeOutput(QWaylandCompositor *c, QWaylandOutput *output) {
    if (!c->d->outputs.contains(output))
        return;
    c->d->outputs.removeAll(output);
    if (c->d->defaultOutput == output)
        c->setDefaultOutput(c->d->outputs.isEmpty() ? nullptr : c->d->outputs.first());
    emit c->outputRemoved(output);
}

void QWaylandCompositorPrivateAccess::addSeat(QWaylandCompositor *c, QWaylandSeat *seat) {
    if (c->d->seats.contains(seat))
        return;
    QWaylandSeat *old = c->defaultSeat();
    c->d->seats.append(seat);
    if (!old)
        emit c->defaultSeatChanged(seat, nullptr);
}

void QWaylandCompositorPrivateAccess::removeSeat(QWaylandCompositor *c, QWaylandSeat *seat) {
    const bool wasDefault = c->defaultSeat() == seat;
    c->d->seats.removeAll(seat);
    if (wasDefault)
        emit c->defaultSeatChanged(c->defaultSeat(), seat);
}

Display *QWaylandCompositorPrivateAccess::display(QWaylandCompositor *c) { return c->d->display; }

#include "moc_qwaylandcompositor.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
