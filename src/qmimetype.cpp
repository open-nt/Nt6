// MIT License - see LICENSE
#include <QtCore/qmimetype.h>

#include <QtCore/qhashfunctions.h>

#include "qmimedatabase_p.h"

/*!
    \class QMimeType
    \brief The QMimeType class describes types of file or data, represented by a MIME type string.
    \since 5.0

    A QMimeType is a type of QMimeDatabase: its name, such as \c{text/html}, a description for
    people, the patterns of file names that it has, its icons, and the types that it is a kind of.
    Get one from QMimeDatabase:

    \code
    const QMimeType type = QMimeDatabase().mimeTypeForName(u"image/svg+xml"_s);
    type.comment();         // "SVG image"
    type.preferredSuffix(); // "svg"
    type.inherits(u"application/xml"_s); // true
    \endcode

    A QMimeType is small: it holds the name of the type and asks the database for the rest.

    \differences
    The properties are read from the database each time, so they change if the database is
    reloaded. The functions that take a name take a \c{const QString &}.
    \enddifferences

    \sa QMimeDatabase
*/

using QtPrivate::MimeDatabase;

/*!
    Constructs an invalid type.

    \sa isValid()
*/
QMimeType::QMimeType() = default;

/*!
    Constructs a copy of \a other.
*/
QMimeType::QMimeType(const QMimeType &other) = default;

/*!
    Assigns \a other to this type and returns a reference to it.
*/
QMimeType &QMimeType::operator=(const QMimeType &other) = default;

/*!
    Move-constructs a type from \a other, which becomes invalid.
*/
QMimeType::QMimeType(QMimeType &&other) noexcept : m_name(std::move(other.m_name)) { other.m_name.clear(); }

/*!
    Move-assigns \a other to this type and returns a reference to it.
*/
QMimeType &QMimeType::operator=(QMimeType &&other) noexcept {
    m_name = std::move(other.m_name);
    other.m_name.clear();
    return *this;
}

/*!
    Destroys the type.
*/
QMimeType::~QMimeType() = default;

/*!
    Returns \c true if the type is one of the database. A type that QMimeDatabase did not find
    is invalid.
*/
bool QMimeType::isValid() const { return !m_name.isEmpty(); }

/*!
    Returns \c true if this is \c{application/octet-stream}, the type of data whose type is not
    known.
*/
bool QMimeType::isDefault() const { return m_name == QString("application/octet-stream"); }

/*!
    Returns the name of the type, such as \c{image/png}.
*/
QString QMimeType::name() const { return m_name; }

/*!
    Returns the description of the type for people, in the language of the default locale if
    the database has it, such as \c{PNG image}. A type without a description gives its name.
*/
QString QMimeType::comment() const {
    if (!isValid())
        return QString();
    std::unique_lock<std::mutex> lock;
    return MimeDatabase::instance(lock).comment(m_name);
}

/*!
    Returns the name of the icon of the group of types that this type belongs to, such as
    \c{image-x-generic}. Without one in the database, it is the media type followed by
    \c{-x-generic}.

    \sa iconName()
*/
QString QMimeType::genericIconName() const {
    if (!isValid())
        return QString();
    std::unique_lock<std::mutex> lock;
    const MimeDatabase::Type *type = MimeDatabase::instance(lock).type(m_name);
    if (type && !type->genericIcon.isEmpty())
        return type->genericIcon;
    return m_name.left(m_name.indexOf(QChar(u'/'))) + QString("-x-generic");
}

/*!
    Returns the name of the icon of the type, in the icon themes of freedesktop.org. Without one in
    the database, it is the name of the type with the slash replaced by a dash, such as
    \c{image-png}.

    \sa genericIconName()
*/
QString QMimeType::iconName() const {
    if (!isValid())
        return QString();
    std::unique_lock<std::mutex> lock;
    const MimeDatabase::Type *type = MimeDatabase::instance(lock).type(m_name);
    if (type && !type->icon.isEmpty())
        return type->icon;
    QString icon = m_name;
    icon.replace(QChar(u'/'), QChar(u'-'));
    return icon;
}

/*!
    Returns the patterns of the names of files of this type, such as \c{*.png}.

    \sa suffixes()
*/
QStringList QMimeType::globPatterns() const {
    if (!isValid())
        return QStringList();
    std::unique_lock<std::mutex> lock;
    return MimeDatabase::instance(lock).globsOf(m_name);
}

/*!
    Returns the types that this type is a kind of, directly. A text type without others is a kind
    of \c{text/plain}, and any other type that is not a directory a kind of
    \c{application/octet-stream}.

    \sa allAncestors(), inherits()
*/
QStringList QMimeType::parentMimeTypes() const {
    if (!isValid())
        return QStringList();
    std::unique_lock<std::mutex> lock;
    return MimeDatabase::instance(lock).parentsOf(m_name);
}

/*!
    Returns the types that this type is a kind of, directly or through others, the closest
    first.

    \sa parentMimeTypes()
*/
QStringList QMimeType::allAncestors() const {
    if (!isValid())
        return QStringList();
    std::unique_lock<std::mutex> lock;
    const MimeDatabase &db = MimeDatabase::instance(lock);
    QStringList ancestors;
    QStringList pending = db.parentsOf(m_name);
    while (!pending.isEmpty()) {
        const QString current = pending.takeFirst();
        if (ancestors.contains(current) || current == m_name)
            continue;
        ancestors.append(current);
        pending.append(db.parentsOf(current));
    }
    return ancestors;
}

/*!
    Returns the other names of the type, which QMimeDatabase::mimeTypeForName() also finds it by.
*/
QStringList QMimeType::aliases() const {
    if (!isValid())
        return QStringList();
    std::unique_lock<std::mutex> lock;
    const MimeDatabase::Type *type = MimeDatabase::instance(lock).type(m_name);
    return type ? type->aliases : QStringList();
}

/*!
    Returns the suffixes of the names of files of this type, without the dot: those of the
    patterns that are \c{*.} followed by a suffix.

    \sa preferredSuffix(), globPatterns()
*/
QStringList QMimeType::suffixes() const {
    QStringList out;
    for (const QString &pattern : globPatterns()) {
        const QString suffix = pattern.mid(2);
        if (pattern.startsWith(QString("*.")) && !suffix.contains(QChar(u'*')) && !suffix.contains(QChar(u'?')) &&
            !suffix.contains(QChar(u'[')))
            out.append(suffix);
    }
    return out;
}

/*!
    Returns the suffix that a file of this type is given when it is saved, the first of
    suffixes(), or an empty string.
*/
QString QMimeType::preferredSuffix() const {
    const QStringList all = suffixes();
    return all.isEmpty() ? QString() : all.first();
}

/*!
    Returns \c true if this type is \a mimeTypeName, or one of its aliases, or a kind of it,
    directly or through others.

    \sa allAncestors()
*/
bool QMimeType::inherits(const QString &mimeTypeName) const {
    if (!isValid())
        return false;
    std::unique_lock<std::mutex> lock;
    const MimeDatabase &db = MimeDatabase::instance(lock);
    const QString resolved = db.resolve(mimeTypeName);
    return db.inherits(m_name, resolved.isEmpty() ? mimeTypeName : resolved);
}

/*!
    Returns a filter for a file dialog, such as \c{PNG image (*.png)}: the description and the
    patterns of the names. It is empty for a type without patterns.
*/
QString QMimeType::filterString() const {
    const QStringList patterns = globPatterns();
    if (patterns.isEmpty())
        return QString();
    return comment() + QString(" (") + patterns.join(QChar(u' ')) + QChar(u')');
}

std::size_t qHash(const QMimeType &key, std::size_t seed) noexcept { return qHash(key.name(), seed); }

#include "moc_qmimetype.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
