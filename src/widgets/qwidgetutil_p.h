// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>

#include <algorithm>

namespace QtPrivate {

inline QSize expandedTo(const QSize &a, const QSize &b) {
    return QSize(std::max(a.width(), b.width()), std::max(a.height(), b.height()));
}

inline QSize boundedTo(const QSize &a, const QSize &b) {
    return QSize(std::min(a.width(), b.width()), std::min(a.height(), b.height()));
}

} // namespace QtPrivate
