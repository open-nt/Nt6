// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetatype.h>

namespace QtPrivate {

QMetaType::ConverterFunction findRegisteredConverter(QMetaType from, QMetaType to);
bool convertBuiltin(QMetaType from, const void *src, QMetaType to, void *dst);
bool canConvertBuiltin(QMetaType from, QMetaType to);

// The text QVariant and QMetaType use for numbers: the shortest form that reads back unchanged.
QString numberToString(double value);
QString numberToString(float value);

} // namespace QtPrivate
