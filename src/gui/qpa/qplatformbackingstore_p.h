// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <QtGui/qregion.h>

class QPaintDevice;
class QWindow;

// Holds the pixels of one window and gets them on the screen.
class QPlatformBackingStore {
public:
    explicit QPlatformBackingStore(QWindow *window) : m_window(window) {}
    virtual ~QPlatformBackingStore();

    QWindow *window() const { return m_window; }
    virtual QPaintDevice *paintDevice() = 0;
    // Copies `region` of the backing store to `window`; `offset` is the position of the window in
    // the backing store.
    virtual void flush(QWindow *window, const QRegion &region, const QPoint &offset) = 0;
    virtual void resize(const QSize &size, const QRegion &staticContents) = 0;
    virtual void beginPaint(const QRegion &region) { (void)region; }
    virtual void endPaint() {}
    virtual QImage toImage() const { return QImage(); }

private:
    QWindow *m_window;
};
