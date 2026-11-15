// MIT License - see LICENSE
#pragma once

// Nt Test for programs that use Nt GUI: the test macros, and functions that simulate the mouse, the keyboard and
// the wheel in a QWindow and wait for windows. Included by <QtTest> when QT_GUI_LIB is defined, as it is for a
// target that links Nt::Gui.
#include <QtGui/qtestsupport_gui.h>
#include <QtTest/qtest.h>
#include <QtTest/qtestkeyboard.h>
#include <QtTest/qtestmouse.h>
#include <QtTest/qtestwheel.h>
