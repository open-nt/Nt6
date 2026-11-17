// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>

namespace QtPrivate {

// Renders the scalable image file `fileName`, such as an SVG file, at `size`; a null image if it cannot. Nt SVG
// installs one, so that QIcon can show SVG files without Nt GUI depending on Nt SVG.
using ScalableIconRenderer = QImage (*)(const QString &fileName, const QSize &size);
void setScalableIconRenderer(ScalableIconRenderer renderer);

} // namespace QtPrivate
