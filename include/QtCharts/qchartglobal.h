// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

// Qt 6 puts the chart classes in the global namespace and keeps these macros for code written for Qt 5.
#define QT_CHARTS_BEGIN_NAMESPACE
#define QT_CHARTS_END_NAMESPACE
#define QT_CHARTS_USE_NAMESPACE
#define QT_CHARTS_NAMESPACE

namespace QtChartsPrivate {
class ChartEngine;
}
