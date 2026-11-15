// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QWaylandCompositor;
class QWaylandOutput;
class QWaylandSeat;
class QWaylandSurface;
class QWaylandView;

class QWaylandPointer : public QWaylandObject {
    Q_OBJECT
    Q_PROPERTY(bool isButtonPressed READ isButtonPressed NOTIFY buttonPressedChanged)
public:
    explicit QWaylandPointer(QWaylandSeat *seat, QObject *parent = nullptr);
    ~QWaylandPointer() override;

    QWaylandSeat *seat() const;
    QWaylandCompositor *compositor() const;

    QWaylandOutput *output() const;
    void setOutput(QWaylandOutput *output);

    virtual uint sendMousePressEvent(Qt::MouseButton button);
    virtual uint sendMouseReleaseEvent(Qt::MouseButton button);
    virtual void sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos, const QPointF &outputSpacePos);
    virtual void sendMouseWheelEvent(Qt::Orientation orientation, int delta);

    QWaylandView *mouseFocus() const;
    QPointF currentLocalPosition() const;
    QPointF currentSpacePosition() const;

    bool isButtonPressed() const;

    void sendEnter(QWaylandView *view, const QPointF &localPos);
    void sendLeave();

signals:
    /*!
        This signal is emitted when the output changes.
    */
    void outputChanged() { emitSignal(&QWaylandPointer::outputChanged); }
    /*!
        This signal is emitted when a button goes down or the last one comes up.
    */
    void buttonPressedChanged() { emitSignal(&QWaylandPointer::buttonPressedChanged); }

private:
    friend class QWaylandSeat;
    struct Private;
    std::unique_ptr<Private> d;
};
