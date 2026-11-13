// MIT License - see LICENSE
#include <QtCore/qnativeipckey.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qurl.h>

/*!
    \class QNativeIpcKey
    \since 6.6
    \brief The QNativeIpcKey class holds the name of a system object that QSharedMemory and QSystemSemaphore use,
    with the kind of object it names.

    A key made from an application's own text with QSharedMemory::platformSafeKey() works on every system. A
    native key names the object directly, so that a program that does not use Nt can open it:

    \code
    QSharedMemory memory(QNativeIpcKey(QStringLiteral("/myapp-frames")));
    memory.create(4096);
    \endcode

    toString() and fromString() turn a key into text and back, to pass it to another process.

    \differences
    Nt uses POSIX real-time objects only: QSharedMemory and QSystemSemaphore report a key error for keys of
    another type. The text of toString() is Nt's own.
    \enddifferences

    \sa QSharedMemory, QSystemSemaphore
*/

/*!
    Returns the type that Qt 5 used on this system, \c Type::SystemV. Nt does not use System V objects; the
    function exists so that code that asks for it compiles.
*/
QNativeIpcKey::Type QNativeIpcKey::legacyDefaultTypeForOs() noexcept { return Type::SystemV; }

namespace {

const char *typeName(QNativeIpcKey::Type type) {
    switch (type) {
    case QNativeIpcKey::Type::SystemV:
        return "systemv";
    case QNativeIpcKey::Type::PosixRealtime:
        return "posix";
    case QNativeIpcKey::Type::Windows:
        return "windows";
    }
    return nullptr;
}

} // namespace

/*!
    Returns the key as text, such as \c{"posix:%2Fmyapp"}, from which fromString() makes the same key again.
    Returns an empty string for a key that is not valid.

    \sa fromString()
*/
QString QNativeIpcKey::toString() const {
    const char *type = typeName(m_type);
    if (!type)
        return QString();
    return QString::fromLatin1(type) + QLatin1Char(':') + QString::fromLatin1(QUrl::toPercentEncoding(m_key));
}

/*!
    Returns the key that \a string, made by toString(), stands for, or an invalid key if it is not such text.

    \sa toString(), isValid()
*/
QNativeIpcKey QNativeIpcKey::fromString(const QString &string) {
    const qsizetype colon = string.indexOf(QLatin1Char(':'));
    QNativeIpcKey invalid;
    invalid.m_valid = false;
    if (colon < 0)
        return invalid;
    const QString type = string.left(colon);
    const QString key = QUrl::fromPercentEncoding(string.mid(colon + 1).toLatin1());
    for (const Type t : {Type::SystemV, Type::PosixRealtime, Type::Windows}) {
        if (type == QLatin1String(typeName(t)))
            return QNativeIpcKey(key, t);
    }
    return invalid;
}
