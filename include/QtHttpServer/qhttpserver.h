// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>
#include <QtCore/qurl.h>
#include <QtHttpServer/qabstracthttpserver.h>
#include <QtHttpServer/qhttpserverrequest.h>
#include <QtHttpServer/qhttpserverresponder.h>
#include <QtHttpServer/qhttpserverresponse.h>
#include <QtHttpServer/qhttpserverrouterrule.h>

#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace QtPrivate {
namespace HttpServer {

// How the router matches one <arg> of a path pattern: the text must be accepted by the argument's
// converter, and only a QUrl argument may contain '/' (it takes the rest of the path).
struct PathArg {
    bool (*accept)(QStringView text);
    bool spansSlashes;
};

bool parseSigned(QStringView text, qint64 min, qint64 max, qint64 *out);
bool parseUnsigned(QStringView text, quint64 max, quint64 *out);
bool parseFloating(QStringView text, double *out);

template <typename T, typename = void> struct ArgConverter {
    static constexpr bool supported = false;
};

template <typename T>
struct ArgConverter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && std::is_signed_v<T>>> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = false;
    static bool convert(QStringView text, T *out) {
        qint64 v = 0;
        if (!parseSigned(text, std::numeric_limits<T>::min(), std::numeric_limits<T>::max(), &v))
            return false;
        *out = static_cast<T>(v);
        return true;
    }
};

template <typename T>
struct ArgConverter<T, std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool> && std::is_unsigned_v<T>>> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = false;
    static bool convert(QStringView text, T *out) {
        quint64 v = 0;
        if (!parseUnsigned(text, std::numeric_limits<T>::max(), &v))
            return false;
        *out = static_cast<T>(v);
        return true;
    }
};

template <typename T> struct ArgConverter<T, std::enable_if_t<std::is_floating_point_v<T>>> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = false;
    static bool convert(QStringView text, T *out) {
        double v = 0;
        if (!parseFloating(text, &v))
            return false;
        if (std::is_same_v<T, float> && (v > static_cast<double>(std::numeric_limits<float>::max()) ||
                                         v < static_cast<double>(std::numeric_limits<float>::lowest())))
            return false;
        *out = static_cast<T>(v);
        return true;
    }
};

template <> struct ArgConverter<QString> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = false;
    static bool convert(QStringView text, QString *out) {
        if (text.isEmpty())
            return false;
        *out = text.toString();
        return true;
    }
};

template <> struct ArgConverter<QByteArray> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = false;
    static bool convert(QStringView text, QByteArray *out) {
        if (text.isEmpty())
            return false;
        *out = text.toString().toUtf8();
        return true;
    }
};

template <> struct ArgConverter<QUrl> {
    static constexpr bool supported = true;
    static constexpr bool spansSlashes = true;
    static bool convert(QStringView text, QUrl *out) {
        *out = QUrl(text.toString());
        return true;
    }
};

template <typename T> PathArg pathArg() {
    return {[](QStringView text) {
                T value{};
                return ArgConverter<T>::convert(text, &value);
            },
            ArgConverter<T>::spansSlashes};
}

template <typename F> struct FunctionTraits : FunctionTraits<decltype(&F::operator())> {};
template <typename R, typename... A> struct FunctionTraits<R (*)(A...)> {
    using ReturnType = R;
    using Arguments = std::tuple<A...>;
    static constexpr std::size_t ArgumentCount = sizeof...(A);
};
template <typename R, typename... A> struct FunctionTraits<R (*)(A...) noexcept> : FunctionTraits<R (*)(A...)> {};
template <typename R, typename... A> struct FunctionTraits<R(A...)> : FunctionTraits<R (*)(A...)> {};
template <typename C, typename R, typename... A> struct FunctionTraits<R (C::*)(A...)> : FunctionTraits<R (*)(A...)> {
    using Class = C;
};
template <typename C, typename R, typename... A>
struct FunctionTraits<R (C::*)(A...) const> : FunctionTraits<R (C::*)(A...)> {};
template <typename C, typename R, typename... A>
struct FunctionTraits<R (C::*)(A...) noexcept> : FunctionTraits<R (C::*)(A...)> {};
template <typename C, typename R, typename... A>
struct FunctionTraits<R (C::*)(A...) const noexcept> : FunctionTraits<R (C::*)(A...)> {};

template <typename T> inline constexpr bool IsRequestArg = std::is_same_v<T, const QHttpServerRequest &>;
template <typename T> inline constexpr bool IsResponderArg = std::is_same_v<T, QHttpServerResponder &>;

// The parameters of a route handler: the path arguments, then optionally
// `const QHttpServerRequest &`, then optionally `QHttpServerResponder &`.
template <typename Functor> struct RouteTraits {
    using Traits = FunctionTraits<std::decay_t<Functor>>;
    using Arguments = typename Traits::Arguments;
    using ReturnType = typename Traits::ReturnType;
    static constexpr std::size_t Count = Traits::ArgumentCount;

    template <std::size_t I, typename = void> struct At {
        using Type = void;
    };
    template <std::size_t I> struct At<I, std::enable_if_t<(I < Count)>> {
        using Type = std::tuple_element_t<I, Arguments>;
    };

    static constexpr bool HasResponder = Count >= 1 && IsResponderArg<typename At<Count - 1>::Type>;
    static constexpr bool HasRequest = HasResponder ? (Count >= 2 && IsRequestArg<typename At<Count - 2>::Type>)
                                                    : (Count >= 1 && IsRequestArg<typename At<Count - 1>::Type>);
    static constexpr std::size_t PathCount = Count - (HasResponder ? 1 : 0) - (HasRequest ? 1 : 0);

    template <std::size_t I> using PathType = std::decay_t<std::tuple_element_t<I, Arguments>>;

    template <std::size_t... I> static constexpr bool pathArgsSupported(std::index_sequence<I...>) {
        return (ArgConverter<PathType<I>>::supported && ...);
    }
    template <std::size_t... I> static QList<PathArg> pathArgs(std::index_sequence<I...>) {
        return {pathArg<PathType<I>>()...};
    }
};

template <typename Functor, typename... Args>
decltype(auto) callSlot(const QObject *context, Functor &slot, Args &&...args) {
    if constexpr (std::is_member_function_pointer_v<Functor>) {
        using C = typename FunctionTraits<Functor>::Class;
        static_assert(std::is_base_of_v<QObject, C>, "A member function slot must belong to a QObject subclass");
        return (static_cast<C *>(const_cast<QObject *>(context))->*slot)(std::forward<Args>(args)...);
    } else {
        Q_UNUSED(context);
        return slot(std::forward<Args>(args)...);
    }
}

} // namespace HttpServer
} // namespace QtPrivate

class QHttpServer final : public QAbstractHttpServer {
    Q_OBJECT
    using RouteHandler = std::function<std::optional<QHttpServerResponse>(
        const QStringList &, const QHttpServerRequest &, QHttpServerResponder &)>;
    using AfterRequestHandler = std::function<void(const QHttpServerRequest &, QHttpServerResponse &)>;
    using MissingHandler = std::function<void(const QHttpServerRequest &, QHttpServerResponder &)>;

public:
    explicit QHttpServer(QObject *parent = nullptr);
    ~QHttpServer() override;

    /*!
        \fn template <typename Functor> QHttpServerRouterRule *QHttpServer::route(const QString &pathPattern, QHttpServerRequest::Methods method, const QObject *context, Functor &&slot)

        Adds a rule that answers requests whose path matches \a pathPattern
        and whose method is one of \a method by calling \a slot, and returns
        the rule, or \c nullptr if \a pathPattern does not fit the parameters
        of \a slot. The rule stops matching when \a context is destroyed. If
        \a slot is a member function, it is called on \a context.

        See \l{Routes} for the patterns and the handler signatures.
    */
    template <typename Functor>
    QHttpServerRouterRule *route(const QString &pathPattern, QHttpServerRequest::Methods method, const QObject *context,
                                 Functor &&slot) {
        return makeRoute(pathPattern, method, context, true, std::forward<Functor>(slot));
    }

    /*!
        \fn template <typename Functor> QHttpServerRouterRule *QHttpServer::route(const QString &pathPattern, const QObject *context, Functor &&slot)
        \overload

        Adds a rule for \a pathPattern that accepts any known method and calls
        \a slot, which belongs to \a context.
    */
    template <typename Functor>
    QHttpServerRouterRule *route(const QString &pathPattern, const QObject *context, Functor &&slot) {
        return makeRoute(pathPattern, QHttpServerRequest::Method::AnyKnown, context, true, std::forward<Functor>(slot));
    }

    /*!
        \fn template <typename Functor> QHttpServerRouterRule *QHttpServer::route(const QString &pathPattern, QHttpServerRequest::Methods method, Functor &&handler)
        \overload

        Adds a rule for \a pathPattern and \a method that calls \a handler. The
        rule lives as long as the server.
    */
    template <typename Functor>
    QHttpServerRouterRule *route(const QString &pathPattern, QHttpServerRequest::Methods method, Functor &&handler) {
        static_assert(!std::is_member_function_pointer_v<std::decay_t<Functor>>,
                      "A member function needs a context object");
        return makeRoute(pathPattern, method, this, false, std::forward<Functor>(handler));
    }

    /*!
        \fn template <typename Functor> QHttpServerRouterRule *QHttpServer::route(const QString &pathPattern, Functor &&handler)
        \overload

        Adds a rule for \a pathPattern that accepts any known method and calls
        \a handler:

        \snippet httpserver/hello_server.cpp routes
    */
    template <typename Functor> QHttpServerRouterRule *route(const QString &pathPattern, Functor &&handler) {
        static_assert(!std::is_member_function_pointer_v<std::decay_t<Functor>>,
                      "A member function needs a context object");
        return makeRoute(pathPattern, QHttpServerRequest::Method::AnyKnown, this, false,
                         std::forward<Functor>(handler));
    }

    /*!
        \fn template <typename Functor> void QHttpServer::addAfterRequestHandler(const QObject *context, Functor &&slot)

        Adds \a slot, which is called with each request and the response that
        a route handler returned, before the response is sent. It takes
        \c{(const QHttpServerRequest &, QHttpServerResponse &)} and may change
        the response, for example to add headers. Handlers run in the order
        they were added, and are dropped when \a context is destroyed.

        Responses that a handler writes through a QHttpServerResponder do not
        pass through these handlers.

        \snippet httpserver/hello_server.cpp after
    */
    template <typename Functor> void addAfterRequestHandler(const QObject *context, Functor &&slot) {
        using F = std::decay_t<Functor>;
        addAfterRequestHandlerImpl(
            context, [context, f = F(std::forward<Functor>(slot))](const QHttpServerRequest &request,
                                                                   QHttpServerResponse &response) mutable {
                QtPrivate::HttpServer::callSlot(context, f, request, response);
            });
    }

    /*!
        \fn template <typename Functor> void QHttpServer::setMissingHandler(const QObject *context, Functor &&slot)

        Sets \a slot as the handler for requests that no route matched. It
        takes \c{(const QHttpServerRequest &, QHttpServerResponder &)}. The
        default handler, which is used again when \a context is destroyed,
        answers \c{404 Not Found}.

        \sa clearMissingHandler()
    */
    template <typename Functor> void setMissingHandler(const QObject *context, Functor &&slot) {
        using F = std::decay_t<Functor>;
        setMissingHandlerImpl(context, [context, f = F(std::forward<Functor>(slot))](
                                           const QHttpServerRequest &request, QHttpServerResponder &responder) mutable {
            QtPrivate::HttpServer::callSlot(context, f, request, responder);
        });
    }
    void clearMissingHandler();

protected:
    bool handleRequest(const QHttpServerRequest &request, QHttpServerResponder &responder) override;
    void missingHandler(const QHttpServerRequest &request, QHttpServerResponder &responder) override;

private:
    template <typename Functor>
    QHttpServerRouterRule *makeRoute(const QString &pathPattern, QHttpServerRequest::Methods method,
                                     const QObject *context, bool hasContext, Functor &&slot) {
        using F = std::decay_t<Functor>;
        using RT = QtPrivate::HttpServer::RouteTraits<F>;
        using Return = typename RT::ReturnType;
        constexpr auto pathIndexes = std::make_index_sequence<RT::PathCount>();
        static_assert(RT::pathArgsSupported(pathIndexes),
                      "Path arguments must be integers, floating point numbers, QString, QByteArray or QUrl");
        static_assert(!std::is_void_v<Return> || RT::HasResponder,
                      "A handler that returns nothing must take a QHttpServerResponder &");
        static_assert(std::is_void_v<Return> || std::is_constructible_v<QHttpServerResponse, Return>,
                      "A handler must return void or something a QHttpServerResponse can be made from");
        RouteHandler handler = [context, f = F(std::forward<Functor>(slot))](
                                   const QStringList &captures, const QHttpServerRequest &request,
                                   QHttpServerResponder &responder) mutable -> std::optional<QHttpServerResponse> {
            return invokeRoute<RT>(context, f, captures, request, responder, std::make_index_sequence<RT::PathCount>());
        };
        return addRoute(pathPattern, method, hasContext ? context : nullptr, RT::pathArgs(pathIndexes),
                        std::move(handler));
    }

    template <typename RT, typename F, std::size_t... I>
    static std::optional<QHttpServerResponse> invokeRoute(const QObject *context, F &f, const QStringList &captures,
                                                          const QHttpServerRequest &request,
                                                          QHttpServerResponder &responder, std::index_sequence<I...>) {
        std::tuple<typename RT::template PathType<I>...> values;
        (QtPrivate::HttpServer::ArgConverter<typename RT::template PathType<I>>::convert(
             QStringView(captures.at(static_cast<qsizetype>(I))), &std::get<I>(values)),
         ...);
        Q_UNUSED(captures);
        auto call = [&]() -> decltype(auto) {
            using QtPrivate::HttpServer::callSlot;
            if constexpr (RT::HasRequest && RT::HasResponder)
                return callSlot(context, f, std::get<I>(values)..., request, responder);
            else if constexpr (RT::HasRequest)
                return callSlot(context, f, std::get<I>(values)..., request);
            else if constexpr (RT::HasResponder)
                return callSlot(context, f, std::get<I>(values)..., responder);
            else
                return callSlot(context, f, std::get<I>(values)...);
        };
        if constexpr (std::is_void_v<typename RT::ReturnType>) {
            call();
            return std::nullopt;
        } else {
            return QHttpServerResponse(call());
        }
    }

    QHttpServerRouterRule *addRoute(const QString &pathPattern, QHttpServerRequest::Methods method,
                                    const QObject *context, const QList<QtPrivate::HttpServer::PathArg> &args,
                                    RouteHandler handler);
    void addAfterRequestHandlerImpl(const QObject *context, AfterRequestHandler handler);
    void setMissingHandlerImpl(const QObject *context, MissingHandler handler);

    struct Private;
    std::unique_ptr<Private> d;
};
