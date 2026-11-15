// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <memory>

struct wl_display;

class QInputEvent;
class QMimeData;
class QWaylandClient;
class QWaylandOutput;
class QWaylandSeat;
class QWaylandSurface;
class QWindow;

namespace QtWaylandPrivate {
class Connection;
class Display;
class Resource;
struct Interface;
} // namespace QtWaylandPrivate

class QWaylandCompositor : public QWaylandObject {
    Q_OBJECT
    Q_PROPERTY(QByteArray socketName READ socketName WRITE setSocketName NOTIFY socketNameChanged)
    Q_PROPERTY(bool created READ isCreated NOTIFY createdChanged)
    Q_PROPERTY(bool retainedSelection READ retainedSelectionEnabled WRITE setRetainedSelectionEnabled NOTIFY
                   retainedSelectionChanged)
    Q_PROPERTY(QWaylandOutput *defaultOutput READ defaultOutput WRITE setDefaultOutput NOTIFY defaultOutputChanged)
    Q_PROPERTY(bool useHardwareIntegrationExtension READ useHardwareIntegrationExtension WRITE
                   setUseHardwareIntegrationExtension NOTIFY useHardwareIntegrationExtensionChanged)
public:
    explicit QWaylandCompositor(QObject *parent = nullptr);
    ~QWaylandCompositor() override;

    void create();
    bool isCreated() const;

    void setSocketName(const QByteArray &name);
    QByteArray socketName() const;

    Q_INVOKABLE void addSocketDescriptor(int fd);

    wl_display *display() const;
    std::uint32_t nextSerial();

    QList<QWaylandClient *> clients() const;
    Q_INVOKABLE void destroyClientForSurface(QWaylandSurface *surface);
    Q_INVOKABLE void destroyClient(QWaylandClient *client);

    QList<QWaylandSurface *> surfaces() const;
    QList<QWaylandSurface *> surfacesForClient(QWaylandClient *client) const;

    Q_INVOKABLE QWaylandOutput *outputFor(QWindow *window) const;

    QWaylandOutput *defaultOutput() const;
    void setDefaultOutput(QWaylandOutput *output);
    QList<QWaylandOutput *> outputs() const;

    /*!
        Returns \c true if the compositor is running in the thread of the application object.
    */
    bool isRunning() const { return isCreated(); }

    void setRetainedSelectionEnabled(bool enabled);
    bool retainedSelectionEnabled() const;
    void overrideSelection(const QMimeData *data);

    QWaylandSeat *defaultSeat() const;
    QWaylandSeat *seatFor(QInputEvent *inputEvent);

    void setUseHardwareIntegrationExtension(bool use);
    bool useHardwareIntegrationExtension() const;

    void grabSurface(QWaylandSeat *seat, const QWaylandSurface *surface);

public slots:
    void processWaylandEvents();

signals:
    /*!
        This signal is emitted when create() has finished.
    */
    void createdChanged() { emitSignal(&QWaylandCompositor::createdChanged); }
    /*!
        This signal is emitted when the socket name changes to \a socketName.
    */
    void socketNameChanged(const QByteArray &socketName) {
        emitSignal(&QWaylandCompositor::socketNameChanged, socketName);
    }
    /*!
        This signal is emitted when the retained-selection setting changes.
    */
    void retainedSelectionChanged() { emitSignal(&QWaylandCompositor::retainedSelectionChanged); }
    /*!
        This signal is emitted when a client asks for a surface: \a client, the object \a id and the protocol
        \a version. Connect to it to create a subclass of QWaylandSurface; otherwise a QWaylandSurface is made.
    */
    void surfaceRequested(QWaylandClient *client, uint id, int version) {
        emitSignal(&QWaylandCompositor::surfaceRequested, client, id, version);
    }
    /*!
        This signal is emitted when \a surface has been created.
    */
    void surfaceCreated(QWaylandSurface *surface) { emitSignal(&QWaylandCompositor::surfaceCreated, surface); }
    /*!
        This signal is emitted just before \a surface is destroyed.
    */
    void surfaceAboutToBeDestroyed(QWaylandSurface *surface) {
        emitSignal(&QWaylandCompositor::surfaceAboutToBeDestroyed, surface);
    }
    /*!
        This signal is emitted when the hardware-integration setting changes.
    */
    void useHardwareIntegrationExtensionChanged() {
        emitSignal(&QWaylandCompositor::useHardwareIntegrationExtensionChanged);
    }
    /*!
        This signal is emitted when the default output changes.
    */
    void defaultOutputChanged() { emitSignal(&QWaylandCompositor::defaultOutputChanged); }
    /*!
        This signal is emitted when the default seat changes from \a oldSeat to \a newSeat.
    */
    void defaultSeatChanged(QWaylandSeat *newSeat, QWaylandSeat *oldSeat) {
        emitSignal(&QWaylandCompositor::defaultSeatChanged, newSeat, oldSeat);
    }
    /*!
        This signal is emitted when \a output has been added to the compositor.
    */
    void outputAdded(QWaylandOutput *output) { emitSignal(&QWaylandCompositor::outputAdded, output); }
    /*!
        This signal is emitted when \a output has been removed from the compositor.
    */
    void outputRemoved(QWaylandOutput *output) { emitSignal(&QWaylandCompositor::outputRemoved, output); }

protected:
    virtual void retainedSelectionReceived(QMimeData *mimeData);
    virtual QWaylandSeat *createSeat();
    virtual QWaylandSurface *createSurface(QWaylandClient *client, uint id, int version);

    bool event(QEvent *event) override;

private:
    friend class QWaylandClient;
    friend class QWaylandOutput;
    friend class QWaylandSeat;
    friend class QWaylandSurface;
    friend class QWaylandXdgShell;
    friend struct QWaylandCompositorPrivateAccess;
    struct Private;
    std::unique_ptr<Private> d;
};
