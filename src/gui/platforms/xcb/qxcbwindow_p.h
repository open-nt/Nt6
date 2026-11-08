// MIT License - see LICENSE
#pragma once

#include "qpa/qplatformbackingstore_p.h"
#include "qpa/qplatformwindow_p.h"

#include <QtGui/qimage.h>

#include <xcb/xcb.h>

class QXcbConnection;

class QXcbWindow : public QPlatformWindow {
public:
    QXcbWindow(QWindow *window, QXcbConnection *connection);
    ~QXcbWindow() override;

    QRect geometry() const override { return m_geometry; }
    void setGeometry(const QRect &rect) override;
    void setVisible(bool visible) override;
    void setWindowTitle(const QString &title) override;
    void requestActivateWindow() override;
    quintptr winId() const override { return m_id; }

    xcb_window_t id() const { return m_id; }
    // The geometry that the X server reported.
    void setServerGeometry(const QRect &rect) { m_geometry = rect; }

private:
    QRect toNative(const QRect &logical) const;

    QXcbConnection *m_connection;
    xcb_window_t m_id;
    QRect m_geometry;
};

class QXcbBackingStore : public QPlatformBackingStore {
public:
    QXcbBackingStore(QWindow *window, QXcbConnection *connection);
    ~QXcbBackingStore() override;

    QPaintDevice *paintDevice() override { return &m_image; }
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;
    QImage toImage() const override { return m_image; }

private:
    QXcbConnection *m_connection;
    QImage m_image;
    xcb_gcontext_t m_gc = 0;
    xcb_window_t m_gcWindow = 0;
};
