// MIT License - see LICENSE
#include <QtWidgets/qproxystyle.h>

#include <QtGui/qfontmetrics.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qstylefactory.h>

/*!
    \class QProxyStyle
    \brief The QProxyStyle class is a convenience class that simplifies dynamically overriding QStyle elements.

    A QProxyStyle wraps another style, its base style, and passes everything on to it. A subclass reimplements
    only what it wants to change; the base style draws the rest, and because a style draws its own elements
    through QStyle::proxy(), what the subclass changes also changes how the base style draws its controls.

    \code
    class BigIndicators : public QProxyStyle {
    public:
        int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override {
            if (metric == PM_IndicatorWidth || metric == PM_IndicatorHeight)
                return 24;
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    };

    QApplication::setStyle(new BigIndicators);
    \endcode

    The proxy takes ownership of its base style. Without one, it makes a style of its own, like the default
    style of the application.

    \differences
    QProxyStyle derives from QStyle, as Nt has no \c QCommonStyle, and so it has only what QStyle has: no
    complex controls, style hints, standard icons or \c polish().
    \enddifferences

    \sa QStyle, QStyleFactory, QApplication::setStyle()
*/

/*!
    Constructs a proxy style for the base style \a style, which it takes ownership of. A \c nullptr makes it
    use a style of its own like the default style.

    \sa setBaseStyle()
*/
QProxyStyle::QProxyStyle(QStyle *style) { setBaseStyle(style); }

/*!
    Constructs a proxy style for a base style made by QStyleFactory from \a key, such as \c "Fusion". If there
    is no style of that name, the default style is the base.
*/
QProxyStyle::QProxyStyle(const QString &key) { setBaseStyle(QStyleFactory::create(key)); }

/*!
    Destroys the proxy style and its base style.
*/
QProxyStyle::~QProxyStyle() = default;

/*!
    Returns the style that the proxy passes everything on to.

    \sa setBaseStyle()
*/
QStyle *QProxyStyle::baseStyle() const { return m_base; }

/*!
    Makes \a style the base style of the proxy, which takes ownership of it, and deletes the base style it
    had. A \c nullptr gives it a style of its own like the default style.

    \sa baseStyle()
*/
void QProxyStyle::setBaseStyle(QStyle *style) {
    if (style && style == m_base)
        return;
    if (m_base && m_base->parent() == this) {
        m_base->m_proxy = nullptr;
        delete m_base.data();
    }
    if (!style) {
        const QStringList keys = QStyleFactory::keys();
        style = QStyleFactory::create(
            keys.contains(QStringLiteral("Fusion")) || keys.isEmpty() ? QStringLiteral("Fusion") : keys.first());
    }
    m_base = style;
    if (m_base) {
        m_base->setParent(this);
        m_base->m_proxy = this;
    }
}

/*!
    \reimp
*/
void QProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                                const QWidget *widget) const {
    if (m_base)
        m_base->drawPrimitive(element, option, painter, widget);
}

/*!
    \reimp
*/
void QProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                              const QWidget *widget) const {
    if (m_base)
        m_base->drawControl(element, option, painter, widget);
}

/*!
    \reimp
*/
QRect QProxyStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const {
    return m_base ? m_base->subElementRect(element, option, widget) : QRect();
}

/*!
    \reimp
*/
int QProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const {
    return m_base ? m_base->pixelMetric(metric, option, widget) : 0;
}

/*!
    \reimp
*/
QSize QProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                                    const QWidget *widget) const {
    return m_base ? m_base->sizeFromContents(type, option, contentsSize, widget) : contentsSize;
}

/*!
    \reimp
*/
QPalette QProxyStyle::standardPalette() const { return m_base ? m_base->standardPalette() : QStyle::standardPalette(); }

/*!
    \reimp
*/
QRect QProxyStyle::itemTextRect(const QFontMetrics &metrics, const QRect &rectangle, int alignment, bool enabled,
                                const QString &text) const {
    return m_base ? m_base->itemTextRect(metrics, rectangle, alignment, enabled, text)
                  : QStyle::itemTextRect(metrics, rectangle, alignment, enabled, text);
}

/*!
    \reimp
*/
QRect QProxyStyle::itemPixmapRect(const QRect &rectangle, int alignment, const QPixmap &pixmap) const {
    return m_base ? m_base->itemPixmapRect(rectangle, alignment, pixmap)
                  : QStyle::itemPixmapRect(rectangle, alignment, pixmap);
}

/*!
    \reimp
*/
void QProxyStyle::drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette,
                               bool enabled, const QString &text, QPalette::ColorRole textRole) const {
    if (m_base)
        m_base->drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole);
    else
        QStyle::drawItemText(painter, rectangle, alignment, palette, enabled, text, textRole);
}

/*!
    \reimp
*/
void QProxyStyle::drawItemPixmap(QPainter *painter, const QRect &rectangle, int alignment,
                                 const QPixmap &pixmap) const {
    if (m_base)
        m_base->drawItemPixmap(painter, rectangle, alignment, pixmap);
    else
        QStyle::drawItemPixmap(painter, rectangle, alignment, pixmap);
}

/*!
    \reimp
*/
void QProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                                     const QWidget *widget) const {
    if (m_base)
        m_base->drawComplexControl(control, option, painter, widget);
}

/*!
    \reimp
*/
QStyle::SubControl QProxyStyle::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                                      const QPoint &position, const QWidget *widget) const {
    return m_base ? m_base->hitTestComplexControl(control, option, position, widget) : SC_None;
}

/*!
    \reimp
*/
QRect QProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                                  const QWidget *widget) const {
    return m_base ? m_base->subControlRect(control, option, subControl, widget) : QRect();
}

/*!
    \reimp
*/
int QProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                           QStyleHintReturn *returnData) const {
    return m_base ? m_base->styleHint(hint, option, widget, returnData)
                  : QStyle::styleHint(hint, option, widget, returnData);
}

/*!
    \reimp
*/
QPixmap QProxyStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option,
                                    const QWidget *widget) const {
    return m_base ? m_base->standardPixmap(standardPixmap, option, widget) : QPixmap();
}

/*!
    \reimp
*/
void QProxyStyle::polish(QWidget *widget) {
    if (m_base)
        m_base->polish(widget);
}

/*!
    \reimp
*/
void QProxyStyle::unpolish(QWidget *widget) {
    if (m_base)
        m_base->unpolish(widget);
}

/*!
    \reimp
*/
void QProxyStyle::polish(QApplication *application) {
    if (m_base)
        m_base->polish(application);
}

/*!
    \reimp
*/
void QProxyStyle::unpolish(QApplication *application) {
    if (m_base)
        m_base->unpolish(application);
}

/*!
    \reimp
*/
void QProxyStyle::polish(QPalette &palette) {
    if (m_base)
        m_base->polish(palette);
}

#include "moc_qproxystyle.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
