// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandsurface.h>

#include "qwaylandcompositor_p.h"

#include <QtCore/qdebug.h>
#include <QtWaylandCompositor/qwaylandclient.h>
#include <QtWaylandCompositor/qwaylandoutput.h>
#include <QtWaylandCompositor/qwaylandview.h>

#include <algorithm>

#include <sys/mman.h>
#include <unistd.h>

using namespace QtWaylandPrivate;

// ---- ShmPool -------------------------------------------------------------------------------------------------

ShmPool::ShmPool(int fd, std::int32_t size) : m_fd(fd) { map(size); }

ShmPool::~ShmPool() {
    if (m_data)
        ::munmap(m_data, static_cast<std::size_t>(m_size));
    if (m_fd >= 0)
        ::close(m_fd);
}

bool ShmPool::map(std::int32_t size) {
    void *data = ::mmap(nullptr, static_cast<std::size_t>(size), PROT_READ, MAP_SHARED, m_fd, 0);
    if (data == MAP_FAILED)
        return false;
    if (m_data)
        ::munmap(m_data, static_cast<std::size_t>(m_size));
    m_data = static_cast<unsigned char *>(data);
    m_size = size;
    return true;
}

void ShmPool::request(Resource *resource, std::uint32_t opcode, const Args &args) {
    if (opcode == kShmPoolCreateBuffer) {
        Resource *bufferResource = resource->connection()->resource(args[0].u);
        const std::int32_t offset = args[1].i;
        const std::int32_t width = args[2].i;
        const std::int32_t height = args[3].i;
        const std::int32_t stride = args[4].i;
        const std::uint32_t format = args[5].u;
        if (format != kShmFormatArgb8888 && format != kShmFormatXrgb8888) {
            resource->postError(kShmErrorInvalidFormat, QString::fromLatin1("unsupported format %1").arg(format));
            return;
        }
        if (width <= 0 || height <= 0 || stride < width * 4 || offset < 0 ||
            static_cast<std::int64_t>(offset) + static_cast<std::int64_t>(stride) * height > m_size) {
            resource->postError(kShmErrorInvalidStride, QString::fromLatin1("invalid buffer geometry"));
            return;
        }
        auto buffer =
            std::make_shared<Buffer>(shared_from_this(), bufferResource, offset, width, height, stride, format);
        buffer->self = buffer;
        bufferResource->setHandler(buffer.get());
    } else if (opcode == kShmPoolDestroy) {
        resource->destroy();
    } else if (opcode == kShmPoolResize) {
        const std::int32_t size = args[0].i;
        if (size < m_size || !map(size))
            resource->postError(kShmErrorInvalidFd, QString::fromLatin1("cannot resize the pool"));
    }
}

void ShmPool::resourceDestroyed(Resource *) {
    self.reset(); // the buffers keep the mapping alive while they need it
}

// ---- Buffer --------------------------------------------------------------------------------------------------

Buffer::Buffer(const std::shared_ptr<ShmPool> &pool, Resource *resource, std::int32_t offset, std::int32_t width,
               std::int32_t height, std::int32_t stride, std::uint32_t format)
    : m_pool(pool), m_resource(resource), m_offset(offset), m_width(width), m_height(height), m_stride(stride),
      m_format(format) {}

Buffer::~Buffer() = default;

void Buffer::request(Resource *resource, std::uint32_t opcode, const Args &) {
    if (opcode == kBufferDestroy)
        resource->destroy();
}

void Buffer::resourceDestroyed(Resource *) {
    m_resource = nullptr;
    self.reset(); // surfaces and references that still hold the buffer keep it, with its pixels
}

QImage Buffer::image() const {
    if (!m_pool || !m_pool->isValid())
        return QImage();
    const QImage::Format format =
        m_format == kShmFormatArgb8888 ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32;
    return QImage(m_pool->data() + m_offset, m_width, m_height, m_stride, format);
}

void Buffer::release() {
    if (locks > 0) {
        releasePending = true;
        return;
    }
    releasePending = false;
    if (m_resource)
        m_resource->send(kBufferRelease);
}

// ---- QWaylandBufferRef ---------------------------------------------------------------------------------------

/*!
    \class QWaylandBufferRef
    \brief The QWaylandBufferRef class is a reference to a buffer a client attached to a surface.

    A reference keeps the buffer's pixels available: image() is a QImage over the client's shared memory, valid
    while the reference, or a lock(), holds the buffer. The client is told that the compositor is done with the
    buffer (\c wl_buffer.release) once the surface has moved on and no lock remains.

    \differences
    Buffers are always shared memory: isSharedMemory() is \c true, there is no \c toOpenGLTexture(),
    \c hasProtectedContent() is \c false, and wl_buffer() returns an opaque handle. image() returns a QImage over
    the client's memory without copying; copy it to keep it.
    \enddifferences

    \sa QWaylandSurface::currentBuffer(), QWaylandView
*/

/*!
    \enum QWaylandBufferRef::Origin

    Where the first row of the buffer is.

    \value OriginTopLeft The first row is the top of the image.
    \value OriginBottomLeft The first row is the bottom of the image.
*/

/*!
    Constructs a null reference.
*/
QWaylandBufferRef::QWaylandBufferRef() = default;

/*!
    Constructs a copy of \a ref.
*/
QWaylandBufferRef::QWaylandBufferRef(const QWaylandBufferRef &ref) = default;

QWaylandBufferRef::QWaylandBufferRef(const std::shared_ptr<Buffer> &buffer) : m_buffer(buffer) {}

/*!
    Destroys the reference.
*/
QWaylandBufferRef::~QWaylandBufferRef() = default;

/*!
    Assigns \a ref to this reference.
*/
QWaylandBufferRef &QWaylandBufferRef::operator=(const QWaylandBufferRef &ref) = default;

/*!
    Returns \c true if \a ref refers to the same buffer.
*/
bool QWaylandBufferRef::operator==(const QWaylandBufferRef &ref) const { return m_buffer == ref.m_buffer; }

/*!
    Returns \c true if the reference holds no buffer.
*/
bool QWaylandBufferRef::isNull() const { return !m_buffer; }

/*!
    Returns \c true if the reference holds a buffer.
*/
bool QWaylandBufferRef::hasBuffer() const { return m_buffer != nullptr; }

/*!
    Returns \c true if the buffer has pixels: it exists and the client has not destroyed its pool.
*/
bool QWaylandBufferRef::hasContent() const { return m_buffer && !m_buffer->image().isNull(); }

/*!
    Returns \c false: shared-memory buffers are never protected.
*/
bool QWaylandBufferRef::hasProtectedContent() const { return false; }

/*!
    Returns \c true if the client has destroyed the buffer object; the pixels stay available to this reference.
*/
bool QWaylandBufferRef::isDestroyed() const { return m_buffer && m_buffer->isDestroyed(); }

/*!
    Returns an opaque handle of the buffer object, or \c nullptr.
*/
wl_resource *QWaylandBufferRef::wl_buffer() const {
    return m_buffer ? reinterpret_cast<wl_resource *>(m_buffer->resource()) : nullptr;
}

/*!
    Returns the size of the buffer in pixels.
*/
QSize QWaylandBufferRef::size() const { return m_buffer ? m_buffer->size() : QSize(); }

/*!
    Returns the origin of the buffer: always OriginTopLeft for shared memory.
*/
QWaylandBufferRef::Origin QWaylandBufferRef::origin() const { return OriginTopLeft; }

/*!
    Returns \c true: Nt buffers are shared memory.
*/
bool QWaylandBufferRef::isSharedMemory() const { return m_buffer != nullptr; }

/*!
    Returns the pixels as a QImage that reads the client's memory, or a null image. The format is
    QImage::Format_ARGB32_Premultiplied or QImage::Format_RGB32.
*/
QImage QWaylandBufferRef::image() const { return m_buffer ? m_buffer->image() : QImage(); }

/*!
    Keeps the client from reusing the buffer: the release is held back until unlock().
*/
void QWaylandBufferRef::lock() {
    if (m_buffer)
        ++m_buffer->locks;
}

/*!
    Undoes a lock(); the release the surface asked for meanwhile is sent when the last lock goes.
*/
void QWaylandBufferRef::unlock() {
    if (!m_buffer || m_buffer->locks == 0)
        return;
    if (--m_buffer->locks == 0 && m_buffer->releasePending)
        m_buffer->release();
}

/*!
    Returns \c true if the buffer has locks.
*/
bool QWaylandBufferRef::isLocked() const { return m_buffer && m_buffer->locks > 0; }

// ---- QWaylandSurface -----------------------------------------------------------------------------------------

/*!
    \class QWaylandSurface
    \brief The QWaylandSurface class is a rectangle of pixels a client draws into.

    A client attaches buffers to a surface and commits them; each commit makes the attached buffer the
    currentBuffer(), updates bufferSize() and destinationSize(), emits damaged() with what changed and redraw().
    A surface has no place on screen by itself: a shell extension gives it a role (a QWaylandXdgToplevel, for
    instance) and the application decides where to show it.

    Frame callbacks the client asked for are answered by sendFrameCallbacks(), which the application calls once it
    has shown the committed content, or QWaylandOutput::sendFrameCallbacks() for every surface on an output.

    \differences
    Buffers are shared memory. There are no subsurfaces yet, so parentSurface() is \c nullptr and
    subsurfaces() is empty. There is no drag and drop (dragStarted() is never emitted), no idle inhibition and no
    \c QWaylandSurfaceGrabber. resource() and fromResource() use an opaque handle.
    \enddifferences

    \sa QWaylandCompositor::surfaceCreated(), QWaylandBufferRef, QWaylandView
*/

/*!
    \enum QWaylandSurface::Origin

    Where the first row of the current buffer is.

    \value OriginTopLeft The first row is the top.
    \value OriginBottomLeft The first row is the bottom.
*/

/*!
    \class QWaylandSurfaceRole
    \brief The QWaylandSurfaceRole class names the role a surface was given.

    A surface can have one role for its lifetime; QWaylandSurface::setRole() refuses a second one with a protocol
    error, as the protocol demands.
*/

/*!
    Constructs a surface that is not initialized; call initialize().
*/
QWaylandSurface::QWaylandSurface() : QWaylandObject(nullptr), d(std::make_unique<Private>(this)) {}

/*!
    Constructs the surface that \a client of \a compositor asked for with the object \a id and protocol
    \a version, and initializes it.
*/
QWaylandSurface::QWaylandSurface(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version)
    : QWaylandObject(compositor), d(std::make_unique<Private>(this)) {
    initialize(compositor, client, id, version);
}

QWaylandSurface::QWaylandSurface(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version,
                                 bool deferInit)
    : QWaylandObject(compositor), d(std::make_unique<Private>(this)) {
    if (!deferInit)
        initialize(compositor, client, id, version);
}

/*!
    Destroys the surface object.
*/
QWaylandSurface::~QWaylandSurface() {
    for (QWaylandView *view : std::as_const(d->views))
        view->setSurface(nullptr);
    if (d->compositor)
        QWaylandCompositorPrivateAccess::removeSurface(d->compositor, this);
    if (d->resource) {
        d->resource->setHandler(nullptr);
        d->resource = nullptr;
    }
    if (d->current.buffer)
        d->current.buffer->release();
}

/*!
    Binds the surface to the object \a id that \a client of \a compositor created, with protocol \a version.
*/
void QWaylandSurface::initialize(QWaylandCompositor *compositor, QWaylandClient *client, uint id, int version) {
    if (d->initialized)
        return;
    (void)version;
    Connection *connection = reinterpret_cast<Connection *>(client->client());
    Resource *resource = connection ? connection->resource(id) : nullptr;
    if (!resource || resource->interface() != &kSurfaceInterface) {
        qWarning("QWaylandSurface: object %u of the client is not a wl_surface", id);
        return;
    }
    d->compositor = compositor;
    d->client = client;
    d->resource = resource;
    resource->setHandler(d.get());
    d->initialized = true;
    QWaylandCompositorPrivateAccess::addSurface(compositor, this);
}

/*!
    Returns \c true once initialize() has bound the surface to its object.
*/
bool QWaylandSurface::isInitialized() const { return d->initialized; }

/*!
    Returns the client that owns the surface.
*/
QWaylandClient *QWaylandSurface::client() const { return d->client; }

/*!
    Returns the handle of the client's connection.
*/
wl_client *QWaylandSurface::waylandClient() const { return d->client ? d->client->client() : nullptr; }

/*!
    Gives the surface \a role. Returns \c false, after sending the protocol error \a errorCode on
    \a errorResource, if it already has another role.
*/
bool QWaylandSurface::setRole(QWaylandSurfaceRole *role, wl_resource *errorResource, uint errorCode) {
    if (d->role && d->role != role) {
        if (auto *resource = reinterpret_cast<Resource *>(errorResource))
            resource->postError(
                errorCode,
                QString::fromLatin1("the surface already has the role %1").arg(QString::fromUtf8(d->role->name())));
        return false;
    }
    d->role = role;
    return true;
}

/*!
    Returns the role of the surface, or \c nullptr while it has none.
*/
QWaylandSurfaceRole *QWaylandSurface::role() const { return d->role; }

/*!
    Returns \c true if the client has committed a buffer and not since attached a null one.
*/
bool QWaylandSurface::hasContent() const { return d->hasContent; }

/*!
    Returns \c true if the client has destroyed the surface.
*/
bool QWaylandSurface::isDestroyed() const { return d->destroyed; }

/*!
    Returns the part of the buffer that is shown, in buffer coordinates: the whole buffer.
*/
QRectF QWaylandSurface::sourceGeometry() const { return QRectF(QPointF(), QSizeF(d->bufferSize)); }

/*!
    Returns the size of the surface in surface coordinates: the buffer size divided by bufferScale().
*/
QSize QWaylandSurface::destinationSize() const { return d->destinationSize; }

/*!
    Returns the size of the current buffer in pixels.
*/
QSize QWaylandSurface::bufferSize() const { return d->bufferSize; }

/*!
    Returns the scale the client set with \c wl_surface.set_buffer_scale, 1 by default.
*/
int QWaylandSurface::bufferScale() const { return d->current.scale; }

/*!
    Returns the orientation of the content, from the buffer transform the client set.
*/
Qt::ScreenOrientation QWaylandSurface::contentOrientation() const {
    switch (d->current.transform) {
    case 1:
    case 5:
        return Qt::PortraitOrientation;
    case 2:
    case 6:
        return Qt::InvertedLandscapeOrientation;
    case 3:
    case 7:
        return Qt::InvertedPortraitOrientation;
    default:
        return Qt::PrimaryOrientation;
    }
}

/*!
    Returns the origin of the current buffer: always OriginTopLeft.
*/
QWaylandSurface::Origin QWaylandSurface::origin() const { return OriginTopLeft; }

/*!
    Returns the compositor.
*/
QWaylandCompositor *QWaylandSurface::compositor() const { return d->compositor; }

/*!
    Returns \c true if \a p, in surface coordinates, is inside the input region the client set, or inside the
    surface if it set none.
*/
bool QWaylandSurface::inputRegionContains(const QPoint &p) const { return inputRegion().contains(p); }

/*!
    \overload

    Returns \c true if \a position is inside the input region.
*/
bool QWaylandSurface::inputRegionContains(const QPointF &position) const {
    return inputRegionContains(position.toPoint());
}

/*!
    Returns the input region: what the client set, cut to the surface, or the whole surface.
*/
QRegion QWaylandSurface::inputRegion() const {
    const QRect whole(QPoint(0, 0), d->destinationSize);
    if (!d->current.inputRegionSet)
        return QRegion(whole);
    return d->current.inputRegion.intersected(whole);
}

/*!
    Returns the region the client marked as opaque.
*/
QRegion QWaylandSurface::opaqueRegion() const { return d->current.opaqueRegion; }

/*!
    Returns \c true if the opaque region covers the whole surface.
*/
bool QWaylandSurface::isOpaque() const {
    const QRect whole(QPoint(0, 0), d->destinationSize);
    return !whole.isEmpty() && d->current.opaqueRegion.intersected(whole) == QRegion(whole);
}

/*!
    Marks the surface destroyed and detaches it from the client's object. The client's later requests on the
    object are ignored.
*/
void QWaylandSurface::destroy() {
    if (d->destroyed)
        return;
    d->destroyed = true;
    if (d->compositor)
        QWaylandCompositorPrivateAccess::removeSurface(d->compositor, this);
    if (d->resource) {
        d->resource->setHandler(nullptr);
        d->resource = nullptr;
    }
    emit surfaceDestroyed();
}

/*!
    Marks the start of a frame: the frame callbacks committed so far are the ones sendFrameCallbacks() answers.
*/
void QWaylandSurface::frameStarted() {}

/*!
    Answers the frame callbacks the client committed, telling it that it may draw the next frame.
*/
void QWaylandSurface::sendFrameCallbacks() {
    const std::uint32_t time = Display::timestamp();
    std::vector<Resource *> callbacks;
    callbacks.swap(d->frameQueue);
    for (Resource *callback : callbacks) {
        callback->send(0, {Arg::Uint(time)});
        callback->destroy();
    }
}

/*!
    Returns the primary view: the one input is directed at and whose output gets the frame callbacks.
*/
QWaylandView *QWaylandSurface::primaryView() const { return d->primaryView; }

/*!
    Makes \a view the primary view.
*/
void QWaylandSurface::setPrimaryView(QWaylandView *view) { d->primaryView = view; }

/*!
    Returns the views that show the surface.
*/
QList<QWaylandView *> QWaylandSurface::views() const { return d->views; }

/*!
    Returns the surface behind the opaque handle \a resource, or \c nullptr.
*/
QWaylandSurface *QWaylandSurface::fromResource(wl_resource *resource) {
    return QWaylandSurfacePrivateAccess::fromResource(reinterpret_cast<Resource *>(resource));
}

/*!
    Returns an opaque handle of the client's surface object.
*/
wl_resource *QWaylandSurface::resource() const { return reinterpret_cast<wl_resource *>(d->resource); }

/*!
    Marks the surface as one a client uses as a mouse cursor, or not, with \a cursorSurface.
*/
void QWaylandSurface::markAsCursorSurface(bool cursorSurface) {
    if (d->cursorSurface == cursorSurface)
        return;
    d->cursorSurface = cursorSurface;
    emit cursorSurfaceChanged();
}

/*!
    Returns \c true if a client set the surface as a cursor.
*/
bool QWaylandSurface::isCursorSurface() const { return d->cursorSurface; }

/*!
    Returns \c false: Nt has no idle-inhibit protocol.
*/
bool QWaylandSurface::inhibitsIdle() const { return false; }

/*!
    Returns the buffer of the last commit.
*/
QWaylandBufferRef QWaylandSurface::currentBuffer() const { return QWaylandBufferRef(d->current.buffer); }

/*!
    Returns the damage of the last commit, in surface coordinates.
*/
QRegion QWaylandSurface::currentDamage() const { return d->currentDamage; }

/*!
    Returns the offset the client gave the last attached buffer.
*/
QPoint QWaylandSurface::offset() const { return d->current.offset; }

/*!
    Returns the parent of a subsurface: always \c nullptr, as there are no subsurfaces yet.
*/
QWaylandSurface *QWaylandSurface::parentSurface() const { return nullptr; }

/*!
    Returns the subsurfaces: always empty.
*/
QList<QWaylandSurface *> QWaylandSurface::subsurfaces() const { return {}; }

/*!
    Returns the position of a subsurface in its parent: always the origin.
*/
QPoint QWaylandSurface::subsurfacePosition() const { return QPoint(); }

/*!
    Tells the client that the surface is shown on \a output (\c wl_surface.enter).
*/
void QWaylandSurface::sendEnter(QWaylandOutput *output) {
    if (!d->resource || !output)
        return;
    d->outputs.insert(output);
    for (Resource *r : QWaylandOutputPrivateAccess::resourcesFor(output, d->client))
        d->resource->send(kSurfaceEnter, {Arg::Object(r)});
}

/*!
    Tells the client that the surface left \a output.
*/
void QWaylandSurface::sendLeave(QWaylandOutput *output) {
    if (!d->resource || !output)
        return;
    d->outputs.erase(output);
    for (Resource *r : QWaylandOutputPrivateAccess::resourcesFor(output, d->client))
        d->resource->send(kSurfaceLeave, {Arg::Object(r)});
}

// ---- the protocol side ---------------------------------------------------------------------------------------

void QWaylandSurface::Private::request(Resource *res, std::uint32_t opcode, const Args &args) {
    switch (opcode) {
    case kSurfaceDestroy:
        res->destroy();
        break;
    case kSurfaceAttach: {
        pending.bufferAttached = true;
        pending.buffer.reset();
        if (!args[0].null) {
            Resource *bufferResource = res->connection()->resource(args[0].u);
            if (bufferResource && bufferResource->interface() == &kBufferInterface)
                pending.buffer = static_cast<Buffer *>(bufferResource->handler())->self;
        }
        pending.offset = QPoint(args[1].i, args[2].i);
        break;
    }
    case kSurfaceDamage:
        pending.damage += QRect(args[0].i, args[1].i, args[2].i, args[3].i);
        break;
    case kSurfaceDamageBuffer:
        pending.bufferDamage += QRect(args[0].i, args[1].i, args[2].i, args[3].i);
        break;
    case kSurfaceFrame:
        if (Resource *callback = res->connection()->resource(args[0].u))
            pending.frameCallbacks.push_back(callback);
        break;
    case kSurfaceSetOpaqueRegion:
        pending.opaqueRegion =
            args[0].null
                ? QRegion()
                : QWaylandCompositorPrivateAccess::regionOf(compositor, res->connection()->resource(args[0].u));
        break;
    case kSurfaceSetInputRegion:
        pending.inputRegionSet = !args[0].null;
        pending.inputRegion =
            args[0].null
                ? QRegion()
                : QWaylandCompositorPrivateAccess::regionOf(compositor, res->connection()->resource(args[0].u));
        break;
    case kSurfaceCommit:
        commit();
        break;
    case kSurfaceSetBufferTransform:
        pending.transform = args[0].i;
        break;
    case kSurfaceSetBufferScale:
        if (args[0].i <= 0) {
            res->postError(2, QString::fromLatin1("buffer scale must be positive"));
            return;
        }
        pending.scale = args[0].i;
        break;
    case kSurfaceOffset:
        pending.offset = QPoint(args[0].i, args[1].i);
        break;
    default:
        break;
    }
}

void QWaylandSurface::Private::commit() {
    const bool hadContent = hasContent;
    const QSize oldBufferSize = bufferSize;
    const int oldScale = current.scale;
    const int oldTransform = current.transform;
    const QRegion oldOpaque = current.opaqueRegion;

    std::shared_ptr<Buffer> previous = current.buffer;
    if (pending.bufferAttached) {
        current.buffer = pending.buffer;
        current.offset = pending.offset;
        if (previous && previous != current.buffer)
            previous->release();
    }
    current.scale = pending.scale;
    current.transform = pending.transform;
    current.opaqueRegion = pending.opaqueRegion;
    current.inputRegionSet = pending.inputRegionSet;
    current.inputRegion = pending.inputRegion;
    for (Resource *callback : pending.frameCallbacks)
        frameQueue.push_back(callback);

    hasContent = current.buffer != nullptr;
    bufferSize = current.buffer ? current.buffer->size() : QSize();
    destinationSize = bufferSize.isValid() ? bufferSize / std::max(1, current.scale) : QSize();

    QRegion damage = pending.damage;
    QRegion bufferDamage = pending.bufferDamage;
    if (!bufferDamage.isEmpty()) {
        for (const QRect &r : bufferDamage.rects()) {
            const int s = std::max(1, current.scale);
            damage += QRect(r.x() / s, r.y() / s, (r.width() + s - 1) / s, (r.height() + s - 1) / s);
        }
    }
    if (pending.bufferAttached && current.buffer && (bufferSize != oldBufferSize || !hadContent))
        damage = QRegion(QRect(QPoint(0, 0), destinationSize));
    damage = damage.intersected(QRect(QPoint(0, 0), destinationSize));
    currentDamage = damage;

    const bool attached = pending.bufferAttached;
    pending = SurfaceState();
    pending.scale = current.scale;
    pending.transform = current.transform;
    pending.opaqueRegion = current.opaqueRegion;
    pending.inputRegionSet = current.inputRegionSet;
    pending.inputRegion = current.inputRegion;

    if (attached && current.offset != QPoint())
        emit q->offsetForNextFrame(current.offset);
    if (bufferSize != oldBufferSize) {
        emit q->bufferSizeChanged();
        emit q->destinationSizeChanged();
        emit q->sourceGeometryChanged();
    }
    if (current.scale != oldScale)
        emit q->bufferScaleChanged();
    if (current.transform != oldTransform)
        emit q->contentOrientationChanged();
    if (current.opaqueRegion != oldOpaque)
        emit q->isOpaqueChanged();
    if (hasContent != hadContent)
        emit q->hasContentChanged();
    const QWaylandBufferRef ref(current.buffer);
    for (QWaylandView *view : std::as_const(views))
        view->bufferCommitted(ref, damage);
    if (!damage.isEmpty())
        emit q->damaged(damage);
    emit q->redraw();
}

void QWaylandSurface::Private::resourceDestroyed(Resource *) {
    resource = nullptr;
    if (destroyed)
        return;
    destroyed = true;
    for (Resource *callback : frameQueue)
        callback->destroy();
    frameQueue.clear();
    for (Resource *callback : pending.frameCallbacks)
        callback->destroy();
    pending.frameCallbacks.clear();
    for (QWaylandView *view : std::as_const(views))
        emit view->surfaceDestroyed();
    emit q->surfaceDestroyed();
    if (compositor)
        QWaylandCompositorPrivateAccess::removeSurface(compositor, q);
    q->deleteLater();
}

QWaylandSurface *QWaylandSurfacePrivateAccess::fromResource(Resource *resource) {
    if (!resource || resource->interface() != &kSurfaceInterface || !resource->handler())
        return nullptr;
    return static_cast<QWaylandSurface::Private *>(resource->handler())->q;
}

// ---- QWaylandView --------------------------------------------------------------------------------------------

struct QWaylandView::Private {
    QObject *renderObject = nullptr;
    QPointer<QWaylandSurface> surface;
    QPointer<QWaylandOutput> output;
    QWaylandBufferRef currentBuffer;
    QWaylandBufferRef nextBuffer;
    QRegion currentDamage;
    QRegion nextDamage;
    bool nextBufferCommitted = false;
    bool bufferLocked = false;
    bool allowDiscardFrontBuffer = false;
};

/*!
    \class QWaylandView
    \brief The QWaylandView class shows a surface on an output.

    A view is the application's handle on the content of one surface: setSurface() attaches it, and each commit
    of the surface arrives through bufferCommitted(). advance() takes the latest committed buffer as
    currentBuffer(), which the application then draws, with currentBufferDamage() saying what changed.

    \differences
    There is no QML item; the application draws currentBuffer().image() itself, for instance with QPainter.
    \enddifferences

    \sa QWaylandSurface, QWaylandBufferRef
*/

/*!
    Constructs a view whose renderObject() is \a renderObject, with the given \a parent.
*/
QWaylandView::QWaylandView(QObject *renderObject, QObject *parent) : QObject(parent), d(std::make_unique<Private>()) {
    d->renderObject = renderObject;
}

/*!
    Destroys the view and detaches it from its surface.
*/
QWaylandView::~QWaylandView() {
    if (d->surface)
        QWaylandSurfacePrivateAccess::d(d->surface)->views.removeAll(this);
}

/*!
    Returns the object that renders the view, as given to the constructor.
*/
QObject *QWaylandView::renderObject() const { return d->renderObject; }

/*!
    Returns the surface the view shows.
*/
QWaylandSurface *QWaylandView::surface() const { return d->surface; }

/*!
    Makes the view show \a surface, or nothing if it is \c nullptr.
*/
void QWaylandView::setSurface(QWaylandSurface *surface) {
    if (d->surface == surface)
        return;
    if (d->surface) {
        QWaylandSurface::Private *sd = QWaylandSurfacePrivateAccess::d(d->surface);
        sd->views.removeAll(this);
        if (sd->primaryView == this)
            sd->primaryView = sd->views.isEmpty() ? nullptr : sd->views.first();
    }
    d->surface = surface;
    d->currentBuffer = QWaylandBufferRef();
    d->nextBuffer = QWaylandBufferRef();
    d->nextBufferCommitted = false;
    if (surface) {
        QWaylandSurface::Private *sd = QWaylandSurfacePrivateAccess::d(surface);
        sd->views.append(this);
        if (!sd->primaryView)
            sd->primaryView = this;
        if (sd->current.buffer) {
            d->nextBuffer = QWaylandBufferRef(sd->current.buffer);
            d->nextDamage = QRegion(QRect(QPoint(0, 0), sd->destinationSize));
            d->nextBufferCommitted = true;
        }
    }
    emit surfaceChanged();
}

/*!
    Returns the output the view is on.
*/
QWaylandOutput *QWaylandView::output() const { return d->output; }

/*!
    Puts the view on \a output.
*/
void QWaylandView::setOutput(QWaylandOutput *output) {
    if (d->output == output)
        return;
    d->output = output;
    emit outputChanged();
}

/*!
    Called when the surface commits \a ref with \a damage: remembers them for the next advance().
*/
void QWaylandView::bufferCommitted(const QWaylandBufferRef &ref, const QRegion &damage) {
    d->nextBuffer = ref;
    d->nextDamage = damage;
    d->nextBufferCommitted = true;
}

/*!
    Makes the last committed buffer the current one. Returns \c true if there was a new buffer; \c false if
    nothing was committed since the last call or the buffer is locked.
*/
bool QWaylandView::advance() {
    if (!d->nextBufferCommitted || d->bufferLocked)
        return false;
    d->currentBuffer = d->nextBuffer;
    d->currentDamage = d->nextDamage;
    d->nextBufferCommitted = false;
    return true;
}

/*!
    Drops the current buffer.
*/
void QWaylandView::discardCurrentBuffer() {
    d->currentBuffer = QWaylandBufferRef();
    d->currentDamage = QRegion();
}

/*!
    Returns the buffer that advance() last took.
*/
QWaylandBufferRef QWaylandView::currentBuffer() { return d->currentBuffer; }

/*!
    Returns the damage that came with the current buffer.
*/
QRegion QWaylandView::currentBufferDamage() { return d->currentDamage; }

/*!
    Returns \c true if advance() is held back.
*/
bool QWaylandView::isBufferLocked() const { return d->bufferLocked; }

/*!
    Sets whether advance() is held back to \a locked, to keep showing the current buffer.
*/
void QWaylandView::setBufferLocked(bool locked) {
    if (d->bufferLocked == locked)
        return;
    d->bufferLocked = locked;
    emit bufferLockedChanged();
}

/*!
    Returns whether the current buffer may be dropped when the surface loses its content.
*/
bool QWaylandView::allowDiscardFrontBuffer() const { return d->allowDiscardFrontBuffer; }

/*!
    Sets whether the current buffer may be dropped when the surface loses its content to \a discard.
*/
void QWaylandView::setAllowDiscardFrontBuffer(bool discard) {
    if (d->allowDiscardFrontBuffer == discard)
        return;
    d->allowDiscardFrontBuffer = discard;
    emit allowDiscardFrontBufferChanged();
}

/*!
    Makes this the primary view of its surface.
*/
void QWaylandView::setPrimary() {
    if (d->surface)
        d->surface->setPrimaryView(this);
}

/*!
    Returns \c true if this is the primary view of its surface.
*/
bool QWaylandView::isPrimary() const { return d->surface && d->surface->primaryView() == this; }

#include "moc_qwaylandsurface.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
#include "moc_qwaylandview.cpp"    // NOLINT(bugprone-suspicious-include): generated by ntmoc
