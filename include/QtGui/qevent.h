// MIT License - see LICENSE
#pragma once

#include <QtCore/qcoreevent.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qregion.h>

class QInputEvent : public QEvent {
public:
    QInputEvent(Type type, Qt::KeyboardModifiers modifiers = Qt::NoModifier) : QEvent(type), m_modifiers(modifiers) {}
    ~QInputEvent() override;

    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    void setModifiers(Qt::KeyboardModifiers modifiers) { m_modifiers = modifiers; }

private:
    Qt::KeyboardModifiers m_modifiers;
};

class QMouseEvent : public QInputEvent {
public:
    QMouseEvent(Type type, const QPointF &localPos, const QPointF &globalPos, Qt::MouseButton button,
                Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    ~QMouseEvent() override;

    Qt::MouseButton button() const { return m_button; }
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }
    QPoint pos() const { return m_local.toPoint(); }
    QPoint globalPos() const { return m_global.toPoint(); }
    int x() const { return pos().x(); }
    int y() const { return pos().y(); }

private:
    QPointF m_local;
    QPointF m_global;
    Qt::MouseButton m_button;
    Qt::MouseButtons m_buttons;
};

class QWheelEvent : public QInputEvent {
public:
    QWheelEvent(const QPointF &pos, const QPointF &globalPos, const QPoint &pixelDelta, const QPoint &angleDelta,
                Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    ~QWheelEvent() override;

    QPoint pixelDelta() const { return m_pixelDelta; }
    QPoint angleDelta() const { return m_angleDelta; }
    Qt::MouseButtons buttons() const { return m_buttons; }
    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }

private:
    QPointF m_local;
    QPointF m_global;
    QPoint m_pixelDelta;
    QPoint m_angleDelta;
    Qt::MouseButtons m_buttons;
};

class QKeyEvent : public QInputEvent {
public:
    QKeyEvent(Type type, int key, Qt::KeyboardModifiers modifiers, const QString &text = QString(),
              bool autorep = false, ushort count = 1);
    ~QKeyEvent() override;

    int key() const { return m_key; }
    QString text() const { return m_text; }
    bool isAutoRepeat() const { return m_autorep; }
    int count() const { return m_count; }

private:
    int m_key;
    QString m_text;
    bool m_autorep;
    ushort m_count;
};

class QFocusEvent : public QEvent {
public:
    explicit QFocusEvent(Type type) : QEvent(type) {}
    ~QFocusEvent() override;

    bool gotFocus() const { return type() == FocusIn; }
    bool lostFocus() const { return type() == FocusOut; }
};

class QEnterEvent : public QEvent {
public:
    QEnterEvent(const QPointF &localPos, const QPointF &globalPos)
        : QEvent(Enter), m_local(localPos), m_global(globalPos) {}
    ~QEnterEvent() override;

    QPointF position() const { return m_local; }
    QPointF globalPosition() const { return m_global; }
    QPoint pos() const { return m_local.toPoint(); }
    QPoint globalPos() const { return m_global.toPoint(); }

private:
    QPointF m_local;
    QPointF m_global;
};

class QPaintEvent : public QEvent {
public:
    explicit QPaintEvent(const QRegion &paintRegion) : QEvent(Paint), m_region(paintRegion) {}
    explicit QPaintEvent(const QRect &paintRect) : QEvent(Paint), m_region(paintRect) {}
    ~QPaintEvent() override;

    const QRegion &region() const { return m_region; }
    QRect rect() const { return m_region.boundingRect(); }

private:
    QRegion m_region;
};

class QExposeEvent : public QEvent {
public:
    explicit QExposeEvent(const QRegion &exposeRegion) : QEvent(Expose), m_region(exposeRegion) {}
    ~QExposeEvent() override;

    const QRegion &region() const { return m_region; }

private:
    QRegion m_region;
};

class QResizeEvent : public QEvent {
public:
    QResizeEvent(const QSize &size, const QSize &oldSize) : QEvent(Resize), m_size(size), m_oldSize(oldSize) {}
    ~QResizeEvent() override;

    const QSize &size() const { return m_size; }
    const QSize &oldSize() const { return m_oldSize; }

private:
    QSize m_size;
    QSize m_oldSize;
};

class QMoveEvent : public QEvent {
public:
    QMoveEvent(const QPoint &pos, const QPoint &oldPos) : QEvent(Move), m_pos(pos), m_oldPos(oldPos) {}
    ~QMoveEvent() override;

    const QPoint &pos() const { return m_pos; }
    const QPoint &oldPos() const { return m_oldPos; }

private:
    QPoint m_pos;
    QPoint m_oldPos;
};

class QShowEvent : public QEvent {
public:
    QShowEvent() : QEvent(Show) {}
    ~QShowEvent() override;
};

class QHideEvent : public QEvent {
public:
    QHideEvent() : QEvent(Hide) {}
    ~QHideEvent() override;
};

class QCloseEvent : public QEvent {
public:
    QCloseEvent() : QEvent(Close) {}
    ~QCloseEvent() override;
};
