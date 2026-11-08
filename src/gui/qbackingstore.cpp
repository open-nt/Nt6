// MIT License - see LICENSE
#include <QtGui/qbackingstore.h>

#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>

/*!
    \class QBackingStore
    \brief The QBackingStore class provides the backing store for a QWindow.

    A backing store keeps the pixels of a window. The window paints into the
    paintDevice() between beginPaint() and endPaint(), and flush() puts the
    result on the screen:

    \code
    backingStore.resize(window.size());
    backingStore.beginPaint(region);
    QPainter painter(backingStore.paintDevice());
    painter.fillRect(region.boundingRect(), Qt::white);
    painter.end();
    backingStore.endPaint();
    backingStore.flush(region);
    \endcode

    QRasterWindow does this for you.

    \differences
    There is no \c staticContents(), \c hasStaticContents() or \c scroll(). The
    paint device is always a QImage.
    \enddifferences

    \sa QRasterWindow, QWindow
*/

/*!
    Constructs a backing store for \a window. A QGuiApplication must exist.
*/
QBackingStore::QBackingStore(QWindow *window) : m_window(window) {
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    if (integration)
        m_handle.reset(integration->createPlatformBackingStore(window));
}

/*!
    Destroys the backing store.
*/
QBackingStore::~QBackingStore() = default;

/*!
    Returns the window the backing store belongs to.
*/
QWindow *QBackingStore::window() const { return m_window; }

/*!
    Returns the device to paint on, which has the size that resize() set.
    Returns \c nullptr if there is no platform.
*/
QPaintDevice *QBackingStore::paintDevice() { return m_handle ? m_handle->paintDevice() : nullptr; }

/*!
    Puts \a region of the backing store on the screen, in \a window or, if that
    is \c nullptr, in the window of the backing store. \a offset is the position
    of the window inside the backing store.
*/
void QBackingStore::flush(const QRegion &region, QWindow *window, const QPoint &offset) {
    if (m_handle)
        m_handle->flush(window ? window : m_window, region, offset);
}

/*!
    Resizes the backing store to \a size. The contents are not preserved.

    \sa size()
*/
void QBackingStore::resize(const QSize &size) {
    m_size = size;
    if (m_handle)
        m_handle->resize(size, QRegion());
}

/*!
    Returns the size that resize() set.
*/
QSize QBackingStore::size() const { return m_size; }

/*!
    Prepares the backing store for painting \a region.

    \sa endPaint()
*/
void QBackingStore::beginPaint(const QRegion &region) {
    if (m_handle)
        m_handle->beginPaint(region);
}

/*!
    Ends the painting that beginPaint() started.
*/
void QBackingStore::endPaint() {
    if (m_handle)
        m_handle->endPaint();
}
