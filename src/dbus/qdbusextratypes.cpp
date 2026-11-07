// MIT License - see LICENSE
#include "qdbusmarshaller_p.h"

#include <QtCore/qdebug.h>
#include <QtDBus/qdbusextratypes.h>

/*!
    \class QDBusObjectPath
    \brief The QDBusObjectPath class enables the programmer to identify the OBJECT_PATH type
    provided by the D-Bus typesystem.

    An object path names an object in a connection, such as \c{/org/example/Clock}. A QString
    is sent as a D-Bus string; wrap it in a QDBusObjectPath to send it as a path.

    \differences
    There is no QDebug stream operator.
    \enddifferences

    \sa QDBusSignature, QDBusVariant
*/

/*!
    \class QDBusSignature
    \brief The QDBusSignature class enables the programmer to identify the SIGNATURE type
    provided by the D-Bus typesystem.

    A signature describes D-Bus types, such as \c{a{sv}}. Wrap a string in a QDBusSignature to
    send it as a signature.

    \differences
    There is no QDebug stream operator.
    \enddifferences

    \sa QDBusObjectPath, QDBusVariant
*/

/*!
    \class QDBusVariant
    \brief The QDBusVariant class enables the programmer to identify the variant type provided
    by the D-Bus typesystem.

    A QVariant in a message carries a value of its own type. To send a D-Bus variant, which
    carries its type on the wire, wrap the value in a QDBusVariant:

    \code
    QDBusMessage m = QDBusMessage::createMethodCall(service, path,
        "org.freedesktop.DBus.Properties", "Set");
    m << "org.example.Clock" << "Volume" << QVariant::fromValue(QDBusVariant(7));
    \endcode

    \differences
    There is no QDebug stream operator.
    \enddifferences

    \sa QDBusArgument
*/

namespace {

QString checkedPath(const QString &path) {
    if (path.isEmpty() || QDBusUtil::isValidObjectPath(path))
        return path;
    qWarning("QDBusObjectPath: invalid path \"%s\"", qPrintable(path));
    return QString();
}

QString checkedSignature(const QString &signature) {
    if (QDBusUtil::isValidSignature(signature.toLatin1()) && signature == QString::fromLatin1(signature.toLatin1()))
        return signature;
    qWarning("QDBusSignature: invalid signature \"%s\"", qPrintable(signature));
    return QString();
}

} // namespace

/*!
    Constructs an object path from \a path. An invalid path prints a warning and gives an empty
    one.
*/
QDBusObjectPath::QDBusObjectPath(const char *path) : m_path(checkedPath(QString::fromUtf8(path))) {}

/*!
    \overload
    Constructs an object path from \a path.
*/
QDBusObjectPath::QDBusObjectPath(QLatin1StringView path) : m_path(checkedPath(QString(path))) {}

/*!
    \overload
    Constructs an object path from \a path.
*/
QDBusObjectPath::QDBusObjectPath(const QString &path) : m_path(checkedPath(path)) {}

/*!
    Sets this object path to \a path. An invalid path prints a warning and gives an empty one.
*/
void QDBusObjectPath::setPath(const QString &path) { m_path = checkedPath(path); }

/*!
    Constructs a signature from \a signature. An invalid signature prints a warning and gives an
    empty one.
*/
QDBusSignature::QDBusSignature(const char *signature) : m_signature(checkedSignature(QString::fromUtf8(signature))) {}

/*!
    \overload
    Constructs a signature from \a signature.
*/
QDBusSignature::QDBusSignature(QLatin1StringView signature) : m_signature(checkedSignature(QString(signature))) {}

/*!
    \overload
    Constructs a signature from \a signature.
*/
QDBusSignature::QDBusSignature(const QString &signature) : m_signature(checkedSignature(signature)) {}

/*!
    Sets this signature to \a signature. An invalid signature prints a warning and gives an empty
    one.
*/
void QDBusSignature::setSignature(const QString &signature) { m_signature = checkedSignature(signature); }
