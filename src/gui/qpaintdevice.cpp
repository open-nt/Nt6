// MIT License - see LICENSE
#include <QtGui/qpaintdevice.h>

/*!
    \class QPaintDevice
    \brief The QPaintDevice class is the base class of objects that QPainter can paint on.

    A paint device is a surface with a width, a height and a paint engine.
    QImage and the windows of Nt GUI are paint devices.

    \differences
    The device pixel ratio is always 1, and there is no \c colorCount() other
    than 16777216. The paint device does not need to be a QImage-sized
    object: any class that returns a paint engine can serve.
    \enddifferences

    \sa QPainter, QPaintEngine, QImage
*/

/*!
    \enum QPaintDevice::PaintDeviceMetric

    This enum describes the metrics that a paint device can report.

    \value PdmWidth The width in pixels.
    \value PdmHeight The height in pixels.
    \value PdmWidthMM The width in millimeters.
    \value PdmHeightMM The height in millimeters.
    \value PdmNumColors The number of colors.
    \value PdmDepth The bit depth of a pixel.
    \value PdmDpiX The horizontal resolution in dots per inch.
    \value PdmDpiY The vertical resolution in dots per inch.
    \value PdmDevicePixelRatio The device pixel ratio, rounded down.
    \value PdmDevicePixelRatioScaled The device pixel ratio multiplied by 65536.
*/

/*!
    Destroys the paint device.
*/
QPaintDevice::~QPaintDevice() = default;

/*!
    \fn QPaintEngine *QPaintDevice::paintEngine() const

    Returns the paint engine that paints on this device. A painter calls it
    when painting begins. The device owns the engine.
*/

/*!
    Returns the value of \a metric. The default implementation reports a
    screen of 96 dots per inch and 32 bits per pixel, and 0 for the sizes.
*/
int QPaintDevice::metric(PaintDeviceMetric metric) const {
    switch (metric) {
    case PdmDepth:
        return 32;
    case PdmNumColors:
        return 16777216;
    case PdmDpiX:
    case PdmDpiY:
        return 96;
    case PdmDevicePixelRatio:
        return 1;
    case PdmDevicePixelRatioScaled:
        return 65536;
    default:
        return 0;
    }
}

/*!
    Returns the device that this device paints on, or \c nullptr if it paints
    on itself. A painter that begins on this device paints on the returned
    device instead, and the position of this device in it is added to \a offset.
    The default returns \c nullptr.
*/
QPaintDevice *QPaintDevice::redirected(QPoint *offset) const {
    (void)offset;
    return nullptr;
}

/*!
    Returns \c true if painting on this device is restricted to a part of the device it is
    redirected to, and stores that part in \a region, in the coordinates of that device. A
    widget returns the part of its window that is visible and being painted. The default
    returns \c false.

    \sa redirected()
*/
bool QPaintDevice::systemClip(QRegion *region) const {
    (void)region;
    return false;
}

/*!
    \fn int QPaintDevice::width() const

    Returns the width of the device in pixels.
*/

/*!
    \fn int QPaintDevice::height() const

    Returns the height of the device in pixels.
*/

/*!
    \fn int QPaintDevice::widthMM() const

    Returns the width of the device in millimeters.
*/

/*!
    \fn int QPaintDevice::heightMM() const

    Returns the height of the device in millimeters.
*/

/*!
    \fn int QPaintDevice::colorCount() const

    Returns the number of colors the device supports.
*/

/*!
    \fn int QPaintDevice::depth() const

    Returns the number of bits per pixel.
*/

/*!
    \fn int QPaintDevice::logicalDpiX() const

    Returns the horizontal resolution in dots per inch.
*/

/*!
    \fn int QPaintDevice::logicalDpiY() const

    Returns the vertical resolution in dots per inch.
*/

/*!
    \fn qreal QPaintDevice::devicePixelRatio() const

    Returns the number of device pixels that a unit of the device's coordinates stands for: 1 on
    an ordinary device and 2 on a high-resolution screen at twice the usual density.
    A QPainter scales what it draws by this factor.
*/

/*!
    \fn bool QPaintDevice::paintingActive() const

    Returns \c true if a QPainter is painting on the device.
*/

/*!
    \fn QPaintDevice::QPaintDevice()

    Constructs a paint device.
*/
