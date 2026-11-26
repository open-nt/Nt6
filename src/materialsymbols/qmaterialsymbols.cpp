// MIT License - see LICENSE
//
// The code is MIT licensed. The icons that the build compiles into this library are Google's Material Symbols, under
// the Apache License 2.0: see third_party/material-symbols.
#include <QtMaterialSymbols/qmaterialsymbols.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qresource.h>
#include <QtGui/qpainter.h>
#include <QtSvg/qsvgrenderer.h>

#include <algorithm>

/*!
    \module NtMaterialSymbols
    \title Nt Material Symbols
    \brief Google's Material Symbols icons, in a package of their own (Nt only, with Apache 2.0 icons).

    The icons are \b not MIT licensed: they are Google's Material Symbols, under the Apache License 2.0. A program
    that links with this library distributes them, and so has to do what that license asks, among it to give its users
    a copy of the license. The rest of Nt does not use this library; the files \c third_party/material-symbols have
    the license and where the icons come from.

    \code
    find_package(Nt6 REQUIRED COMPONENTS Widgets MaterialSymbols)
    target_link_libraries(app PRIVATE Nt6::Widgets Nt6::MaterialSymbols)
    \endcode
*/

/*!
    \class QMaterialSymbols
    \inmodule NtMaterialSymbols
    \since 6.0
    \brief The QMaterialSymbols class gives Material Symbols as icons, in any color and size (Nt only).

    A symbol is asked for by its name, as Google lists it at \c fonts.google.com/icons, and comes outlined or filled:

    \code
    auto *button = new QToolButton;
    button->setIcon(QMaterialSymbols::icon("search", button->palette().color(QPalette::ButtonText)));
    save->setIcon(QMaterialSymbols::icon("favorite", Qt::red, true));
    \endcode

    The icons are under the Apache License 2.0, unlike the rest of Nt (see \l{Nt Material Symbols}).

    \differences
    Qt has no such class. The package holds a selection of the symbols, in the outlined style at weight 400, not the
    whole set, the other styles or the variable font. An icon keeps the color that it was made with: it does not
    follow the style or a change of the palette.
    \enddifferences
*/

// A static library's resources are there only once something asks for them. Q_INIT_RESOURCE declares the function that
// registers them where it is used, which must be outside every namespace.
static void initResources() { Q_INIT_RESOURCE(ntmaterialsymbols); }

namespace {

void initialize() {
    static const bool done = [] {
        initResources();
        return true;
    }();
    (void)done;
}

constexpr const char *folder = ":/nt/material-symbols/";
const char *const outlinedEnd = "_24px.svg";
const char *const filledEnd = "_fill1_24px.svg";

} // namespace

/*!
    Returns the names of the symbols that the package holds, in order.

    \sa hasSymbol()
*/
QStringList QMaterialSymbols::names() {
    initialize();
    QStringList found;
    const QStringList files = QDir(QString::fromLatin1(folder)).entryList(QDir::Files);
    for (const QString &file : files) {
        if (file.endsWith(QLatin1String(outlinedEnd)) && !file.endsWith(QLatin1String(filledEnd)))
            found.append(file.left(file.size() - static_cast<qsizetype>(qstrlen(outlinedEnd))));
    }
    std::sort(found.begin(), found.end());
    return found;
}

/*!
    Returns \c true if the package holds a symbol called \a name.

    \sa names()
*/
bool QMaterialSymbols::hasSymbol(const QString &name) { return !svg(name).isEmpty(); }

/*!
    Returns the SVG file of the symbol \a name, outlined, or filled if \a filled is \c true, exactly as Google
    publishes it. It is empty if there is no such symbol.
*/
QByteArray QMaterialSymbols::svg(const QString &name, bool filled) {
    initialize();
    // A name is letters, digits and underscores: nothing that could reach another file.
    const bool plain = !name.isEmpty() && std::all_of(name.begin(), name.end(), [](QChar c) {
        return c.isLetterOrNumber() || c == QLatin1Char('_');
    });
    if (!plain)
        return QByteArray();
    QFile file(QString::fromLatin1(folder) + name + QString::fromLatin1(filled ? filledEnd : outlinedEnd));
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

/*!
    Returns the symbol \a name drawn \a extent wide and high in logical pixels, for a screen with
    \a devicePixelRatio device pixels to each, in \a color; an invalid color leaves the symbol black, as it is
    published. \a filled chooses the filled form. The pixmap is null if there is no such symbol or \a extent is not
    positive.
*/
QPixmap QMaterialSymbols::pixmap(const QString &name, int extent, const QColor &color, bool filled,
                                 qreal devicePixelRatio) {
    QByteArray data = svg(name, filled);
    if (data.isEmpty() || extent <= 0 || devicePixelRatio <= 0)
        return QPixmap();
    // The paths of a symbol say no color of their own.
    if (color.isValid()) {
        const QByteArray fill = "<path fill=\"" + color.name().toLatin1() + "\" fill-opacity=\"" +
                                QByteArray::number(static_cast<double>(color.alphaF())) + "\" ";
        data.replace("<path ", fill);
    }
    QSvgRenderer renderer(data);
    if (!renderer.isValid())
        return QPixmap();
    const int pixels = qRound(extent * devicePixelRatio);
    QImage image(pixels, pixels, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, pixels, pixels));
    painter.end();
    image.setDevicePixelRatio(devicePixelRatio);
    return QPixmap::fromImage(image);
}

/*!
    Returns an icon of the symbol \a name in \a color, outlined, or filled if \a filled is \c true. An invalid color
    leaves it black. The icon holds the symbol at the sizes that icons are commonly shown at, with those of screens of
    two and three device pixels to a logical one. It is null if there is no such symbol.

    \sa pixmap()
*/
QIcon QMaterialSymbols::icon(const QString &name, const QColor &color, bool filled) {
    QIcon result;
    for (const int pixels : {20, 24, 32, 40, 48, 64, 72, 96, 120, 144}) {
        const QPixmap drawn = pixmap(name, pixels, color, filled);
        if (drawn.isNull())
            return QIcon();
        result.addPixmap(drawn);
    }
    return result;
}
