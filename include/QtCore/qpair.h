// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <utility>

// QPair is std::pair, as in Qt 6; qMakePair() is in <QtGlobal>.
template <typename T1, typename T2> using QPair = std::pair<T1, T2>;
