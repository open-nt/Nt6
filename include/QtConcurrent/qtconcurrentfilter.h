// MIT License - see LICENSE
#pragma once

#include <QtConcurrent/qtconcurrentthreadengine.h>

namespace QtConcurrent {

/*!
    \fn template <typename Sequence, typename KeepFunctor> QFuture<void> QtConcurrent::filter(QThreadPool *pool, Sequence &sequence, KeepFunctor &&filterFunction)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool, and
    removes the items for which it returns \c false. Returns a future for the end of it. \a
    filterFunction takes a const reference to an item and may be a const member function of the
    item type.

    \code
    QStringList words = load();
    QFuture<void> done = QtConcurrent::filter(&pool, words, [](const QString &word) { return !word.isEmpty(); });
    \endcode

    \a sequence is not copied: it has to stay alive, and untouched by other threads, until the
    future finishes. It is assigned once, at the end, with the items that are kept in their
    order; a computation that is canceled or that throws leaves it as it was. The sequence type
    needs a default constructor and \c push_back() or a one-argument \c insert().

    \since 6.0
    \sa blockingFilter(), filtered(), filteredReduced()
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<void> filter(QThreadPool *pool, Sequence &sequence, KeepFunctor &&filterFunction) {
    return QtPrivate::startFilter(pool, sequence, std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> QFuture<void> QtConcurrent::filter(Sequence &sequence, KeepFunctor &&filterFunction)

    \overload

    Filters \a sequence in place with \a filterFunction, in the threads of the global
    QThreadPool.
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<void> filter(Sequence &sequence, KeepFunctor &&filterFunction) {
    return QtPrivate::startFilter(nullptr, sequence, std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> void QtConcurrent::blockingFilter(QThreadPool *pool, Sequence &sequence, KeepFunctor &&filterFunction)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool and in
    the calling thread, and removes the items for which it returns \c false before it returns.
    An exception that \a filterFunction throws is thrown again here, and \a sequence is then
    unchanged.

    \since 6.0
    \sa filter(), blockingFiltered()
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
void blockingFilter(QThreadPool *pool, Sequence &sequence, KeepFunctor &&filterFunction) {
    QtPrivate::startFilter(pool, sequence, std::forward<KeepFunctor>(filterFunction), true).waitForFinished();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> void QtConcurrent::blockingFilter(Sequence &sequence, KeepFunctor &&filterFunction)

    \overload

    Filters \a sequence in place with \a filterFunction, in the threads of the global
    QThreadPool and in the calling thread, and waits for it.
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
void blockingFilter(Sequence &sequence, KeepFunctor &&filterFunction) {
    QtPrivate::startFilter(nullptr, sequence, std::forward<KeepFunctor>(filterFunction), true).waitForFinished();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> QFuture<Item> QtConcurrent::filtered(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool, and
    returns a future with copies of the items for which it returns \c true, in their order. \a
    filterFunction takes a const reference to an item and may be a const member function of the
    item type.

    \code
    QFuture<QString> longWords = QtConcurrent::filtered(&pool, words, [](const QString &word) { return word.size() > 7; });
    \endcode

    \a sequence is copied, or moved if it is an rvalue, into the computation. The results become
    available in order while the computation runs.

    \since 6.0
    \sa blockingFiltered(), filter(), filteredReduced()
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<QtPrivate::ItemOf<QtPrivate::ConstIteratorOf<Sequence>>> filtered(QThreadPool *pool, Sequence &&sequence,
                                                                          KeepFunctor &&filterFunction) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFiltered(pool, held, std::cbegin(*held), std::cend(*held),
                                    std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Iterator, typename KeepFunctor> QFuture<Item> QtConcurrent::filtered(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool,
    and returns a future with the items that are kept. The range has to stay valid until the
    future finishes.
*/
template <typename Iterator, typename KeepFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<QtPrivate::ItemOf<Iterator>> filtered(QThreadPool *pool, Iterator begin, Iterator end,
                                              KeepFunctor &&filterFunction) {
    return QtPrivate::startFiltered(pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> QFuture<Item> QtConcurrent::filtered(Sequence &&sequence, KeepFunctor &&filterFunction)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool, and returns a future with the items that are kept.
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<QtPrivate::ItemOf<QtPrivate::ConstIteratorOf<Sequence>>> filtered(Sequence &&sequence,
                                                                          KeepFunctor &&filterFunction) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFiltered(nullptr, held, std::cbegin(*held), std::cend(*held),
                                    std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Iterator, typename KeepFunctor> QFuture<Item> QtConcurrent::filtered(Iterator begin, Iterator end, KeepFunctor &&filterFunction)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool, and returns a future with the items that are kept. The range has to stay
    valid until the future finishes.
*/
template <typename Iterator, typename KeepFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<QtPrivate::ItemOf<Iterator>> filtered(Iterator begin, Iterator end, KeepFunctor &&filterFunction) {
    return QtPrivate::startFiltered(nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction), false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> std::decay_t<Sequence> QtConcurrent::blockingFiltered(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool and in
    the calling thread, and returns a sequence of the same type with the items for which it
    returns \c true, in their order. An exception that \a filterFunction throws is thrown again
    here.

    \since 6.0
    \sa filtered(), blockingFilter()
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
std::decay_t<Sequence> blockingFiltered(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<std::decay_t<Sequence>>(QtPrivate::startFiltered(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction), true));
}

/*!
    \fn template <typename OutputSequence, typename Iterator, typename KeepFunctor> OutputSequence QtConcurrent::blockingFiltered(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool
    and in the calling thread, and returns the items that are kept in an \c OutputSequence,
    which has to be named.
*/
template <typename OutputSequence, typename Iterator, typename KeepFunctor, QtPrivate::IfIterator<Iterator> = true>
OutputSequence blockingFiltered(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction) {
    return QtPrivate::collectResults<OutputSequence>(
        QtPrivate::startFiltered(pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction), true));
}

/*!
    \fn template <typename Sequence, typename KeepFunctor> std::decay_t<Sequence> QtConcurrent::blockingFiltered(Sequence &&sequence, KeepFunctor &&filterFunction)

    \overload

    Filters \a sequence with \a filterFunction, in the threads of the global QThreadPool and in
    the calling thread, and returns the items that are kept.
*/
template <typename Sequence, typename KeepFunctor, QtPrivate::IfSequence<Sequence> = true>
std::decay_t<Sequence> blockingFiltered(Sequence &&sequence, KeepFunctor &&filterFunction) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<std::decay_t<Sequence>>(QtPrivate::startFiltered(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction), true));
}

/*!
    \fn template <typename OutputSequence, typename Iterator, typename KeepFunctor> OutputSequence QtConcurrent::blockingFiltered(Iterator begin, Iterator end, KeepFunctor &&filterFunction)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool and in the calling thread, and returns the items that are kept in an \c
    OutputSequence, which has to be named.
*/
template <typename OutputSequence, typename Iterator, typename KeepFunctor, QtPrivate::IfIterator<Iterator> = true>
OutputSequence blockingFiltered(Iterator begin, Iterator end, KeepFunctor &&filterFunction) {
    return QtPrivate::collectResults<OutputSequence>(
        QtPrivate::startFiltered(nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction), true));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool, and
    combines the items for which it returns \c true into one value with \a reduceFunction.
    Returns a future for that value, whose type is \c ResultType. \a reduceFunction is called as
    \c{reduceFunction(result, item)}, by one thread at a time, and \a options says whether the
    items are reduced in their order or as they come. The value starts out value-initialized, or
    as \c initialValue in the overloads that take one.

    \code
    QFuture<int> sumOfEven = QtConcurrent::filteredReduced<int>(&pool, numbers, [](int n) { return n % 2 == 0; }, [](int &sum, int n) { sum += n; });
    \endcode

    The overloads without a \c ResultType template argument take it from the first parameter of
    \a reduceFunction, or from its class if it is a member function. \a sequence is copied, or
    moved, into the computation.

    \since 6.0
    \sa blockingFilteredReduced(), filtered(), mappedReduced(), ReduceOption
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool, and
    reduces the items that are kept with \a reduceFunction as \a options says, starting from \a
    initialValue.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool,
    and reduces the items that are kept with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(pool, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool,
    and reduces the items that are kept with \a reduceFunction as \a options says, starting from
    \a initialValue.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
        pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options says,
    starting from \a initialValue.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                    InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options
    says.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options
    says, starting from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
        nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool, and
    reduces the items that are kept with \a reduceFunction as \a options says. The result type
    is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool, and
    reduces the items that are kept with \a reduceFunction as \a options says, starting from \a
    initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool,
    and reduces the items that are kept with \a reduceFunction as \a options says. The result
    type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(pool, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool,
    and reduces the items that are kept with \a reduceFunction as \a options says, starting from
    \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
        pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options says.
    The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options says,
    starting from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                    InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options
    says. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool, and reduces the items that are kept with \a reduceFunction as \a options
    says, starting from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> filteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                    ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                    ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
        nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    Calls \a filterFunction once for each item of \a sequence, in the threads of \a pool and in
    the calling thread, combines the items for which it returns \c true into one value of type
    \c ResultType with \a reduceFunction, and returns that value. \a options says in which order
    the items are reduced. An exception that one of the functions throws is thrown again here.

    \since 6.0
    \sa filteredReduced(), blockingFiltered()
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool and in
    the calling thread, and reduces the items that are kept with \a reduceFunction as \a options
    says, starting from \a initialValue.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
               pool, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool
    and in the calling thread, and reduces the items that are kept with \a reduceFunction as \a
    options says.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(pool, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool
    and in the calling thread, and reduces the items that are kept with \a reduceFunction as \a
    options says, starting from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
               pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
ResultType blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says, starting from \a initialValue.
*/
template <typename ResultType, typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                   InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
               nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
ResultType blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says, starting from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
               nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool and in
    the calling thread, and reduces the items that are kept with \a reduceFunction as \a options
    says. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of \a pool and in
    the calling thread, and reduces the items that are kept with \a reduceFunction as \a options
    says, starting from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Sequence &&sequence, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
               pool, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool
    and in the calling thread, and reduces the items that are kept with \a reduceFunction as \a
    options says. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(pool, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of \a pool
    and in the calling thread, and reduces the items that are kept with \a reduceFunction as \a
    options says, starting from \a initialValue. The result type is taken from \a
    reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(QThreadPool *pool, Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
               pool, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
ResultType blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items of \a sequence with \a filterFunction, in the threads of the global
    QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says, starting from \a initialValue. The result type is taken
    from \a reduceFunction.
*/
template <typename Sequence, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(Sequence &&sequence, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction,
                                   InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startFilteredReduced<ResultType>(
               nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
ResultType blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(nullptr, nullptr, begin, end,
                                                       std::forward<KeepFunctor>(filterFunction),
                                                       std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Filters the items from \a begin to \a end with \a filterFunction, in the threads of the
    global QThreadPool and in the calling thread, and reduces the items that are kept with \a
    reduceFunction as \a options says, starting from \a initialValue. The result type is taken
    from \a reduceFunction.
*/
template <typename Iterator, typename KeepFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingFilteredReduced(Iterator begin, Iterator end, KeepFunctor &&filterFunction,
                                   ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                   ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startFilteredReduced<ResultType>(
               nullptr, nullptr, begin, end, std::forward<KeepFunctor>(filterFunction),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

} // namespace QtConcurrent
