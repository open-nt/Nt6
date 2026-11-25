// MIT License - see LICENSE
//
// The virtualcan backend: a CAN bus inside the process. Every device of an interface name (can0, or any other) is a
// node on the bus of that name; a frame that one sends reaches the others, and itself with ReceiveOwnKey, by a queued
// call into the thread of each, so that the devices may live in different threads.
#include <QtSerialBus/qcanbusdevice.h>
#include <QtSerialBus/qcanbusfactory.h>

#include "qcanbus_p.h"

#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>

#include <chrono>
#include <mutex>

namespace QtSerialBusPrivate {

namespace {

class VirtualCanDevice;

struct Buses {
    std::mutex mutex;
    QMap<QString, QList<VirtualCanDevice *>> nodes;
};

Buses &buses() {
    static Buses b;
    return b;
}

qint64 microsecondsNow() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

class VirtualCanDevice : public QCanBusDevice {
public:
    explicit VirtualCanDevice(QString name) : m_name(std::move(name)) {}
    ~VirtualCanDevice() override { leave(); }
    VirtualCanDevice(const VirtualCanDevice &) = delete;
    VirtualCanDevice &operator=(const VirtualCanDevice &) = delete;

    bool writeFrame(const QCanBusFrame &frame) override {
        if (state() != ConnectedState) {
            setError(QStringLiteral("cannot write a frame to a device that is not connected"), WriteError);
            return false;
        }
        if (!frame.isValid() || frame.frameType() == QCanBusFrame::ErrorFrame) {
            setError(QStringLiteral("cannot write an invalid frame or an error frame"), WriteError);
            return false;
        }
        if (frame.hasFlexibleDataRateFormat() && !configurationParameter(CanFdKey).toBool()) {
            setError(QStringLiteral("cannot write a CAN FD frame without CanFdKey"), WriteError);
            return false;
        }
        QCanBusFrame sent = frame;
        sent.setTimeStamp(QCanBusFrame::TimeStamp::fromMicroSeconds(microsecondsNow()));
        QList<QPointer<VirtualCanDevice>> targets;
        {
            Buses &b = buses();
            const std::lock_guard<std::mutex> lock(b.mutex);
            for (VirtualCanDevice *node : b.nodes.value(m_name))
                if (node != this || configurationParameter(ReceiveOwnKey).toBool())
                    targets.append(QPointer<VirtualCanDevice>(node));
        }
        for (const QPointer<VirtualCanDevice> &target : targets) {
            QCanBusFrame copy = sent;
            copy.setLocalEcho(target.data() == this);
            QMetaObject::invokeMethod(
                target.data(),
                [target, copy] {
                    if (target)
                        target->receive(copy);
                },
                Qt::QueuedConnection);
        }
        QPointer<VirtualCanDevice> self(this);
        QMetaObject::invokeMethod(
            this,
            [self] {
                if (self && self->state() == ConnectedState)
                    emit self->framesWritten(1);
            },
            Qt::QueuedConnection);
        return true;
    }

    QString interpretErrorFrame(const QCanBusFrame &errorFrame) override {
        (void)errorFrame;
        return QString(); // the virtual bus has no errors
    }

    QCanBusDeviceInfo deviceInfo() const override {
        return createDeviceInfo(QStringLiteral("virtualcan"), m_name, QString(), QStringLiteral("Virtual CAN bus"),
                                QString(), 0, true, true);
    }

protected:
    bool open() override {
        Buses &b = buses();
        const std::lock_guard<std::mutex> lock(b.mutex);
        b.nodes[m_name].append(this);
        return true;
    }

    void close() override { leave(); }

private:
    void leave() {
        Buses &b = buses();
        const std::lock_guard<std::mutex> lock(b.mutex);
        auto it = b.nodes.find(m_name);
        if (it != b.nodes.end()) {
            it->removeAll(this);
            if (it->isEmpty())
                b.nodes.erase(it);
        }
    }

    void receive(const QCanBusFrame &frame) {
        if (state() != ConnectedState)
            return;
        if (frame.hasFlexibleDataRateFormat() && !configurationParameter(CanFdKey).toBool())
            return;
        if (!passesFilters(frame, configurationParameter(RawFilterKey)))
            return;
        enqueueReceivedFrames({frame});
    }

    QString m_name;
};

class VirtualCanFactory : public QCanBusFactory {
public:
    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override {
        if (interfaceName.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("a virtual CAN interface needs a name");
            return nullptr;
        }
        return new VirtualCanDevice(interfaceName);
    }

    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override {
        if (errorMessage)
            errorMessage->clear();
        QList<QCanBusDeviceInfo> list;
        for (const char *name : {"can0", "can1"}) {
            VirtualCanDevice device{QString::fromLatin1(name)};
            list.append(device.deviceInfo());
        }
        return list;
    }
};

} // namespace

QCanBusFactory *virtualCanFactory() {
    static VirtualCanFactory factory;
    return &factory;
}

} // namespace QtSerialBusPrivate
