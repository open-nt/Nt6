// MIT License - see LICENSE
#pragma once

#include <QtCore/qdebug.h>
#include <QtCore/qfuture.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qpromise.h>
#include <QtCore/qthreadpool.h>

#include <atomic>
#include <exception>
#include <functional>
#include <initializer_list>
#include <memory>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace QtPrivate {

struct ContinuationAccess {
    template <typename T>
    static QFutureInterface<T> &iface(const QFuture<T> &f) {
        return f.d;
    }
};

// Runs `job` as the launcher says. A job that is dropped, because its context is gone, takes the promise it
// holds with it, which cancels the future of the continuation.
inline void dispatchJob(const Launcher &launcher, const QPointer<QObject> &context, QThreadPool *parentPool,
                        QFutureInterfaceBase &child, std::function<void()> job) {
    QThreadPool *pool = nullptr;
    switch (launcher.kind) {
    case Launcher::Sync:
        break;
    case Launcher::Pool:
        pool = launcher.pool ? launcher.pool : QThreadPool::globalInstance();
        break;
    case Launcher::Inherit:
        pool = parentPool;
        break;
    case Launcher::Context:
        if (QObject *c = context.data())
            QMetaObject::invokeMethod(c, std::move(job));
        return;
    }
    if (pool) {
        child.setThreadPool(pool);
        pool->start(std::move(job));
    } else {
        job();
    }
}

// Hooks `runner`, which completes the future of a continuation, to the end of `parent`.
template <typename T, typename R, typename Runner>
QFuture<R> attachContinuation(const QFuture<T> &parent, const Launcher &launcher, const std::shared_ptr<Runner> &runner) {
    QFutureInterface<T> &parentIface = ContinuationAccess::iface(parent);
    QFuture<R> result = runner->promise.future();
    FutureAccess::setParent(runner->child, parentIface);
    const QPointer<QObject> context(launcher.context);
    auto continuation = [runner, launcher, context](const QFutureInterfaceBase &finished) {
        QFuture<T> pf{QFutureInterface<T>(finished)};
        dispatchJob(launcher, context, finished.threadPool(), runner->child, [runner, pf] { runner->run(pf); });
    };
    if (!FutureAccess::setContinuation(parentIface, std::move(continuation))) {
        qWarning("QFuture: a continuation is already attached to this future");
        runner->promise.start();
        result.cancel();
        runner->promise.finish();
    }
    return result;
}

template <typename T>
T fetchResult(QFuture<T> &parent) {
    if constexpr (std::is_copy_constructible_v<T>)
        return parent.result();
    else
        return parent.takeResult();
}

template <typename T, typename R, typename Fn>
struct ThenRunner {
    ThenRunner(Fn f, const QFutureInterface<R> &c) : fn(std::move(f)), child(c), promise(c) {}

    decltype(auto) call(QFuture<T> &parent) {
        if constexpr (ThenInvoke<Fn, T>::byFuture)
            return std::invoke(fn, parent);
        else if constexpr (std::is_void_v<T>)
            return std::invoke(fn);
        else
            return std::invoke(fn, fetchResult(parent));
    }

    void run(QFuture<T> parent) {
        promise.start();
        if (promise.isCanceled()) {
            promise.finish();
            return;
        }
        if constexpr (!ThenInvoke<Fn, T>::byFuture) {
            const std::exception_ptr ex = FutureAccess::exception(ContinuationAccess::iface(parent));
            if (ex) {
                promise.setException(ex);
                promise.finish();
                return;
            }
            bool skip = parent.isCanceled();
            if constexpr (!std::is_void_v<T>)
                skip = skip || parent.resultCount() == 0;
            if (skip) {
                promise.future().cancel();
                promise.finish();
                return;
            }
        }
        try {
            if constexpr (std::is_void_v<R>)
                call(parent);
            else
                promise.addResult(call(parent));
        } catch (...) {
            promise.setException(std::current_exception());
        }
        promise.finish();
    }

    Fn fn;
    QFutureInterface<R> child;
    QPromise<R> promise;
};

template <typename T, typename Fn>
QFuture<ThenResult<std::decay_t<Fn>, T>> startThen(const QFuture<T> &parent, Fn &&fn, Launcher launcher) {
    using F = std::decay_t<Fn>;
    using R = ThenResult<F, T>;
    static_assert(ThenInvoke<F, T>::byValue || ThenInvoke<F, T>::byFuture,
                  "the continuation must take the result of the future, or the future");
    QFutureInterface<R> child(QFutureInterfaceBase::Pending);
    auto runner = std::make_shared<ThenRunner<T, R, F>>(F(std::forward<Fn>(fn)), child);
    return attachContinuation<T, R>(parent, launcher, runner);
}

// Gives the results of a finished future, which was not canceled, to the promise.
template <typename T>
void forwardResults(QFuture<T> &parent, QPromise<T> &promise) {
    if constexpr (!std::is_void_v<T>) {
        const int n = parent.resultCount();
        for (int i = 0; i < n; ++i) {
            if constexpr (std::is_copy_constructible_v<T>)
                promise.addResult(parent.resultAt(i));
            else if (i == 0)
                promise.addResult(parent.takeResult());
        }
    }
}

template <typename T, typename Fn>
struct OnCanceledRunner {
    OnCanceledRunner(Fn f, const QFutureInterface<T> &c) : fn(std::move(f)), child(c), promise(c) {}

    void run(QFuture<T> parent) {
        promise.start();
        if (promise.isCanceled()) {
            promise.finish();
            return;
        }
        try {
            const std::exception_ptr ex = FutureAccess::exception(ContinuationAccess::iface(parent));
            if (!parent.isCanceled()) {
                forwardResults(parent, promise);
            } else if (ex) {
                promise.setException(ex);
            } else if constexpr (std::is_void_v<T>) {
                fn();
            } else {
                promise.addResult(fn());
            }
        } catch (...) {
            promise.setException(std::current_exception());
        }
        promise.finish();
    }

    Fn fn;
    QFutureInterface<T> child;
    QPromise<T> promise;
};

template <typename T, typename Fn>
QFuture<T> startOnCanceled(const QFuture<T> &parent, Fn &&fn, Launcher launcher) {
    using F = std::decay_t<Fn>;
    static_assert(std::is_invocable_v<F>, "the handler of onCanceled() takes no arguments");
    static_assert(std::is_convertible_v<std::invoke_result_t<F>, T>,
                  "the handler of onCanceled() must return the type of the future");
    QFutureInterface<T> child(QFutureInterfaceBase::Pending);
    auto runner = std::make_shared<OnCanceledRunner<T, F>>(F(std::forward<Fn>(fn)), child);
    return attachContinuation<T, T>(parent, launcher, runner);
}

// The exception type that a handler takes, or void if it takes none.
template <typename... A>
struct HandlerArgs;
template <>
struct HandlerArgs<> {
    using type = void;
};
template <typename A>
struct HandlerArgs<A> {
    using type = std::decay_t<A>;
};
template <typename F>
struct HandlerArg : HandlerArg<decltype(&F::operator())> {};
template <typename R, typename C, typename... A>
struct HandlerArg<R (C::*)(A...)> : HandlerArgs<A...> {};
template <typename R, typename C, typename... A>
struct HandlerArg<R (C::*)(A...) const> : HandlerArgs<A...> {};
template <typename R, typename C, typename... A>
struct HandlerArg<R (C::*)(A...) noexcept> : HandlerArgs<A...> {};
template <typename R, typename C, typename... A>
struct HandlerArg<R (C::*)(A...) const noexcept> : HandlerArgs<A...> {};
template <typename R, typename... A>
struct HandlerArg<R (*)(A...)> : HandlerArgs<A...> {};
template <typename R, typename... A>
struct HandlerArg<R (*)(A...) noexcept> : HandlerArgs<A...> {};

template <typename T, typename Fn>
struct OnFailedRunner {
    using Exception = typename HandlerArg<Fn>::type;

    OnFailedRunner(Fn f, const QFutureInterface<T> &c) : fn(std::move(f)), child(c), promise(c) {}

    template <typename... A>
    void handle(A &&...a) {
        if constexpr (std::is_void_v<T>)
            fn(std::forward<A>(a)...);
        else
            promise.addResult(fn(std::forward<A>(a)...));
    }

    void run(QFuture<T> parent) {
        promise.start();
        if (promise.isCanceled()) {
            promise.finish();
            return;
        }
        try {
            const std::exception_ptr ex = FutureAccess::exception(ContinuationAccess::iface(parent));
            if (!parent.isCanceled()) {
                forwardResults(parent, promise);
            } else if (!ex) {
                promise.future().cancel();
            } else if constexpr (std::is_void_v<Exception>) {
                handle();
            } else {
                try {
                    std::rethrow_exception(ex);
                } catch (const Exception &e) {
                    handle(e);
                } catch (...) {
                    promise.setException(ex);
                }
            }
        } catch (...) {
            promise.setException(std::current_exception());
        }
        promise.finish();
    }

    Fn fn;
    QFutureInterface<T> child;
    QPromise<T> promise;
};

template <typename T, typename Fn>
QFuture<T> startOnFailed(const QFuture<T> &parent, Fn &&fn, Launcher launcher) {
    using F = std::decay_t<Fn>;
    QFutureInterface<T> child(QFutureInterfaceBase::Pending);
    auto runner = std::make_shared<OnFailedRunner<T, F>>(F(std::forward<Fn>(fn)), child);
    return attachContinuation<T, T>(parent, launcher, runner);
}

// The type of the result of a future that is made from a signal with the arguments `A`.
template <typename... A>
struct SignalResult {
    using type = std::tuple<std::decay_t<A>...>;
};
template <>
struct SignalResult<> {
    using type = void;
};
template <typename A>
struct SignalResult<A> {
    using type = std::decay_t<A>;
};

template <typename Signal>
struct SignalFuture;

template <typename C, typename... A>
struct SignalFuture<void (C::*)(A...)> {
    using Result = typename SignalResult<A...>::type;

    template <typename Sender>
    static QFuture<Result> make(const Sender *sender, void (C::*signal)(A...)) {
        auto promise = std::make_shared<QPromise<Result>>();
        auto connection = std::make_shared<QMetaObject::Connection>();
        auto done = std::make_shared<std::atomic<bool>>(false);
        QFuture<Result> future = promise->future();
        promise->start();
        *connection = QObject::connect(sender, signal, [promise, connection, done](const A &...args) {
            if (done->exchange(true))
                return;
            if constexpr (sizeof...(A) == 0)
                (void)0;
            else if constexpr (sizeof...(A) == 1)
                promise->addResult(args...);
            else
                promise->emplaceResult(args...);
            promise->finish();
            QObject::disconnect(*connection);
        });
        return future;
    }
};

template <typename F>
struct FutureResultOf;
template <typename T>
struct FutureResultOf<QFuture<T>> {
    using type = T;
};

// The state of whenAll(): the result is made when the last future has finished.
template <typename Futures>
struct WhenAllContext {
    explicit WhenAllContext(Futures f, std::size_t count) : futures(std::move(f)), remaining(count) {}

    void oneDone() {
        if (remaining.fetch_sub(1) != 1)
            return;
        if (failed)
            promise.future().cancel();
        else
            promise.addResult(futures);
        promise.finish();
    }

    Futures futures;
    std::atomic<std::size_t> remaining;
    std::atomic<bool> failed{false};
    QPromise<Futures> promise;
};

template <typename Ctx, typename F>
void attachWhenAll(const std::shared_ptr<Ctx> &ctx, const F &future) {
    const bool set = FutureAccess::setContinuation(ContinuationAccess::iface(future),
                                                   [ctx](const QFutureInterfaceBase &) { ctx->oneDone(); });
    if (!set) {
        qWarning("QtFuture::whenAll: a continuation is already attached to a future");
        ctx->failed = true;
        ctx->oneDone();
    }
}

template <typename Tuple, std::size_t... I>
void attachWhenAllTuple(const std::shared_ptr<WhenAllContext<Tuple>> &ctx, std::index_sequence<I...>) {
    (attachWhenAll(ctx, std::get<I>(ctx->futures)), ...);
}

// The state of whenAny(): the first future to finish makes the result.
template <typename Result>
struct WhenAnyContext {
    std::atomic<bool> taken{false};
    QPromise<Result> promise;
};

template <typename Ctx, typename Make>
void attachWhenAny(const std::shared_ptr<Ctx> &ctx, bool *failed, Make make, const QFutureInterfaceBase &iface) {
    const bool set = FutureAccess::setContinuation(iface, [ctx, make](const QFutureInterfaceBase &) {
        if (!ctx->taken.exchange(true)) {
            ctx->promise.addResult(make());
            ctx->promise.finish();
        }
    });
    if (!set) {
        qWarning("QtFuture::whenAny: a continuation is already attached to a future");
        *failed = true;
    }
}

template <typename Variant, typename Tuple, std::size_t... I>
void attachWhenAnyTuple(const std::shared_ptr<WhenAnyContext<Variant>> &ctx, const Tuple &futures, bool *failed,
                        std::index_sequence<I...>) {
    (attachWhenAny(
         ctx, failed, [f = std::get<I>(futures)] { return Variant(std::in_place_index<I>, f); },
         ContinuationAccess::iface(std::get<I>(futures))),
     ...);
}

} // namespace QtPrivate

namespace QtFuture {

/*!
    \class QtFuture::WhenAnyResult
    \brief The WhenAnyResult struct is the result of whenAny() on a range of futures.

    It holds the first future that finished and its position in the range.
*/
template <typename T>
struct WhenAnyResult {
    /*!
        \variable QtFuture::WhenAnyResult::index

        The position of the future in the range.
    */
    qsizetype index = -1;
    /*!
        \variable QtFuture::WhenAnyResult::future

        The future that finished first.
    */
    QFuture<T> future;
};

/*!
    \fn template <typename... Futures> QFuture<std::tuple<std::decay_t<Futures>...>> QtFuture::whenAll(Futures &&...futures)

    Returns a future that finishes when all \a futures have finished. Its result is a tuple of them. A future
    takes one continuation, so the futures must not have one, and must not be given another afterwards.

    \sa whenAny()
*/
template <typename... Futures>
QFuture<std::tuple<std::decay_t<Futures>...>> whenAll(Futures &&...futures) {
    using Tuple = std::tuple<std::decay_t<Futures>...>;
    auto ctx = std::make_shared<QtPrivate::WhenAllContext<Tuple>>(Tuple(futures...), sizeof...(Futures));
    QFuture<Tuple> result = ctx->promise.future();
    ctx->promise.start();
    if constexpr (sizeof...(Futures) == 0) {
        ctx->oneDone();
    } else {
        QtPrivate::attachWhenAllTuple(ctx, std::index_sequence_for<Futures...>{});
    }
    return result;
}

/*!
    \fn template <typename InputIt> auto QtFuture::whenAll(InputIt first, InputIt last)

    \overload
    Returns a future that finishes when all the futures in the range from \a first to \a last have finished.
    Its result is a list of them.
*/
template <typename InputIt, typename = typename std::iterator_traits<InputIt>::value_type>
auto whenAll(InputIt first, InputIt last) -> QFuture<QList<typename std::iterator_traits<InputIt>::value_type>> {
    using List = QList<typename std::iterator_traits<InputIt>::value_type>;
    List list;
    for (; first != last; ++first)
        list.append(*first);
    const std::size_t count = static_cast<std::size_t>(list.size());
    auto ctx = std::make_shared<QtPrivate::WhenAllContext<List>>(list, count);
    QFuture<List> result = ctx->promise.future();
    ctx->promise.start();
    if (count == 0) {
        ctx->oneDone();
        return result;
    }
    for (const auto &f : list)
        QtPrivate::attachWhenAll(ctx, f);
    return result;
}

/*!
    \fn template <typename... Futures> QFuture<std::variant<std::decay_t<Futures>...>> QtFuture::whenAny(Futures &&...futures)

    Returns a future that finishes when the first of \a futures has finished. Its result is a \c std::variant that
    holds that future. A future takes one continuation, so the futures must not have one, and must not be given
    another afterwards.

    \sa whenAll()
*/
template <typename... Futures>
QFuture<std::variant<std::decay_t<Futures>...>> whenAny(Futures &&...futures) {
    using Variant = std::variant<std::decay_t<Futures>...>;
    static_assert(sizeof...(Futures) > 0, "whenAny() needs at least one future");
    auto ctx = std::make_shared<QtPrivate::WhenAnyContext<Variant>>();
    QFuture<Variant> result = ctx->promise.future();
    ctx->promise.start();
    bool failed = false;
    QtPrivate::attachWhenAnyTuple(ctx, std::make_tuple(futures...), &failed, std::index_sequence_for<Futures...>{});
    if (failed)
        result.cancel();
    return result;
}

/*!
    \fn template <typename InputIt> auto QtFuture::whenAny(InputIt first, InputIt last)

    \overload
    Returns a future that finishes when the first future in the range from \a first to \a last has finished. Its
    result is a WhenAnyResult. The range must not be empty.
*/
template <typename InputIt, typename = typename std::iterator_traits<InputIt>::value_type>
auto whenAny(InputIt first, InputIt last) {
    using F = typename std::iterator_traits<InputIt>::value_type;
    using Result = WhenAnyResult<typename QtPrivate::FutureResultOf<F>::type>;
    auto ctx = std::make_shared<QtPrivate::WhenAnyContext<Result>>();
    QFuture<Result> result = ctx->promise.future();
    ctx->promise.start();
    bool failed = false;
    qsizetype index = 0;
    for (; first != last; ++first, ++index) {
        const F f = *first;
        QtPrivate::attachWhenAny(
            ctx, &failed, [index, f] { return Result{index, f}; }, QtPrivate::ContinuationAccess::iface(f));
    }
    if (failed || index == 0)
        result.cancel();
    return result;
}

/*!
    \fn template <typename T> QFuture<std::decay_t<T>> QtFuture::makeReadyValueFuture(T &&value)

    Returns a finished future with \a value as its only result.

    \sa makeReadyVoidFuture(), makeReadyRangeFuture()
*/
template <typename T>
QFuture<std::decay_t<T>> makeReadyValueFuture(T &&value) {
    QPromise<std::decay_t<T>> promise;
    promise.start();
    promise.addResult(std::forward<T>(value));
    promise.finish();
    return promise.future();
}

/*!
    \fn inline QFuture<void> QtFuture::makeReadyVoidFuture()

    Returns a finished \c{QFuture<void>}.

    \sa makeReadyValueFuture()
*/
inline QFuture<void> makeReadyVoidFuture() {
    QPromise<void> promise;
    promise.start();
    promise.finish();
    return promise.future();
}

/*!
    \fn template <typename Container> QFuture<typename std::decay_t<Container>::value_type> QtFuture::makeReadyRangeFuture(Container &&values)

    Returns a finished future with the elements of \a values as its results.

    \sa makeReadyValueFuture()
*/
template <typename Container>
QFuture<typename std::decay_t<Container>::value_type> makeReadyRangeFuture(Container &&values) {
    using T = typename std::decay_t<Container>::value_type;
    QList<T> list;
    for (const auto &v : values)
        list.append(v);
    QPromise<T> promise;
    promise.start();
    promise.addResults(list);
    promise.finish();
    return promise.future();
}

/*!
    \fn template <typename T> QFuture<T> QtFuture::makeReadyRangeFuture(std::initializer_list<T> values)

    \overload
    Returns a finished future with the elements of \a values as its results.
*/
template <typename T>
QFuture<T> makeReadyRangeFuture(std::initializer_list<T> values) {
    return makeReadyRangeFuture(QList<T>(values));
}

/*!
    \fn template <typename T = void> QFuture<T> QtFuture::makeExceptionalFuture(std::exception_ptr exception)

    Returns a finished and canceled future that carries \a exception.

    \sa makeReadyValueFuture()
*/
template <typename T = void>
QFuture<T> makeExceptionalFuture(std::exception_ptr exception) {
    QPromise<T> promise;
    promise.start();
    promise.setException(std::move(exception));
    promise.finish();
    return promise.future();
}

/*!
    \fn template <typename T = void> QFuture<T> QtFuture::makeExceptionalFuture(const QException &exception)

    \overload
    Returns a finished and canceled future that carries a copy of \a exception.
*/
template <typename T = void>
QFuture<T> makeExceptionalFuture(const QException &exception) {
    QPromise<T> promise;
    promise.start();
    promise.setException(exception);
    promise.finish();
    return promise.future();
}

/*!
    \fn template <typename Sender, typename Signal> auto QtFuture::connect(Sender *sender, Signal signal)

    Returns a future that becomes finished the first time \a sender emits \a signal. The result is the argument of
    the signal, a \c std::tuple of its arguments if there are several, and there is none if it has none.
    The future is canceled if \a sender is destroyed first.
*/
template <typename Sender, typename Signal>
auto connect(const Sender *sender, Signal signal) -> QFuture<typename QtPrivate::SignalFuture<Signal>::Result> {
    return QtPrivate::SignalFuture<Signal>::make(sender, signal);
}

} // namespace QtFuture
