// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtWidgets/qgraphicslayout.h>
#include <QtWidgets/qsizepolicy.h>

#include <memory>
#include <vector>

class QGraphicsAnchorLayout;

class QGraphicsAnchor : public QObject {
    Q_OBJECT
public:
    ~QGraphicsAnchor() override;

    void setSpacing(qreal spacing);
    void unsetSpacing();
    qreal spacing() const;
    void setSizePolicy(QSizePolicy::Policy policy);
    QSizePolicy::Policy sizePolicy() const { return m_policy; }

private:
    friend class QGraphicsAnchorLayout;
    explicit QGraphicsAnchor(QGraphicsAnchorLayout *parent);

    QGraphicsAnchorLayout *m_layout;
    qreal m_spacing = 0;
    bool m_spacingSet = false;
    QSizePolicy::Policy m_policy = QSizePolicy::Fixed;
    QGraphicsLayoutItem *m_first = nullptr;
    QGraphicsLayoutItem *m_second = nullptr;
    Qt::AnchorPoint m_firstEdge = Qt::AnchorLeft;
    Qt::AnchorPoint m_secondEdge = Qt::AnchorLeft;
};

class QGraphicsAnchorLayout : public QGraphicsLayout {
public:
    explicit QGraphicsAnchorLayout(QGraphicsLayoutItem *parent = nullptr);
    ~QGraphicsAnchorLayout() override;

    QGraphicsAnchor *addAnchor(QGraphicsLayoutItem *firstItem, Qt::AnchorPoint firstEdge,
                               QGraphicsLayoutItem *secondItem, Qt::AnchorPoint secondEdge);
    QGraphicsAnchor *anchor(QGraphicsLayoutItem *firstItem, Qt::AnchorPoint firstEdge,
                            QGraphicsLayoutItem *secondItem, Qt::AnchorPoint secondEdge);
    void addCornerAnchors(QGraphicsLayoutItem *firstItem, Qt::Corner firstCorner, QGraphicsLayoutItem *secondItem,
                          Qt::Corner secondCorner);
    void addAnchors(QGraphicsLayoutItem *firstItem, QGraphicsLayoutItem *secondItem,
                    Qt::Orientations orientations = Qt::Horizontal | Qt::Vertical);

    void setHorizontalSpacing(qreal spacing);
    void setVerticalSpacing(qreal spacing);
    void setSpacing(qreal spacing);
    qreal horizontalSpacing() const { return m_horizontalSpacing; }
    qreal verticalSpacing() const { return m_verticalSpacing; }

    void removeAt(int index) override;
    void setGeometry(const QRectF &rect) override;
    int count() const override;
    QGraphicsLayoutItem *itemAt(int index) const override;
    void invalidate() override;

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;

private:
    friend class QGraphicsAnchor;
    qreal anchorSpacing(const QGraphicsAnchor *anchor, Qt::Orientation orientation) const;
    void solve(Qt::SizeHint which, const QSizeF *fixed, std::vector<QRectF> *result, QSizeF *layoutSize) const;

    std::vector<QGraphicsAnchor *> m_anchors;
    QList<QGraphicsLayoutItem *> m_items;
    qreal m_horizontalSpacing = 6;
    qreal m_verticalSpacing = 6;
};
