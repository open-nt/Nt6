// MIT License - see LICENSE
#pragma once

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <QtNetwork/qhostaddress.h>

#include <cstring>

// Without the flag, a write to a closed connection raises SIGPIPE, on a system that has signals.
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace QtPrivate {

// Makes `fd` non-blocking and close-on-exec, for a system that cannot ask for both as the socket is made.
// Closes it and returns -1 if that fails.
inline int prepareSocket(int fd) {
    if (fd < 0)
        return fd;
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        ::close(fd);
        return -1;
    }
#ifdef FD_CLOEXEC
    (void)::fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif
    return fd;
}

// socket() for a non-blocking, close-on-exec socket of `type` (SOCK_STREAM or SOCK_DGRAM).
inline int nonBlockingSocket(int family, int type) {
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
    return ::socket(family, type | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
#else
    return prepareSocket(::socket(family, type, 0));
#endif
}

// accept() that returns a non-blocking, close-on-exec socket.
inline int nonBlockingAccept(int listener) {
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
    return ::accept4(listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
    return prepareSocket(::accept(listener, nullptr, nullptr));
#endif
}

// Fills `ss` for an AF_INET or AF_INET6 socket; IPv4 addresses are mapped on IPv6 sockets.
inline socklen_t toSockaddr(const QHostAddress &a, quint16 port, int family, sockaddr_storage *ss) {
    std::memset(ss, 0, sizeof *ss);
    if (family == AF_INET) {
        auto *s4 = reinterpret_cast<sockaddr_in *>(ss);
        s4->sin_family = AF_INET;
        s4->sin_port = htons(port);
        bool ok = false;
        const quint32 v4 = a.toIPv4Address(&ok);
        s4->sin_addr.s_addr = htonl(ok ? v4 : 0);
        return sizeof(sockaddr_in);
    }
    auto *s6 = reinterpret_cast<sockaddr_in6 *>(ss);
    s6->sin6_family = AF_INET6;
    s6->sin6_port = htons(port);
    const Q_IPV6ADDR v6 = a.toIPv6Address();
    std::memcpy(&s6->sin6_addr, v6.c, 16);
    return sizeof(sockaddr_in6);
}

inline QHostAddress fromSockaddr(const sockaddr_storage &ss, quint16 *port) {
    if (ss.ss_family == AF_INET) {
        const auto *s4 = reinterpret_cast<const sockaddr_in *>(&ss);
        if (port)
            *port = ntohs(s4->sin_port);
        return QHostAddress(ntohl(s4->sin_addr.s_addr));
    }
    if (ss.ss_family == AF_INET6) {
        const auto *s6 = reinterpret_cast<const sockaddr_in6 *>(&ss);
        if (port)
            *port = ntohs(s6->sin6_port);
        return QHostAddress(reinterpret_cast<const quint8 *>(&s6->sin6_addr));
    }
    if (port)
        *port = 0;
    return QHostAddress();
}

} // namespace QtPrivate
