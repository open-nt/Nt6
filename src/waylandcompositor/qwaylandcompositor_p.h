// MIT License - see LICENSE
//
// The private state of the compositor classes, shared between the sources of the module.
#pragma once

#include "qwaylandprotocol_p.h"

#include <QtCore/qlist.h>
#include <QtCore/qpointer.h>
#include <QtGui/qimage.h>
#include <QtGui/qregion.h>
#include <QtWaylandCompositor/qwaylandcompositor.h>
#include <QtWaylandCompositor/qwaylandoutput.h>
#include <QtWaylandCompositor/qwaylandseat.h>
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <QtWaylandCompositor/qwaylandview.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

namespace QtWaylandPrivate {

// A wl_shm_pool: the client's memory, mapped for reading. Buffers keep it alive after the client destroyed the
// pool object.
class ShmPool : public ResourceHandler, public std::enable_shared_from_this<ShmPool> {
public:
    ShmPool(int fd, std::int32_t size);
    ~ShmPool() override;
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override;
    void resourceDestroyed(Resource *resource) override;
    const unsigned char *data() const { return m_data; }
    std::int32_t size() const { return m_size; }
    bool isValid() const { return m_data != nullptr; }
    std::shared_ptr<ShmPool> self; // keeps the pool while the client owns the object

private:
    bool map(std::int32_t size);
    int m_fd;
    unsigned char *m_data = nullptr;
    std::int32_t m_size = 0;
};

// A wl_buffer in shared memory.
class Buffer : public ResourceHandler {
public:
    Buffer(const std::shared_ptr<ShmPool> &pool, Resource *resource, std::int32_t offset, std::int32_t width,
           std::int32_t height, std::int32_t stride, std::uint32_t format);
    ~Buffer() override;
    void request(Resource *resource, std::uint32_t opcode, const Args &args) override;
    void resourceDestroyed(Resource *resource) override;

    QSize size() const { return QSize(m_width, m_height); }
    bool isDestroyed() const { return m_resource == nullptr; }
    Resource *resource() const { return m_resource; }
    // A QImage over the client's pixels. Valid while the pool is mapped: as long as this buffer lives.
    QImage image() const;
    // Tells the client that the compositor no longer reads the buffer.
    void release();
    // The locks of QWaylandBufferRef, and whether a release waits for them.
    int locks = 0;
    bool releasePending = false;
    std::shared_ptr<Buffer> self; // the handle shared with QWaylandBufferRef, reset when the client destroys it

private:
    std::shared_ptr<ShmPool> m_pool;
    Resource *m_resource;
    std::int32_t m_offset;
    std::int32_t m_width;
    std::int32_t m_height;
    std::int32_t m_stride;
    std::uint32_t m_format;
};

// What a wl_surface accumulates between commits.
struct SurfaceState {
    bool bufferAttached = false;
    std::shared_ptr<Buffer> buffer;
    QPoint offset;
    QRegion damage;       // in surface coordinates
    QRegion bufferDamage; // in buffer coordinates
    QRegion opaqueRegion;
    bool inputRegionSet = false;
    QRegion inputRegion;
    int scale = 1;
    int transform = 0;
    std::vector<Resource *> frameCallbacks;
};

// The state behind an xdg_positioner object.
struct PositionerData {
    QSize size;
    QRect anchorRect;
    std::uint32_t anchor = 0;
    std::uint32_t gravity = 0;
    std::uint32_t constraintAdjustment = 0;
    QPoint offset;
    bool reactive = false;
    QSize parentSize;
    std::uint32_t parentConfigure = 0;
};

} // namespace QtWaylandPrivate

struct QWaylandCompositor::Private {
    QtWaylandPrivate::Display *display = nullptr;
    QByteArray socketName;
    bool created = false;
    bool retainedSelection = false;
    bool useHardwareIntegration = false;
    QList<QWaylandClient *> clients;
    QList<QWaylandOutput *> outputs;
    QWaylandOutput *defaultOutput = nullptr;
    QList<QWaylandSeat *> seats;
    QList<QWaylandSurface *> surfaces;
    std::uint32_t compositorGlobal = 0;
    std::uint32_t shmGlobal = 0;
    // The wl_compositor and wl_shm objects of the clients.
    class CompositorHandler;
    class ShmHandler;
    std::unique_ptr<CompositorHandler> compositorHandler;
    std::unique_ptr<ShmHandler> shmHandler;
};

struct QWaylandCompositorPrivateAccess {
    static QWaylandCompositor::Private *d(QWaylandCompositor *c) { return c->d.get(); }
    static QWaylandClient *clientFor(QWaylandCompositor *c, QtWaylandPrivate::Connection *connection);
    static void addSurface(QWaylandCompositor *c, QWaylandSurface *surface);
    static void removeSurface(QWaylandCompositor *c, QWaylandSurface *surface);
    static void addOutput(QWaylandCompositor *c, QWaylandOutput *output);
    static void removeOutput(QWaylandCompositor *c, QWaylandOutput *output);
    static void addSeat(QWaylandCompositor *c, QWaylandSeat *seat);
    static void removeSeat(QWaylandCompositor *c, QWaylandSeat *seat);
    static QtWaylandPrivate::Display *display(QWaylandCompositor *c);
    static QRegion regionOf(QWaylandCompositor *c, QtWaylandPrivate::Resource *region);
};

struct QWaylandSurface::Private : public QtWaylandPrivate::ResourceHandler {
    explicit Private(QWaylandSurface *surface) : q(surface) {}
    void request(QtWaylandPrivate::Resource *resource, std::uint32_t opcode,
                 const QtWaylandPrivate::Args &args) override;
    void resourceDestroyed(QtWaylandPrivate::Resource *resource) override;
    void commit();

    QWaylandSurface *q;
    QWaylandCompositor *compositor = nullptr;
    QWaylandClient *client = nullptr;
    QtWaylandPrivate::Resource *resource = nullptr;
    bool initialized = false;
    bool destroyed = false;
    QWaylandSurfaceRole *role = nullptr;
    QtWaylandPrivate::SurfaceState pending;
    QtWaylandPrivate::SurfaceState current;
    std::vector<QtWaylandPrivate::Resource *> frameQueue; // committed, waiting for sendFrameCallbacks()
    bool hasContent = false;
    QSize bufferSize;
    QSize destinationSize;
    QRegion currentDamage;
    bool cursorSurface = false;
    QList<QWaylandView *> views;
    QWaylandView *primaryView = nullptr;
    std::set<QWaylandOutput *> outputs;
};

struct QWaylandSurfacePrivateAccess {
    static QWaylandSurface::Private *d(QWaylandSurface *s) { return s->d.get(); }
    static QWaylandSurface *fromResource(QtWaylandPrivate::Resource *resource);
};

struct QWaylandOutput::Private {
    QWaylandCompositor *compositor = nullptr;
    QWindow *window = nullptr;
    QString manufacturer;
    QString model;
    QPoint position;
    QRect availableGeometry;
    QSize physicalSize;
    Subpixel subpixel = SubpixelUnknown;
    Transform transform = TransformNormal;
    int scaleFactor = 1;
    bool sizeFollowsWindow = false;
    bool physicalSizeFollowsSize = false;
    QList<QWaylandOutputMode> modes;
    int currentMode = -1;
    int preferredMode = -1;
    bool initialized = false;
    std::uint32_t global = 0;
    class Handler;
    std::unique_ptr<Handler> handler;
    std::vector<QtWaylandPrivate::Resource *> resources;
    void sendGeometry(QtWaylandPrivate::Resource *r) const;
    void sendModes(QtWaylandPrivate::Resource *r) const;
    void sendDone(QtWaylandPrivate::Resource *r) const;
    void sendAll() const;
};

struct QWaylandOutputPrivateAccess {
    static QWaylandOutput::Private *d(QWaylandOutput *o) { return o->d.get(); }
    static std::vector<QtWaylandPrivate::Resource *> resourcesFor(QWaylandOutput *o, QWaylandClient *client);
};

struct QWaylandSeat::Private {
    QWaylandCompositor *compositor = nullptr;
    CapabilityFlags capabilities;
    bool initialized = false;
    std::uint32_t global = 0;
    class Handler;
    std::unique_ptr<Handler> handler;
    QWaylandPointer *pointer = nullptr;
    QWaylandKeyboard *keyboard = nullptr;
    QWaylandTouch *touch = nullptr;
    QWaylandKeymap *keymap = nullptr;
    QPointer<QWaylandView> mouseFocus;
    QPointer<QWaylandSurface> keyboardFocus;
};

struct QWaylandSeatPrivateAccess {
    static QWaylandSeat::Private *d(QWaylandSeat *s) { return s->d.get(); }
};
