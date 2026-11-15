// MIT License - see LICENSE
#include <QtWidgets/qgraphicseffect.h>

#include <QtWidgets/qgraphicsitem.h>

/*!
    \class QGraphicsEffect
    \brief The QGraphicsEffect class is the base of the effects a QGraphicsItem can be drawn with.

    Give an item an effect with QGraphicsItem::setGraphicsEffect(); the item owns it.

    \differences
    The effects of Nt are drawn: QGraphicsDropShadowEffect, QGraphicsBlurEffect, QGraphicsColorizeEffect and
    QGraphicsOpacityEffect. There is no \c draw() or \c QGraphicsEffectSource to subclass, so an effect of the
    application's own has no visible result. The effect applies to the item's own painting, not to its children, and
    only items in a QGraphicsScene have effects: there is no \c QWidget::setGraphicsEffect().
    \enddifferences
*/

/*!
    Constructs an effect with the given \a parent.
*/
QGraphicsEffect::QGraphicsEffect(QObject *parent) : QObject(parent) {}

/*!
    Destroys the effect and takes it off its item.
*/
QGraphicsEffect::~QGraphicsEffect() {
    if (m_item && m_item->graphicsEffect() == this)
        m_item->setGraphicsEffect(nullptr);
}

/*!
    Returns the rectangle the effect paints in for an item that paints in \a sourceRect: \a sourceRect itself.
*/
QRectF QGraphicsEffect::boundingRectFor(const QRectF &sourceRect) const { return sourceRect; }

/*!
    Returns the rectangle the effect paints in, in the coordinates of its item, or a null rectangle without one.
*/
QRectF QGraphicsEffect::boundingRect() const { return m_item ? boundingRectFor(m_item->boundingRect()) : QRectF(); }

/*!
    Returns \c true if the effect is drawn; the default.
*/
bool QGraphicsEffect::isEnabled() const { return m_enabled; }

/*!
    Turns the effect on if \a enable, or off.
*/
void QGraphicsEffect::setEnabled(bool enable) {
    if (enable == m_enabled)
        return;
    m_enabled = enable;
    update();
    emit enabledChanged(enable);
}

/*!
    Redraws the item the effect belongs to.
*/
void QGraphicsEffect::update() {
    if (m_item)
        m_item->update(boundingRect());
}

/*!
    \class QGraphicsDropShadowEffect
    \brief The QGraphicsDropShadowEffect class draws a blurred shadow behind an item.

    \differences
    The shadow follows the alpha of the item's own painting, offset in device pixels by the offset scaled with the
    item's transformation, and blurred with a box blur of the radius.
    \enddifferences
*/

/*!
    Constructs a shadow effect with the given \a parent: offset by (8, 8), a blur radius of 1, and a translucent
    dark gray.
*/
QGraphicsDropShadowEffect::QGraphicsDropShadowEffect(QObject *parent) : QGraphicsEffect(parent) {}

/*!
    Destroys the effect.
*/
QGraphicsDropShadowEffect::~QGraphicsDropShadowEffect() = default;

/*!
    \reimp

    Returns \a rect grown by the blur radius and moved by the offset, united with \a rect.
*/
QRectF QGraphicsDropShadowEffect::boundingRectFor(const QRectF &rect) const {
    const qreal r = m_blurRadius;
    return rect.united(rect.translated(m_offset).adjusted(-r, -r, r, r));
}

/*!
    Sets the offset of the shadow to \a ofs.
*/
void QGraphicsDropShadowEffect::setOffset(const QPointF &ofs) {
    if (ofs == m_offset)
        return;
    update();
    m_offset = ofs;
    update();
    emit offsetChanged(ofs);
}

/*!
    Sets the blur radius to \a blurRadius device pixels.
*/
void QGraphicsDropShadowEffect::setBlurRadius(qreal blurRadius) {
    if (blurRadius == m_blurRadius)
        return;
    update();
    m_blurRadius = blurRadius < 0 ? 0 : blurRadius;
    update();
    emit blurRadiusChanged(m_blurRadius);
}

/*!
    Sets the color of the shadow to \a color; its alpha sets how dark it is.
*/
void QGraphicsDropShadowEffect::setColor(const QColor &color) {
    if (color == m_color)
        return;
    m_color = color;
    update();
    emit colorChanged(color);
}

/*!
    \class QGraphicsBlurEffect
    \brief The QGraphicsBlurEffect class provides a blur effect.

    The effect draws its item blurred, which makes it look out of focus or shows that it is not available:

    \code
    auto *blur = new QGraphicsBlurEffect;
    blur->setBlurRadius(8);
    item->setGraphicsEffect(blur);
    \endcode

    \differences
    The blur is three passes of a box blur over the device pixels of the item, close to a Gaussian blur, whatever
    blurHints() says.
    \enddifferences
*/

/*!
    \enum QGraphicsBlurEffect::BlurHint

    This enum says what matters most about the blur. The BlurHints type is a typedef for \c{QFlags<BlurHint>}. Nt
    keeps the hints but blurs in one way.

    \value PerformanceHint Speed: the blur is drawn quickly.
    \value QualityHint Quality: the blur looks as good as it can.
    \value AnimationHint The blur radius is animated.
*/

/*!
    Constructs a blur effect with the given \a parent and a blur radius of 5.
*/
QGraphicsBlurEffect::QGraphicsBlurEffect(QObject *parent) : QGraphicsEffect(parent) {}

/*!
    Destroys the effect.
*/
QGraphicsBlurEffect::~QGraphicsBlurEffect() = default;

/*!
    \reimp

    Returns \a rect grown by the blur radius, as far as the blur spreads.
*/
QRectF QGraphicsBlurEffect::boundingRectFor(const QRectF &rect) const {
    const qreal r = m_blurRadius;
    return rect.adjusted(-r, -r, r, r);
}

/*!
    \fn qreal QGraphicsBlurEffect::blurRadius() const

    Returns how far the blur spreads, in device pixels. The default is 5; 0 draws the item as it is.

    \sa setBlurRadius()
*/

/*!
    Sets the blur radius to \a blurRadius device pixels.

    \sa blurRadius()
*/
void QGraphicsBlurEffect::setBlurRadius(qreal blurRadius) {
    blurRadius = blurRadius < 0 ? 0 : blurRadius;
    if (blurRadius == m_blurRadius)
        return;
    update();
    m_blurRadius = blurRadius;
    update();
    emit blurRadiusChanged(m_blurRadius);
}

/*!
    \fn QGraphicsBlurEffect::BlurHints QGraphicsBlurEffect::blurHints() const

    Returns the hints that setBlurHints() set. The default is \c PerformanceHint.
*/

/*!
    Keeps \a hints for blurHints(); Nt blurs in one way.
*/
void QGraphicsBlurEffect::setBlurHints(QGraphicsBlurEffect::BlurHints hints) {
    if (hints == m_hints)
        return;
    m_hints = hints;
    emit blurHintsChanged(hints);
}

/*!
    \class QGraphicsColorizeEffect
    \brief The QGraphicsColorizeEffect class provides a colorize effect.

    The effect draws its item in shades of one color: the item is turned into gray, and the gray is tinted with
    color(). strength() mixes the tinted item with the item as it is.

    \code
    auto *sepia = new QGraphicsColorizeEffect;
    sepia->setColor(QColor(112, 66, 20));
    sepia->setStrength(0.8);
    item->setGraphicsEffect(sepia);
    \endcode

    \differences
    None.
    \enddifferences
*/

/*!
    Constructs a colorize effect with the given \a parent: a dark blue, at full strength.
*/
QGraphicsColorizeEffect::QGraphicsColorizeEffect(QObject *parent) : QGraphicsEffect(parent) {}

/*!
    Destroys the effect.
*/
QGraphicsColorizeEffect::~QGraphicsColorizeEffect() = default;

/*!
    \fn QColor QGraphicsColorizeEffect::color() const

    Returns the color that the item is tinted with. The default is a dark blue, \c{QColor(0, 0, 192)}.

    \sa setColor()
*/

/*!
    Tints the item with the color \a c.

    \sa color()
*/
void QGraphicsColorizeEffect::setColor(const QColor &c) {
    if (c == m_color)
        return;
    m_color = c;
    update();
    emit colorChanged(c);
}

/*!
    \fn qreal QGraphicsColorizeEffect::strength() const

    Returns how much of the tinted item is drawn, from 0, the item as it is, to 1, the item tinted only. The
    default is 1.

    \sa setStrength()
*/

/*!
    Sets how much of the tinted item is drawn to \a strength, from 0 to 1.

    \sa strength()
*/
void QGraphicsColorizeEffect::setStrength(qreal strength) {
    strength = strength < 0 ? 0 : (strength > 1 ? 1 : strength);
    if (strength == m_strength)
        return;
    m_strength = strength;
    update();
    emit strengthChanged(strength);
}

/*!
    \class QGraphicsOpacityEffect
    \brief The QGraphicsOpacityEffect class provides an opacity effect.

    The effect draws its item translucent, as if the item had a lower QGraphicsItem::opacity(), and with an
    opacity mask fades parts of it: where the alpha of the mask is low, the item is faint.

    \code
    auto *fade = new QGraphicsOpacityEffect;
    QLinearGradient gradient(0, 0, 0, 100);
    gradient.setColorAt(0, Qt::black);
    gradient.setColorAt(1, Qt::transparent);
    fade->setOpacityMask(gradient);
    item->setGraphicsEffect(fade);
    \endcode

    \differences
    The mask is drawn in the coordinates of the item's device rectangle, from its top left corner.
    \enddifferences
*/

/*!
    Constructs an opacity effect with the given \a parent and an opacity of 0.7.
*/
QGraphicsOpacityEffect::QGraphicsOpacityEffect(QObject *parent) : QGraphicsEffect(parent) {}

/*!
    Destroys the effect.
*/
QGraphicsOpacityEffect::~QGraphicsOpacityEffect() = default;

/*!
    \fn qreal QGraphicsOpacityEffect::opacity() const

    Returns the opacity that the item is drawn with, from 0, invisible, to 1, opaque. The default is 0.7.

    \sa setOpacity()
*/

/*!
    Draws the item with the opacity \a opacity, from 0 to 1.

    \sa opacity()
*/
void QGraphicsOpacityEffect::setOpacity(qreal opacity) {
    opacity = opacity < 0 ? 0 : (opacity > 1 ? 1 : opacity);
    if (opacity == m_opacity)
        return;
    m_opacity = opacity;
    update();
    emit opacityChanged(opacity);
}

/*!
    \fn QBrush QGraphicsOpacityEffect::opacityMask() const

    Returns the brush whose alpha fades the item, or a brush with no style if the whole item has the same
    opacity, which is the default.

    \sa setOpacityMask()
*/

/*!
    Fades the item with the alpha of \a mask: where the mask is transparent, so is the item.

    \sa opacityMask()
*/
void QGraphicsOpacityEffect::setOpacityMask(const QBrush &mask) {
    m_mask = mask;
    update();
    emit opacityMaskChanged(mask);
}

#include "moc_qgraphicseffect.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
