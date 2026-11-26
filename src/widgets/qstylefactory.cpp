// MIT License - see LICENSE
#include <QtWidgets/qstylefactory.h>

#include "qfusionstyle_p.h"

#include <QtWidgets/qmaterialstyle.h>

/*!
    \class QStyleFactory
    \brief The QStyleFactory class creates QStyle objects.

    A style draws the widgets of an application. The factory makes a style
    from its name, which is how a program lets its user or a setting choose
    one:

    \code
    if (QStyle *style = QStyleFactory::create(settings.value("style").toString()))
        QApplication::setStyle(style);
    \endcode

    keys() lists the names that create() knows.

    \differences
    Nt has two styles, \c Fusion and \c Material3 (QMaterialStyle, which Qt Widgets does not have), and no style
    plugins, so keys() has those names. The names \c Windows and \c WindowsVista, which programs
    written for Qt ask for, are not known: create() returns \c nullptr for
    them, as Qt does on a platform that lacks them.
    \enddifferences

    \sa QStyle, QApplication::setStyle()
*/

/*!
    Returns the names of the styles that create() can make.

    \sa create()
*/
QStringList QStyleFactory::keys() { return QStringList{QStringLiteral("Fusion"), QStringLiteral("Material3")}; }

/*!
    Returns a new style of the kind called \a key, or \c nullptr if there
    is no such style. The case of \a key does not matter. The caller owns
    the style; QApplication::setStyle() takes it over.

    \sa keys()
*/
QStyle *QStyleFactory::create(const QString &key) {
    if (key.compare(QStringLiteral("Fusion"), Qt::CaseInsensitive) == 0) {
        auto *style = new QFusionStyle;
        style->m_name = QStringLiteral("Fusion");
        return style;
    }
    // Nt only: Material 3 Expressive.
    if (key.compare(QStringLiteral("Material3"), Qt::CaseInsensitive) == 0) {
        auto *style = new QMaterialStyle;
        style->m_name = QStringLiteral("Material3");
        return style;
    }
    return nullptr;
}
