// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qgraphicslayoutitem.h>

class QEvent;
class QGraphicsWidget;

class QGraphicsLayout : public QGraphicsLayoutItem {
public:
    explicit QGraphicsLayout(QGraphicsLayoutItem *parent = nullptr);
    ~QGraphicsLayout() override;

    void setContentsMargins(qreal left, qreal top, qreal right, qreal bottom);
    void getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const override;

    void activate();
    bool isActivated() const { return m_activated; }
    virtual void invalidate();
    void updateGeometry() override;
    virtual void widgetEvent(QEvent *e);

    virtual int count() const = 0;
    virtual QGraphicsLayoutItem *itemAt(int i) const = 0;
    virtual void removeAt(int index) = 0;

    static void setInstantInvalidatePropagation(bool enable);
    static bool instantInvalidatePropagation();

protected:
    void addChildLayoutItem(QGraphicsLayoutItem *layoutItem);

private:
    friend class QGraphicsWidget;
    QGraphicsWidget *parentWidget() const; // the widget whose layout this is, through the parent layouts
    qreal m_margins[4] = {-1, -1, -1, -1}; // -1: from the style
    bool m_activated = false;
};
