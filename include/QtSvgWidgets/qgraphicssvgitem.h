// MIT License - see LICENSE
#pragma once

#include <QtCore/qstring.h>
#include <QtWidgets/qgraphicsitem.h>

class QSvgRenderer;

class QGraphicsSvgItem : public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(QString elementId READ elementId WRITE setElementId)
    Q_PROPERTY(QSize maximumCacheSize READ maximumCacheSize WRITE setMaximumCacheSize)
public:
    /*! The type of the item, returned by type(). \omitvalue Type */
    enum { Type = 13 };

    explicit QGraphicsSvgItem(QGraphicsItem *parentItem = nullptr);
    explicit QGraphicsSvgItem(const QString &fileName, QGraphicsItem *parentItem = nullptr);
    ~QGraphicsSvgItem() override;

    void setSharedRenderer(QSvgRenderer *renderer);
    QSvgRenderer *renderer() const;
    void setElementId(const QString &id);
    QString elementId() const;
    void setCachingEnabled(bool enabled);
    bool isCachingEnabled() const;
    void setMaximumCacheSize(const QSize &size);
    QSize maximumCacheSize() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

private:
    void updateBounds();

    QSvgRenderer *m_renderer = nullptr;
    bool m_shared = false;
    QString m_elementId;
    QRectF m_bounds;
    QSize m_maxCache = QSize(1024, 768);
    bool m_caching = false;
    QMetaObject::Connection m_repaint;
};
