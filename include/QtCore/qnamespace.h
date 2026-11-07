// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

namespace Qt {

enum AspectRatioMode {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

enum DateFormat {
    TextDate,
    ISODate = 1,
    ISODateWithMs = 9,
    RFC2822Date = 8
};

enum TimeSpec {
    LocalTime,
    UTC,
    OffsetFromUTC,
    TimeZone
};

enum ConnectionType {
    AutoConnection = 0,
    DirectConnection = 1,
    QueuedConnection = 2
};

enum CaseSensitivity {
    CaseInsensitive = 0,
    CaseSensitive = 1
};

enum SplitBehaviorFlags {
    KeepEmptyParts = 0,
    SkipEmptyParts = 1
};
Q_DECLARE_FLAGS(SplitBehavior, SplitBehaviorFlags)

enum TimerType {
    PreciseTimer,
    CoarseTimer,
    VeryCoarseTimer
};

enum FindChildOption {
    FindDirectChildrenOnly = 0,
    FindChildrenRecursively = 1
};
Q_DECLARE_FLAGS(FindChildOptions, FindChildOption)

} // namespace Qt
