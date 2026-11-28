// MIT License - see LICENSE
#pragma once

// The position source of Android (qgeopositioninfosource_android.cpp), which the registry of
// QGeoPositionInfoSource holds as "android" from the start.

#include <QtPositioning/qgeopositioninfosource.h>

namespace QtPositioningPrivate {

QGeoPositionInfoSource *createAndroidSource(const QVariantMap &parameters, QObject *parent);

} // namespace QtPositioningPrivate
