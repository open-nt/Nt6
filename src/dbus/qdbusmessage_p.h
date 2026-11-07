// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbusunixfiledescriptor.h>

#include <memory>

struct QDBusPendingCallPrivate;

// A message ready for the socket. The serial is written later, at offset 8.
struct QDBusWireMessage {
    QByteArray bytes;
    QList<QDBusUnixFileDescriptor> fds;
};

class QDBusMessagePrivate : public QSharedData {
public:
    QDBusMessage::MessageType type = QDBusMessage::InvalidMessage;
    QString service; // destination of a message that is sent, sender of one that is received
    QString path;
    QString interface;
    QString name;
    QString errorName;
    QString errorMessage;
    QList<QVariant> arguments;
    QByteArray signature; // of a received message
    uint serial = 0;
    uint replySerial = 0;
    bool noReplyFlag = false; // a received call that wants no reply
    bool autoStart = true;
    bool interactive = false;
    bool received = false;
    mutable bool delayedReply = false;
    // A call to an object of this process that did not go through the socket: its reply
    // completes this call directly.
    std::shared_ptr<QDBusPendingCallPrivate> localCall;

    static QDBusMessagePrivate *get(const QDBusMessage &m) {
        return const_cast<QDBusMessagePrivate *>(m.d_ptr.constData());
    }
    static QDBusMessagePrivate *edit(QDBusMessage &m) { return m.d_ptr.data(); }

    // Serializes `message`. `expectReply` sets the flags of a method call.
    static bool toWire(const QDBusMessage &message, bool expectReply, QDBusWireMessage *out, QDBusError *error);
    // Parses a complete message, which arrived with `fds`. Returns an invalid message on error.
    static QDBusMessage fromWire(const QByteArray &frame, const QList<QDBusUnixFileDescriptor> &fds, QString *why);
    // The size of the message whose first 16 bytes are at `header`; -1 if it is malformed.
    static qint64 frameSize(const char *header);
    // The number of file descriptors that the header of a complete message announces; -1 if the
    // header is malformed.
    static int announcedUnixFds(const QByteArray &frame);
    // A copy that has been through the wire format, as if it had been received.
    static QDBusMessage makeLocal(const QDBusMessage &message, const QString &sender, QDBusError *error);
};
