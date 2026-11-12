// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace QtPrivate {

// The MIME database that QMimeDatabase and QMimeType read: the shared-mime-info files of the XDG data
// directories, or the built-in table where there are none.
class MimeDatabase {
public:
    struct Glob {
        QString pattern;
        QString type;
        int weight = 50;
        bool caseSensitive = false;
    };
    // A test of the magic of a type: `value` (with `mask`) somewhere in [offset, offset + range).
    struct MagicRule {
        qsizetype offset = 0;
        qsizetype range = 1;
        QByteArray value;
        QByteArray mask;
        std::vector<MagicRule> children; // one of them must match too, if there are any
    };
    struct Magic {
        QString type;
        int priority = 50;
        std::vector<MagicRule> rules; // any of them
    };
    struct Type {
        QStringList parents;
        QStringList aliases;
        QString genericIcon;
        QString icon;
        QString comment; // of the built-in table, or one that has been read
        bool commentRead = false;
    };

    // The database, loaded or reloaded when the XDG directories have changed. Use it under the lock.
    static MimeDatabase &instance(std::unique_lock<std::mutex> &lock);

    QString resolve(const QString &nameOrAlias) const; // the name of the type, or an empty string
    bool has(const QString &name) const { return m_types.contains(name); }
    const Type *type(const QString &name) const;
    QString comment(const QString &name);
    QStringList globsOf(const QString &name) const;
    QStringList parentsOf(const QString &name) const;
    QStringList allTypes() const { return m_typeOrder; }

    // The types whose globs fit the name best, and the suffix that the best "*.ext" glob matched.
    QStringList typesForFileName(const QString &fileName, QString *suffix = nullptr) const;
    // The type that the contents give, or an empty string; *accuracy is the priority of the magic.
    QString typeForData(const QByteArray &data, int *accuracy = nullptr) const;
    qsizetype magicExtent() const { return m_magicExtent; }
    bool inherits(const QString &name, const QString &ancestor) const;

private:
    void load(const QStringList &directories);
    void loadBuiltIn();
    void addType(const QString &name);
    void readDirectory(const QString &directory);
    void readMagic(const QByteArray &data);

    QStringList m_directories;     // that were looked at; a change of them reloads the database
    QStringList m_mimeDirectories; // that hold a database, the most important first
    QHash<QString, Type> m_types;
    QStringList m_typeOrder;
    QHash<QString, QString> m_aliases;
    std::vector<Glob> m_globs;
    std::vector<Magic> m_magic; // ordered by priority, highest first
    qsizetype m_magicExtent = 0;
};

// The built-in table, in qmimebuiltin.cpp: each type with its description, patterns, parents, aliases and generic
// icon, and each test of magic.
void addBuiltInMimeTypes(
    const std::function<void(const char *, const char *, const char *, const char *, const char *, const char *)> &type,
    const std::function<void(const char *, int, qsizetype, const char *, qsizetype)> &magic);

} // namespace QtPrivate
