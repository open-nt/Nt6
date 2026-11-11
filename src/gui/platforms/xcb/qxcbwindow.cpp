// MIT License - see LICENSE
#include "qxcbwindow_p.h"

#include "qxcbconnection_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qwindow.h>

#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

QXcbWindow::QXcbWindow(QWindow *window, QXcbConnection *connection)
    : QPlatformWindow(window), m_connection(connection), m_id(xcb_generate_id(connection->xcb())),
      m_geometry(window->geometry()) {
    xcb_connection_t *c = connection->xcb();
    const uint32_t mask = XCB_CW_BIT_GRAVITY | XCB_CW_EVENT_MASK;
    const uint32_t values[] = {XCB_GRAVITY_NORTH_WEST, XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                                                           XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                                                           XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                                           XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_ENTER_WINDOW |
                                                           XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
    const QRect r = toNative(m_geometry);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, m_id, connection->screen()->root, static_cast<int16_t>(r.x()),
                      static_cast<int16_t>(r.y()), static_cast<uint16_t>(std::max(1, r.width())),
                      static_cast<uint16_t>(std::max(1, r.height())), 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      connection->visual(), mask, values);

    const xcb_atom_t protocols[] = {connection->wmDeleteWindow(), connection->netWmPing()};
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, connection->wmProtocols(), XCB_ATOM_ATOM, 32, 2, protocols);
    const uint32_t pid = static_cast<uint32_t>(::getpid());
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, connection->netWmPid(), XCB_ATOM_CARDINAL, 32, 1, &pid);

    // WM_CLASS: instance name and class name, each ended by a null byte.
    const QByteArray name = QCoreApplication::applicationName().toUtf8();
    QByteArray wmClass = name;
    wmClass.append('\0');
    wmClass.append(name);
    wmClass.append('\0');
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
                        static_cast<uint32_t>(wmClass.size()), wmClass.constData());
    connection->registerWindow(m_id, this);
    connection->flush();
}

QXcbWindow::~QXcbWindow() {
    m_connection->unregisterWindow(m_id);
    xcb_destroy_window(m_connection->xcb(), m_id);
    m_connection->flush();
}

void QXcbWindow::setCursor(Qt::CursorShape shape) {
    const xcb_cursor_t cursor = m_connection->cursor(shape);
    xcb_change_window_attributes(m_connection->xcb(), m_id, XCB_CW_CURSOR, &cursor);
    m_connection->flush();
}

void QXcbWindow::setGeometry(const QRect &rect) {
    m_geometry = rect;
    const QRect native = toNative(rect);
    const uint32_t values[] = {static_cast<uint32_t>(native.x()), static_cast<uint32_t>(native.y()),
                               static_cast<uint32_t>(std::max(1, native.width())),
                               static_cast<uint32_t>(std::max(1, native.height()))};
    xcb_configure_window(m_connection->xcb(), m_id,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         values);
    m_connection->flush();
}

QRect QXcbWindow::toNative(const QRect &logical) const {
    return QRect(m_connection->toNative(logical.x()), m_connection->toNative(logical.y()),
                 m_connection->toNative(logical.width()), m_connection->toNative(logical.height()));
}

void QXcbWindow::setVisible(bool visible) {
    if (visible)
        xcb_map_window(m_connection->xcb(), m_id);
    else
        xcb_unmap_window(m_connection->xcb(), m_id);
    m_connection->flush();
}

void QXcbWindow::setTransientParent(const QPlatformWindow *parent) {
    xcb_connection_t *c = m_connection->xcb();
    const xcb_atom_t windowType = m_connection->atom("_NET_WM_WINDOW_TYPE");
    if (parent) {
        const xcb_window_t parentId = static_cast<xcb_window_t>(parent->winId());
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, XCB_ATOM_WM_TRANSIENT_FOR, XCB_ATOM_WINDOW, 32, 1,
                            &parentId);
        const xcb_atom_t dialog = m_connection->atom("_NET_WM_WINDOW_TYPE_DIALOG");
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, windowType, XCB_ATOM_ATOM, 32, 1, &dialog);
    } else {
        xcb_delete_property(c, m_id, XCB_ATOM_WM_TRANSIENT_FOR);
        xcb_delete_property(c, m_id, windowType);
    }
    m_connection->flush();
}

void QXcbWindow::setWindowTitle(const QString &title) {
    const QByteArray utf8 = title.toUtf8();
    xcb_connection_t *c = m_connection->xcb();
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, m_connection->netWmName(), m_connection->utf8String(), 8,
                        static_cast<uint32_t>(utf8.size()), utf8.constData());
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, m_id, XCB_ATOM_WM_NAME, m_connection->utf8String(), 8,
                        static_cast<uint32_t>(utf8.size()), utf8.constData());
    m_connection->flush();
}

void QXcbWindow::requestActivateWindow() {
    xcb_set_input_focus(m_connection->xcb(), XCB_INPUT_FOCUS_POINTER_ROOT, m_id, XCB_CURRENT_TIME);
    m_connection->flush();
}

QXcbBackingStore::QXcbBackingStore(QWindow *window, QXcbConnection *connection)
    : QPlatformBackingStore(window), m_connection(connection) {}

QXcbBackingStore::~QXcbBackingStore() {
    if (m_gc)
        xcb_free_gc(m_connection->xcb(), m_gc);
}

void QXcbBackingStore::resize(const QSize &size, const QRegion &staticContents) {
    (void)staticContents;
    const qreal ratio = m_connection->scale();
    const QSize native(m_connection->toNative(size.width()), m_connection->toNative(size.height()));
    if (m_image.size() == native && m_image.devicePixelRatio() == ratio)
        return;
    QImage image(native, QImage::Format_RGB32);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::white);
    m_image = image;
}

void QXcbBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset) {
    if (m_image.isNull() || !window || !window->handle())
        return;
    xcb_connection_t *c = m_connection->xcb();
    const xcb_window_t target = static_cast<xcb_window_t>(window->winId());
    if (!m_gc || m_gcWindow != target) {
        if (m_gc)
            xcb_free_gc(c, m_gc);
        m_gc = xcb_generate_id(c);
        m_gcWindow = target;
        const uint32_t noExposures = 0;
        xcb_create_gc(c, m_gc, target, XCB_GC_GRAPHICS_EXPOSURES, &noExposures);
    }
    // 24 bytes of the request are the header of PutImage.
    const size_t maxBytes = m_connection->maxRequestBytes() - 24;
    std::vector<uchar> buffer;
    // The region is in window coordinates; the image has the pixels of the screen.
    const QPoint nativeOffset(m_connection->toNative(offset.x()), m_connection->toNative(offset.y()));
    for (const QRect &logical : region.rects()) {
        const QRect shifted = logical.translated(offset);
        const int left = m_connection->toNative(shifted.x());
        const int top = m_connection->toNative(shifted.y());
        const QRect rectIn(left, top, m_connection->toNative(shifted.x() + shifted.width()) - left,
                           m_connection->toNative(shifted.y() + shifted.height()) - top);
        const QRect rect = rectIn.intersected(m_image.rect());
        if (rect.isEmpty())
            continue;
        const size_t rowBytes = static_cast<size_t>(rect.width()) * 4;
        const int rowsPerRequest = static_cast<int>(std::max<size_t>(1, std::min<size_t>(maxBytes / rowBytes, 0xffff)));
        for (int y = rect.top(); y <= rect.bottom(); y += rowsPerRequest) {
            const int rows = std::min(rowsPerRequest, rect.bottom() - y + 1);
            buffer.resize(rowBytes * static_cast<size_t>(rows));
            for (int r = 0; r < rows; ++r)
                std::memcpy(buffer.data() + rowBytes * static_cast<size_t>(r),
                            m_image.constScanLine(y + r) + static_cast<size_t>(rect.left()) * 4, rowBytes);
            xcb_put_image(c, XCB_IMAGE_FORMAT_Z_PIXMAP, target, m_gc, static_cast<uint16_t>(rect.width()),
                          static_cast<uint16_t>(rows), static_cast<int16_t>(rect.left() - nativeOffset.x()),
                          static_cast<int16_t>(y - nativeOffset.y()), 0, m_connection->depth(),
                          static_cast<uint32_t>(buffer.size()), buffer.data());
        }
    }
    m_connection->flush();
}
