// MIT License - see LICENSE
#include <QtMultimedia/qvideoframeformat.h>

#include <QtCore/qsysinfo.h>

/*!
    \class QVideoFrameFormat
    \inmodule NtMultimedia
    \brief The QVideoFrameFormat class describes the pixels of a QVideoFrame: their layout and size.

    \code
    QVideoFrameFormat format(QSize(1280, 720), QVideoFrameFormat::Format_NV12);
    int planes = format.planeCount(); // 2: luma, then interleaved chroma
    \endcode

    \differences
    There is no HDR information (\c maxLuminance(), \c colorTransfer()), no shader file names
    (\c vertexShaderFileName(), \c fragmentShaderFileName()) and no \c updateUniformData(). The YCbCr
    color space and color range only steer QVideoFrame::toImage(), which honors BT.601 and BT.709
    in video and full range.
    \enddifferences

    \sa QVideoFrame, QVideoSink
*/

/*!
    \enum QVideoFrameFormat::PixelFormat

    How the pixels of a frame are stored. The names give the order of the bytes in memory.

    \value Format_Invalid No format.
    \value Format_ARGB8888 Alpha, red, green and blue bytes.
    \value Format_ARGB8888_Premultiplied The same, with the colors multiplied by alpha.
    \value Format_XRGB8888 An unused byte, then red, green and blue.
    \value Format_BGRA8888 Blue, green, red and alpha bytes; QImage::Format_ARGB32 on little-endian machines.
    \value Format_BGRA8888_Premultiplied The same, with the colors multiplied by alpha.
    \value Format_BGRX8888 Blue, green and red, then an unused byte; QImage::Format_RGB32 on little-endian machines.
    \value Format_ABGR8888 Alpha, blue, green and red bytes.
    \value Format_XBGR8888 An unused byte, then blue, green and red.
    \value Format_RGBA8888 Red, green, blue and alpha bytes.
    \value Format_RGBX8888 Red, green and blue, then an unused byte.
    \value Format_AYUV Alpha, Y, U and V bytes.
    \value Format_AYUV_Premultiplied The same, premultiplied.
    \value Format_YUV420P Planar Y, then U and V at half the width and height.
    \value Format_YUV422P Planar Y, then U and V at half the width.
    \value Format_YV12 Planar Y, then V and U at half the width and height.
    \value Format_UYVY Packed U, Y, V, Y for each pair of pixels.
    \value Format_YUYV Packed Y, U, Y, V for each pair of pixels, as most webcams send.
    \value Format_NV12 Planar Y, then interleaved U and V at half the width and height.
    \value Format_NV21 Planar Y, then interleaved V and U at half the width and height.
    \value Format_IMC1 A planar YUV 4:2:0 layout with padded chroma lines. Nt cannot lay it out.
    \value Format_IMC2 As IMC1, with the chroma planes side by side. Nt cannot lay it out.
    \value Format_IMC3 As IMC1, with V before U. Nt cannot lay it out.
    \value Format_IMC4 As IMC2, with V before U. Nt cannot lay it out.
    \value Format_Y8 8-bit grey.
    \value Format_Y16 16-bit grey.
    \value Format_P010 As NV12, with 16-bit samples of which the top 10 bits are used.
    \value Format_P016 As NV12, with 16-bit samples.
    \value Format_SamplerExternalOES An Android texture. Nt has none.
    \value Format_Jpeg A JPEG image, as a MJPEG camera sends.
    \value Format_SamplerRect A macOS texture. Nt has none.
    \value Format_YUV420P10 As YUV420P, with 16-bit samples of which the low 10 bits are used.
*/

/*!
    \variable QVideoFrameFormat::NPixelFormats

    The number of values of PixelFormat.
*/

/*!
    \enum QVideoFrameFormat::Direction

    The order of the lines in memory.

    \value TopToBottom The first line is the top of the picture.
    \value BottomToTop The first line is the bottom of the picture.
*/

/*!
    \enum QVideoFrameFormat::YCbCrColorSpace

    The YCbCr color spaces of Qt 5, kept for source compatibility.

    \value YCbCr_Undefined Not known.
    \value YCbCr_BT601 ITU-R BT.601.
    \value YCbCr_BT709 ITU-R BT.709.
    \value YCbCr_xvYCC601 Extended-gamut BT.601.
    \value YCbCr_xvYCC709 Extended-gamut BT.709.
    \value YCbCr_JPEG BT.601 in full range, as JPEG uses.
*/

/*!
    \enum QVideoFrameFormat::ColorSpace

    The color space of YUV pixels.

    \value ColorSpace_Undefined Not known; treated as BT.601.
    \value ColorSpace_BT601 ITU-R BT.601, for standard definition.
    \value ColorSpace_BT709 ITU-R BT.709, for high definition.
    \value ColorSpace_AdobeRgb Adobe RGB. Treated as BT.601 by toImage().
    \value ColorSpace_BT2020 ITU-R BT.2020. Treated as BT.709 by toImage().
*/

/*!
    \enum QVideoFrameFormat::ColorRange

    The range of the YUV values.

    \value ColorRange_Unknown Not known; treated as video range.
    \value ColorRange_Video Y from 16 to 235, U and V from 16 to 240.
    \value ColorRange_Full Every value from 0 to 255.
*/

/*!
    \fn QVideoFrameFormat::QVideoFrameFormat()

    Constructs an invalid format.
*/

/*!
    Constructs a format for frames of \a size in \a pixelFormat. The viewport covers the whole frame.
*/
QVideoFrameFormat::QVideoFrameFormat(const QSize &size, PixelFormat pixelFormat)
    : m_pixelFormat(pixelFormat), m_size(size), m_viewport(QPoint(0, 0), size) {}

/*!
    Returns \c true if the format has a pixel format and a non-empty size; otherwise returns \c false.
*/
bool QVideoFrameFormat::isValid() const { return m_pixelFormat != Format_Invalid && !m_size.isEmpty(); }

/*!
    \fn QVideoFrameFormat::PixelFormat QVideoFrameFormat::pixelFormat() const

    Returns how the pixels are stored.
*/

/*!
    \fn QSize QVideoFrameFormat::frameSize() const

    Returns the size of a frame in pixels.
*/

/*!
    Sets the size of a frame to \a size, and the viewport to the whole frame.
*/
void QVideoFrameFormat::setFrameSize(const QSize &size) {
    m_size = size;
    m_viewport = QRect(QPoint(0, 0), size);
}

/*!
    \fn void QVideoFrameFormat::setFrameSize(int width, int height)

    \overload

    Sets the size of a frame to \a width by \a height.
*/

/*!
    \fn int QVideoFrameFormat::frameWidth() const

    Returns the width of a frame in pixels.
*/

/*!
    \fn int QVideoFrameFormat::frameHeight() const

    Returns the height of a frame in pixels.
*/

/*!
    Returns the number of planes the pixels are stored in: 1 for packed formats, 2 for NV12 and its
    relatives, and 3 for the planar YUV formats.
*/
int QVideoFrameFormat::planeCount() const {
    switch (m_pixelFormat) {
    case Format_Invalid:
    case Format_SamplerExternalOES:
    case Format_SamplerRect:
        return 0;
    case Format_NV12:
    case Format_NV21:
    case Format_P010:
    case Format_P016:
        return 2;
    case Format_YUV420P:
    case Format_YUV422P:
    case Format_YV12:
    case Format_YUV420P10:
    case Format_IMC1:
    case Format_IMC2:
    case Format_IMC3:
    case Format_IMC4:
        return 3;
    default:
        return 1;
    }
}

/*!
    \fn QRect QVideoFrameFormat::viewport() const

    Returns the part of the frame that holds the picture.
*/

/*!
    \fn void QVideoFrameFormat::setViewport(const QRect &viewport)

    Sets the part of the frame that holds the picture to \a viewport.
*/

/*!
    \fn QVideoFrameFormat::Direction QVideoFrameFormat::scanLineDirection() const

    Returns the order of the lines in memory.
*/

/*!
    \fn void QVideoFrameFormat::setScanLineDirection(QVideoFrameFormat::Direction direction)

    Sets the order of the lines in memory to \a direction.
*/

/*!
    \fn qreal QVideoFrameFormat::streamFrameRate() const

    Returns the number of frames a second of the stream, or 0 if not known.
*/

/*!
    \fn void QVideoFrameFormat::setStreamFrameRate(qreal rate)

    Sets the number of frames a second to \a rate.
*/

/*!
    \fn QVideoFrameFormat::ColorSpace QVideoFrameFormat::colorSpace() const

    Returns the color space of YUV pixels.
*/

/*!
    \fn void QVideoFrameFormat::setColorSpace(QVideoFrameFormat::ColorSpace colorSpace)

    Sets the color space of YUV pixels to \a colorSpace.
*/

/*!
    \fn QVideoFrameFormat::ColorRange QVideoFrameFormat::colorRange() const

    Returns the range of YUV values.
*/

/*!
    \fn void QVideoFrameFormat::setColorRange(QVideoFrameFormat::ColorRange range)

    Sets the range of YUV values to \a range.
*/

/*!
    \fn bool QVideoFrameFormat::isMirrored() const

    Returns \c true if the frames are mirrored left to right; otherwise returns \c false.
*/

/*!
    \fn void QVideoFrameFormat::setMirrored(bool mirrored)

    Sets whether the frames are mirrored left to right to \a mirrored.
*/

/*!
    \fn bool QVideoFrameFormat::operator!=(const QVideoFrameFormat &other) const

    Returns \c true if this format differs from \a other; otherwise returns \c false.
*/

/*!
    Returns the pixel format with the layout of the QImage \a format, or Format_Invalid.
*/
QVideoFrameFormat::PixelFormat QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format format) {
    const bool little = QSysInfo::ByteOrder == QSysInfo::LittleEndian;
    switch (format) {
    case QImage::Format_RGB32:
        return little ? Format_BGRX8888 : Format_XRGB8888;
    case QImage::Format_ARGB32:
        return little ? Format_BGRA8888 : Format_ARGB8888;
    case QImage::Format_ARGB32_Premultiplied:
        return little ? Format_BGRA8888_Premultiplied : Format_ARGB8888_Premultiplied;
    case QImage::Format_Invalid:
        break;
    }
    return Format_Invalid;
}

/*!
    Returns the QImage format with the layout of the pixel format \a format, or QImage::Format_Invalid
    if QImage has none.
*/
QImage::Format QVideoFrameFormat::imageFormatFromPixelFormat(PixelFormat format) {
    const bool little = QSysInfo::ByteOrder == QSysInfo::LittleEndian;
    switch (format) {
    case Format_BGRX8888:
        return little ? QImage::Format_RGB32 : QImage::Format_Invalid;
    case Format_BGRA8888:
        return little ? QImage::Format_ARGB32 : QImage::Format_Invalid;
    case Format_BGRA8888_Premultiplied:
        return little ? QImage::Format_ARGB32_Premultiplied : QImage::Format_Invalid;
    case Format_XRGB8888:
        return little ? QImage::Format_Invalid : QImage::Format_RGB32;
    case Format_ARGB8888:
        return little ? QImage::Format_Invalid : QImage::Format_ARGB32;
    case Format_ARGB8888_Premultiplied:
        return little ? QImage::Format_Invalid : QImage::Format_ARGB32_Premultiplied;
    default:
        return QImage::Format_Invalid;
    }
}

/*!
    Returns the name of \a pixelFormat, such as \c{YUYV} or \c{NV12}.
*/
QString QVideoFrameFormat::pixelFormatToString(PixelFormat pixelFormat) {
    static const char *const names[] = {"Invalid",
                                        "ARGB8888",
                                        "ARGB8888_Premultiplied",
                                        "XRGB8888",
                                        "BGRA8888",
                                        "BGRA8888_Premultiplied",
                                        "BGRX8888",
                                        "ABGR8888",
                                        "XBGR8888",
                                        "RGBA8888",
                                        "RGBX8888",
                                        "AYUV",
                                        "AYUV_Premultiplied",
                                        "YUV420P",
                                        "YUV422P",
                                        "YV12",
                                        "UYVY",
                                        "YUYV",
                                        "NV12",
                                        "NV21",
                                        "IMC1",
                                        "IMC2",
                                        "IMC3",
                                        "IMC4",
                                        "Y8",
                                        "Y16",
                                        "P010",
                                        "P016",
                                        "SamplerExternalOES",
                                        "Jpeg",
                                        "SamplerRect",
                                        "YUV420P10"};
    if (pixelFormat < 0 || pixelFormat >= NPixelFormats)
        return QString();
    return QString::fromLatin1(names[pixelFormat]);
}

/*!
    Returns \c true if this format equals \a other in every property; otherwise returns \c false.
*/
bool QVideoFrameFormat::operator==(const QVideoFrameFormat &other) const {
    return m_pixelFormat == other.m_pixelFormat && m_size == other.m_size && m_viewport == other.m_viewport &&
           m_direction == other.m_direction && m_frameRate == other.m_frameRate && m_colorSpace == other.m_colorSpace &&
           m_colorRange == other.m_colorRange && m_mirrored == other.m_mirrored;
}
