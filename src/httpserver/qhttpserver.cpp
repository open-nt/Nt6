// MIT License - see LICENSE
#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtHttpServer/qhttpserver.h>

#include <vector>

/*!
    \module NtHttpServer
    \title Nt HTTP Server C++ Classes
    \brief Classes for embedding an HTTP server in an application.
    \cmaketarget Nt::HttpServer

    Nt HTTP Server answers HTTP/1.1 requests from the Nt Core event loop.
    QHttpServer maps URL paths to handlers, with typed path arguments;
    QAbstractHttpServer is the base for servers that answer requests
    another way.

    See \l{Using Nt HTTP Server} for a walk through, and
    \c{examples/httpserver/hello_server.cpp} for a complete program.
*/

/*!
    \page nt-httpserver.html
    \title Using Nt HTTP Server
    \brief Serving HTTP from an application: routes with typed path arguments, responses, streaming, HTTPS and local sockets.

    Link \c Nt::HttpServer (or \c Nt6::HttpServer from an installed package)
    and include \c{<QtHttpServer>}:

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::HttpServer)
    \endcode

    \section1 Starting a Server

    A QHttpServer does not listen by itself. Create a QTcpServer, make it
    listen, and bind it. Port 0 picks a free port:

    \snippet httpserver/hello_server.cpp bind

    The server runs from the event loop, so the program must call
    QCoreApplication::exec(). One QHttpServer can serve several QTcpServer
    and QLocalServer objects at once.

    \section1 Routes

    QHttpServer::route() adds a rule that maps a path to a handler. Each
    \c{<arg>} in the path pattern is converted to the type of the matching
    handler parameter. The handler may also take the request, and the
    responder as its last parameter:

    \snippet httpserver/hello_server.cpp routes

    A path argument can be any integer type, \c float, \c double, QString,
    QByteArray or QUrl. A number must be written in decimal and fit the type;
    a path whose argument does not convert does not match the rule. Strings
    match one path segment and are percent-decoded; a QUrl matches the rest
    of the path, slashes included. If the pattern ends with \c{/} and has
    one \c{<arg>} fewer than the handler has path parameters, \c{<arg>} is
    appended.

    The rules are tried in the order they were added, and the first one whose
    pattern and method match handles the request. A request that no rule
    matches gets \c{404 Not Found}, or whatever QHttpServer::setMissingHandler()
    answers.

    A handler that belongs to an object takes the object as a context: the
    rule stops matching when the object is destroyed, and a member function
    is called on it:

    \code
    server.route("/status", &monitor, &Monitor::status);
    \endcode

    \section1 Responses

    A handler returns a QHttpServerResponse, or a value that converts to one:

    \snippet httpserver/hello_server.cpp responses

    To change every response, for example to add a header, use
    QHttpServer::addAfterRequestHandler():

    \snippet httpserver/hello_server.cpp after

    \section1 Writing the Response Yourself

    A handler that takes a QHttpServerResponder writes the response through
    it, and returns nothing. This is the way to stream a body in pieces, to
    send a file without loading it into memory, or to answer later:

    \snippet httpserver/hello_server.cpp responder

    A responder can be moved into a lambda or a member to answer later. The
    connection waits for that answer before it reads the next request.

    \section1 HTTPS and Local Sockets

    Bind a QSslServer instead of a QTcpServer to serve HTTPS. Requests reach
    the handlers after the TLS handshake, and their URLs have the scheme
    \c https:

    \code
    auto *tls = new QSslServer;
    tls->setSslConfiguration(configuration);   // certificate and private key
    tls->listen(QHostAddress::Any, 8443);
    server.bind(tls);
    \endcode

    A QLocalServer serves HTTP on a Unix domain socket, which only processes
    on the same machine can reach:

    \code
    auto *local = new QLocalServer;
    local->listen("/run/app/http.sock");
    server.bind(local);
    \endcode

    Try it with \c{curl --unix-socket /run/app/http.sock http://localhost/}.

    \section1 Limits and Safety

    The server checks every request before a handler sees it. A request it
    cannot parse, a request line over 8 KiB, header fields over 64 KiB, a
    body over 64 MiB, or a request with both \c Transfer-Encoding and
    \c Content-Length is refused with a 4xx status, and the connection is
    closed. A handler can rely on the request being well formed, but not on
    its contents: path arguments, header values and the body come from the
    client.

    There is no idle timeout: a client may keep a connection open as long as
    it likes. Put the server behind a reverse proxy when it faces the
    Internet.
*/

/*!
    \class QHttpServer
    \brief The QHttpServer class is an HTTP server that answers requests with routes.
    \since 6.4

    Add rules with route(), bind a listening QTcpServer, and run the event
    loop:

    \snippet httpserver/hello_server.cpp main

    See \l{Using Nt HTTP Server} for the path patterns and the handler
    signatures.

    \differences
    There is no \c router() and no custom \c QHttpServerRouterRule subclass
    or path converter. Handlers cannot return a \c QFuture. A context object
    must live in the thread of the server.
    \enddifferences

    \sa QHttpServerRequest, QHttpServerResponse, QHttpServerResponder
*/

/*!
    \class QHttpServerRouterRule
    \brief The QHttpServerRouterRule class is a rule that QHttpServer::route() added.
    \since 6.4

    A rule is owned by its QHttpServer, and lives as long as the server.

    \differences
    Rules cannot be created or subclassed by the application, so there are
    no \c exec(), \c matches() or \c hasValidMethods() functions.
    \enddifferences

    \sa QHttpServer::route()
*/

namespace QtPrivate {
namespace HttpServer {

bool parseSigned(QStringView text, qint64 min, qint64 max, qint64 *out) {
    const qsizetype n = text.size();
    qsizetype i = 0;
    bool negative = false;
    if (n > 0 && text.at(0) == QLatin1Char('-')) {
        negative = true;
        ++i;
    }
    if (i == n)
        return false;
    // Accumulate the magnitude as unsigned, so the most negative value does not overflow.
    const quint64 limit = negative ? static_cast<quint64>(-(min + 1)) + 1 : static_cast<quint64>(max);
    quint64 v = 0;
    for (; i < n; ++i) {
        const char16_t c = text.at(i).unicode();
        if (c < u'0' || c > u'9')
            return false;
        const auto digit = static_cast<quint64>(c - u'0');
        if (v > (limit - digit) / 10)
            return false;
        v = v * 10 + digit;
    }
    if (negative)
        *out = v == 0 ? 0 : -static_cast<qint64>(v - 1) - 1;
    else
        *out = static_cast<qint64>(v);
    return true;
}

bool parseUnsigned(QStringView text, quint64 max, quint64 *out) {
    if (text.isEmpty())
        return false;
    quint64 v = 0;
    for (QChar ch : text) {
        const char16_t c = ch.unicode();
        if (c < u'0' || c > u'9')
            return false;
        const auto digit = static_cast<quint64>(c - u'0');
        if (v > (max - digit) / 10)
            return false;
        v = v * 10 + digit;
    }
    *out = v;
    return true;
}

bool parseFloating(QStringView text, double *out) {
    if (text.isEmpty())
        return false;
    for (QChar ch : text) {
        const char16_t c = ch.unicode();
        if (!((c >= u'0' && c <= u'9') || c == u'.' || c == u'-' || c == u'+' || c == u'e' || c == u'E'))
            return false;
    }
    bool ok = false;
    const double v = text.toString().toDouble(&ok);
    if (!ok)
        return false;
    *out = v;
    return true;
}

} // namespace HttpServer
} // namespace QtPrivate

struct QHttpServerRouterRule::Private {
    QString pathPattern;
    QHttpServerRequest::Methods methods;
    bool hasContext = false;
    QPointer<QObject> context;
    QStringList literals; // the text around the <arg>s; one more than args
    QList<QtPrivate::HttpServer::PathArg> args;
    std::function<std::optional<QHttpServerResponse>(const QStringList &, const QHttpServerRequest &,
                                                     QHttpServerResponder &)>
        handler;

    bool isActive() const { return !hasContext || context; }
    bool match(const QString &path, QStringList *captures) const;
    bool matchFrom(const QString &path, qsizetype pos, qsizetype index, QStringList *captures) const;
};

namespace {

bool literalAt(const QString &path, qsizetype pos, const QString &literal) {
    if (path.size() - pos < literal.size())
        return false;
    for (qsizetype i = 0; i < literal.size(); ++i) {
        if (path.at(pos + i) != literal.at(i))
            return false;
    }
    return true;
}

} // namespace

bool QHttpServerRouterRule::Private::match(const QString &path, QStringList *captures) const {
    captures->clear();
    return matchFrom(path, 0, 0, captures);
}

bool QHttpServerRouterRule::Private::matchFrom(const QString &path, qsizetype pos, qsizetype index,
                                               QStringList *captures) const {
    const QString &literal = literals.at(index);
    if (!literalAt(path, pos, literal))
        return false;
    pos += literal.size();
    if (index == args.size())
        return pos == path.size();
    const QtPrivate::HttpServer::PathArg &arg = args.at(index);
    const QString &next = literals.at(index + 1);
    qsizetype limit = path.size();
    if (!arg.spansSlashes) {
        const qsizetype slash = path.indexOf(QLatin1Char('/'), pos);
        if (slash >= 0)
            limit = slash;
    }
    // Longest first, and only where the text that follows the argument can start.
    for (qsizetype end = limit; end >= pos; --end) {
        if (!next.isEmpty() && !literalAt(path, end, next))
            continue;
        const QString text = path.mid(pos, end - pos);
        if (!arg.accept(QStringView(text)))
            continue;
        captures->append(text);
        if (matchFrom(path, end, index + 1, captures))
            return true;
        captures->removeLast();
    }
    return false;
}

QHttpServerRouterRule::QHttpServerRouterRule(std::unique_ptr<Private> p) : d(std::move(p)) {}

/*!
    Destroys the rule.
*/
QHttpServerRouterRule::~QHttpServerRouterRule() = default;

/*!
    Returns the context object of the rule, or \c nullptr if the rule has
    none or the object was destroyed.
*/
const QObject *QHttpServerRouterRule::contextObject() const { return d->context.data(); }

/*!
    Returns the path pattern of the rule, with \c{<arg>} for each path argument.
*/
QString QHttpServerRouterRule::pathPattern() const { return d->pathPattern; }

/*!
    Returns the methods the rule accepts.
*/
QHttpServerRequest::Methods QHttpServerRouterRule::methods() const { return d->methods; }

struct QHttpServer::Private {
    struct After {
        QPointer<QObject> context;
        AfterRequestHandler handler;
    };
    std::vector<std::unique_ptr<QHttpServerRouterRule>> rules;
    std::vector<After> afterHandlers;
    bool hasMissingHandler = false;
    QPointer<QObject> missingContext;
    MissingHandler missingHandler;
};

/*!
    Constructs an HTTP server with the given \a parent. It has no routes and
    serves nothing until a server is bound to it.
*/
QHttpServer::QHttpServer(QObject *parent) : QAbstractHttpServer(parent), d(std::make_unique<Private>()) {}

/*!
    Destroys the HTTP server, its routes and its connections.
*/
QHttpServer::~QHttpServer() = default;

QHttpServerRouterRule *QHttpServer::addRoute(const QString &pathPattern, QHttpServerRequest::Methods method,
                                             const QObject *context, const QList<QtPrivate::HttpServer::PathArg> &args,
                                             RouteHandler handler) {
    static const QString placeholder = QStringLiteral("<arg>");
    QString pattern = pathPattern;
    qsizetype count = pattern.count(placeholder);
    if (pattern.endsWith(QLatin1Char('/')) && count + 1 == args.size()) {
        pattern += placeholder;
        ++count;
    }
    if (count != args.size()) {
        qWarning("QHttpServer::route: the pattern \"%s\" has %lld <arg>, but the handler takes %lld path arguments",
                 qPrintable(pathPattern), static_cast<long long>(count), static_cast<long long>(args.size()));
        return nullptr;
    }
    if (!(method & QHttpServerRequest::Method::AnyKnown)) {
        qWarning("QHttpServer::route: no valid method for \"%s\"", qPrintable(pathPattern));
        return nullptr;
    }
    auto p = std::make_unique<QHttpServerRouterRule::Private>();
    p->pathPattern = pattern;
    p->methods = method;
    p->hasContext = context != nullptr;
    p->context = const_cast<QObject *>(context);
    p->literals = pattern.split(placeholder);
    p->args = args;
    p->handler = std::move(handler);
    d->rules.push_back(std::unique_ptr<QHttpServerRouterRule>(new QHttpServerRouterRule(std::move(p))));
    return d->rules.back().get();
}

void QHttpServer::addAfterRequestHandlerImpl(const QObject *context, AfterRequestHandler handler) {
    if (!context) {
        qWarning("QHttpServer::addAfterRequestHandler: a context object is required");
        return;
    }
    d->afterHandlers.push_back({const_cast<QObject *>(context), std::move(handler)});
}

void QHttpServer::setMissingHandlerImpl(const QObject *context, MissingHandler handler) {
    if (!context) {
        qWarning("QHttpServer::setMissingHandler: a context object is required");
        return;
    }
    d->hasMissingHandler = true;
    d->missingContext = const_cast<QObject *>(context);
    d->missingHandler = std::move(handler);
}

/*!
    Removes the handler set with setMissingHandler(), so that requests no
    route matches get \c{404 Not Found} again.
*/
void QHttpServer::clearMissingHandler() {
    d->hasMissingHandler = false;
    d->missingContext = nullptr;
    d->missingHandler = nullptr;
}

/*!
    \reimp

    Tries the rules in the order they were added, and calls the handler of
    the first one that matches \a request, answering through \a responder.
    Returns \c false if no rule matches.
*/
bool QHttpServer::handleRequest(const QHttpServerRequest &request, QHttpServerResponder &responder) {
    const QString path = request.url().path();
    QStringList captures;
    for (std::size_t i = 0; i < d->rules.size(); ++i) {
        QHttpServerRouterRule::Private &rule = *d->rules[i]->d;
        if (!rule.isActive() || !(rule.methods & request.method()))
            continue;
        if (!rule.match(path, &captures))
            continue;
        // The handler may add routes, so the rule is not used after this call.
        auto handler = rule.handler;
        std::optional<QHttpServerResponse> response = handler(captures, request, responder);
        if (response) {
            // Copied, since a handler may add more handlers.
            const std::vector<Private::After> after = d->afterHandlers;
            for (const Private::After &a : after) {
                if (a.context)
                    a.handler(request, *response);
            }
            responder.sendResponse(*response);
        }
        return true;
    }
    return false;
}

/*!
    \reimp

    Calls the handler set with setMissingHandler() for \a request, or
    answers \c{404 Not Found} through \a responder if there is none.
*/
void QHttpServer::missingHandler(const QHttpServerRequest &request, QHttpServerResponder &responder) {
    if (d->hasMissingHandler && d->missingContext) {
        auto handler = d->missingHandler;
        handler(request, responder);
        return;
    }
    responder.write(QHttpServerResponder::StatusCode::NotFound);
}

#include "moc_qhttpserver.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
