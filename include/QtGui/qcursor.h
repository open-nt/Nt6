// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>

class QDebug;
class QScreen;

class QCursor {
public:
    QCursor() = default;
    QCursor(Qt::CursorShape shape);

    Qt::CursorShape shape() const { return m_shape; }
    void setShape(Qt::CursorShape newShape);
    void swap(QCursor &other) noexcept;

    static QPoint pos();
    static QPoint pos(const QScreen *screen);
    static void setPos(int x, int y);
    static void setPos(const QPoint &p) { setPos(p.x(), p.y()); }
    static void setPos(QScreen *screen, int x, int y);
    static void setPos(QScreen *screen, const QPoint &p) { setPos(screen, p.x(), p.y()); }

    /*!
        Returns \c true if \a lhs and \a rhs have the same shape; otherwise
        returns \c false.
    */
    friend bool operator==(const QCursor &lhs, const QCursor &rhs) noexcept { return lhs.m_shape == rhs.m_shape; }
    /*!
        Returns \c true if \a lhs and \a rhs have different shapes;
        otherwise returns \c false.
    */
    friend bool operator!=(const QCursor &lhs, const QCursor &rhs) noexcept { return lhs.m_shape != rhs.m_shape; }

private:
    Qt::CursorShape m_shape = Qt::ArrowCursor;
};

QDebug operator<<(QDebug dbg, const QCursor &cursor);
