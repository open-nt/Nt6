// MIT License - see LICENSE
#pragma once

// Nt Test for programs that use Nt Widgets: everything of qtest_gui.h, with the overloads that take a QWidget.
// Included by <QtTest> when QT_WIDGETS_LIB is defined, as it is for a target that links Nt::Widgets.
// Including this header says that Nt Widgets is there, so the QWidget overloads are declared even when the build
// did not define the macro.
#ifndef QT_WIDGETS_LIB
#define QT_WIDGETS_LIB
#endif

#include <QtWidgets/qwidget.h>

#include <QtTest/qtest_gui.h>
