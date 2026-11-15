// MIT License - see LICENSE
#pragma once

#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

class QWaylandClient;
class QWaylandCompositor;
class QWaylandKeymap;
class QWaylandSeat;
class QWaylandSurface;

class QWaylandKeyboard : public QWaylandObject {
    Q_OBJECT
    Q_PROPERTY(quint32 repeatRate READ repeatRate WRITE setRepeatRate NOTIFY repeatRateChanged)
    Q_PROPERTY(quint32 repeatDelay READ repeatDelay WRITE setRepeatDelay NOTIFY repeatDelayChanged)
public:
    explicit QWaylandKeyboard(QWaylandSeat *seat, QObject *parent = nullptr);
    ~QWaylandKeyboard() override;

    QWaylandSeat *seat() const;
    QWaylandCompositor *compositor() const;

    quint32 repeatRate() const;
    void setRepeatRate(quint32 rate);

    quint32 repeatDelay() const;
    void setRepeatDelay(quint32 delay);

    virtual void setFocus(QWaylandSurface *surface);
    virtual void sendKeyModifiers(QWaylandClient *client, uint serial);
    virtual void sendKeyPressEvent(uint code);
    virtual void sendKeyReleaseEvent(uint code);
    uint keyToScanCode(int qtKey) const;

    QWaylandSurface *focus() const;
    QWaylandClient *focusClient() const;

signals:
    /*!
        This signal is emitted when the keyboard focus moves to \a surface.
    */
    void focusChanged(QWaylandSurface *surface) { emitSignal(&QWaylandKeyboard::focusChanged, surface); }
    /*!
        This signal is emitted when the repeat rate changes to \a repeatRate.
    */
    void repeatRateChanged(quint32 repeatRate) { emitSignal(&QWaylandKeyboard::repeatRateChanged, repeatRate); }
    /*!
        This signal is emitted when the repeat delay changes to \a repeatDelay.
    */
    void repeatDelayChanged(quint32 repeatDelay) { emitSignal(&QWaylandKeyboard::repeatDelayChanged, repeatDelay); }

private:
    friend class QWaylandSeat;
    struct Private;
    std::unique_ptr<Private> d;
};
