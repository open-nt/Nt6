// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

namespace QtConcurrent {

/*!
    \enum QtConcurrent::ReduceOption

    This enum describes the order in which the reduce function of mappedReduced() and
    filteredReduced() gets the intermediate results. The reduce function is never called by two
    threads at a time, whatever the option.

    \value UnorderedReduce The results are reduced in the order in which the threads produce them.
    \value OrderedReduce The results are reduced in the order of the items in the sequence.
    \value SequentialReduce One thread at a time enters the reduce function. This is always the
    case, so the value only exists to be combined with one of the other two.
*/
enum ReduceOption { UnorderedReduce = 0x1, OrderedReduce = 0x2, SequentialReduce = 0x4 };
Q_DECLARE_FLAGS(ReduceOptions, ReduceOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(ReduceOptions)

} // namespace QtConcurrent
