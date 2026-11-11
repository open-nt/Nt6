// MIT License - see LICENSE
#include <QtCore/qatomic.h>
#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>
#include <QtCore/qpointer.h>
#include <QtNetwork/qhostinfo.h>

#include "sockaddr_p.h"
#include "threaddata_p.h"

#include <netdb.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstring>
#include <memory>
#include <mutex>
#include <set>

namespace {

std::atomic<int> g_nextId{1};
std::mutex g_abortLock;
std::set<int> g_aborted;

class ResultEvent : public QEvent {
public:
    explicit ResultEvent(const QHostInfo &result) : QEvent(QEvent::User), info(result) {}
    QHostInfo info;
};

// Lives on the caller's thread; the worker only ever posts events to it.
class LookupRelay : public QObject {
public:
    LookupRelay(const QObject *context, std::function<void(const QHostInfo &)> callback)
        : m_context(const_cast<QObject *>(context)), m_hasContext(context != nullptr), m_callback(std::move(callback)) {
    }

protected:
    void customEvent(QEvent *event) override {
        const QHostInfo &info = static_cast<ResultEvent *>(event)->info;
        bool aborted;
        {
            std::lock_guard<std::mutex> lock(g_abortLock);
            aborted = g_aborted.erase(info.lookupId()) != 0;
        }
        if (!aborted && (!m_hasContext || m_context))
            m_callback(info);
        deleteLater();
    }

private:
    QPointer<QObject> m_context;
    bool m_hasContext;
    std::function<void(const QHostInfo &)> m_callback;
};

} // namespace

/*!
    \class QHostInfo
    \brief The QHostInfo class provides host name lookup.

    Call lookupHost() to resolve a name without blocking the event loop. The
    function you pass receives the result:

    \code
    QHostInfo::lookupHost("example.com", [](const QHostInfo &info) {
        if (info.error() == QHostInfo::NoError)
            qDebug() << info.addresses().first().toString();
    });
    \endcode

    fromName() does the same lookup and blocks until it finishes.

    \differences
    There is no string-based \c SLOT() overload of lookupHost(). Each lookup
    uses its own thread rather than a shared pool, and abortHostLookup() only
    discards the result: the system resolver call itself runs to completion.
    \enddifferences

    \sa QHostAddress
*/

/*!
    \enum QHostInfo::HostInfoError

    Describes the outcome of a lookup.

    \value NoError The lookup succeeded.
    \value HostNotFound No addresses were found for the name.
    \value UnknownError Another error occurred.
*/

/*!
    Constructs an empty result with the lookup ID \a lookupId.
*/
QHostInfo::QHostInfo(int lookupId) : m_id(lookupId) {}

/*!
    Returns the name that was looked up.

    \sa setHostName()
*/
QString QHostInfo::hostName() const { return m_name; }

/*!
    Sets the host name to \a name.

    \sa hostName()
*/
void QHostInfo::setHostName(const QString &name) { m_name = name; }

/*!
    Returns the addresses found for the host.

    \sa setAddresses()
*/
QList<QHostAddress> QHostInfo::addresses() const { return m_addresses; }

/*!
    Sets the list of found addresses to \a addresses.

    \sa addresses()
*/
void QHostInfo::setAddresses(const QList<QHostAddress> &addresses) { m_addresses = addresses; }

/*!
    Returns the error of the lookup, or NoError if it succeeded.

    \sa errorString()
*/
QHostInfo::HostInfoError QHostInfo::error() const { return m_error; }

/*!
    Sets the error to \a error.

    \sa error()
*/
void QHostInfo::setError(HostInfoError error) { m_error = error; }

/*!
    Returns a description of the last error.

    \sa error()
*/
QString QHostInfo::errorString() const { return m_errorString; }

/*!
    Sets the error description to \a errorString.

    \sa errorString()
*/
void QHostInfo::setErrorString(const QString &errorString) { m_errorString = errorString; }

/*!
    Returns the ID of this lookup, or \c -1 if there is none.

    \sa setLookupId()
*/
int QHostInfo::lookupId() const { return m_id; }

/*!
    Sets the lookup ID to \a id.

    \sa lookupId()
*/
void QHostInfo::setLookupId(int id) { m_id = id; }

/*!
    Looks up \a name and blocks until the result is known. An IP address is
    reverse-resolved; if that fails, its text is used as the host name.

    \sa lookupHost()
*/
QHostInfo QHostInfo::fromName(const QString &name) {
    QHostInfo result;
    result.setHostName(name);
    if (name.isEmpty()) {
        result.setError(HostNotFound);
        result.setErrorString(QString("No host name given"));
        return result;
    }

    const QHostAddress literal(name);
    if (!literal.isNull()) {
        result.setAddresses({literal});
        sockaddr_storage ss;
        const socklen_t len = QtPrivate::toSockaddr(
            literal, 0, literal.protocol() == QAbstractSocket::IPv4Protocol ? AF_INET : AF_INET6, &ss);
        char host[NI_MAXHOST];
        if (getnameinfo(reinterpret_cast<sockaddr *>(&ss), len, host, sizeof host, nullptr, 0, NI_NAMEREQD) == 0)
            result.setHostName(QString::fromUtf8(host));
        return result;
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *res = nullptr;
    const QByteArray encoded = name.toUtf8();
    const int rc = getaddrinfo(encoded.constData(), nullptr, &hints, &res);
    if (rc != 0) {
        const bool missing = rc == EAI_NONAME
#ifdef EAI_NODATA
                             || rc == EAI_NODATA
#endif
            ;
        result.setError(missing ? HostNotFound : UnknownError);
        result.setErrorString(QString::fromUtf8(gai_strerror(rc)));
        return result;
    }
    QList<QHostAddress> addresses;
    for (addrinfo *a = res; a; a = a->ai_next) {
        sockaddr_storage ss = {};
        std::memcpy(&ss, a->ai_addr, a->ai_addrlen);
        const QHostAddress address = QtPrivate::fromSockaddr(ss, nullptr);
        if (!address.isNull() && !addresses.contains(address))
            addresses.append(address);
    }
    freeaddrinfo(res);
    result.setAddresses(addresses);
    if (addresses.isEmpty()) {
        result.setError(HostNotFound);
        result.setErrorString(QString("Host not found"));
    }
    return result;
}

int QHostInfo::startLookup(const QString &name, const QObject *context,
                           std::function<void(const QHostInfo &)> callback) {
    const int id = g_nextId.fetch_add(1);
    auto *relay = new LookupRelay(context, std::move(callback));
    const auto lookup = [name, id, relay] {
        QHostInfo info = fromName(name);
        info.setLookupId(id);
        QCoreApplication::postEvent(relay, new ResultEvent(info));
    };
    // Without a thread to run it in, the lookup blocks the caller; the result is still delivered later.
    if (!QtPrivate::startDetachedThread(lookup))
        lookup();
    return id;
}

/*!
    Discards the result of the lookup \a lookupId, if it has not been delivered.

    \sa lookupHost()
*/
void QHostInfo::abortHostLookup(int lookupId) {
    std::lock_guard<std::mutex> lock(g_abortLock);
    g_aborted.insert(lookupId);
}

/*!
    Returns this machine's host name.

    \sa localDomainName()
*/
QString QHostInfo::localHostName() {
    char name[256] = {};
    if (gethostname(name, sizeof name - 1) != 0)
        return QString();
    return QString::fromUtf8(name);
}

/*!
    Returns this machine's DNS domain, or an empty string if it has none.

    \sa localHostName()
*/
QString QHostInfo::localDomainName() {
    const QString host = localHostName();
    const qsizetype dot = host.indexOf(QChar(u'.'));
    return dot >= 0 ? host.mid(dot + 1) : QString();
}
