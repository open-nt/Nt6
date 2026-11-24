// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtGui/qcolor.h>

class QColormap {
public:
    enum Mode { Direct, Indexed, Gray };

    static void initialize() {}
    static void cleanup() {}
    static QColormap instance(int screen = -1);

    Mode mode() const { return Direct; }
    int depth() const { return 32; }
    int size() const { return -1; }

    uint pixel(const QColor &color) const;
    QColor colorAt(uint pixel) const;
    QList<QColor> colormap() const { return QList<QColor>(); }

private:
    QColormap() = default;
};
