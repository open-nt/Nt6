// MIT License - see LICENSE
#include "qwaylandwire_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtenvironmentvariables.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace QtWaylandPrivate {

namespace {

constexpr std::uint32_t kDisplayId = 1;
constexpr std::uint32_t kServerIdBase = 0xff000000u;
constexpr std::size_t kMaxMessage = 4096;
constexpr std::size_t kMaxFdsPerMessage = 28;

// wl_display errors
constexpr std::uint32_t kErrorInvalidObject = 0;
constexpr std::uint32_t kErrorInvalidMethod = 1;
constexpr std::uint32_t kErrorImplementation = 3;

std::uint32_t readU32(const char *p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void putU32(std::vector<char> &out, std::uint32_t v) {
    const std::size_t n = out.size();
    out.resize(n + 4);
    std::memcpy(out.data() + n, &v, 4);
}

std::size_t padded(std::size_t n) { return (n + 3) & ~std::size_t(3); }

int setCloExec(int fd) {
    if (fd >= 0)
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

} // namespace

const Interface kCallbackInterface = {"wl_callback", 1, {}, {{"done", "u"}}};

const Interface kRegistryInterface = {
    "wl_registry", 1, {{"bind", "usun"}}, {{"global", "usu"}, {"global_remove", "u"}}};

const Interface kDisplayInterface = {"wl_display",
                                     1,
                                     {{"sync", "n", "wl_callback"}, {"get_registry", "n", "wl_registry"}},
                                     {{"error", "ous"}, {"delete_id", "u"}}};

Arg Arg::Object(const Resource *r) {
    Arg a;
    a.u = r ? r->id() : 0;
    a.null = r == nullptr;
    return a;
}

// ---- Resource ------------------------------------------------------------------------------------------------

Resource::Resource(Connection *connection, const Interface *interface, std::uint32_t id, std::uint32_t version)
    : m_connection(connection), m_interface(interface), m_id(id), m_version(version) {}

void Resource::send(std::uint32_t opcode, const Args &args) {
    if (opcode >= m_interface->events.size())
        return;
    if (m_interface->events[opcode].since > m_version)
        return;
    m_connection->queueEvent(this, opcode, args);
}

void Resource::postError(std::uint32_t code, const QString &message) { m_connection->postError(m_id, code, message); }

void Resource::destroy() {
    Connection *connection = m_connection;
    const std::uint32_t id = m_id;
    if (m_handler) {
        ResourceHandler *handler = m_handler;
        m_handler = nullptr;
        handler->resourceDestroyed(this);
    }
    connection->freeId(id); // deletes this
}

// ---- Connection ----------------------------------------------------------------------------------------------

Connection::Connection(Display *display, int fd) : QObject(display), m_display(display), m_fd(fd) {
    ucred cred{};
    socklen_t len = sizeof(cred);
    if (::getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0) {
        m_pid = cred.pid;
        m_uid = cred.uid;
        m_gid = cred.gid;
    }
    m_readNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    QObject::connect(m_readNotifier, &QSocketNotifier::activated, this, [this] { readable(); });
    m_writeNotifier = new QSocketNotifier(fd, QSocketNotifier::Write, this);
    m_writeNotifier->setEnabled(false);
    QObject::connect(m_writeNotifier, &QSocketNotifier::activated, this, [this] { writable(); });
    // The display object exists from the start, with id 1.
    createResource(&kDisplayInterface, kDisplayId, 1);
}

Connection::~Connection() {
    if (m_fd >= 0)
        ::close(m_fd);
    for (int fd : m_inFds)
        ::close(fd);
    for (int fd : m_outFds)
        ::close(fd);
}

Resource *Connection::resource(std::uint32_t id) const {
    const auto it = m_resources.find(id);
    return it == m_resources.end() ? nullptr : it->second.get();
}

Resource *Connection::createResource(const Interface *interface, std::uint32_t id, std::uint32_t version) {
    auto resource = std::make_unique<Resource>(this, interface, id, version);
    Resource *raw = resource.get();
    m_resources[id] = std::move(resource);
    return raw;
}

Resource *Connection::createServerResource(const Interface *interface, std::uint32_t version) {
    while (m_resources.count(m_nextServerId) != 0)
        ++m_nextServerId;
    return createResource(interface, m_nextServerId++, version);
}

std::vector<Resource *> Connection::resourcesOf(const Interface *interface) const {
    std::vector<Resource *> out;
    for (const auto &entry : m_resources) {
        if (entry.second->interface() == interface)
            out.push_back(entry.second.get());
    }
    return out;
}

void Connection::announceGlobal(const Global &global) {
    for (Resource *registry : resourcesOf(&kRegistryInterface))
        registry->send(
            0, {Arg::Uint(global.name), Arg::String(QByteArray(global.interface->name)), Arg::Uint(global.version)});
}

void Connection::withdrawGlobal(std::uint32_t name) {
    for (Resource *registry : resourcesOf(&kRegistryInterface))
        registry->send(1, {Arg::Uint(name)});
}

void Connection::freeId(std::uint32_t id) {
    const auto it = m_resources.find(id);
    if (it == m_resources.end())
        return;
    const bool clientId = id < kServerIdBase;
    m_resources.erase(it);
    if (clientId && id != kDisplayId && !m_closing) {
        if (Resource *display = resource(kDisplayId))
            display->send(1, {Arg::Uint(id)});
    }
}

void Connection::queueEvent(Resource *resource, std::uint32_t opcode, const Args &args) {
    if (m_fd < 0 || m_errorPosted)
        return;
    const Message &message = resource->interface()->events[opcode];
    const std::size_t start = m_out.size();
    putU32(m_out, resource->id());
    putU32(m_out, 0); // size and opcode, filled in below
    std::size_t argIndex = 0;
    for (const char *s = message.signature; *s != '\0'; ++s) {
        if (*s == '?' || (*s >= '0' && *s <= '9'))
            continue;
        if (argIndex >= args.size()) {
            qWarning("QtWaylandPrivate: too few arguments for %s.%s", resource->interface()->name, message.name);
            m_out.resize(start);
            return;
        }
        const Arg &a = args[argIndex++];
        switch (*s) {
        case 'i':
            putU32(m_out, static_cast<std::uint32_t>(a.i));
            break;
        case 'u':
        case 'o':
        case 'n':
            putU32(m_out, a.null ? 0u : a.u);
            break;
        case 'f':
            putU32(m_out, static_cast<std::uint32_t>(toFixed(a.f)));
            break;
        case 's': {
            if (a.null) {
                putU32(m_out, 0);
                break;
            }
            const std::size_t len = static_cast<std::size_t>(a.bytes.size()) + 1;
            putU32(m_out, static_cast<std::uint32_t>(len));
            const std::size_t n = m_out.size();
            m_out.resize(n + padded(len), '\0');
            std::memcpy(m_out.data() + n, a.bytes.constData(), static_cast<std::size_t>(a.bytes.size()));
            break;
        }
        case 'a': {
            const std::size_t len = static_cast<std::size_t>(a.bytes.size());
            putU32(m_out, static_cast<std::uint32_t>(len));
            const std::size_t n = m_out.size();
            m_out.resize(n + padded(len), '\0');
            std::memcpy(m_out.data() + n, a.bytes.constData(), len);
            break;
        }
        case 'h':
            m_outFds.push_back(::dup(a.fd));
            break;
        default:
            break;
        }
    }
    const std::uint32_t size = static_cast<std::uint32_t>(m_out.size() - start);
    const std::uint32_t header = (size << 16) | (opcode & 0xffffu);
    std::memcpy(m_out.data() + start + 4, &header, 4);
    if (!m_dispatching)
        flush();
}

void Connection::postError(std::uint32_t objectId, std::uint32_t code, const QString &message) {
    if (m_errorPosted)
        return;
    qWarning("QWaylandCompositor: protocol error from client %lld on object %u: %s", static_cast<long long>(m_pid),
             objectId, qPrintable(message));
    if (Resource *display = resource(kDisplayId))
        display->send(0, {Arg::Object(objectId), Arg::Uint(code), Arg::String(message)});
    m_errorPosted = true;
    flush();
    close();
}

bool Connection::flush() {
    if (m_fd < 0)
        return false;
    while (!m_out.empty()) {
        msghdr msg{};
        iovec iov{m_out.data(), m_out.size()};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        std::vector<char> control;
        const std::size_t fdCount = std::min(m_outFds.size(), kMaxFdsPerMessage);
        if (fdCount > 0) {
            control.resize(CMSG_SPACE(sizeof(int) * fdCount));
            msg.msg_control = control.data();
            msg.msg_controllen = control.size();
            cmsghdr *cm = CMSG_FIRSTHDR(&msg);
            cm->cmsg_level = SOL_SOCKET;
            cm->cmsg_type = SCM_RIGHTS;
            cm->cmsg_len = CMSG_LEN(sizeof(int) * fdCount);
            std::memcpy(CMSG_DATA(cm), m_outFds.data(), sizeof(int) * fdCount);
        }
        const ssize_t n = ::sendmsg(m_fd, &msg, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                m_writeNotifier->setEnabled(true);
                return true;
            }
            close();
            return false;
        }
        for (std::size_t i = 0; i < fdCount; ++i)
            ::close(m_outFds[i]);
        m_outFds.erase(m_outFds.begin(), m_outFds.begin() + static_cast<std::ptrdiff_t>(fdCount));
        m_out.erase(m_out.begin(), m_out.begin() + n);
    }
    m_writeNotifier->setEnabled(false);
    if (m_errorPosted)
        close();
    return true;
}

void Connection::writable() { flush(); }

void Connection::close() {
    if (m_closing)
        return;
    m_closing = true;
    m_readNotifier->setEnabled(false);
    m_writeNotifier->setEnabled(false);
    // Destroy the objects in reverse creation order, roughly: higher ids first, the display last.
    while (!m_resources.empty()) {
        auto it = std::prev(m_resources.end());
        Resource *r = it->second.get();
        if (ResourceHandler *handler = r->handler()) {
            r->setHandler(nullptr);
            handler->resourceDestroyed(r);
        }
        m_resources.erase(it);
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_display->connectionClosed(this);
    deleteLater();
}

void Connection::readable() {
    if (m_fd < 0)
        return;
    char buffer[kMaxMessage];
    alignas(cmsghdr) char control[CMSG_SPACE(sizeof(int) * kMaxFdsPerMessage)];
    for (;;) {
        msghdr msg{};
        iovec iov{buffer, sizeof(buffer)};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);
        const ssize_t n = ::recvmsg(m_fd, &msg, MSG_DONTWAIT | MSG_CMSG_CLOEXEC);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            close();
            return;
        }
        if (n == 0) {
            close();
            return;
        }
        for (cmsghdr *cm = CMSG_FIRSTHDR(&msg); cm; cm = CMSG_NXTHDR(&msg, cm)) {
            if (cm->cmsg_level != SOL_SOCKET || cm->cmsg_type != SCM_RIGHTS)
                continue;
            const std::size_t count = (cm->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            const auto *fds = reinterpret_cast<const int *>(CMSG_DATA(cm));
            for (std::size_t i = 0; i < count; ++i)
                m_inFds.push_back(fds[i]);
        }
        m_in.insert(m_in.end(), buffer, buffer + n);
        if (static_cast<std::size_t>(n) < sizeof(buffer))
            break;
    }
    dispatch();
}

void Connection::dispatch() {
    m_dispatching = true;
    std::size_t offset = 0;
    while (m_fd >= 0 && !m_closing && m_in.size() - offset >= 8) {
        const std::uint32_t header = readU32(m_in.data() + offset + 4);
        const std::size_t size = header >> 16;
        if (size < 8) {
            postError(kDisplayId, kErrorInvalidMethod, QString::fromLatin1("malformed message"));
            break;
        }
        if (m_in.size() - offset < size)
            break;
        std::uint32_t objectId = 0;
        std::uint32_t opcode = 0;
        Args args;
        const Message *message = nullptr;
        Resource *target = nullptr;
        const bool ok = parseMessage(m_in.data() + offset, size, &objectId, &opcode, &args, &message, &target);
        offset += size;
        if (!ok)
            break; // an error was posted
        if (objectId == kDisplayId) {
            handleDisplayRequest(opcode, args);
        } else if (target->interface() == &kRegistryInterface) {
            handleRegistryRequest(target, args);
        } else if (ResourceHandler *handler = target->handler()) {
            handler->request(target, opcode, args);
        }
    }
    if (m_fd >= 0) {
        m_in.erase(m_in.begin(), m_in.begin() + static_cast<std::ptrdiff_t>(std::min(offset, m_in.size())));
    }
    m_dispatching = false;
    if (m_fd >= 0)
        flush();
}

bool Connection::parseMessage(const char *data, std::size_t size, std::uint32_t *objectId, std::uint32_t *opcode,
                              Args *args, const Message **message, Resource **target) {
    *objectId = readU32(data);
    *opcode = readU32(data + 4) & 0xffffu;
    *target = resource(*objectId);
    if (!*target) {
        // A request on an object the server has already destroyed (the client had not yet seen delete_id) is
        // dropped, as the protocol asks; an id that never existed is an error.
        if (*objectId < kServerIdBase && *objectId != 0)
            return true;
        postError(kDisplayId, kErrorInvalidObject, QString::fromLatin1("invalid object %1").arg(*objectId));
        return false;
    }
    const Interface *interface = (*target)->interface();
    if (*opcode >= interface->requests.size()) {
        (*target)->postError(kErrorInvalidMethod, QString::fromLatin1("invalid opcode %1").arg(*opcode));
        return false;
    }
    *message = &interface->requests[*opcode];
    if ((*message)->since > (*target)->version()) {
        (*target)->postError(kErrorInvalidMethod, QString::fromLatin1("request %1 needs version %2")
                                                      .arg(QLatin1String((*message)->name))
                                                      .arg((*message)->since));
        return false;
    }
    std::size_t p = 8;
    bool nullable = false;
    const auto fail = [&](const char *what) {
        (*target)->postError(kErrorInvalidMethod, QString::fromLatin1("malformed %1.%2: %3")
                                                      .arg(QLatin1String(interface->name),
                                                           QLatin1String((*message)->name), QLatin1String(what)));
        return false;
    };
    for (const char *s = (*message)->signature; *s != '\0'; ++s) {
        if (*s == '?') {
            nullable = true;
            continue;
        }
        if (*s >= '0' && *s <= '9')
            continue;
        Arg a;
        switch (*s) {
        case 'i':
        case 'u':
        case 'o':
        case 'n':
        case 'f':
            if (size - p < 4)
                return fail("truncated");
            a.u = readU32(data + p);
            a.i = static_cast<std::int32_t>(a.u);
            a.f = fromFixed(a.i);
            p += 4;
            if (*s == 'o') {
                a.null = a.u == 0;
                if (a.null && !nullable)
                    return fail("null object");
                if (!a.null && !resource(a.u))
                    return fail("unknown object");
            }
            break;
        case 's':
        case 'a': {
            if (size - p < 4)
                return fail("truncated");
            const std::size_t len = readU32(data + p);
            p += 4;
            if (padded(len) > size - p)
                return fail("truncated");
            if (*s == 's') {
                if (len == 0) {
                    a.null = true;
                    if (!nullable)
                        return fail("null string");
                } else {
                    a.bytes = QByteArray(data + p, static_cast<qsizetype>(len - 1));
                }
            } else {
                a.bytes = QByteArray(data + p, static_cast<qsizetype>(len));
            }
            p += padded(len);
            break;
        }
        case 'h':
            if (m_inFds.empty())
                return fail("missing file descriptor");
            a.fd = m_inFds.front();
            m_inFds.erase(m_inFds.begin());
            break;
        default:
            return fail("bad signature");
        }
        nullable = false;
        args->push_back(a);
    }
    // Make the resource a new_id names, with the interface of the request, unless the request is a bind.
    if ((*message)->newInterface) {
        for (std::size_t k = 0, argIndex = 0; (*message)->signature[k] != '\0'; ++k) {
            const char c = (*message)->signature[k];
            if (c == '?' || (c >= '0' && c <= '9'))
                continue;
            if (c == 'n') {
                const std::uint32_t id = (*args)[argIndex].u;
                if (id == 0 || id >= kServerIdBase || resource(id))
                    return fail("bad new id");
                const Interface *newInterface = nullptr;
                for (const Interface *candidate : {&kCallbackInterface, &kRegistryInterface})
                    if (std::strcmp(candidate->name, (*message)->newInterface) == 0)
                        newInterface = candidate;
                if (!newInterface)
                    newInterface = m_display->interfaceNamed((*message)->newInterface);
                if (!newInterface)
                    return fail("unknown interface");
                createResource(newInterface, id, (*target)->version());
            }
            ++argIndex;
        }
    }
    return true;
}

void Connection::handleDisplayRequest(std::uint32_t opcode, const Args &args) {
    if (opcode == 0) { // sync
        Resource *callback = resource(args[0].u);
        if (!callback)
            return;
        callback->send(0, {Arg::Uint(m_display->nextSerial())});
        callback->destroy();
    } else if (opcode == 1) { // get_registry
        Resource *registry = resource(args[0].u);
        if (!registry)
            return;
        for (const Global &global : m_display->globals())
            registry->send(0, {Arg::Uint(global.name), Arg::String(QByteArray(global.interface->name)),
                               Arg::Uint(global.version)});
    }
}

void Connection::handleRegistryRequest(Resource *registry, const Args &args) {
    // bind: name, interface, version, new id
    const std::uint32_t name = args[0].u;
    const QByteArray interfaceName = args[1].bytes;
    const std::uint32_t version = args[2].u;
    const std::uint32_t id = args[3].u;
    const Global *global = nullptr;
    for (const Global &g : m_display->globals())
        if (g.name == name)
            global = &g;
    if (!global || interfaceName != global->interface->name) {
        registry->postError(
            0, QString::fromLatin1("invalid global %1 (%2)").arg(name).arg(QString::fromUtf8(interfaceName)));
        return;
    }
    if (version == 0 || version > global->version) {
        registry->postError(
            0, QString::fromLatin1("invalid version %1 for %2").arg(version).arg(QString::fromUtf8(interfaceName)));
        return;
    }
    if (id == 0 || id >= kServerIdBase || resource(id)) {
        registry->postError(kErrorInvalidObject, QString::fromLatin1("bad new id %1").arg(id));
        return;
    }
    Resource *bound = createResource(global->interface, id, version);
    global->bind(bound);
}

// ---- Display -------------------------------------------------------------------------------------------------

Display::Display(QObject *parent) : QObject(parent) {}

Display::~Display() { close(); }

bool Display::listen(const QByteArray &socketName) {
    close();
    const QByteArray runtimeDir = qgetenv("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        qWarning("QWaylandCompositor: XDG_RUNTIME_DIR is not set; cannot create the socket");
        return false;
    }
    std::vector<QByteArray> candidates;
    if (!socketName.isEmpty()) {
        candidates.push_back(socketName);
    } else {
        for (int i = 0; i < 32; ++i)
            candidates.push_back("wayland-" + QByteArray::number(i));
    }
    for (const QByteArray &name : candidates) {
        const QByteArray path = name.startsWith('/') ? name : runtimeDir + '/' + name;
        const QByteArray lockPath = path + ".lock";
        sockaddr_un addr{};
        if (static_cast<std::size_t>(path.size()) >= sizeof(addr.sun_path)) {
            qWarning("QWaylandCompositor: socket path too long: %s", path.constData());
            return false;
        }
        const int lockFd = setCloExec(::open(lockPath.constData(), O_CREAT | O_RDWR, 0660));
        if (lockFd < 0)
            continue;
        if (::flock(lockFd, LOCK_EX | LOCK_NB) < 0) {
            ::close(lockFd); // another compositor owns this name
            continue;
        }
        struct stat st{};
        if (::stat(path.constData(), &st) == 0)
            ::unlink(path.constData()); // stale: nobody holds the lock
        const int fd = setCloExec(::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
        if (fd < 0) {
            ::close(lockFd);
            return false;
        }
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.constData(), static_cast<std::size_t>(path.size()) + 1);
        if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 || ::listen(fd, 128) < 0) {
            qWarning("QWaylandCompositor: cannot listen on %s: %s", path.constData(), std::strerror(errno));
            ::close(fd);
            ::close(lockFd);
            return false;
        }
        m_listenFd = fd;
        m_lockFd = lockFd;
        m_socketName = name;
        m_socketPath = path;
        m_lockPath = lockPath;
        m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        QObject::connect(m_notifier, &QSocketNotifier::activated, this, [this] { accept(); });
        return true;
    }
    qWarning("QWaylandCompositor: no free socket name in %s", runtimeDir.constData());
    return false;
}

void Display::close() {
    while (!m_connections.empty())
        m_connections.back()->close(); // removes itself
    if (m_notifier) {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
        ::unlink(m_socketPath.constData());
    }
    if (m_lockFd >= 0) {
        ::unlink(m_lockPath.constData());
        ::close(m_lockFd);
        m_lockFd = -1;
    }
    m_socketName.clear();
}

void Display::accept() {
    for (;;) {
        const int fd = ::accept4(m_listenFd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        auto *connection = new Connection(this, fd);
        m_connections.push_back(connection);
        if (onConnected)
            onConnected(connection);
    }
}

std::uint32_t Display::addGlobal(const Interface *interface, std::uint32_t version,
                                 std::function<void(Resource *)> bind) {
    Global global{m_nextGlobalName++, interface, std::min(version, interface->version), std::move(bind)};
    m_globals.push_back(global);
    for (Connection *c : m_connections)
        c->announceGlobal(global);
    return global.name;
}

void Display::removeGlobal(std::uint32_t name) {
    m_globals.erase(
        std::remove_if(m_globals.begin(), m_globals.end(), [name](const Global &g) { return g.name == name; }),
        m_globals.end());
    for (Connection *c : m_connections)
        c->withdrawGlobal(name);
}

const Interface *Display::interfaceNamed(const char *name) const {
    for (const Global &g : m_globals)
        if (std::strcmp(g.interface->name, name) == 0)
            return g.interface;
    for (const Interface *i : m_interfaces)
        if (std::strcmp(i->name, name) == 0)
            return i;
    return nullptr;
}

void Display::registerInterface(const Interface *interface) {
    if (std::find(m_interfaces.begin(), m_interfaces.end(), interface) == m_interfaces.end())
        m_interfaces.push_back(interface);
}

std::uint32_t Display::timestamp() {
    using namespace std::chrono;
    return static_cast<std::uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

void Display::flushClients() {
    for (Connection *c : std::vector<Connection *>(m_connections))
        c->flush();
}

void Display::connectionClosed(Connection *connection) {
    m_connections.erase(std::remove(m_connections.begin(), m_connections.end(), connection), m_connections.end());
    if (onDisconnected)
        onDisconnected(connection);
}

} // namespace QtWaylandPrivate
