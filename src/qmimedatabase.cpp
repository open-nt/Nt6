// MIT License - see LICENSE
#include <QtCore/qmimedatabase.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlocale.h>
#include <QtCore/qregularexpression.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtCore/qurl.h>
#include <QtCore/qxmlstream.h>

#include "qmimedatabase_p.h"

#include <algorithm>
#include <cstring>
#include <functional>

/*!
    \class QMimeDatabase
    \brief The QMimeDatabase class maintains a database of MIME types.
    \since 5.0

    A MIME type, such as \c{image/png} or \c{text/html}, says what kind of data a file holds.
    QMimeDatabase finds the type of a file from its name, from its contents, or from both, and
    describes every type it knows with a QMimeType:

    \code
    QMimeDatabase db;
    const QMimeType type = db.mimeTypeForFile(path);
    if (type.inherits(u"text/plain"_s))
        openInEditor(path);
    qDebug() << type.name() << type.comment() << type.preferredSuffix();
    \endcode

    On Linux the types are those of the freedesktop.org shared-mime-info database, which the
    system keeps in \c{mime} under the XDG data directories: \c{$XDG_DATA_HOME} (by default
    \c{~/.local/share}) and \c{$XDG_DATA_DIRS} (by default \c{/usr/local/share} and
    \c{/usr/share}). Where there is no such database, a built-in table of about sixty common types
    is used instead. The database is read when it is first used, and again when the XDG
    directories change.

    The functions may be called from any thread.

    \differences
    The database is read from the text files that \c{update-mime-database} writes (\c globs2,
    \c magic, \c subclasses, \c aliases and the icon lists), not from \c mime.cache or the XML
    packages; the descriptions of the types are read from their XML files when they are first
    asked for. A change of the files is noticed only when the XDG directories change. There is
    no built-in copy of the whole freedesktop.org database, so without one only the types of the
    built-in table are known. Special files such as sockets and devices get the type of their
    name, not an \c{inode/} type. The functions that take a name take a \c{const QString &}.
    \enddifferences

    \sa QMimeType
*/

namespace QtPrivate {

namespace {

bool hasWildcard(const QString &pattern) {
    return pattern.contains(QChar(u'*')) || pattern.contains(QChar(u'?')) || pattern.contains(QChar(u'['));
}

// A "*.ext" pattern whose extension has no wildcards: the common case, matched without a regular expression.
bool isSuffixPattern(const QString &pattern) {
    return pattern.startsWith(QString("*.")) && !hasWildcard(pattern.mid(2));
}

QStringList lines(const QByteArray &data) {
    QStringList out;
    for (const QByteArray &line : data.split('\n')) {
        if (!line.isEmpty() && line.at(0) != '#')
            out.append(QString::fromUtf8(line));
    }
    return out;
}

QByteArray readFile(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool ruleMatches(const MimeDatabase::MagicRule &rule, const QByteArray &data) {
    const qsizetype length = rule.value.size();
    bool found = false;
    for (qsizetype at = rule.offset; !found && at < rule.offset + rule.range && at + length <= data.size(); ++at) {
        bool same = true;
        for (qsizetype i = 0; same && i < length; ++i) {
            const auto byte = static_cast<unsigned char>(data.at(at + i));
            const auto want = static_cast<unsigned char>(rule.value.at(i));
            const unsigned char mask = rule.mask.isEmpty() ? 0xffu : static_cast<unsigned char>(rule.mask.at(i));
            same = (byte & mask) == (want & mask);
        }
        found = same;
    }
    if (!found)
        return false;
    if (rule.children.empty())
        return true;
    return std::any_of(rule.children.begin(), rule.children.end(),
                       [&](const MimeDatabase::MagicRule &child) { return ruleMatches(child, data); });
}

qsizetype ruleExtent(const MimeDatabase::MagicRule &rule) {
    qsizetype extent = rule.offset + rule.range - 1 + rule.value.size();
    for (const MimeDatabase::MagicRule &child : rule.children)
        extent = std::max(extent, ruleExtent(child));
    return extent;
}

// The directories that may hold a database, the most important first.
QStringList mimeDirectories() {
    QString home = qEnvironmentVariable("XDG_DATA_HOME");
    if (home.isEmpty())
        home = QDir::homePath() + QString("/.local/share");
    QStringList directories{home};
    QString system = qEnvironmentVariable("XDG_DATA_DIRS");
    if (system.isEmpty())
        system = QString("/usr/local/share:/usr/share");
    for (const QString &directory : system.split(QChar(u':'), Qt::SkipEmptyParts))
        directories.append(directory);
    for (QString &directory : directories)
        directory += QString("/mime");
    return directories;
}

} // namespace

MimeDatabase &MimeDatabase::instance(std::unique_lock<std::mutex> &lock) {
    static std::mutex mutex;
    static MimeDatabase db;
    lock = std::unique_lock<std::mutex>(mutex);
    const QStringList directories = mimeDirectories();
    if (db.m_typeOrder.isEmpty() || directories != db.m_directories)
        db.load(directories);
    return db;
}

void MimeDatabase::addType(const QString &name) {
    if (name.isEmpty() || m_types.contains(name))
        return;
    m_types.insert(name, Type());
    m_typeOrder.append(name);
}

void MimeDatabase::load(const QStringList &directories) {
    *this = MimeDatabase();
    m_directories = directories;
    for (const QString &directory : directories) {
        const QDir dir(directory);
        if (dir.exists(QString("globs2")) || dir.exists(QString("globs")) || dir.exists(QString("magic")) ||
            dir.exists(QString("types")))
            m_mimeDirectories.append(directory);
    }
    if (m_mimeDirectories.isEmpty()) {
        loadBuiltIn();
    } else {
        // The less important directories first, so that the more important ones can take things back.
        for (auto it = m_mimeDirectories.crbegin(); it != m_mimeDirectories.crend(); ++it)
            readDirectory(*it);
        // A database has these, but they must be there in any case.
        MimeDatabase builtIn;
        builtIn.loadBuiltIn();
        for (const char *name :
             {"application/octet-stream", "text/plain", "inode/directory", "application/x-zerosize"}) {
            const QString key = QString::fromLatin1(name);
            if (!m_types.contains(key)) {
                addType(key);
                m_types[key] = builtIn.m_types.value(key);
            }
        }
    }
    std::stable_sort(m_magic.begin(), m_magic.end(),
                     [](const Magic &a, const Magic &b) { return a.priority > b.priority; });
    for (const Magic &magic : m_magic) {
        for (const MagicRule &rule : magic.rules)
            m_magicExtent = std::max(m_magicExtent, ruleExtent(rule));
    }
    m_magicExtent = std::min<qsizetype>(m_magicExtent, 64 * 1024);
}

void MimeDatabase::loadBuiltIn() {
    addBuiltInMimeTypes(
        [this](const char *name, const char *comment, const char *globs, const char *parents, const char *aliases,
               const char *genericIcon) {
            const QString key = QString::fromLatin1(name);
            addType(key);
            Type &type = m_types[key];
            type.comment = QString::fromLatin1(comment);
            type.commentRead = true;
            type.genericIcon = QString::fromLatin1(genericIcon);
            for (const QString &glob : QString::fromLatin1(globs).split(QChar(u' '), Qt::SkipEmptyParts))
                m_globs.push_back(Glob{glob, key, 50, false});
            for (const QString &parent : QString::fromLatin1(parents).split(QChar(u' '), Qt::SkipEmptyParts))
                type.parents.append(parent);
            for (const QString &alias : QString::fromLatin1(aliases).split(QChar(u' '), Qt::SkipEmptyParts)) {
                type.aliases.append(alias);
                m_aliases.insert(alias, key);
            }
        },
        [this](const char *name, int priority, qsizetype offset, const char *bytes, qsizetype length) {
            Magic magic;
            magic.type = QString::fromLatin1(name);
            magic.priority = priority;
            MagicRule rule;
            rule.offset = offset;
            rule.value = QByteArray(bytes, length);
            magic.rules.push_back(rule);
            // Several tests of one type and priority are alternatives.
            for (Magic &existing : m_magic) {
                if (existing.type == magic.type && existing.priority == priority) {
                    existing.rules.push_back(rule);
                    return;
                }
            }
            m_magic.push_back(magic);
        });
}

void MimeDatabase::readDirectory(const QString &directory) {
    for (const QString &name : lines(readFile(directory + QString("/types"))))
        addType(name.trimmed());

    const QByteArray globs2 = readFile(directory + QString("/globs2"));
    if (!globs2.isEmpty()) {
        for (const QString &line : lines(globs2)) {
            // weight:type:pattern[:flags]; the pattern may itself contain colons only before the flags.
            const QStringList parts = line.split(QChar(u':'));
            if (parts.size() < 3)
                continue;
            const QString &type = parts.at(1);
            const QString &pattern = parts.at(2);
            addType(type);
            if (pattern == QString("__NOGLOBS__")) {
                m_globs.erase(
                    std::remove_if(m_globs.begin(), m_globs.end(), [&](const Glob &g) { return g.type == type; }),
                    m_globs.end());
                continue;
            }
            const bool caseSensitive = parts.size() > 3 && parts.at(3).split(QChar(u',')).contains(QString("cs"));
            m_globs.push_back(Glob{pattern, type, parts.at(0).toInt(), caseSensitive});
        }
    } else {
        for (const QString &line : lines(readFile(directory + QString("/globs")))) {
            const qsizetype colon = line.indexOf(QChar(u':'));
            if (colon <= 0)
                continue;
            addType(line.left(colon));
            m_globs.push_back(Glob{line.mid(colon + 1), line.left(colon), 50, false});
        }
    }

    for (const QString &line : lines(readFile(directory + QString("/aliases")))) {
        const QStringList parts = line.split(QChar(u' '), Qt::SkipEmptyParts);
        if (parts.size() != 2)
            continue;
        addType(parts.at(1));
        m_aliases.insert(parts.at(0), parts.at(1));
        if (!m_types[parts.at(1)].aliases.contains(parts.at(0)))
            m_types[parts.at(1)].aliases.append(parts.at(0));
    }
    for (const QString &line : lines(readFile(directory + QString("/subclasses")))) {
        const QStringList parts = line.split(QChar(u' '), Qt::SkipEmptyParts);
        if (parts.size() != 2)
            continue;
        addType(parts.at(0));
        if (!m_types[parts.at(0)].parents.contains(parts.at(1)))
            m_types[parts.at(0)].parents.append(parts.at(1));
    }
    for (const char *file : {"/generic-icons", "/icons"}) {
        const bool generic = std::strcmp(file, "/generic-icons") == 0;
        for (const QString &line : lines(readFile(directory + QString::fromLatin1(file)))) {
            const qsizetype colon = line.indexOf(QChar(u':'));
            if (colon <= 0)
                continue;
            addType(line.left(colon));
            (generic ? m_types[line.left(colon)].genericIcon : m_types[line.left(colon)].icon) = line.mid(colon + 1);
        }
    }
    readMagic(readFile(directory + QString("/magic")));
}

// The magic file of shared-mime-info: sections "[priority:type]" of rules
// "[indent]>offset=<length><value>[&<mask>][~word-size][+range]".
void MimeDatabase::readMagic(const QByteArray &data) {
    static const char header[] = "MIME-Magic\0\n";
    if (data.size() < 12 || std::memcmp(data.constData(), header, 12) != 0)
        return;
    qsizetype pos = 12;
    const qsizetype size = data.size();
    const auto number = [&](qsizetype &at) {
        qsizetype value = 0;
        while (at < size && data.at(at) >= '0' && data.at(at) <= '9')
            value = value * 10 + (data.at(at++) - '0');
        return value;
    };
    const auto skipLine = [&](qsizetype &at) {
        while (at < size && data.at(at) != '\n')
            ++at;
        ++at;
    };
    Magic current;
    std::vector<std::vector<MagicRule> *> levels;
    const auto finish = [&] {
        if (!current.type.isEmpty() && !current.rules.empty())
            m_magic.push_back(std::move(current));
        current = Magic();
        levels.clear();
    };
    while (pos < size) {
        if (data.at(pos) == '[') {
            finish();
            const qsizetype close = data.indexOf("]\n", pos);
            if (close < 0)
                return;
            const QByteArray section = data.mid(pos + 1, close - pos - 1);
            const qsizetype colon = section.indexOf(':');
            current.priority = section.left(colon).toInt();
            current.type = QString::fromUtf8(section.mid(colon + 1));
            addType(current.type);
            levels.push_back(&current.rules);
            pos = close + 2;
            continue;
        }
        if (levels.empty() || data.at(pos) == '_') {
            // __NOMAGIC__ takes back the magic of the less important directories.
            if (data.mid(pos, 11) == QByteArray("__NOMAGIC__")) {
                const QString type = current.type;
                m_magic.erase(
                    std::remove_if(m_magic.begin(), m_magic.end(), [&](const Magic &m) { return m.type == type; }),
                    m_magic.end());
            }
            skipLine(pos);
            continue;
        }
        const qsizetype indent = number(pos);
        if (pos >= size || data.at(pos) != '>') {
            skipLine(pos);
            continue;
        }
        ++pos;
        MagicRule rule;
        rule.offset = number(pos);
        if (pos + 3 > size || data.at(pos) != '=')
            return;
        const qsizetype length =
            (static_cast<unsigned char>(data.at(pos + 1)) << 8) | static_cast<unsigned char>(data.at(pos + 2));
        pos += 3;
        if (pos + length > size)
            return;
        rule.value = data.mid(pos, length);
        pos += length;
        if (pos < size && data.at(pos) == '&') {
            rule.mask = data.mid(pos + 1, length);
            pos += 1 + length;
        }
        qsizetype wordSize = 1;
        if (pos < size && data.at(pos) == '~') {
            ++pos;
            wordSize = number(pos);
        }
        if (pos < size && data.at(pos) == '+') {
            ++pos;
            rule.range = std::max<qsizetype>(1, number(pos));
        }
        skipLine(pos); // the newline, after anything that is not understood
        // The value is big-endian; on a little-endian host words of more than one byte are swapped.
        const quint16 probe = 1;
        unsigned char firstByte = 0;
        std::memcpy(&firstByte, &probe, 1);
        if (wordSize > 1 && firstByte == 1 && length % wordSize == 0) {
            for (QByteArray *bytes : {&rule.value, &rule.mask}) {
                for (qsizetype i = 0; i + wordSize <= bytes->size(); i += wordSize)
                    std::reverse(bytes->begin() + i, bytes->begin() + i + wordSize);
            }
        }
        if (indent + 1 > static_cast<qsizetype>(levels.size()))
            continue; // a rule without a parent
        levels.resize(static_cast<std::size_t>(indent + 1));
        std::vector<MagicRule> *target = levels.back();
        target->push_back(std::move(rule));
        levels.push_back(&target->back().children);
    }
    finish();
}

QString MimeDatabase::resolve(const QString &nameOrAlias) const {
    if (m_types.contains(nameOrAlias))
        return nameOrAlias;
    QString canonical = m_aliases.value(nameOrAlias);
    if (!canonical.isEmpty() && m_types.contains(canonical))
        return canonical;
    // Names are case-insensitive.
    const QString lower = nameOrAlias.toLower();
    if (lower != nameOrAlias)
        return resolve(lower);
    return QString();
}

const MimeDatabase::Type *MimeDatabase::type(const QString &name) const {
    const auto it = m_types.constFind(name);
    return it == m_types.constEnd() ? nullptr : &it.value();
}

QString MimeDatabase::comment(const QString &name) {
    auto it = m_types.find(name);
    if (it == m_types.end())
        return QString();
    if (!it->commentRead) {
        it->commentRead = true;
        // The description in the language of the default locale, or else the one without a language.
        const QString locale = QLocale().name();
        const QString language = locale.left(locale.indexOf(QChar(u'_')));
        QString best, plain;
        int bestScore = 0;
        for (const QString &directory : m_mimeDirectories) {
            QFile file(directory + QChar(u'/') + name + QString(".xml"));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QXmlStreamReader xml(&file);
            while (!xml.atEnd()) {
                if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != u"comment")
                    continue;
                const QString lang =
                    xml.attributes().value(QString("http://www.w3.org/XML/1998/namespace"), QString("lang")).toString();
                const QString text = xml.readElementText();
                QString normalized = lang;
                normalized.replace(QChar(u'-'), QChar(u'_'));
                const int score =
                    normalized.isEmpty() ? 0 : (normalized == locale ? 2 : (normalized == language ? 1 : -1));
                if (normalized.isEmpty())
                    plain = text;
                else if (score > bestScore) {
                    best = text;
                    bestScore = score;
                }
            }
            break; // the most important file that exists
        }
        const QString found = bestScore > 0 ? best : plain;
        if (!found.isEmpty())
            it->comment = found;
    }
    return it->comment.isEmpty() ? name : it->comment;
}

QStringList MimeDatabase::globsOf(const QString &name) const {
    QStringList out;
    for (const Glob &glob : m_globs) {
        if (glob.type == name && !out.contains(glob.pattern))
            out.append(glob.pattern);
    }
    return out;
}

QStringList MimeDatabase::parentsOf(const QString &name) const {
    QStringList parents;
    if (const Type *t = type(name)) {
        for (const QString &parent : t->parents) {
            const QString resolved = resolve(parent);
            if (!resolved.isEmpty() && !parents.contains(resolved))
                parents.append(resolved);
        }
    }
    if (parents.isEmpty()) {
        // Every text is plain text, and everything that is not a directory or the like is a stream of bytes.
        const QString plain("text/plain"), octet("application/octet-stream");
        if (name.startsWith(QString("text/")) && name != plain)
            parents.append(plain);
        else if (name != octet && !name.startsWith(QString("inode/")) && !name.startsWith(QString("all/")) &&
                 !name.startsWith(QString("x-content/")))
            parents.append(octet);
    }
    return parents;
}

bool MimeDatabase::inherits(const QString &name, const QString &ancestor) const {
    QStringList pending{name}, seen;
    while (!pending.isEmpty()) {
        const QString current = pending.takeFirst();
        if (current == ancestor)
            return true;
        if (seen.contains(current))
            continue;
        seen.append(current);
        pending.append(parentsOf(current));
    }
    return false;
}

QStringList MimeDatabase::typesForFileName(const QString &fileName, QString *suffix) const {
    const QString name = fileName.mid(fileName.lastIndexOf(QChar(u'/')) + 1);
    QStringList best;
    int bestWeight = -1;
    qsizetype bestLength = -1;
    QString bestSuffix;
    if (name.isEmpty())
        return best;
    const QString lowerName = name.toLower();
    for (const Glob &glob : m_globs) {
        bool match = false;
        if (!hasWildcard(glob.pattern)) {
            match = glob.caseSensitive ? name == glob.pattern : lowerName == glob.pattern.toLower();
        } else if (isSuffixPattern(glob.pattern)) {
            const QString ext = glob.pattern.mid(1); // ".ext"
            match = name.size() > ext.size() &&
                    (glob.caseSensitive ? name.endsWith(ext) : lowerName.endsWith(ext.toLower()));
        } else {
            match = QRegularExpression::fromWildcard(glob.pattern,
                                                     glob.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive)
                        .match(name)
                        .hasMatch();
        }
        if (!match)
            continue;
        if (isSuffixPattern(glob.pattern) && glob.pattern.size() - 2 > bestSuffix.size())
            bestSuffix = glob.pattern.mid(2);
        const qsizetype length = glob.pattern.size();
        if (glob.weight > bestWeight || (glob.weight == bestWeight && length > bestLength)) {
            best.clear();
            bestWeight = glob.weight;
            bestLength = length;
        }
        if (glob.weight == bestWeight && length == bestLength && !best.contains(glob.type))
            best.append(glob.type);
    }
    if (suffix)
        *suffix = bestSuffix;
    return best;
}

QString MimeDatabase::typeForData(const QByteArray &data, int *accuracy) const {
    QString best;
    int bestPriority = -1;
    for (const Magic &magic : m_magic) {
        if (magic.priority < bestPriority)
            break; // ordered by priority: nothing better comes
        const bool matches = std::any_of(magic.rules.begin(), magic.rules.end(),
                                         [&](const MagicRule &rule) { return ruleMatches(rule, data); });
        if (!matches)
            continue;
        // Of two matches of one priority, the more specific one wins.
        if (best.isEmpty() || inherits(magic.type, best)) {
            best = magic.type;
            bestPriority = magic.priority;
        }
    }
    if (accuracy)
        *accuracy = bestPriority;
    return best;
}

} // namespace QtPrivate

using QtPrivate::MimeDatabase;

namespace {

// Text has no control characters other than those of layout, or starts with a byte order mark.
bool looksLikeText(const QByteArray &data) {
    if (data.startsWith("\xef\xbb\xbf") || data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff"))
        return true;
    const qsizetype n = std::min<qsizetype>(data.size(), 512);
    for (qsizetype i = 0; i < n; ++i) {
        const auto c = static_cast<unsigned char>(data.at(i));
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != 0x1b)
            return false;
    }
    return true;
}

QByteArray peekData(QIODevice *device, qsizetype size) {
    if (!device)
        return QByteArray();
    if (!device->isOpen()) {
        if (!device->open(QIODevice::ReadOnly))
            return QByteArray();
        const QByteArray data = device->read(size);
        device->close();
        return data;
    }
    return device->isReadable() ? device->peek(size) : QByteArray();
}

} // namespace

/*!
    Constructs a database. All databases share the same data, which is read once.
*/
QMimeDatabase::QMimeDatabase() = default;


/*!
    Returns the type called \a nameOrAlias, which may be an alias of a type, such as
    \c{application/x-pdf} for \c{application/pdf}. Returns an invalid type if there is no such
    type.

    \sa QMimeType::isValid()
*/
QMimeType QMimeDatabase::mimeTypeForName(const QString &nameOrAlias) const {
    std::unique_lock<std::mutex> lock;
    const QString name = MimeDatabase::instance(lock).resolve(nameOrAlias);
    return name.isEmpty() ? QMimeType() : QMimeType(name);
}

/*!
    Returns the type of the file \a fileName, found as \a mode says. A directory is
    \c{inode/directory}. A file whose type cannot be found is \c{application/octet-stream}, or
    \c{text/plain} if its contents look like text.

    \sa mimeTypeForData(), mimeTypesForFileName()
*/
QMimeType QMimeDatabase::mimeTypeForFile(const QString &fileName, MatchMode mode) const {
    return mimeTypeForFile(QFileInfo(fileName), mode);
}

/*!
    \overload
    Returns the type of the file \a fileInfo, found as \a mode says.
*/
QMimeType QMimeDatabase::mimeTypeForFile(const QFileInfo &fileInfo, MatchMode mode) const {
    if (fileInfo.isDir())
        return mimeTypeForName(QString("inode/directory"));
    const QString path = fileInfo.filePath();
    if (mode == MatchExtension) {
        const QList<QMimeType> byName = mimeTypesForFileName(path);
        return byName.isEmpty() ? mimeTypeForName(QString("application/octet-stream")) : byName.first();
    }
    if (mode == MatchDefault) {
        const QList<QMimeType> byName = mimeTypesForFileName(path);
        if (byName.size() == 1)
            return byName.first();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // Without the contents the name is all there is.
        if (mode == MatchDefault) {
            const QList<QMimeType> byName = mimeTypesForFileName(path);
            if (!byName.isEmpty())
                return byName.first();
        }
        return mimeTypeForName(QString("application/octet-stream"));
    }
    if (mode == MatchContent)
        return mimeTypeForData(&file);
    return mimeTypeForFileNameAndData(path, &file);
}

/*!
    Returns the types that the name \a fileName fits best. The directory part of the name is
    ignored, and the file need not exist. Usually there is one type; a name such as \c{foo.ts}
    fits several equally well. The list is empty if the name fits no type.

    \sa mimeTypeForFile(), suffixForFileName()
*/
QList<QMimeType> QMimeDatabase::mimeTypesForFileName(const QString &fileName) const {
    std::unique_lock<std::mutex> lock;
    const QStringList names = MimeDatabase::instance(lock).typesForFileName(fileName);
    QList<QMimeType> types;
    for (const QString &name : names)
        types.append(QMimeType(name));
    return types;
}

/*!
    Returns the type of \a data, found from its contents. Empty data is
    \c{application/x-zerosize}; data that fits no type is \c{text/plain} if it looks like text,
    and \c{application/octet-stream} otherwise.
*/
QMimeType QMimeDatabase::mimeTypeForData(const QByteArray &data) const {
    QString found;
    {
        std::unique_lock<std::mutex> lock;
        found = MimeDatabase::instance(lock).typeForData(data);
    }
    if (!found.isEmpty())
        return QMimeType(found);
    if (data.isEmpty())
        return mimeTypeForName(QString("application/x-zerosize"));
    return mimeTypeForName(looksLikeText(data) ? QString("text/plain") : QString("application/octet-stream"));
}

/*!
    \overload
    Returns the type of the data that \a device holds, found from its contents. The data is
    peeked at, so the position of an open device does not change; a device that is not open is
    opened for reading and closed again.
*/
QMimeType QMimeDatabase::mimeTypeForData(QIODevice *device) const {
    qsizetype extent = 0;
    {
        std::unique_lock<std::mutex> lock;
        extent = MimeDatabase::instance(lock).magicExtent();
    }
    return mimeTypeForData(peekData(device, std::max<qsizetype>(extent, 512)));
}

/*!
    Returns the type of the file that \a url refers to: from the name and contents of a local
    file, and from the name alone for any other URL.

    \sa mimeTypeForFile()
*/
QMimeType QMimeDatabase::mimeTypeForUrl(const QUrl &url) const {
    if (url.isLocalFile())
        return mimeTypeForFile(url.toLocalFile());
    const QString path = url.path();
    const QList<QMimeType> byName = mimeTypesForFileName(path);
    return byName.isEmpty() ? mimeTypeForName(QString("application/octet-stream")) : byName.first();
}

/*!
    Returns the type of a file called \a fileName whose contents \a device holds. The name
    decides when it fits one type; when it fits several, or none, the contents decide. The
    device is read as mimeTypeForData() reads it.
*/
QMimeType QMimeDatabase::mimeTypeForFileNameAndData(const QString &fileName, QIODevice *device) const {
    qsizetype extent = 0;
    {
        std::unique_lock<std::mutex> lock;
        extent = MimeDatabase::instance(lock).magicExtent();
    }
    return mimeTypeForFileNameAndData(fileName, peekData(device, std::max<qsizetype>(extent, 512)));
}

/*!
    \overload
    Returns the type of a file called \a fileName whose contents are \a data.
*/
QMimeType QMimeDatabase::mimeTypeForFileNameAndData(const QString &fileName, const QByteArray &data) const {
    QStringList candidates;
    QString magic;
    {
        std::unique_lock<std::mutex> lock;
        MimeDatabase &db = MimeDatabase::instance(lock);
        candidates = db.typesForFileName(fileName);
        if (candidates.size() == 1)
            return QMimeType(candidates.first());
        magic = db.typeForData(data);
        if (!magic.isEmpty()) {
            // The contents choose among the names: the same type, or one that is more specific.
            for (const QString &candidate : candidates) {
                if (candidate == magic || db.inherits(magic, candidate))
                    return QMimeType(magic);
                if (db.inherits(candidate, magic))
                    return QMimeType(candidate);
            }
        }
        if (!candidates.isEmpty())
            return QMimeType(candidates.first());
    }
    return mimeTypeForData(data);
}

/*!
    Returns the suffix of \a fileName that a type of the database knows, such as \c{tar.gz} for
    \c{archive.tar.gz}, or an empty string if the name has none.

    \sa QMimeType::suffixes()
*/
QString QMimeDatabase::suffixForFileName(const QString &fileName) const {
    std::unique_lock<std::mutex> lock;
    QString suffix;
    MimeDatabase::instance(lock).typesForFileName(fileName, &suffix);
    // The suffix as the name has it, which may be in other case than the pattern.
    return suffix.isEmpty() ? suffix : fileName.right(suffix.size());
}

/*!
    Returns every type that the database knows, without the aliases.
*/
QList<QMimeType> QMimeDatabase::allMimeTypes() const {
    std::unique_lock<std::mutex> lock;
    const QStringList names = MimeDatabase::instance(lock).allTypes();
    QList<QMimeType> types;
    for (const QString &name : names)
        types.append(QMimeType(name));
    return types;
}
