// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QWaylandClient;
class QWaylandCompositor;
class QWaylandSeat;
class QWaylandSurface;
class QWaylandView;

class QWaylandTouch : public QWaylandObject {
    Q_OBJECT
public:
    explicit QWaylandTouch(QWaylandSeat *seat, QObject *parent = nullptr);
    ~QWaylandTouch() override;

    QWaylandSeat *seat() const;
    QWaylandCompositor *compositor() const;

    virtual uint sendTouchPointEvent(QWaylandSurface *surface, int id, const QPointF &position,
                                     Qt::TouchPointState state);
    virtual void sendFrameEvent(QWaylandClient *client);
    virtual void sendCancelEvent(QWaylandClient *client);

    QWaylandSurface *focus() const;

private:
    friend class QWaylandSeat;
    struct Private;
    std::unique_ptr<Private> d;
};
