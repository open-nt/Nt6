// MIT License - see LICENSE
//
// The Wayland wire protocol, server side, without libwayland: a listening Unix socket, one connection per client,
// messages of typed arguments with file descriptors in ancillary data, the object tables of each client, and the
// core objects every client starts from (wl_display, wl_registry, wl_callback). The interfaces above it describe
// their requests and events as signature strings, as wayland-scanner would read them from the protocol XML.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

class QSocketNotifier;

namespace QtWaylandPrivate {

class Connection;
class Display;
class Resource;

// One argument of a message. Which member is set follows the signature character of the message: i (int),
// u (uint), f (fixed, a double here), s (string), o (object id), n (new id), a (array), h (file descriptor).
// A '?' before o or s in a signature allows a null object or string.
struct Arg {
    std::int32_t i = 0;
    std::uint32_t u = 0;
    double f = 0.0;
    QByteArray bytes; // string without its terminator, or the array
    int fd = -1;
    bool null = false; // a null object or string

    static Arg Int(std::int32_t v) {
        Arg a;
        a.i = v;
        return a;
    }
    static Arg Uint(std::uint32_t v) {
        Arg a;
        a.u = v;
        return a;
    }
    static Arg Fixed(double v) {
        Arg a;
        a.f = v;
        return a;
    }
    static Arg String(const QByteArray &v) {
        Arg a;
        a.bytes = v;
        return a;
    }
    static Arg String(const QString &v) { return String(v.toUtf8()); }
    static Arg Object(const Resource *r); // null if r is null
    static Arg Object(std::uint32_t id) {
        Arg a;
        a.u = id;
        a.null = id == 0;
        return a;
    }
    static Arg Array(const QByteArray &v) {
        Arg a;
        a.bytes = v;
        return a;
    }
    static Arg Fd(int v) {
        Arg a;
        a.fd = v;
        return a;
    }
};

using Args = std::vector<Arg>;

// A request or event: its name, and the signature of its arguments. A new_id in a request names the interface of
// the object it creates, or none for wl_registry.bind, whose client sends the interface name and version first.
struct Message {
    const char *name;
    const char *signature;
    const char *newInterface = nullptr;
    // The version of the interface from which the message exists.
    std::uint32_t since = 1;
};

struct Interface {
    const char *name;
    std::uint32_t version;
    std::vector<Message> requests;
    std::vector<Message> events;
};

// What receives the requests of a resource. The implementation of an interface derives from it.
class ResourceHandler {
public:
    virtual ~ResourceHandler() = default;
    // A request arrived. `args` follow the signature of the request; a new_id argument has already been turned
    // into a Resource of the named interface, found with resource->connection()->resource(args[k].u), unless the
    // request is wl_registry.bind.
    virtual void request(Resource *resource, std::uint32_t opcode, const Args &args) = 0;
    // The resource is gone: the client destroyed it, or the client went away. The pointer is dead after this.
    virtual void resourceDestroyed(Resource *resource) = 0;
};

// One protocol object of one client.
class Resource {
public:
    Resource(Connection *connection, const Interface *interface, std::uint32_t id, std::uint32_t version);
    Resource(const Resource &) = delete;
    Resource &operator=(const Resource &) = delete;

    Connection *connection() const { return m_connection; }
    const Interface *interface() const { return m_interface; }
    std::uint32_t id() const { return m_id; }
    std::uint32_t version() const { return m_version; }
    ResourceHandler *handler() const { return m_handler; }
    void setHandler(ResourceHandler *handler) { m_handler = handler; }

    // Sends the event `opcode` with `args`, which follow the signature of the event. Nothing is sent when the
    // client's version of the interface is older than the event.
    void send(std::uint32_t opcode, const Args &args = Args());
    // Sends wl_display.error about this object and disconnects the client once the error is flushed.
    void postError(std::uint32_t code, const QString &message);
    // Removes the object: tells the handler, frees the id (wl_display.delete_id for a client-made id), deletes
    // this.
    void destroy();

private:
    Connection *m_connection;
    const Interface *m_interface;
    std::uint32_t m_id;
    std::uint32_t m_version;
    ResourceHandler *m_handler = nullptr;
};

// A global object a client can bind through the registry.
struct Global {
    std::uint32_t name;
    const Interface *interface;
    std::uint32_t version;
    // Called when a client binds the global: makes the implementation of the new resource.
    std::function<void(Resource *resource)> bind;
};

// One client connection.
class Connection : public QObject {
public:
    Connection(Display *display, int fd);
    ~Connection() override;

    Display *display() const { return m_display; }
    int fd() const { return m_fd; }
    Resource *resource(std::uint32_t id) const;
    // Makes the resource `id` of `interface`, which a request's new_id argument named (ids below 0xff000000)
    // or which the server creates (ids from there).
    Resource *createResource(const Interface *interface, std::uint32_t id, std::uint32_t version);
    Resource *createServerResource(const Interface *interface, std::uint32_t version);
    // Every resource of the interface, for sending an event to all of them.
    std::vector<Resource *> resourcesOf(const Interface *interface) const;
    // Credentials of the peer, from SO_PEERCRED.
    qint64 processId() const { return m_pid; }
    qint64 userId() const { return m_uid; }
    qint64 groupId() const { return m_gid; }
    // A new global exists: announce it to this client's registries.
    void announceGlobal(const Global &global);
    void withdrawGlobal(std::uint32_t name);
    // Writes what is queued. Returns false if the client is gone.
    bool flush();
    // Closes the connection: destroys every resource and deletes this object later.
    void close();
    bool isClosed() const { return m_fd < 0; }
    void *userData = nullptr; // the QWaylandClient

    // Called by Resource.
    void queueEvent(Resource *resource, std::uint32_t opcode, const Args &args);
    void postError(std::uint32_t objectId, std::uint32_t code, const QString &message);
    void freeId(std::uint32_t id);

private:
    void readable();
    void writable();
    void dispatch();
    bool parseMessage(const char *data, std::size_t size, std::uint32_t *objectId, std::uint32_t *opcode, Args *args,
                      const Message **message, Resource **target);
    void handleDisplayRequest(std::uint32_t opcode, const Args &args);
    void handleRegistryRequest(Resource *registry, const Args &args);

    Display *m_display;
    int m_fd;
    qint64 m_pid = 0;
    qint64 m_uid = 0;
    qint64 m_gid = 0;
    QSocketNotifier *m_readNotifier = nullptr;
    QSocketNotifier *m_writeNotifier = nullptr;
    std::vector<char> m_in;
    std::vector<int> m_inFds;
    std::vector<char> m_out;
    std::vector<int> m_outFds; // in the order their messages were queued
    std::map<std::uint32_t, std::unique_ptr<Resource>> m_resources;
    std::uint32_t m_nextServerId = 0xff000000u;
    bool m_errorPosted = false;
    bool m_closing = false;
    bool m_dispatching = false;
};

// The display: the socket that clients connect to, the globals, and the clients.
class Display : public QObject {
public:
    explicit Display(QObject *parent = nullptr);
    ~Display() override;

    // Listens on `socketName` in XDG_RUNTIME_DIR (or on the next free "wayland-N" if it is empty, which
    // socketName() then returns). Returns false, after a warning, if that fails.
    bool listen(const QByteArray &socketName);
    QByteArray socketName() const { return m_socketName; }
    bool isListening() const { return m_listenFd >= 0; }
    void close();

    // Registers a global; `bind` makes the implementation when a client binds it. Returns its name.
    std::uint32_t addGlobal(const Interface *interface, std::uint32_t version, std::function<void(Resource *)> bind);
    void removeGlobal(std::uint32_t name);
    const std::vector<Global> &globals() const { return m_globals; }
    // Interfaces that requests create objects of (wl_surface, xdg_toplevel and so on) but that are not globals;
    // registered by the implementation of the interface that creates them.
    void registerInterface(const Interface *interface);
    const Interface *interfaceNamed(const char *name) const;

    const std::vector<Connection *> &connections() const { return m_connections; }
    // Called for each client that connects, before it has sent anything.
    std::function<void(Connection *)> onConnected;
    std::function<void(Connection *)> onDisconnected;

    // The next serial number for events that carry one.
    std::uint32_t nextSerial() { return ++m_serial; }
    // Milliseconds for the time argument of input events.
    static std::uint32_t timestamp();
    // Flushes every client; called after handling events from the loop.
    void flushClients();

    // Called by Connection.
    void connectionClosed(Connection *connection);

private:
    void accept();

    int m_listenFd = -1;
    int m_lockFd = -1;
    QByteArray m_socketName;
    QByteArray m_socketPath;
    QByteArray m_lockPath;
    QSocketNotifier *m_notifier = nullptr;
    std::vector<Global> m_globals;
    std::vector<const Interface *> m_interfaces;
    std::uint32_t m_nextGlobalName = 1;
    std::vector<Connection *> m_connections;
    std::uint32_t m_serial = 0;
};

// The core interfaces, as the protocol defines them.
extern const Interface kDisplayInterface;
extern const Interface kRegistryInterface;
extern const Interface kCallbackInterface;

// Converts between the protocol's 24.8 fixed point and double.
inline std::int32_t toFixed(double v) { return static_cast<std::int32_t>(v * 256.0); }
inline double fromFixed(std::int32_t v) { return static_cast<double>(v) / 256.0; }

} // namespace QtWaylandPrivate
