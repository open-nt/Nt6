// MIT License - see LICENSE
#pragma once

// Internals of Nt Concurrent, shared by its public headers. Not part of the API.

#include <QtConcurrent/qtconcurrentreducekit.h>
#include <QtCore/qfuture.h>
#include <QtCore/qfutureinterface.h>
#include <QtCore/qlist.h>
#include <QtCore/qpromise.h>
#include <QtCore/qthreadpool.h>

#include <cstddef>
#include <exception>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace QtPrivate {

// A computation over the items 0 .. count - 1, run in blocks by several threads of a pool.
struct ConcurrentJob {
    // Shares its state with the QFutureInterface<T> that the results go to.
    QFutureInterfaceBase future;
    // The global pool if null.
    QThreadPool *pool = nullptr;
    qsizetype count = 0;
    // Handles the items begin .. end - 1; called by several threads at a time, for distinct blocks.
    std::function<void(qsizetype begin, qsizetype end)> runBlock;
    // Called once, after the last block, unless the job was canceled or threw.
    std::function<void()> finish;
};

// Reports the future as started and runs the job in the pool. With `blocking`, the calling thread
// works too, so that a job started from a thread of a busy pool still makes progress; the caller
// then waits for the future.
void startConcurrentJob(ConcurrentJob job, bool blocking);

template <typename T, typename = void> struct IsIterator : std::false_type {};
template <typename T>
struct IsIterator<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> : std::true_type {};
template <typename T> using IfIterator = std::enable_if_t<IsIterator<std::decay_t<T>>::value, bool>;

template <typename T, typename = void> struct IsSequence : std::false_type {};
template <typename T>
struct IsSequence<T, std::void_t<decltype(std::begin(std::declval<T &>())), decltype(std::end(std::declval<T &>()))>>
    : std::true_type {};
template <typename T> using IfSequence = std::enable_if_t<IsSequence<std::remove_reference_t<T>>::value, bool>;

template <typename Sequence>
using ConstIteratorOf = decltype(std::cbegin(std::declval<const std::decay_t<Sequence> &>()));
template <typename Iterator> using ItemOf = std::decay_t<decltype(*std::declval<Iterator &>())>;
template <typename Map, typename Iterator>
using MapResult = std::decay_t<std::invoke_result_t<std::decay_t<Map> &, decltype(*std::declval<Iterator &>())>>;
// A reduce option is not an initial value, even where it would convert to the result type.
template <typename From, typename To>
using IfInitialValue = std::enable_if_t<std::is_convertible_v<From, To> &&
                                            !std::is_same_v<std::decay_t<From>, QtConcurrent::ReduceOption> &&
                                            !std::is_same_v<std::decay_t<From>, QtConcurrent::ReduceOptions>,
                                        bool>;

// The type of the first parameter of a function, a member function or a function object whose call
// operator is neither overloaded nor a template; void if it cannot be told.
template <typename F, typename = void> struct CallableFirstParam {
    using type = void;
};
template <typename R, typename A0, typename... A> struct CallableFirstParam<R (*)(A0, A...), void> {
    using type = A0;
};
template <typename R, typename A0, typename... A> struct CallableFirstParam<R (*)(A0, A...) noexcept, void> {
    using type = A0;
};
template <typename R, typename C, typename A0, typename... A> struct CallableFirstParam<R (C::*)(A0, A...), void> {
    using type = A0;
};
template <typename R, typename C, typename A0, typename... A>
struct CallableFirstParam<R (C::*)(A0, A...) const, void> {
    using type = A0;
};
template <typename R, typename C, typename A0, typename... A>
struct CallableFirstParam<R (C::*)(A0, A...) noexcept, void> {
    using type = A0;
};
template <typename R, typename C, typename A0, typename... A>
struct CallableFirstParam<R (C::*)(A0, A...) const noexcept, void> {
    using type = A0;
};
template <typename F>
struct CallableFirstParam<F, std::void_t<decltype(&F::operator())>> : CallableFirstParam<decltype(&F::operator())> {};

// The result type of a reduce function: the type that its first parameter refers to, or the class
// of a member function. Has no `type` if it cannot be told, as for a generic lambda.
template <typename F, typename = void> struct ReduceResultOf {};
template <typename F>
struct ReduceResultOf<F, std::enable_if_t<!std::is_member_function_pointer_v<F> &&
                                          std::is_lvalue_reference_v<typename CallableFirstParam<F>::type>>> {
    using type = std::remove_reference_t<typename CallableFirstParam<F>::type>;
};
template <typename R, typename C, typename... A> struct ReduceResultOf<R (C::*)(A...), void> {
    using type = C;
};
template <typename R, typename C, typename... A> struct ReduceResultOf<R (C::*)(A...) noexcept, void> {
    using type = C;
};
template <typename Reduce> using ReduceResult = typename ReduceResultOf<std::decay_t<Reduce>>::type;

// T if P is QPromise<T> &.
template <typename P> struct PromiseParam {
    static constexpr bool value = false;
    using type = void;
};
template <typename T> struct PromiseParam<QPromise<T> &> {
    static constexpr bool value = true;
    using type = T;
};

// The container that blockingMapped() returns when it is not named: the input container with the
// mapped type, or a QList if the input is not a template of that shape.
template <typename Sequence, typename U> struct RebindSequence {
    using type = QList<U>;
};
template <template <typename...> class C, typename T, typename... Rest, typename U>
struct RebindSequence<C<T, Rest...>, U> {
    using type = C<U>;
};

template <typename Container, typename V>
auto appendTo(Container &c, V &&v, int) -> decltype(c.push_back(std::forward<V>(v)), void()) {
    c.push_back(std::forward<V>(v));
}
template <typename Container, typename V> void appendTo(Container &c, V &&v, long) { c.insert(std::forward<V>(v)); }

// The items of a range by index. Iterators that are not random access are looked up in a table.
template <typename Iterator> class ConcurrentRange {
    static constexpr bool kRandom =
        std::is_base_of_v<std::random_access_iterator_tag, typename std::iterator_traits<Iterator>::iterator_category>;

public:
    ConcurrentRange(Iterator begin, Iterator end)
        : m_begin(begin), m_count(static_cast<qsizetype>(std::distance(begin, end))) {
        if (!kRandom) {
            m_at.reserve(static_cast<std::size_t>(m_count));
            for (Iterator it = begin; it != end; ++it)
                m_at.push_back(it);
        }
    }
    qsizetype count() const { return m_count; }
    Iterator at(qsizetype i) const {
        if constexpr (kRandom)
            return m_begin + static_cast<typename std::iterator_traits<Iterator>::difference_type>(i);
        else
            return m_at[static_cast<std::size_t>(i)];
    }

private:
    Iterator m_begin;
    qsizetype m_count;
    std::vector<Iterator> m_at;
};

// Hands blocks to a sink one thread at a time, and, if `ordered`, in the order of the items.
template <typename Block> class BlockSequencer {
public:
    explicit BlockSequencer(bool ordered) : m_ordered(ordered) {}

    template <typename Sink> void add(qsizetype begin, qsizetype end, Block &&block, Sink &&sink) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!m_ordered) {
            sink(block);
            return;
        }
        if (begin != m_next) {
            m_pending.emplace(begin, std::make_pair(end, std::move(block)));
            return;
        }
        sink(block);
        m_next = end;
        for (auto it = m_pending.find(m_next); it != m_pending.end(); it = m_pending.find(m_next)) {
            sink(it->second.second);
            m_next = it->second.first;
            m_pending.erase(it);
        }
    }

private:
    std::mutex m_mutex;
    const bool m_ordered;
    qsizetype m_next = 0;
    std::map<qsizetype, std::pair<qsizetype, Block>> m_pending;
};

// Keeps a copy of a sequence alive for as long as a job reads it.
template <typename Sequence> std::shared_ptr<const std::decay_t<Sequence>> holdSequence(Sequence &&sequence) {
    return std::make_shared<const std::decay_t<Sequence>>(std::forward<Sequence>(sequence));
}

template <typename Iterator, typename Map>
QFuture<void> startMap(QThreadPool *pool, Iterator begin, Iterator end, Map &&map, bool blocking) {
    struct State {
        State(Iterator b, Iterator e, Map &&m) : range(b, e), map(std::forward<Map>(m)) {}
        ConcurrentRange<Iterator> range;
        std::decay_t<Map> map;
    };
    auto s = std::make_shared<State>(begin, end, std::forward<Map>(map));
    QFutureInterface<void> iface;
    ConcurrentJob job;
    job.future = iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        for (qsizetype i = b; i < e; ++i)
            std::invoke(s->map, *s->range.at(i));
    };
    startConcurrentJob(std::move(job), blocking);
    return iface.future();
}

template <typename Iterator, typename Map>
QFuture<MapResult<Map, Iterator>> startMapped(QThreadPool *pool, std::shared_ptr<const void> keep, Iterator begin,
                                              Iterator end, Map &&map, bool blocking) {
    using U = MapResult<Map, Iterator>;
    struct State {
        State(std::shared_ptr<const void> k, Iterator b, Iterator e, Map &&m)
            : keep(std::move(k)), range(b, e), map(std::forward<Map>(m)) {}
        std::shared_ptr<const void> keep;
        ConcurrentRange<Iterator> range;
        std::decay_t<Map> map;
        QFutureInterface<U> iface;
    };
    auto s = std::make_shared<State>(std::move(keep), begin, end, std::forward<Map>(map));
    ConcurrentJob job;
    job.future = s->iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        for (qsizetype i = b; i < e; ++i)
            s->iface.reportAndEmplaceResult(static_cast<int>(i), std::invoke(s->map, *s->range.at(i)));
    };
    startConcurrentJob(std::move(job), blocking);
    return s->iface.future();
}

template <typename Result, typename Iterator, typename Map, typename Reduce, typename... Initial>
QFuture<Result> startMappedReduced(QThreadPool *pool, std::shared_ptr<const void> keep, Iterator begin, Iterator end,
                                   Map &&map, Reduce &&reduce, QtConcurrent::ReduceOptions options, bool blocking,
                                   Initial &&...initial) {
    using U = MapResult<Map, Iterator>;
    struct State {
        State(std::shared_ptr<const void> k, Iterator b, Iterator e, Map &&m, Reduce &&r, bool ordered,
              Initial &&...init)
            : keep(std::move(k)), range(b, e), map(std::forward<Map>(m)), reduce(std::forward<Reduce>(r)),
              blocks(ordered), result(std::forward<Initial>(init)...) {}
        std::shared_ptr<const void> keep;
        ConcurrentRange<Iterator> range;
        std::decay_t<Map> map;
        std::decay_t<Reduce> reduce;
        BlockSequencer<std::vector<U>> blocks;
        Result result;
        QFutureInterface<Result> iface;
    };
    auto s = std::make_shared<State>(std::move(keep), begin, end, std::forward<Map>(map), std::forward<Reduce>(reduce),
                                     options.testFlag(QtConcurrent::OrderedReduce), std::forward<Initial>(initial)...);
    ConcurrentJob job;
    job.future = s->iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        std::vector<U> mapped;
        mapped.reserve(static_cast<std::size_t>(e - b));
        for (qsizetype i = b; i < e; ++i)
            mapped.push_back(std::invoke(s->map, *s->range.at(i)));
        s->blocks.add(b, e, std::move(mapped), [&s](std::vector<U> &block) {
            for (auto &&value : block)
                std::invoke(s->reduce, s->result, static_cast<const U &>(value));
        });
    };
    job.finish = [s] { s->iface.reportAndEmplaceResult(-1, std::move(s->result)); };
    startConcurrentJob(std::move(job), blocking);
    return s->iface.future();
}

template <typename Sequence, typename Keep>
QFuture<void> startFilter(QThreadPool *pool, Sequence &sequence, Keep &&keep, bool blocking) {
    using Iterator = ConstIteratorOf<Sequence>;
    using V = ItemOf<Iterator>;
    struct State {
        State(Sequence &seq, Keep &&k)
            : sequence(seq), range(std::cbegin(std::as_const(seq)), std::cend(std::as_const(seq))),
              keep(std::forward<Keep>(k)) {}
        Sequence &sequence;
        ConcurrentRange<Iterator> range;
        std::decay_t<Keep> keep;
        BlockSequencer<std::vector<V>> blocks{true};
        std::vector<V> kept;
    };
    auto s = std::make_shared<State>(sequence, std::forward<Keep>(keep));
    QFutureInterface<void> iface;
    ConcurrentJob job;
    job.future = iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        std::vector<V> kept;
        for (qsizetype i = b; i < e; ++i) {
            const auto it = s->range.at(i);
            if (std::invoke(s->keep, *it))
                kept.push_back(*it);
        }
        s->blocks.add(b, e, std::move(kept), [&s](std::vector<V> &block) {
            for (auto &&value : block)
                s->kept.push_back(std::move(value));
        });
    };
    job.finish = [s] {
        Sequence filtered;
        for (auto &&value : s->kept)
            appendTo(filtered, std::move(value), 0);
        s->sequence = std::move(filtered);
    };
    startConcurrentJob(std::move(job), blocking);
    return iface.future();
}

template <typename Iterator, typename Keep>
QFuture<ItemOf<Iterator>> startFiltered(QThreadPool *pool, std::shared_ptr<const void> hold, Iterator begin,
                                        Iterator end, Keep &&keep, bool blocking) {
    using V = ItemOf<Iterator>;
    struct State {
        State(std::shared_ptr<const void> h, Iterator b, Iterator e, Keep &&k)
            : hold(std::move(h)), range(b, e), keep(std::forward<Keep>(k)) {}
        std::shared_ptr<const void> hold;
        ConcurrentRange<Iterator> range;
        std::decay_t<Keep> keep;
        BlockSequencer<QList<V>> blocks{true};
        QFutureInterface<V> iface;
    };
    auto s = std::make_shared<State>(std::move(hold), begin, end, std::forward<Keep>(keep));
    ConcurrentJob job;
    job.future = s->iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        QList<V> kept;
        for (qsizetype i = b; i < e; ++i) {
            const auto it = s->range.at(i);
            if (std::invoke(s->keep, *it))
                kept.append(*it);
        }
        s->blocks.add(b, e, std::move(kept), [&s](QList<V> &block) {
            if (!block.isEmpty())
                s->iface.reportResults(block);
        });
    };
    startConcurrentJob(std::move(job), blocking);
    return s->iface.future();
}

template <typename Result, typename Iterator, typename Keep, typename Reduce, typename... Initial>
QFuture<Result> startFilteredReduced(QThreadPool *pool, std::shared_ptr<const void> hold, Iterator begin, Iterator end,
                                     Keep &&keep, Reduce &&reduce, QtConcurrent::ReduceOptions options, bool blocking,
                                     Initial &&...initial) {
    using V = ItemOf<Iterator>;
    struct State {
        State(std::shared_ptr<const void> h, Iterator b, Iterator e, Keep &&k, Reduce &&r, bool ordered,
              Initial &&...init)
            : hold(std::move(h)), range(b, e), keep(std::forward<Keep>(k)), reduce(std::forward<Reduce>(r)),
              blocks(ordered), result(std::forward<Initial>(init)...) {}
        std::shared_ptr<const void> hold;
        ConcurrentRange<Iterator> range;
        std::decay_t<Keep> keep;
        std::decay_t<Reduce> reduce;
        BlockSequencer<std::vector<Iterator>> blocks;
        Result result;
        QFutureInterface<Result> iface;
    };
    auto s =
        std::make_shared<State>(std::move(hold), begin, end, std::forward<Keep>(keep), std::forward<Reduce>(reduce),
                                options.testFlag(QtConcurrent::OrderedReduce), std::forward<Initial>(initial)...);
    ConcurrentJob job;
    job.future = s->iface;
    job.pool = pool;
    job.count = s->range.count();
    job.runBlock = [s](qsizetype b, qsizetype e) {
        std::vector<Iterator> kept;
        for (qsizetype i = b; i < e; ++i) {
            const auto it = s->range.at(i);
            if (std::invoke(s->keep, *it))
                kept.push_back(it);
        }
        s->blocks.add(b, e, std::move(kept), [&s](std::vector<Iterator> &block) {
            for (const Iterator &it : block)
                std::invoke(s->reduce, s->result, static_cast<const V &>(*it));
        });
    };
    job.finish = [s] { s->iface.reportAndEmplaceResult(-1, std::move(s->result)); };
    startConcurrentJob(std::move(job), blocking);
    return s->iface.future();
}

// Waits for `future` and returns its results in a container of the caller's choice.
template <typename Output, typename U> Output collectResults(const QFuture<U> &future) {
    QFuture<U> f = future;
    f.waitForFinished();
    Output out;
    const int n = f.resultCount();
    for (int i = 0; i < n; ++i)
        appendTo(out, f.resultAt(i), 0);
    return out;
}

template <typename Fn, typename Promise, typename Object, typename... Args>
void invokeWithPromise(std::true_type, Fn &fn, Promise &promise, Object &&object, Args &&...args) {
    std::invoke(fn, std::forward<Object>(object), promise, std::forward<Args>(args)...);
}
template <typename Fn, typename Promise, typename... Args>
void invokeWithPromise(std::false_type, Fn &fn, Promise &promise, Args &&...args) {
    std::invoke(fn, promise, std::forward<Args>(args)...);
}

// Runs fn(args...) in the pool. A function whose first parameter is a QPromise<T> & gets a promise
// and makes a QFuture<T>; any other function makes a future of what it returns.
template <typename Function, typename... Args>
auto concurrentRun(QThreadPool *pool, int priority, Function &&function, Args &&...args) {
    using Fn = std::decay_t<Function>;
    using Stored = std::tuple<std::decay_t<Args>...>;
    using First = typename CallableFirstParam<Fn>::type;
    if (!pool)
        pool = QThreadPool::globalInstance();
    if constexpr (PromiseParam<First>::value) {
        using T = typename PromiseParam<First>::type;
        QFutureInterface<T> iface;
        iface.setThreadPool(pool);
        iface.reportStarted();
        pool->start(
            [iface, fn = Fn(std::forward<Function>(function)), stored = Stored(std::forward<Args>(args)...)]() mutable {
                QPromise<T> promise(iface);
                if (!promise.isCanceled()) {
                    try {
                        std::apply(
                            [&](auto &...a) {
                                invokeWithPromise(std::is_member_function_pointer<Fn>(), fn, promise, std::move(a)...);
                            },
                            stored);
                    } catch (...) {
                        iface.reportException(std::current_exception());
                    }
                }
                promise.finish();
            },
            priority);
        return iface.future();
    } else {
        using R = std::invoke_result_t<Fn, std::decay_t<Args>...>;
        using T = std::conditional_t<std::is_void_v<R>, void, std::decay_t<R>>;
        QFutureInterface<T> iface;
        iface.setThreadPool(pool);
        iface.reportStarted();
        pool->start(
            [iface, fn = Fn(std::forward<Function>(function)), stored = Stored(std::forward<Args>(args)...)]() mutable {
                if (!iface.isCanceled()) {
                    try {
                        if constexpr (std::is_void_v<R>)
                            std::apply(std::move(fn), std::move(stored));
                        else
                            iface.reportAndEmplaceResult(-1, std::apply(std::move(fn), std::move(stored)));
                    } catch (...) {
                        iface.reportException(std::current_exception());
                    }
                }
                iface.reportFinished();
            },
            priority);
        return iface.future();
    }
}

} // namespace QtPrivate
