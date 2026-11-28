// MIT License - see LICENSE
#pragma once

// <QtCore/qnativeinterface.h> of Qt 6, so code that includes it compiles. In Qt it holds the machinery for
// declaring native interfaces (QT_DECLARE_NATIVE_INTERFACE, nativeInterface<T>()), which Nt does not have; the
// native interfaces Nt has, such as QNativeInterface::QAndroidApplication on Android, are declared in
// qcoreapplication_platform.h, which this header includes.
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qglobal.h>
