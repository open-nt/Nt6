// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

// The native handle of a window, as QWindow::winId() and QWidget::winId() return it: an X11 window id on Linux.
using WId = quintptr;
