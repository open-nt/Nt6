// MIT License - see LICENSE
#pragma once

#include <QtCore/qversionnumber.h>

class QLibraryInfo {
public:
    QLibraryInfo() = delete;

    static QVersionNumber version() noexcept;
    static const char *build() noexcept;
    static bool isDebugBuild() noexcept;
    static bool isSharedBuild() noexcept;
};
