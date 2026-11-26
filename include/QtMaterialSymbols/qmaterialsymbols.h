// MIT License - see LICENSE
//
// The code of Nt Material Symbols is MIT licensed. The icons that it holds are Google's Material Symbols, under the
// Apache License 2.0: see third_party/material-symbols. Nt only.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qcolor.h>
#include <QtGui/qicon.h>
#include <QtGui/qpixmap.h>

class QMaterialSymbols {
public:
    // The names of the symbols that the package holds, in order.
    static QStringList names();
    static bool hasSymbol(const QString &name);

    // The SVG file of a symbol as Google publishes it; empty if there is none of that name.
    static QByteArray svg(const QString &name, bool filled = false);
    static QPixmap pixmap(const QString &name, int extent, const QColor &color = QColor(), bool filled = false,
                          qreal devicePixelRatio = 1.0);
    static QIcon icon(const QString &name, const QColor &color = QColor(), bool filled = false);

    QMaterialSymbols() = delete;
};
