// MIT License - see LICENSE
#include "qdbusmarshaller_p.h"
#include "qdbusmessage_p.h"

#include <QtDBus/qdbusmessage.h>

#include <cstring>

/*!
    \class QDBusMessage
    \brief The QDBusMessage class represents one message sent or received over the D-Bus bus.

    A message is a method call, a reply to one, an error or a signal. Create one with the static
    functions, add arguments with operator<<() and send it with QDBusConnection:

    \code
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.example.Clock", "/clock", "org.example.Clock", "SetAlarm");
    call << 7 << 30;
    QDBusMessage reply = QDBusConnection::sessionBus().call(call);
    if (reply.type() == QDBusMessage::ErrorMessage)
        qWarning() << reply.errorMessage();
    \endcode

    QDBusMessage is implicitly shared.

    \differences
    There is no QDebug stream operator and no \c createSignal() overload taking only a path.
    \enddifferences

    \sa QDBusConnection, QDBusReply
*/

/*!
    \enum QDBusMessage::MessageType

    The kind of a message.

    \value InvalidMessage A message that was default-constructed or failed to parse.
    \value MethodCallMessage A call of a method.
    \value ReplyMessage A successful reply to a call.
    \value ErrorMessage An error reply to a call.
    \value SignalMessage A signal.
*/

/*!
    Constructs an invalid message.
*/
QDBusMessage::QDBusMessage() : d_ptr(new QDBusMessagePrivate) {}

/*!
    Constructs a copy of \a other.
*/
QDBusMessage::QDBusMessage(const QDBusMessage &other) = default;

/*!
    Makes this message a copy of \a other and returns a reference to it.
*/
QDBusMessage &QDBusMessage::operator=(const QDBusMessage &other) = default;

/*!
    Destroys the message.
*/
QDBusMessage::~QDBusMessage() = default;

/*!
    Returns a signal \a name of \a interface, emitted by the object at \a path, to every
    connection that listens for it.

    \sa createTargetedSignal(), QDBusConnection::send()
*/
QDBusMessage QDBusMessage::createSignal(const QString &path, const QString &interface, const QString &name) {
    QDBusMessage m;
    m.d_ptr->type = SignalMessage;
    m.d_ptr->path = path;
    m.d_ptr->interface = interface;
    m.d_ptr->name = name;
    return m;
}

/*!
    Returns a signal \a name of \a interface, emitted by the object at \a path, that is sent to
    \a service only.
*/
QDBusMessage QDBusMessage::createTargetedSignal(const QString &service, const QString &path, const QString &interface,
                                                const QString &name) {
    QDBusMessage m = createSignal(path, interface, name);
    m.d_ptr->service = service;
    return m;
}

/*!
    Returns a call of \a method of \a interface on the object at \a path in \a destination.
    \a interface may be empty.

    \sa QDBusConnection::call()
*/
QDBusMessage QDBusMessage::createMethodCall(const QString &destination, const QString &path, const QString &interface,
                                            const QString &method) {
    QDBusMessage m;
    m.d_ptr->type = MethodCallMessage;
    m.d_ptr->service = destination;
    m.d_ptr->path = path;
    m.d_ptr->interface = interface;
    m.d_ptr->name = method;
    return m;
}

/*!
    Returns an error message with the error \a name and the text \a message.
*/
QDBusMessage QDBusMessage::createError(const QString &name, const QString &message) {
    QDBusMessage m;
    m.d_ptr->type = ErrorMessage;
    m.d_ptr->errorName = name;
    m.d_ptr->errorMessage = message;
    return m;
}

/*!
    \overload
    Returns an error message for \a error.
*/
QDBusMessage QDBusMessage::createError(const QDBusError &error) { return createError(error.name(), error.message()); }

/*!
    \overload
    Returns an error message of the type \a type with the text \a message.
*/
QDBusMessage QDBusMessage::createError(QDBusError::ErrorType type, const QString &message) {
    return createError(QDBusError(type, message));
}

/*!
    Returns a reply to this call that carries \a arguments.
*/
QDBusMessage QDBusMessage::createReply(const QList<QVariant> &arguments) const {
    QDBusMessage m;
    m.d_ptr->type = ReplyMessage;
    m.d_ptr->service = d_ptr->service;
    m.d_ptr->replySerial = d_ptr->serial;
    m.d_ptr->arguments = arguments;
    m.d_ptr->localCall = d_ptr->localCall;
    d_ptr->delayedReply = true; // a reply exists, so none is sent automatically
    return m;
}

/*!
    \overload
    Returns a reply to this call that carries the single \a argument.
*/
QDBusMessage QDBusMessage::createReply(const QVariant &argument) const {
    return createReply(QList<QVariant>{argument});
}

/*!
    Returns an error reply to this call with the error \a name and the text \a message.
*/
QDBusMessage QDBusMessage::createErrorReply(const QString &name, const QString &message) const {
    QDBusMessage m = createError(name, message);
    m.d_ptr->service = d_ptr->service;
    m.d_ptr->replySerial = d_ptr->serial;
    m.d_ptr->localCall = d_ptr->localCall;
    d_ptr->delayedReply = true;
    return m;
}

/*!
    \overload
    Returns an error reply to this call for \a error.
*/
QDBusMessage QDBusMessage::createErrorReply(const QDBusError &error) const {
    return createErrorReply(error.name(), error.message());
}

/*!
    \overload
    Returns an error reply to this call of the type \a type with the text \a message.
*/
QDBusMessage QDBusMessage::createErrorReply(QDBusError::ErrorType type, const QString &message) const {
    return createErrorReply(QDBusError(type, message));
}

/*!
    Returns the destination of a message that is to be sent, or the unique name of the sender
    of a message that was received.
*/
QString QDBusMessage::service() const { return d_ptr->service; }

/*!
    Returns the object path of a call or a signal.
*/
QString QDBusMessage::path() const { return d_ptr->path; }

/*!
    Returns the interface of a call or a signal.
*/
QString QDBusMessage::interface() const { return d_ptr->interface; }

/*!
    Returns the name of the method or signal.
*/
QString QDBusMessage::member() const { return d_ptr->name; }

/*!
    Returns the name of the error of an error message.
*/
QString QDBusMessage::errorName() const { return d_ptr->errorName; }

/*!
    Returns the text of an error message.
*/
QString QDBusMessage::errorMessage() const { return d_ptr->errorMessage; }

/*!
    Returns the kind of the message.
*/
QDBusMessage::MessageType QDBusMessage::type() const { return d_ptr->type; }

/*!
    Returns the D-Bus signature of the arguments.
*/
QString QDBusMessage::signature() const {
    if (d_ptr->received)
        return QString::fromLatin1(d_ptr->signature);
    QByteArray sig;
    for (const QVariant &v : d_ptr->arguments)
        sig += QDBusWriter::signatureOf(v);
    return QString::fromLatin1(sig);
}

/*!
    Returns \c true if this is a received method call whose caller waits for a reply.
*/
bool QDBusMessage::isReplyRequired() const {
    return d_ptr->type == MethodCallMessage && d_ptr->received && !d_ptr->noReplyFlag;
}

/*!
    Sets whether the reply to this call is sent later, by the code that handles it, to
    \a enable. Nt then sends no reply when the slot returns.

    \sa QDBusContext::setDelayedReply()
*/
void QDBusMessage::setDelayedReply(bool enable) const { d_ptr->delayedReply = enable; }

/*!
    Returns \c true if the reply to this call is to be sent later.
*/
bool QDBusMessage::isDelayedReply() const { return d_ptr->delayedReply; }

/*!
    Sets whether the bus may start the destination service to receive this call to \a enable.
    It is enabled by default.
*/
void QDBusMessage::setAutoStartService(bool enable) { d_ptr->autoStart = enable; }

/*!
    Returns \c true if the bus may start the destination service to receive this call.
*/
bool QDBusMessage::autoStartService() const { return d_ptr->autoStart; }

/*!
    Sets whether the receiver may ask the user to authorize this call to \a enable.
*/
void QDBusMessage::setInteractiveAuthorizationAllowed(bool enable) { d_ptr->interactive = enable; }

/*!
    Returns \c true if the receiver may ask the user to authorize this call.
*/
bool QDBusMessage::isInteractiveAuthorizationAllowed() const { return d_ptr->interactive; }

/*!
    Replaces the arguments of the message with \a arguments.
*/
void QDBusMessage::setArguments(const QList<QVariant> &arguments) {
    d_ptr->arguments = arguments;
    d_ptr->received = false;
}

/*!
    Returns the arguments of the message. Containers and structures that have no C++ type are
    QDBusArgument values; use qdbus_cast() to extract them.
*/
QList<QVariant> QDBusMessage::arguments() const { return d_ptr->arguments; }

/*!
    Appends \a argument to the arguments of the message and returns a reference to the message.
*/
QDBusMessage &QDBusMessage::operator<<(const QVariant &argument) {
    d_ptr->arguments.append(argument);
    d_ptr->received = false;
    return *this;
}

// ---- the wire format ---------------------------------------------------------------------

namespace {

enum HeaderField : uchar {
    FieldPath = 1,
    FieldInterface = 2,
    FieldMember = 3,
    FieldErrorName = 4,
    FieldReplySerial = 5,
    FieldDestination = 6,
    FieldSender = 7,
    FieldSignature = 8,
    FieldUnixFds = 9
};

constexpr uchar kNoReplyExpected = 0x1;
constexpr uchar kNoAutoStart = 0x2;
constexpr uchar kAllowInteractive = 0x4;

char nativeEndian() {
    const uint one = 1;
    char first;
    std::memcpy(&first, &one, 1);
    return first ? 'l' : 'B';
}

uint readU32(const char *p, bool swapped) {
    uint v;
    std::memcpy(&v, p, 4);
    if (swapped)
        v = ((v & 0xffU) << 24) | ((v & 0xff00U) << 8) | ((v & 0xff0000U) >> 8) | (v >> 24);
    return v;
}

} // namespace

bool QDBusMessagePrivate::toWire(const QDBusMessage &message, bool expectReply, QDBusWireMessage *out,
                                 QDBusError *error) {
    const QDBusMessagePrivate *d = message.d_ptr.constData();
    auto bad = [error](QDBusError::ErrorType type, const QString &text) {
        if (error)
            *error = QDBusError(type, text);
        return false;
    };
    switch (d->type) {
    case QDBusMessage::MethodCallMessage:
        if (!QDBusUtil::isValidObjectPath(d->path))
            return bad(QDBusError::InvalidObjectPath, QStringLiteral("Invalid object path: %1").arg(d->path));
        if (!d->interface.isEmpty() && !QDBusUtil::isValidInterfaceName(d->interface))
            return bad(QDBusError::InvalidInterface, QStringLiteral("Invalid interface: %1").arg(d->interface));
        if (!QDBusUtil::isValidMemberName(d->name))
            return bad(QDBusError::InvalidMember, QStringLiteral("Invalid method name: %1").arg(d->name));
        if (!d->service.isEmpty() && !QDBusUtil::isValidBusName(d->service))
            return bad(QDBusError::InvalidService, QStringLiteral("Invalid service name: %1").arg(d->service));
        break;
    case QDBusMessage::SignalMessage:
        if (!QDBusUtil::isValidObjectPath(d->path))
            return bad(QDBusError::InvalidObjectPath, QStringLiteral("Invalid object path: %1").arg(d->path));
        if (!QDBusUtil::isValidInterfaceName(d->interface))
            return bad(QDBusError::InvalidInterface, QStringLiteral("Invalid interface: %1").arg(d->interface));
        if (!QDBusUtil::isValidMemberName(d->name))
            return bad(QDBusError::InvalidMember, QStringLiteral("Invalid signal name: %1").arg(d->name));
        if (!d->service.isEmpty() && !QDBusUtil::isValidBusName(d->service))
            return bad(QDBusError::InvalidService, QStringLiteral("Invalid service name: %1").arg(d->service));
        break;
    case QDBusMessage::ErrorMessage:
        if (!QDBusUtil::isValidErrorName(d->errorName))
            return bad(QDBusError::InvalidArgs, QStringLiteral("Invalid error name: %1").arg(d->errorName));
        break;
    case QDBusMessage::ReplyMessage:
        break;
    case QDBusMessage::InvalidMessage:
        return bad(QDBusError::Failed, QStringLiteral("Cannot send an invalid message"));
    }

    QDBusWriter body;
    if (d->type == QDBusMessage::ErrorMessage && d->arguments.isEmpty() && !d->errorMessage.isEmpty())
        body.writeString(d->errorMessage);
    for (const QVariant &arg : d->arguments)
        body.appendVariant(arg);
    if (body.failed)
        return bad(QDBusError::Failed, QStringLiteral("Marshalling failed: %1").arg(body.errorText));
    if (body.signature.size() > QDBusUtil::kMaxSignature)
        return bad(QDBusError::InvalidArgs, QStringLiteral("Too many arguments for one message"));
    if (body.fds.size() > QDBusUtil::kMaxUnixFds)
        return bad(QDBusError::LimitsExceeded, QStringLiteral("Too many file descriptors in one message"));
    if (!QDBusUtil::validateValues(body.data, 0, body.signature, false, static_cast<int>(body.fds.size())))
        return bad(QDBusError::Failed, QStringLiteral("Marshalling failed: the arguments do not match their types"));

    QDBusWriter h;
    h.writeByte(static_cast<uchar>(nativeEndian()));
    h.writeByte(static_cast<uchar>(d->type));
    uchar flags = 0;
    if (d->type == QDBusMessage::MethodCallMessage) {
        if (!expectReply)
            flags |= kNoReplyExpected;
        if (!d->autoStart)
            flags |= kNoAutoStart;
        if (d->interactive)
            flags |= kAllowInteractive;
    }
    h.writeByte(flags);
    h.writeByte(1);
    h.writeUInt32(static_cast<uint>(body.data.size()));
    h.writeUInt32(0); // the serial
    h.beginArray("(yv)");
    auto field = [&h](HeaderField code, const QVariant &value) {
        h.beginStructure();
        h.writeByte(code);
        h.writeDBusVariant(value);
        h.endStructure();
    };
    if (!d->path.isEmpty())
        field(FieldPath, QVariant::fromValue(QDBusObjectPath(d->path)));
    if (!d->interface.isEmpty())
        field(FieldInterface, d->interface);
    if (!d->name.isEmpty() && (d->type == QDBusMessage::MethodCallMessage || d->type == QDBusMessage::SignalMessage))
        field(FieldMember, d->name);
    if (d->type == QDBusMessage::ErrorMessage)
        field(FieldErrorName, d->errorName);
    if (d->type == QDBusMessage::ReplyMessage || d->type == QDBusMessage::ErrorMessage)
        field(FieldReplySerial, d->replySerial);
    if (!d->service.isEmpty())
        field(FieldDestination, d->service);
    if (!body.signature.isEmpty())
        field(FieldSignature, QVariant::fromValue(QDBusSignature(QString::fromLatin1(body.signature))));
    if (!body.fds.isEmpty())
        field(FieldUnixFds, static_cast<uint>(body.fds.size()));
    h.endArray();
    h.align(8);
    if (h.failed)
        return bad(QDBusError::Failed, QStringLiteral("Marshalling failed: %1").arg(h.errorText));
    if (h.data.size() + body.data.size() > QDBusUtil::kMaxMessageBytes)
        return bad(QDBusError::LimitsExceeded, QStringLiteral("The message is too large for D-Bus"));
    out->bytes = h.data + body.data;
    out->fds = body.fds;
    return true;
}

qint64 QDBusMessagePrivate::frameSize(const char *header) {
    if ((header[0] != 'l' && header[0] != 'B') || header[3] != 1)
        return -1;
    const bool swapped = header[0] != nativeEndian();
    const qint64 body = readU32(header + 4, swapped);
    const qint64 fields = readU32(header + 12, swapped);
    if (fields > QDBusUtil::kMaxArrayBytes)
        return -1;
    const qint64 headerSize = (16 + fields + 7) / 8 * 8;
    const qint64 total = headerSize + body;
    return total > QDBusUtil::kMaxMessageBytes ? -1 : total;
}

int QDBusMessagePrivate::announcedUnixFds(const QByteArray &frame) {
    if (frame.size() < 16 || frameSize(frame.constData()) != frame.size())
        return -1;
    const bool swapped = frame.at(0) != nativeEndian();
    if (!QDBusUtil::validateValues(frame, 12, "a(yv)", swapped, 0))
        return -1;
    QDBusReader fields(frame, 12, "a(yv)", swapped, {});
    int count = 0;
    fields.beginArray();
    while (!fields.atEnd()) {
        fields.beginStructure();
        const uchar code = fields.readByte();
        const QVariant value = fields.readDBusVariant();
        fields.endStructure();
        if (code == FieldUnixFds) {
            if (value.metaType() != QMetaType::fromType<uint>() || value.toUInt() > QDBusUtil::kMaxUnixFds)
                return -1;
            count = static_cast<int>(value.toUInt());
        }
    }
    return count;
}

QDBusMessage QDBusMessagePrivate::fromWire(const QByteArray &frame, const QList<QDBusUnixFileDescriptor> &fds,
                                           QString *why) {
    auto fail = [why](const char *text) {
        if (why)
            *why = QString::fromLatin1(text);
        return QDBusMessage();
    };
    if (frame.size() < 16)
        return fail("truncated header");
    const char *raw = frame.constData();
    if (frameSize(raw) != frame.size())
        return fail("bad header");
    const bool swapped = raw[0] != nativeEndian();
    const auto type = static_cast<uchar>(raw[1]);
    const auto flags = static_cast<uchar>(raw[2]);
    if (type < 1 || type > 4)
        return fail("unknown message type");
    const uint serial = readU32(raw + 8, swapped);
    if (serial == 0)
        return fail("zero serial");
    if (!QDBusUtil::validateValues(frame, 12, "a(yv)", swapped, 0))
        return fail("malformed header fields");

    QDBusMessage message;
    QDBusMessagePrivate *d = message.d_ptr.data();
    d->type = static_cast<QDBusMessage::MessageType>(type);
    d->serial = serial;
    d->received = true;
    d->noReplyFlag = (flags & kNoReplyExpected) != 0;
    d->autoStart = (flags & kNoAutoStart) == 0;
    d->interactive = (flags & kAllowInteractive) != 0;

    QDBusReader fields(frame, 12, "a(yv)", swapped, {});
    uint unixFds = 0;
    bool haveReplySerial = false;
    fields.beginArray();
    while (!fields.atEnd()) {
        fields.beginStructure();
        const uchar code = fields.readByte();
        const QVariant value = fields.readDBusVariant();
        fields.endStructure();
        const QString sig = QString::fromLatin1(QDBusWriter::signatureOf(value));
        auto want = [&](const char *expected) { return sig == QLatin1String(expected); };
        switch (code) {
        case FieldPath:
            if (!want("o"))
                return fail("bad PATH field");
            d->path = value.value<QDBusObjectPath>().path();
            break;
        case FieldInterface:
            if (!want("s") || !QDBusUtil::isValidInterfaceName(value.toString()))
                return fail("bad INTERFACE field");
            d->interface = value.toString();
            break;
        case FieldMember:
            if (!want("s") || !QDBusUtil::isValidMemberName(value.toString()))
                return fail("bad MEMBER field");
            d->name = value.toString();
            break;
        case FieldErrorName:
            if (!want("s") || !QDBusUtil::isValidErrorName(value.toString()))
                return fail("bad ERROR_NAME field");
            d->errorName = value.toString();
            break;
        case FieldReplySerial:
            if (!want("u"))
                return fail("bad REPLY_SERIAL field");
            d->replySerial = value.toUInt();
            haveReplySerial = true;
            break;
        case FieldDestination:
            if (!want("s"))
                return fail("bad DESTINATION field");
            break;
        case FieldSender:
            if (!want("s") || !QDBusUtil::isValidBusName(value.toString()))
                return fail("bad SENDER field");
            d->service = value.toString();
            break;
        case FieldSignature:
            if (!want("g"))
                return fail("bad SIGNATURE field");
            d->signature = value.value<QDBusSignature>().signature().toLatin1();
            break;
        case FieldUnixFds:
            if (!want("u"))
                return fail("bad UNIX_FDS field");
            unixFds = value.toUInt();
            break;
        default:
            break; // unknown fields are ignored
        }
    }
    fields.endArray();

    switch (d->type) {
    case QDBusMessage::MethodCallMessage:
        if (d->path.isEmpty() || d->name.isEmpty())
            return fail("method call without PATH or MEMBER");
        break;
    case QDBusMessage::SignalMessage:
        if (d->path.isEmpty() || d->name.isEmpty() || d->interface.isEmpty())
            return fail("signal without PATH, INTERFACE or MEMBER");
        break;
    case QDBusMessage::ErrorMessage:
        if (d->errorName.isEmpty() || !haveReplySerial)
            return fail("error without ERROR_NAME or REPLY_SERIAL");
        break;
    case QDBusMessage::ReplyMessage:
        if (!haveReplySerial)
            return fail("reply without REPLY_SERIAL");
        break;
    case QDBusMessage::InvalidMessage:
        break;
    }
    if (unixFds != static_cast<uint>(fds.size()))
        return fail("the message does not have the file descriptors it announces");

    const int bodyStart = static_cast<int>((16 + readU32(raw + 12, swapped) + 7) / 8 * 8);
    for (int i = static_cast<int>(16 + readU32(raw + 12, swapped)); i < bodyStart; ++i)
        if (raw[i])
            return fail("non-zero header padding");
    if (!QDBusUtil::validateValues(frame, bodyStart, d->signature, swapped, static_cast<int>(fds.size())))
        return fail("malformed body");
    // The body starts on an 8 byte boundary, so it can be read in a buffer of its own.
    const QByteArray body = frame.mid(bodyStart);
    QDBusReader reader(body, 0, d->signature, swapped, fds);
    while (!reader.atEnd())
        d->arguments.append(reader.readValue());
    if (d->type == QDBusMessage::ErrorMessage && !d->arguments.isEmpty() &&
        d->arguments.first().metaType() == QMetaType::fromType<QString>())
        d->errorMessage = d->arguments.first().toString();
    return message;
}

QDBusMessage QDBusMessagePrivate::makeLocal(const QDBusMessage &message, const QString &sender, QDBusError *error) {
    QDBusWireMessage wire;
    if (!toWire(message, true, &wire, error))
        return QDBusMessage();
    const uint serial = 1;
    std::memcpy(wire.bytes.data() + 8, &serial, 4);
    QString why;
    QDBusMessage local = fromWire(wire.bytes, wire.fds, &why);
    if (local.type() == QDBusMessage::InvalidMessage) {
        if (error)
            *error = QDBusError(QDBusError::InternalError, why);
        return local;
    }
    local.d_ptr->service = sender;
    return local;
}
