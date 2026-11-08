// MIT License - see LICENSE
#include <QtGui/qpixmap.h>

#include <utility>

/*!
    \class QPixmap
    \brief The QPixmap class is an image to paint and to show, such as a splash screen or a picture in a label.

    A pixmap is a picture with an alpha channel that a QPainter paints on and that widgets show:

    \code
    QPixmap picture(200, 100);
    picture.fill(Qt::white);
    QPainter painter(&picture);
    painter.drawText(picture.rect(), Qt::AlignCenter, "Nt");
    painter.end();
    label->setPixmap(picture);
    \endcode

    Pixmaps share their data: copying one is cheap, and it is copied when one of the copies is painted on.

    \differences
    A pixmap is a QImage in memory, not a resource of the window system. It cannot be loaded from or saved to a file
    (there is no \c load(), \c save() or \c QPixmap(fileName)), and there is no \c scaled(), \c transformed(),
    \c mask(), \c QPixmapCache or device pixel ratio.
    \enddifferences

    \sa QImage, QPainter
*/

/*!
    Constructs a null pixmap.

    \sa isNull()
*/
QPixmap::QPixmap() = default;

/*!
    Constructs a pixmap of \a width by \a height pixels. Its contents are transparent until it is filled or painted
    on.
*/
QPixmap::QPixmap(int width, int height) : QPixmap(QSize(width, height)) {}

/*!
    Constructs a pixmap of the given \a size, transparent.
*/
QPixmap::QPixmap(const QSize &size) {
    if (size.width() > 0 && size.height() > 0) {
        m_image = QImage(size, QImage::Format_ARGB32_Premultiplied);
        m_image.fill(Qt::transparent);
    }
}

/*!
    Constructs a pixmap that shares the data of \a other.
*/
QPixmap::QPixmap(const QPixmap &other) : QPaintDevice(), m_image(other.m_image) {}

/*!
    Constructs a pixmap that takes the data of \a other.
*/
QPixmap::QPixmap(QPixmap &&other) noexcept : QPaintDevice(), m_image(std::move(other.m_image)) {}

/*!
    Destroys the pixmap.
*/
QPixmap::~QPixmap() = default;

/*!
    Makes the pixmap share the data of \a other and returns it.
*/
QPixmap &QPixmap::operator=(const QPixmap &other) {
    m_image = other.m_image;
    return *this;
}

/*!
    Makes the pixmap take the data of \a other and returns it.
*/
QPixmap &QPixmap::operator=(QPixmap &&other) noexcept {
    m_image = std::move(other.m_image);
    return *this;
}

/*!
    \fn void QPixmap::swap(QPixmap &other)

    Swaps the pixmap with \a other.
*/

/*!
    \fn bool QPixmap::isNull() const

    Returns \c true if the pixmap has no pixels.
*/

/*!
    \fn QSize QPixmap::size() const

    Returns the size of the pixmap.
*/

/*!
    \fn QRect QPixmap::rect() const

    Returns the rectangle of the pixmap, at the origin.
*/

/*!
    \fn bool QPixmap::hasAlphaChannel() const

    Returns \c true if the pixmap has an alpha channel; pixmaps made by Nt do.
*/

/*!
    Fills the pixmap with \a color.
*/
void QPixmap::fill(const QColor &color) {
    if (!m_image.isNull())
        m_image.fill(color);
}

/*!
    Returns a copy of the part \a rect of the pixmap, or of all of it if \a rect is empty.
*/
QPixmap QPixmap::copy(const QRect &rect) const { return fromImage(m_image.copy(rect)); }

/*!
    \fn QImage QPixmap::toImage() const

    Returns the pixmap as an image.

    \sa fromImage()
*/

/*!
    Returns a pixmap with the pixels of \a image.

    \sa toImage()
*/
QPixmap QPixmap::fromImage(const QImage &image) {
    QPixmap pixmap;
    if (!image.isNull())
        pixmap.m_image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return pixmap;
}

/*!
    \reimp
*/
QPaintEngine *QPixmap::paintEngine() const { return m_image.paintEngine(); }

/*!
    \reimp
*/
int QPixmap::metric(PaintDeviceMetric metric) const {
    switch (metric) {
    case PdmWidth:
        return m_image.width();
    case PdmHeight:
        return m_image.height();
    case PdmDepth:
        return m_image.isNull() ? 0 : 32;
    default:
        return QPaintDevice::metric(metric);
    }
}

/*!
    \reimp

    A painter paints on the image of the pixmap, which is detached from the copies first.
*/
QPaintDevice *QPixmap::redirected(QPoint *offset) const {
    (void)offset;
    if (m_image.isNull())
        return nullptr;
    m_image.detach();
    return &m_image;
}
