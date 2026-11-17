// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtWidgets/qsizepolicy.h>

class QGraphicsItem;

class QGraphicsLayoutItem {
public:
    explicit QGraphicsLayoutItem(QGraphicsLayoutItem *parent = nullptr, bool isLayout = false);
    virtual ~QGraphicsLayoutItem();

    void setSizePolicy(const QSizePolicy &policy);
    void setSizePolicy(QSizePolicy::Policy hPolicy, QSizePolicy::Policy vPolicy);
    QSizePolicy sizePolicy() const { return m_sizePolicy; }

    void setMinimumSize(const QSizeF &size);
    void setMinimumSize(qreal w, qreal h) { setMinimumSize(QSizeF(w, h)); }
    QSizeF minimumSize() const { return effectiveSizeHint(Qt::MinimumSize); }
    void setMinimumWidth(qreal width);
    qreal minimumWidth() const { return minimumSize().width(); }
    void setMinimumHeight(qreal height);
    qreal minimumHeight() const { return minimumSize().height(); }
    void setPreferredSize(const QSizeF &size);
    void setPreferredSize(qreal w, qreal h) { setPreferredSize(QSizeF(w, h)); }
    QSizeF preferredSize() const { return effectiveSizeHint(Qt::PreferredSize); }
    void setPreferredWidth(qreal width);
    qreal preferredWidth() const { return preferredSize().width(); }
    void setPreferredHeight(qreal height);
    qreal preferredHeight() const { return preferredSize().height(); }
    void setMaximumSize(const QSizeF &size);
    void setMaximumSize(qreal w, qreal h) { setMaximumSize(QSizeF(w, h)); }
    QSizeF maximumSize() const { return effectiveSizeHint(Qt::MaximumSize); }
    void setMaximumWidth(qreal width);
    qreal maximumWidth() const { return maximumSize().width(); }
    void setMaximumHeight(qreal height);
    qreal maximumHeight() const { return maximumSize().height(); }

    virtual void setGeometry(const QRectF &rect);
    QRectF geometry() const { return m_geometry; }
    virtual void getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const;
    QRectF contentsRect() const;
    QSizeF effectiveSizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const;
    virtual void updateGeometry();
    virtual bool isEmpty() const;

    QGraphicsLayoutItem *parentLayoutItem() const { return m_parent; }
    void setParentLayoutItem(QGraphicsLayoutItem *parent) { m_parent = parent; }
    bool isLayout() const { return m_isLayout; }
    QGraphicsItem *graphicsItem() const { return m_graphicsItem; }
    bool ownedByLayout() const { return m_ownedByLayout; }

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const = 0;
    void setGraphicsItem(QGraphicsItem *item) { m_graphicsItem = item; }
    void setOwnedByLayout(bool ownedByLayout) { m_ownedByLayout = ownedByLayout; }

private:
    QGraphicsLayoutItem(const QGraphicsLayoutItem &) = delete;
    QGraphicsLayoutItem &operator=(const QGraphicsLayoutItem &) = delete;
    void setUserSize(Qt::SizeHint which, Qt::Orientations orientations, const QSizeF &size);

    QGraphicsLayoutItem *m_parent;
    QGraphicsItem *m_graphicsItem = nullptr;
    QSizePolicy m_sizePolicy{QSizePolicy::Preferred, QSizePolicy::Preferred};
    QRectF m_geometry;
    QSizeF m_userSizes[3] = {QSizeF(-1, -1), QSizeF(-1, -1), QSizeF(-1, -1)}; // -1: not set
    bool m_isLayout;
    bool m_ownedByLayout = false;
};
