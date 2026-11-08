// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qsize.h>
#include <QtGui/qregion.h>

#include <memory>

class QPaintDevice;
class QPlatformBackingStore;
class QWindow;

class QBackingStore {
public:
    explicit QBackingStore(QWindow *window);
    ~QBackingStore();
    Q_DISABLE_COPY(QBackingStore)

    QWindow *window() const;
    QPaintDevice *paintDevice();
    void flush(const QRegion &region, QWindow *window = nullptr, const QPoint &offset = QPoint());
    void resize(const QSize &size);
    QSize size() const;
    void beginPaint(const QRegion &region);
    void endPaint();

private:
    std::unique_ptr<QPlatformBackingStore> m_handle;
    QWindow *m_window;
    QSize m_size;
};
