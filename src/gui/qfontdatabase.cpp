// MIT License - see LICENSE
#include <QtGui/qfontdatabase.h>

#include "qfontengine_p.h"

#include <QtCore/qfile.h>
#include <QtGui/qguiapplication.h>

/*!
    \class QFontDatabase
    \brief The QFontDatabase class lists the fonts that are available and adds fonts shipped with the application.

    The families the system knows come from fontconfig. A font file that ships with the application, as a file or a
    resource, is added with addApplicationFont(); its families are then available to QFont like any other:

    \code
    const int id = QFontDatabase::addApplicationFont(":/fonts/Nasalization-Rg.otf");
    if (id >= 0)
        label->setFont(QFont(QFontDatabase::applicationFontFamilies(id).first(), 14));
    \endcode

    \differences
    Every function is static, as in Qt 6, and there is no instance API. There are no \c styles(),
    \c pointSizes(), \c smoothSizes(), \c standardSizes(), \c weight(), \c italic(), \c bold(),
    \c isScalable(), \c isSmoothlyScalable(), \c isBitmapScalable(), \c isPrivateFamily(),
    \c writingSystems(), \c writingSystemName(), \c writingSystemSample() or \c styleString(). families() takes
    a WritingSystem, but only Any and Latin exist and both give every family. isFixedPitch() asks the font
    engine about the family's regular face and ignores the style. systemFont() returns the application font,
    with a monospace family for FixedFont and a smaller size for SmallestReadableFont. Application fonts need Nt GUI
    built with FreeType; without it addApplicationFont() returns -1.
    \enddifferences

    \sa QFont, QFontMetrics
*/

/*!
    \enum QFontDatabase::WritingSystem

    The writing systems a family can be asked to support. Nt does not filter by writing system.

    \value Any Any writing system.
    \value Latin The Latin script.
*/

/*!
    \enum QFontDatabase::SystemFont

    The fonts the system uses for particular purposes.

    \value GeneralFont The font for ordinary text: the application font.
    \value FixedFont A monospaced font of the same size.
    \value TitleFont The font for titles: the application font.
    \value SmallestReadableFont The application font two points smaller, and at least six points.
*/

/*!
    Returns the font families that fontconfig and the application fonts provide, sorted, for \a writingSystem, which
    is ignored.
*/
QStringList QFontDatabase::families(WritingSystem writingSystem) {
    (void)writingSystem;
    return QFontEngine::families();
}

/*!
    Returns \c true if \a family is one of families(), compared without regard to case.
*/
bool QFontDatabase::hasFamily(const QString &family) {
    const QString wanted = family.toLower();
    for (const QString &f : QFontEngine::families()) {
        if (f.toLower() == wanted)
            return true;
    }
    return false;
}

/*!
    Returns \c true if the regular face of \a family has glyphs of one width. The \a style is ignored.
*/
bool QFontDatabase::isFixedPitch(const QString &family, const QString &style) {
    (void)style;
    return QFontEngine::create(QFont(family))->isFixedPitch();
}

/*!
    Returns the font the system would use for \a type; see QFontDatabase::SystemFont.
*/
QFont QFontDatabase::systemFont(SystemFont type) {
    QFont font = QGuiApplication::font();
    switch (type) {
    case FixedFont:
        font.setFamily(QString::fromLatin1("monospace"));
        break;
    case SmallestReadableFont: {
        const qreal points = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
        font.setPointSizeF(points - 2.0 < 6.0 ? 6.0 : points - 2.0);
        break;
    }
    case GeneralFont:
    case TitleFont:
        break;
    }
    return font;
}

/*!
    Loads the font file \a fileName, which may be a resource path, and makes its families available to QFont.
    Returns an id for removeApplicationFont() and applicationFontFamilies(), or -1 if the file cannot be read or is
    not a font.
*/
int QFontDatabase::addApplicationFont(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return -1;
    return addApplicationFontFromData(file.readAll());
}

/*!
    Makes the families of the font in \a fontData, the bytes of a TrueType or OpenType file, available to QFont.
    Returns an id, or -1 if the data is not a font.
*/
int QFontDatabase::addApplicationFontFromData(const QByteArray &fontData) {
    return QFontEngine::addApplicationFont(fontData);
}

/*!
    Returns the families that the application font \a id added, or an empty list for an unknown id.
*/
QStringList QFontDatabase::applicationFontFamilies(int id) { return QFontEngine::applicationFontFamilies(id); }

/*!
    Removes the application font \a id. Returns \c true if there was such a font. Text that is already laid out with
    its families keeps the engines it has.
*/
bool QFontDatabase::removeApplicationFont(int id) { return QFontEngine::removeApplicationFont(id); }

/*!
    Removes every application font. Returns \c true if there was any.
*/
bool QFontDatabase::removeAllApplicationFonts() { return QFontEngine::removeAllApplicationFonts(); }
