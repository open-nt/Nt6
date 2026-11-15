// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>
#include <QtGui/qimage.h>

#include <memory>

struct wl_resource;

namespace QtWaylandPrivate {
class Buffer;
}

class QWaylandBufferRef {
public:
    enum Origin { OriginTopLeft, OriginBottomLeft };

    QWaylandBufferRef();
    QWaylandBufferRef(const QWaylandBufferRef &ref);
    ~QWaylandBufferRef();

    QWaylandBufferRef &operator=(const QWaylandBufferRef &ref);
    bool operator==(const QWaylandBufferRef &ref) const;
    /*!
        Returns \c true if \a ref refers to another buffer.
    */
    bool operator!=(const QWaylandBufferRef &ref) const { return !(*this == ref); }

    bool isNull() const;
    bool hasBuffer() const;
    bool hasContent() const;
    bool hasProtectedContent() const;
    bool isDestroyed() const;
    wl_resource *wl_buffer() const;

    QSize size() const;
    Origin origin() const;

    bool isSharedMemory() const;
    QImage image() const;

    void lock();
    void unlock();
    bool isLocked() const;

private:
    friend class QWaylandSurface;
    friend class QWaylandView;
    explicit QWaylandBufferRef(const std::shared_ptr<QtWaylandPrivate::Buffer> &buffer);
    std::shared_ptr<QtWaylandPrivate::Buffer> m_buffer;
};
