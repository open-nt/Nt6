// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>

#include <memory>

class QDBusConnectionInterface;
class QDBusConnectionPrivate;
class QDBusPendingCall;
class QDBusVirtualObject;

/*!
    \namespace QDBus
    \brief The QDBus namespace contains the call modes of Nt D-Bus.

    \differences
    None.
    \enddifferences
*/
namespace QDBus {
/*!
    \enum QDBus::CallMode

    Selects how a call waits for its reply.

    \value NoBlock Sends the call and does not wait; there is no reply.
    \value Block Blocks the calling thread until the reply arrives, without running its event loop.
    \value BlockWithGui Runs a local event loop until the reply arrives.
    \value AutoDetect The same as \c Block.
*/
enum CallMode { NoBlock, Block, BlockWithGui, AutoDetect };
} // namespace QDBus

class QDBusConnection {
    Q_GADGET
public:
    enum BusType { SessionBus, SystemBus, ActivationBus };
    Q_ENUM(BusType)

    enum RegisterOption {
        ExportAdaptors = 0x01,
        ExportScriptableSlots = 0x10,
        ExportScriptableSignals = 0x20,
        ExportScriptableProperties = 0x40,
        ExportScriptableInvokables = 0x80,
        ExportScriptableContents = 0xf0,
        ExportNonScriptableSlots = 0x100,
        ExportNonScriptableSignals = 0x200,
        ExportNonScriptableProperties = 0x400,
        ExportNonScriptableInvokables = 0x800,
        ExportNonScriptableContents = 0xf00,
        ExportAllSlots = ExportScriptableSlots | ExportNonScriptableSlots,
        ExportAllSignals = ExportScriptableSignals | ExportNonScriptableSignals,
        ExportAllProperties = ExportScriptableProperties | ExportNonScriptableProperties,
        ExportAllInvokables = ExportScriptableInvokables | ExportNonScriptableInvokables,
        ExportAllContents = ExportScriptableContents | ExportNonScriptableContents,
        ExportChildObjects = 0x1000
    };
    Q_DECLARE_FLAGS(RegisterOptions, RegisterOption)

    enum UnregisterMode { UnregisterNode, UnregisterTree };
    enum VirtualObjectRegisterOption { SingleNode = 0x0, SubPath = 0x1 };
    Q_DECLARE_FLAGS(VirtualObjectRegisterOptions, VirtualObjectRegisterOption)

    enum ConnectionCapability { UnixFileDescriptorPassing = 0x1 };
    Q_DECLARE_FLAGS(ConnectionCapabilities, ConnectionCapability)

    explicit QDBusConnection(const QString &name);
    QDBusConnection(const QDBusConnection &other);
    QDBusConnection(QDBusConnection &&other) noexcept;
    QDBusConnection &operator=(const QDBusConnection &other);
    QDBusConnection &operator=(QDBusConnection &&other) noexcept;
    ~QDBusConnection();
    void swap(QDBusConnection &other) noexcept;

    bool isConnected() const;
    QString baseService() const;
    QDBusError lastError() const;
    QString name() const;
    ConnectionCapabilities connectionCapabilities() const;

    bool send(const QDBusMessage &message) const;
    bool callWithCallback(const QDBusMessage &message, QObject *receiver, const char *returnMethod,
                          const char *errorMethod, int timeout = -1) const;
    bool callWithCallback(const QDBusMessage &message, QObject *receiver, const char *slot, int timeout = -1) const;
    QDBusMessage call(const QDBusMessage &message, QDBus::CallMode mode = QDBus::Block, int timeout = -1) const;
    QDBusPendingCall asyncCall(const QDBusMessage &message, int timeout = -1) const;

    bool connect(const QString &service, const QString &path, const QString &interface, const QString &name,
                 QObject *receiver, const char *slot);
    bool connect(const QString &service, const QString &path, const QString &interface, const QString &name,
                 const QString &signature, QObject *receiver, const char *slot);
    bool connect(const QString &service, const QString &path, const QString &interface, const QString &name,
                 const QStringList &argumentMatch, const QString &signature, QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface, const QString &name,
                    QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface, const QString &name,
                    const QString &signature, QObject *receiver, const char *slot);
    bool disconnect(const QString &service, const QString &path, const QString &interface, const QString &name,
                    const QStringList &argumentMatch, const QString &signature, QObject *receiver, const char *slot);

    bool registerObject(const QString &path, QObject *object, RegisterOptions options = ExportAdaptors);
    bool registerObject(const QString &path, const QString &interface, QObject *object,
                        RegisterOptions options = ExportAdaptors);
    void unregisterObject(const QString &path, UnregisterMode mode = UnregisterNode);
    QObject *objectRegisteredAt(const QString &path) const;
    bool registerVirtualObject(const QString &path, QDBusVirtualObject *object,
                               VirtualObjectRegisterOption options = SingleNode);

    bool registerService(const QString &serviceName);
    bool unregisterService(const QString &serviceName);

    QDBusConnectionInterface *interface() const;

    static QDBusConnection connectToBus(BusType type, const QString &name);
    static QDBusConnection connectToBus(const QString &address, const QString &name);
    static QDBusConnection connectToPeer(const QString &address, const QString &name);
    static void disconnectFromBus(const QString &name);
    static void disconnectFromPeer(const QString &name);
    static QByteArray localMachineId();

    static QDBusConnection sessionBus();
    static QDBusConnection systemBus();

private:
    friend class QDBusConnectionPrivate;
    explicit QDBusConnection(std::shared_ptr<QDBusConnectionPrivate> d);

    std::shared_ptr<QDBusConnectionPrivate> d;
};

Q_DECLARE_METATYPE(QDBusConnection)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::RegisterOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::VirtualObjectRegisterOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QDBusConnection::ConnectionCapabilities)
