// MIT License - see LICENSE
#include <QtDBus/qdbuserror.h>
#include <QtDBus/qdbusmessage.h>

/*!
    \class QDBusError
    \brief The QDBusError class represents an error received from the D-Bus bus or from
    remote applications found on it.

    An error has a type, a D-Bus error name such as
    \c{org.freedesktop.DBus.Error.UnknownMethod}, and a text for people:

    \code
    QDBusReply<QString> reply = iface.call("Name");
    if (!reply.isValid())
        qWarning() << reply.error().name() << reply.error().message();
    \endcode

    \differences
    There is no QDebug stream operator. The errors that Nt itself generates use the same names
    as Qt's, so that peers which match on them work unchanged.
    \enddifferences

    \sa QDBusMessage, QDBusReply
*/

/*!
    \enum QDBusError::ErrorType

    The errors that Nt knows by name.

    \value NoError No error.
    \value Other An error whose name Nt does not know; see name().
    \value Failed A generic failure.
    \value NoMemory Out of memory.
    \value ServiceUnknown No service of that name exists and none can be started.
    \value NoReply No reply arrived in time.
    \value BadAddress The bus address is malformed.
    \value NotSupported The operation is not supported.
    \value LimitsExceeded A limit was exceeded.
    \value AccessDenied Access was denied.
    \value NoServer The bus or the peer could not be reached.
    \value Timeout A timeout of the bus.
    \value NoNetwork The network is not available.
    \value AddressInUse The address is already in use.
    \value Disconnected The connection is closed.
    \value InvalidArgs The arguments are not valid.
    \value UnknownMethod The method does not exist.
    \value TimedOut The call timed out.
    \value InvalidSignature The signature does not match.
    \value UnknownInterface The interface does not exist.
    \value UnknownObject No object exists at that path.
    \value UnknownProperty The property does not exist.
    \value PropertyReadOnly The property cannot be written.
    \value InternalError An error inside Nt.
    \value InvalidService The service name is not valid.
    \value InvalidObjectPath The object path is not valid.
    \value InvalidInterface The interface name is not valid.
    \value InvalidMember The method or signal name is not valid.
    \omitvalue LastErrorType
*/

namespace {

const char *const kErrorNames[] = {
    "",
    "other",
    "org.freedesktop.DBus.Error.Failed",
    "org.freedesktop.DBus.Error.NoMemory",
    "org.freedesktop.DBus.Error.ServiceUnknown",
    "org.freedesktop.DBus.Error.NoReply",
    "org.freedesktop.DBus.Error.BadAddress",
    "org.freedesktop.DBus.Error.NotSupported",
    "org.freedesktop.DBus.Error.LimitsExceeded",
    "org.freedesktop.DBus.Error.AccessDenied",
    "org.freedesktop.DBus.Error.NoServer",
    "org.freedesktop.DBus.Error.Timeout",
    "org.freedesktop.DBus.Error.NoNetwork",
    "org.freedesktop.DBus.Error.AddressInUse",
    "org.freedesktop.DBus.Error.Disconnected",
    "org.freedesktop.DBus.Error.InvalidArgs",
    "org.freedesktop.DBus.Error.UnknownMethod",
    "org.freedesktop.DBus.Error.TimedOut",
    "org.freedesktop.DBus.Error.InvalidSignature",
    "org.freedesktop.DBus.Error.UnknownInterface",
    "org.freedesktop.DBus.Error.UnknownObject",
    "org.freedesktop.DBus.Error.UnknownProperty",
    "org.freedesktop.DBus.Error.PropertyReadOnly",
    "org.qtproject.QtDBus.Error.InternalError",
    "org.qtproject.QtDBus.Error.InvalidService",
    "org.qtproject.QtDBus.Error.InvalidObjectPath",
    "org.qtproject.QtDBus.Error.InvalidInterface",
    "org.qtproject.QtDBus.Error.InvalidMember",
};

QDBusError::ErrorType typeOfName(const QString &name) {
    if (name.isEmpty())
        return QDBusError::NoError;
    for (int i = QDBusError::Failed; i <= QDBusError::LastErrorType; ++i)
        if (name == QLatin1String(kErrorNames[i]))
            return static_cast<QDBusError::ErrorType>(i);
    return QDBusError::Other;
}

} // namespace

/*!
    Constructs an error of the type \c NoError.
*/
QDBusError::QDBusError() = default;

/*!
    Constructs the error that the error message \a message carries. Any other kind of message
    gives \c NoError.
*/
QDBusError::QDBusError(const QDBusMessage &message) {
    if (message.type() != QDBusMessage::ErrorMessage)
        return;
    m_name = message.errorName();
    m_message = message.errorMessage();
    m_type = typeOfName(m_name);
}

/*!
    Constructs an error of the type \a error with the text \a message.
*/
QDBusError::QDBusError(ErrorType error, const QString &message)
    : m_type(error), m_name(error >= NoError && error <= LastErrorType ? QString::fromLatin1(kErrorNames[error])
                                                                       : QString::fromLatin1(kErrorNames[Other])),
      m_message(message) {}

/*!
    Returns the type of the error.
*/
QDBusError::ErrorType QDBusError::type() const { return m_type; }

/*!
    Returns the D-Bus name of the error.
*/
QString QDBusError::name() const { return m_name; }

/*!
    Returns the text of the error.
*/
QString QDBusError::message() const { return m_message; }

/*!
    Returns \c true if this is an error, that is, not of the type \c NoError.
*/
bool QDBusError::isValid() const { return m_type != NoError; }

/*!
    Swaps this error with \a other.
*/
void QDBusError::swap(QDBusError &other) noexcept {
    std::swap(m_type, other.m_type);
    m_name.swap(other.m_name);
    m_message.swap(other.m_message);
}

/*!
    Returns the D-Bus name of the errors of the type \a error.
*/
QString QDBusError::errorString(ErrorType error) {
    if (error < NoError || error > LastErrorType)
        error = Other;
    return QString::fromLatin1(kErrorNames[error]);
}

#include "moc_qdbuserror.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
