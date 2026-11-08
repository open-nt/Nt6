// MIT License - see LICENSE
#pragma once

#include <QtGui/qfont.h>

namespace QtPrivate {

// The default font that QFont() copies and QGuiApplication::setFont() replaces.
QFont applicationFont();
void setApplicationFont(const QFont &font);

} // namespace QtPrivate
