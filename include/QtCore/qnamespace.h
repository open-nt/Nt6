// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

/*!
    \namespace Qt
    \inmodule NtCore
    \brief The Qt namespace contains miscellaneous identifiers used throughout Nt.

    It holds the enums shared by several classes, such as Qt::CaseSensitivity
    and Qt::TimeSpec.

    \differences
    Only the enums listed here exist. Qt's other \c Qt:: enums, such as the
    key, mouse and widget enums, belong to modules Nt does not provide.
    \enddifferences
*/
namespace Qt {

/*!
    \enum Qt::AspectRatioMode

    This enum describes how to scale a rectangle or size to fit another.

    \value IgnoreAspectRatio The size is scaled freely; the aspect ratio is not preserved.
    \value KeepAspectRatio The size is scaled to the largest rectangle that fits inside the given size, preserving the aspect ratio.
    \value KeepAspectRatioByExpanding The size is scaled to the smallest rectangle that contains the given size, preserving the aspect ratio.

    \sa QSize::scaled()
*/
enum AspectRatioMode {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

/*!
    \enum Qt::DateFormat

    This enum selects the textual format of dates and times.

    \value TextDate A locale-independent text form such as \c{Sat Nov 7 2026}; the default.
    \value ISODate ISO 8601 extended format, such as \c{2026-11-07} and \c{20:10:14}.
    \value ISODateWithMs ISO 8601 extended format with milliseconds, such as \c{20:10:14.844}.
    \value RFC2822Date The RFC 2822 format, such as \c{07 Nov 2026}.

    The \c toString() and \c fromString() functions of \c QDate, \c QTime
    and \c QDateTime accept this enum.
*/
enum DateFormat {
    TextDate,
    ISODate = 1,
    ISODateWithMs = 9,
    RFC2822Date = 8
};

/*!
    \enum Qt::TimeSpec

    This enum describes how a date and time is to be interpreted.

    \value LocalTime The local time of the system.
    \value UTC Coordinated Universal Time.
    \value OffsetFromUTC A fixed offset in seconds from UTC.
    \value TimeZone Declared for compatibility; Nt has no time zone database and QDateTime does not support it.

    The \c QDateTime class uses this enum.
*/
enum TimeSpec {
    LocalTime,
    UTC,
    OffsetFromUTC,
    TimeZone
};

/*!
    \enum Qt::ConnectionType

    This enum describes how a signal is delivered to a slot.

    \value AutoConnection The default. The slot is called directly if the receiver lives in the thread that emits the signal, and queued otherwise. The decision is made at emission time.
    \value DirectConnection The slot is called immediately when the signal is emitted, in the emitting thread.
    \value QueuedConnection The arguments are copied and the slot is called later from the event loop of the receiver's thread.
    \value BlockingQueuedConnection Like \c QueuedConnection, but the emitting thread blocks until the slot has returned. If the receiver lives in the emitting thread, the slot is called directly instead of deadlocking.

    \sa QObject::connect(), {Signals and Slots}
*/
enum ConnectionType { AutoConnection = 0, DirectConnection = 1, QueuedConnection = 2, BlockingQueuedConnection = 3 };

/*!
    \enum Qt::CaseSensitivity

    This enum selects whether a string comparison or search distinguishes
    upper and lower case.

    \value CaseInsensitive Case is ignored.
    \value CaseSensitive Case is significant.
*/
enum CaseSensitivity {
    CaseInsensitive = 0,
    CaseSensitive = 1
};

/*!
    \enum Qt::SplitBehaviorFlags

    This enum controls whether empty parts are kept when splitting a string.
    The SplitBehavior type is a typedef for \c{QFlags<SplitBehaviorFlags>}.

    \value KeepEmptyParts Empty parts are kept in the result.
    \value SkipEmptyParts Empty parts are left out of the result.
*/
enum SplitBehaviorFlags {
    KeepEmptyParts = 0,
    SkipEmptyParts = 1
};
Q_DECLARE_FLAGS(SplitBehavior, SplitBehaviorFlags)

/*!
    \enum Qt::TimerType

    This enum describes the accuracy a timer should have.

    \value PreciseTimer Accepted for compatibility; it has no effect.
    \value CoarseTimer Accepted for compatibility; it has no effect.
    \value VeryCoarseTimer Accepted for compatibility; it has no effect.

    \sa QBasicTimer, QTimer
*/
enum TimerType {
    PreciseTimer,
    CoarseTimer,
    VeryCoarseTimer
};

/*!
    \enum Qt::FindChildOption

    This enum controls how QObject::findChild() and QObject::findChildren()
    search. The FindChildOptions type is a typedef for
    \c{QFlags<FindChildOption>}.

    \value FindDirectChildrenOnly Only direct children are searched.
    \value FindChildrenRecursively All descendants are searched.
*/
enum FindChildOption {
    FindDirectChildrenOnly = 0,
    FindChildrenRecursively = 1
};
Q_DECLARE_FLAGS(FindChildOptions, FindChildOption)

/*!
    \enum Qt::DayOfWeek

    This enum names the days of the week, numbered as in ISO 8601 and as
    QDate::dayOfWeek() returns them.

    \value Monday Day 1.
    \value Tuesday Day 2.
    \value Wednesday Day 3.
    \value Thursday Day 4.
    \value Friday Day 5.
    \value Saturday Day 6.
    \value Sunday Day 7.

    \sa QLocale::firstDayOfWeek(), QLocale::weekdays()
*/
enum DayOfWeek {
    Monday = 1,
    Tuesday = 2,
    Wednesday = 3,
    Thursday = 4,
    Friday = 5,
    Saturday = 6,
    Sunday = 7
};

/*!
    \enum Qt::LayoutDirection

    This enum gives the direction in which text is laid out.

    \value LeftToRight Text runs from left to right.
    \value RightToLeft Text runs from right to left.
    \value LayoutDirectionAuto The direction is taken from the text itself.

    \sa QLocale::textDirection()
*/
enum LayoutDirection {
    LeftToRight,
    RightToLeft,
    LayoutDirectionAuto
};

/*!
    \typedef Qt::HANDLE

    An opaque thread handle: the value QThread::currentThreadId() returns, a \c{void *}.
*/
using HANDLE = void *;

} // namespace Qt
