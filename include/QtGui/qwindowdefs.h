// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qlist.h>

// The classes that Qt declares here, so that code that names them through <QWidget> alone compiles as with Qt.
class QApplication;
class QBrush;
class QByteArray;
class QColor;
class QCursor;
class QDialog;
class QFont;
class QFontMetrics;
class QImage;
class QPaintDevice;
class QPainter;
class QPalette;
class QPen;
class QPixmap;
class QPoint;
class QPolygon;
class QRect;
class QRegion;
class QSize;
class QString;
class QTime;
class QTimer;
class QWidget;
class QWindow;

// The native handle of a window, as QWindow::winId() and QWidget::winId() return it: an X11 window id on Linux.
using WId = quintptr;

// A list of widgets, as QApplication::topLevelWidgets() returns.
using QWidgetList = QList<QWidget *>;
// A list of windows.
using QWindowList = QList<QWindow *>;
