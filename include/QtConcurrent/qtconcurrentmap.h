// MIT License - see LICENSE
#pragma once

#include <QtConcurrent/qtconcurrent_p.h>

namespace QtConcurrent {

/*!
    \fn template <typename Sequence, typename MapFunctor> QFuture<void> QtConcurrent::map(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function)

    Calls \a function once for each item of \a sequence, in the threads of \a pool, and returns
    a future for the end of it. The items are changed in place: \a function takes a reference to
    an item, and what it returns is ignored. It may be a member function of the item type.

    \code
    QList<QImage> images = load();
    QFuture<void> done = QtConcurrent::map(&pool, images, [](QImage &image) { image.invertPixels(); });
    \endcode

    \a sequence is not copied, so it has to stay alive, and untouched by other threads, until
    the future finishes. The future reports the number of items handled as its progress, and can
    be canceled and suspended.

    \since 6.0
    \sa blockingMap(), mapped(), mappedReduced()
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<void> map(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function) {
    return QtPrivate::startMap(pool, std::begin(sequence), std::end(sequence), std::forward<MapFunctor>(function),
                               false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QFuture<void> QtConcurrent::map(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in place, in the threads of \a
    pool.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<void> map(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::startMap(pool, begin, end, std::forward<MapFunctor>(function), false);
}

/*!
    \fn template <typename Sequence, typename MapFunctor> QFuture<void> QtConcurrent::map(Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in place, in the threads of the global
    QThreadPool.
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<void> map(Sequence &&sequence, MapFunctor &&function) {
    return QtPrivate::startMap(nullptr, std::begin(sequence), std::end(sequence), std::forward<MapFunctor>(function),
                               false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QFuture<void> QtConcurrent::map(Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in place, in the threads of the
    global QThreadPool.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<void> map(Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::startMap(nullptr, begin, end, std::forward<MapFunctor>(function), false);
}

/*!
    \fn template <typename Sequence, typename MapFunctor> void QtConcurrent::blockingMap(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function)

    Calls \a function once for each item of \a sequence, in the threads of \a pool and in the
    calling thread, and returns when all items have been handled. The items are changed in
    place. An exception that \a function throws is thrown again here.

    \since 6.0
    \sa map(), blockingMapped()
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
void blockingMap(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function) {
    QtPrivate::startMap(pool, std::begin(sequence), std::end(sequence), std::forward<MapFunctor>(function), true)
        .waitForFinished();
}

/*!
    \fn template <typename Iterator, typename MapFunctor> void QtConcurrent::blockingMap(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in place, in the threads of \a pool
    and in the calling thread, and waits for it.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
void blockingMap(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function) {
    QtPrivate::startMap(pool, begin, end, std::forward<MapFunctor>(function), true).waitForFinished();
}

/*!
    \fn template <typename Sequence, typename MapFunctor> void QtConcurrent::blockingMap(Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in place, in the threads of the global
    QThreadPool and in the calling thread, and waits for it.
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
void blockingMap(Sequence &&sequence, MapFunctor &&function) {
    QtPrivate::startMap(nullptr, std::begin(sequence), std::end(sequence), std::forward<MapFunctor>(function), true)
        .waitForFinished();
}

/*!
    \fn template <typename Iterator, typename MapFunctor> void QtConcurrent::blockingMap(Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in place, in the threads of the
    global QThreadPool and in the calling thread, and waits for it.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
void blockingMap(Iterator begin, Iterator end, MapFunctor &&function) {
    QtPrivate::startMap(nullptr, begin, end, std::forward<MapFunctor>(function), true).waitForFinished();
}

/*!
    \fn template <typename Sequence, typename MapFunctor> QFuture<MapResult> QtConcurrent::mapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function)

    Calls \a function once for each item of \a sequence, in the threads of \a pool, and returns
    a future with what the calls return, in the order of the items. \a function takes an item,
    by value or by const reference, and may be a const member function of the item type. The
    result type of the future is the decayed return type of \a function.

    \code
    QFuture<QImage> thumbnails = QtConcurrent::mapped(&pool, images, [](const QImage &image) { return image.scaled(100, 100); });
    \endcode

    \a sequence is copied, or moved if it is an rvalue, into the computation, so it does not
    have to outlive the call. Results become available as they are computed: QFuture::resultAt()
    waits for one of them.

    \since 6.0
    \sa blockingMapped(), map(), mappedReduced()
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>>
mapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMapped(pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
                                  false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QFuture<MapResult> QtConcurrent::mapped(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of \a pool, and
    returns a future with the results in order. The range has to stay valid until the future
    finishes.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<QtPrivate::MapResult<MapFunctor, Iterator>> mapped(QThreadPool *pool, Iterator begin, Iterator end,
                                                           MapFunctor &&function) {
    return QtPrivate::startMapped(pool, nullptr, begin, end, std::forward<MapFunctor>(function), false);
}

/*!
    \fn template <typename Sequence, typename MapFunctor> QFuture<MapResult> QtConcurrent::mapped(Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in the threads of the global QThreadPool,
    and returns a future with the results in order.
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
QFuture<QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>> mapped(Sequence &&sequence,
                                                                                       MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMapped(nullptr, held, std::cbegin(*held), std::cend(*held),
                                  std::forward<MapFunctor>(function), false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QFuture<MapResult> QtConcurrent::mapped(Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of the global
    QThreadPool, and returns a future with the results in order. The range has to stay valid
    until the future finishes.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QFuture<QtPrivate::MapResult<MapFunctor, Iterator>> mapped(Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::startMapped(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function), false);
}

/*!
    \fn template <typename OutputSequence, typename Sequence, typename MapFunctor> OutputSequence QtConcurrent::blockingMapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function)

    Calls \a function once for each item of \a sequence, in the threads of \a pool and in the
    calling thread, and returns the results, in the order of the items, in a container of type
    \c OutputSequence. The container needs \c push_back() or a one-argument \c insert(). An
    exception that \a function throws is thrown again here.

    \code
    std::vector<int> lengths = QtConcurrent::blockingMapped<std::vector<int>>(&pool, words, &QString::size);
    \endcode

    \since 6.0
    \sa mapped(), blockingMap()
*/
template <typename OutputSequence, typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
OutputSequence blockingMapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<OutputSequence>(QtPrivate::startMapped(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename OutputSequence, typename Iterator, typename MapFunctor> OutputSequence QtConcurrent::blockingMapped(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of \a pool and in
    the calling thread, and returns the results in an \c OutputSequence.
*/
template <typename OutputSequence, typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
OutputSequence blockingMapped(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::collectResults<OutputSequence>(
        QtPrivate::startMapped(pool, nullptr, begin, end, std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename OutputSequence, typename Sequence, typename MapFunctor> OutputSequence QtConcurrent::blockingMapped(Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in the threads of the global QThreadPool and
    in the calling thread, and returns the results in an \c OutputSequence.
*/
template <typename OutputSequence, typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
OutputSequence blockingMapped(Sequence &&sequence, MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<OutputSequence>(QtPrivate::startMapped(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename OutputSequence, typename Iterator, typename MapFunctor> OutputSequence QtConcurrent::blockingMapped(Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of the global
    QThreadPool and in the calling thread, and returns the results in an \c OutputSequence.
*/
template <typename OutputSequence, typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
OutputSequence blockingMapped(Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::collectResults<OutputSequence>(
        QtPrivate::startMapped(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename Sequence, typename MapFunctor> auto QtConcurrent::blockingMapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in the threads of \a pool and in the calling
    thread, and returns the results in a container like \a sequence that holds the mapped type:
    a \c{QList<int>} gives a \c{QList<QString>}, for example. A sequence that is not a template
    with the item type as its first argument gives a QList.
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
typename QtPrivate::RebindSequence<std::decay_t<Sequence>,
                                   QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>>::type
blockingMapped(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<typename QtPrivate::RebindSequence<
        std::decay_t<Sequence>, QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>>::type>(
        QtPrivate::startMapped(pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
                               true));
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QList<MapResult> QtConcurrent::blockingMapped(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of \a pool and in
    the calling thread, and returns the results in a QList.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QList<QtPrivate::MapResult<MapFunctor, Iterator>> blockingMapped(QThreadPool *pool, Iterator begin, Iterator end,
                                                                 MapFunctor &&function) {
    return QtPrivate::collectResults<QList<QtPrivate::MapResult<MapFunctor, Iterator>>>(
        QtPrivate::startMapped(pool, nullptr, begin, end, std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename Sequence, typename MapFunctor> auto QtConcurrent::blockingMapped(Sequence &&sequence, MapFunctor &&function)

    \overload

    Calls \a function for each item of \a sequence, in the threads of the global QThreadPool and
    in the calling thread, and returns the results in a container like \a sequence that holds
    the mapped type: a \c{QList<int>} gives a \c{QList<QString>}, for example. A sequence that
    is not a template with the item type as its first argument gives a QList.
*/
template <typename Sequence, typename MapFunctor, QtPrivate::IfSequence<Sequence> = true>
typename QtPrivate::RebindSequence<std::decay_t<Sequence>,
                                   QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>>::type
blockingMapped(Sequence &&sequence, MapFunctor &&function) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::collectResults<typename QtPrivate::RebindSequence<
        std::decay_t<Sequence>, QtPrivate::MapResult<MapFunctor, QtPrivate::ConstIteratorOf<Sequence>>>::type>(
        QtPrivate::startMapped(nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
                               true));
}

/*!
    \fn template <typename Iterator, typename MapFunctor> QList<MapResult> QtConcurrent::blockingMapped(Iterator begin, Iterator end, MapFunctor &&function)

    \overload

    Calls \a function for each item from \a begin to \a end, in the threads of the global
    QThreadPool and in the calling thread, and returns the results in a QList.
*/
template <typename Iterator, typename MapFunctor, QtPrivate::IfIterator<Iterator> = true>
QList<QtPrivate::MapResult<MapFunctor, Iterator>> blockingMapped(Iterator begin, Iterator end, MapFunctor &&function) {
    return QtPrivate::collectResults<QList<QtPrivate::MapResult<MapFunctor, Iterator>>>(
        QtPrivate::startMapped(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function), true));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    Calls \a function once for each item of \a sequence, in the threads of \a pool, and combines
    what the calls return into one value with \a reduceFunction. Returns a future for that
    value, whose type is \c ResultType. \a reduceFunction is called as \c{reduceFunction(result,
    intermediate)} with a reference to the value and one result of \a function; it is never
    called by two threads at a time. \a options says whether the results are reduced in the
    order of the items or as they come. The value starts out value-initialized, or as \c
    initialValue in the overloads that take one.

    \code
    QFuture<int> total = QtConcurrent::mappedReduced<int>(&pool, words, &QString::size, [](int &sum, qsizetype n) { sum += int(n); });
    \endcode

    The overloads without a \c ResultType template argument take it from the first parameter of
    \a reduceFunction, or from its class if it is a member function. That does not work for a
    generic lambda or an overloaded function object, which need the explicit form. \a sequence
    is copied, or moved, into the computation.

    \since 6.0
    \sa blockingMappedReduced(), mapped(), filteredReduced(), ReduceOption
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool, and reduces the
    results with \a reduceFunction as \a options says, starting from \a initialValue.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool, and
    reduces the results with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool, and
    reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false,
                                                     std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool,
    and reduces the results with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool,
    and reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool, and reduces the results with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool, and reduces the results with \a reduceFunction as \a options says, starting
    from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false,
                                                     std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool, and reduces the
    results with \a reduceFunction as \a options says. The result type is taken from \a
    reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool, and reduces the
    results with \a reduceFunction as \a options says, starting from \a initialValue. The result
    type is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
        pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool, and
    reduces the results with \a reduceFunction as \a options says. The result type is taken from
    \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool, and
    reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                  ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false,
                                                     std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool,
    and reduces the results with \a reduceFunction as \a options says. The result type is taken
    from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
QFuture<ResultType> mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool,
    and reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
        nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
        std::forward<ReduceFunctor>(reduceFunction), options, false, std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor> QFuture<ResultType> QtConcurrent::mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool, and reduces the results with \a reduceFunction as \a options says. The result
    type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
QFuture<ResultType> mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false);
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> QFuture<ResultType> QtConcurrent::mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool, and reduces the results with \a reduceFunction as \a options says, starting
    from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
QFuture<ResultType> mappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                  InitialValueType &&initialValue,
                                  ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, false,
                                                     std::forward<InitialValueType>(initialValue));
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    Calls \a function once for each item of \a sequence, in the threads of \a pool and in the
    calling thread, combines what the calls return into one value of type \c ResultType with \a
    reduceFunction, and returns that value. \a options says in which order the results are
    reduced. An exception that one of the functions throws is thrown again here.

    \since 6.0
    \sa mappedReduced(), blockingMapped()
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool and in the calling
    thread, and reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
               pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool and in
    the calling thread, and reduces the results with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool and in
    the calling thread, and reduces the results with \a reduceFunction as \a options says,
    starting from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true,
                                                     std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool and
    in the calling thread, and reduces the results with \a reduceFunction as \a options says.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfSequence<Sequence> = true>
ResultType blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool and
    in the calling thread, and reduces the results with \a reduceFunction as \a options says,
    starting from \a initialValue.
*/
template <typename ResultType, typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
               nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool and in the calling thread, and reduces the results with \a reduceFunction as \a
    options says.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          QtPrivate::IfIterator<Iterator> = true>
ResultType blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool and in the calling thread, and reduces the results with \a reduceFunction as \a
    options says, starting from \a initialValue.
*/
template <typename ResultType, typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename InitialValueType, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true,
                                                     std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool and in the calling
    thread, and reduces the results with \a reduceFunction as \a options says. The result type
    is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(pool, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of \a pool and in the calling
    thread, and reduces the results with \a reduceFunction as \a options says, starting from \a
    initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Sequence &&sequence, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
               pool, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool and in
    the calling thread, and reduces the results with \a reduceFunction as \a options says. The
    result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of \a pool and in
    the calling thread, and reduces the results with \a reduceFunction as \a options says,
    starting from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(QThreadPool *pool, Iterator begin, Iterator end, MapFunctor &&function,
                                 ReduceFunctor &&reduceFunction, InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(pool, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true,
                                                     std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool and
    in the calling thread, and reduces the results with \a reduceFunction as \a options says.
    The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true>
ResultType blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(nullptr, held, std::cbegin(*held), std::cend(*held),
                                                     std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items of \a sequence with \a function, in the threads of the global QThreadPool and
    in the calling thread, and reduces the results with \a reduceFunction as \a options says,
    starting from \a initialValue. The result type is taken from \a reduceFunction.
*/
template <typename Sequence, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfSequence<Sequence> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(Sequence &&sequence, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    auto held = QtPrivate::holdSequence(std::forward<Sequence>(sequence));
    return QtPrivate::startMappedReduced<ResultType>(
               nullptr, held, std::cbegin(*held), std::cend(*held), std::forward<MapFunctor>(function),
               std::forward<ReduceFunctor>(reduceFunction), options, true, std::forward<InitialValueType>(initialValue))
        .result();
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor> ResultType QtConcurrent::blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool and in the calling thread, and reduces the results with \a reduceFunction as \a
    options says. The result type is taken from \a reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true>
ResultType blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true)
        .result();
}

/*!
    \fn template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType> ResultType QtConcurrent::blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction, InitialValueType &&initialValue, ReduceOptions options)

    \overload

    Maps the items from \a begin to \a end with \a function, in the threads of the global
    QThreadPool and in the calling thread, and reduces the results with \a reduceFunction as \a
    options says, starting from \a initialValue. The result type is taken from \a
    reduceFunction.
*/
template <typename Iterator, typename MapFunctor, typename ReduceFunctor, typename InitialValueType,
          typename ResultType = QtPrivate::ReduceResult<ReduceFunctor>, QtPrivate::IfIterator<Iterator> = true,
          QtPrivate::IfInitialValue<InitialValueType, ResultType> = true>
ResultType blockingMappedReduced(Iterator begin, Iterator end, MapFunctor &&function, ReduceFunctor &&reduceFunction,
                                 InitialValueType &&initialValue,
                                 ReduceOptions options = ReduceOptions(UnorderedReduce | SequentialReduce)) {
    return QtPrivate::startMappedReduced<ResultType>(nullptr, nullptr, begin, end, std::forward<MapFunctor>(function),
                                                     std::forward<ReduceFunctor>(reduceFunction), options, true,
                                                     std::forward<InitialValueType>(initialValue))
        .result();
}

} // namespace QtConcurrent
