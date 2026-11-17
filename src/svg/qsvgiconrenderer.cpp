// MIT License - see LICENSE
//
// Draws SVG files for QIcon. Nt GUI cannot depend on Nt SVG, so Nt SVG installs this renderer when the program
// starts. The linker keeps this file in a static link through nt_svg_icon_renderer, which Nt::Svg asks for.

#include "qicon_p.h"

#include <QtGui/qpainter.h>
#include <QtSvg/qsvgrenderer.h>

namespace {

QImage renderSvgIcon(const QString &fileName, const QSize &size) {
    QSvgRenderer renderer(fileName);
    if (!renderer.isValid() || size.isEmpty())
        return QImage();
    // As large as fits in `size`, keeping the proportions of the document.
    const QSizeF natural = renderer.defaultSize().isEmpty() ? QSizeF(size) : QSizeF(renderer.defaultSize());
    const QSize fitted = natural.scaled(QSizeF(size), Qt::KeepAspectRatio).toSize().expandedTo(QSize(1, 1));
    QImage image(fitted, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(fitted)));
    return image;
}

struct Installer {
    Installer() { QtPrivate::setScalableIconRenderer(renderSvgIcon); }
};

const Installer installer;

} // namespace

// The symbol that makes the linker keep this file, and with it the installer, in a static link.
extern "C" int nt_svg_icon_renderer; // NOLINT(readability-identifier-naming)
int nt_svg_icon_renderer = 1;        // NOLINT(readability-identifier-naming)
