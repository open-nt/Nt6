// MIT License - see LICENSE
#include <QtCore/qtranslator.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qlocale.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

// The .qm format: a 16-byte magic number, then sections of a tag byte and a big-endian 32-bit
// length. The hash section is a sorted table of (hash, offset) pairs pointing into the message
// section; each message is a list of tagged fields ending with an end tag.
const unsigned char kMagic[16] = {0x3c, 0xb8, 0x64, 0x18, 0xca, 0xef, 0x9c, 0x95, 0xcd, 0x21, 0x1c, 0xbf, 0x60, 0xa1, 0xbd, 0xdd};

enum Section : unsigned char { Contexts = 0x2f, Hashes = 0x42, Messages = 0x69, NumerusRules = 0x88, Dependencies = 0x96, Language = 0xa7 };
enum MessageTag : unsigned char {
    TagEnd = 1, TagSourceText16 = 2, TagTranslation = 3, TagContext16 = 4, TagObsolete1 = 5, TagSourceText = 6, TagContext = 7, TagComment = 8
};

// Numerus rule operators.
enum : unsigned char { OpEq = 0x01, OpLt = 0x02, OpLeq = 0x03, OpBetween = 0x04, OpMask = 0x07, OpNot = 0x08, OpMod10 = 0x10, OpMod100 = 0x20,
                       OpLead1000 = 0x40, OpAnd = 0xfd, OpOr = 0xfe, OpNewRule = 0xff };

quint32 readU32(const uchar *p) {
    return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8) | p[3];
}

// The ELF hash of the UTF-8 bytes of the source text followed by the comment.
quint32 elfHash(const QByteArray &bytes) {
    quint32 h = 0;
    for (char ch : bytes) {
        h = (h << 4) + static_cast<uchar>(ch);
        const quint32 g = h & 0xf0000000u;
        if (g != 0)
            h ^= g >> 24;
        h &= ~g;
    }
    return h != 0 ? h : 1;
}

QString fromUtf16BE(const uchar *p, quint32 bytes) {
    std::u16string s;
    s.reserve(bytes / 2);
    for (quint32 i = 0; i + 1 < bytes; i += 2)
        s.push_back(static_cast<char16_t>((p[i] << 8) | p[i + 1]));
    return QString(s);
}

// Returns the plural form for the count n under the numerus rules, or 0 if there are none. Within
// a rule, the "and" operator binds more tightly than "or"; rules are separated by OpNewRule, and n
// takes the form of the first rule that matches, or the one after the last rule.
int numerusForm(const QByteArray &rules, int n) {
    const auto *r = reinterpret_cast<const uchar *>(rules.constData());
    const qsizetype size = rules.size();
    if (size == 0)
        return 0;
    qsizetype i = 0;
    int form = 0;
    auto condition = [&](bool *value) {
        if (i + 1 >= size)
            return false;
        const uchar op = r[i++];
        int left = n;
        if (op & OpMod10)
            left %= 10;
        else if (op & OpMod100)
            left %= 100;
        else if (op & OpLead1000)
            while (left >= 1000)
                left /= 1000;
        const int right = r[i++];
        bool v = false;
        switch (op & OpMask) {
        case OpEq:
            v = left == right;
            break;
        case OpLt:
            v = left < right;
            break;
        case OpLeq:
            v = left <= right;
            break;
        case OpBetween:
            if (i >= size)
                return false;
            v = left >= right && left <= r[i++];
            break;
        default:
            return false;
        }
        *value = (op & OpNot) ? !v : v;
        return true;
    };
    for (;;) {
        bool anyOr = false;
        for (;;) {
            bool all = true;
            for (;;) {
                bool v = false;
                if (!condition(&v))
                    return form;
                all = all && v;
                if (i >= size || r[i] != OpAnd)
                    break;
                ++i;
            }
            anyOr = anyOr || all;
            if (i >= size || r[i] != OpOr)
                break;
            ++i;
        }
        if (anyOr)
            return form;
        ++form;
        if (i >= size || r[i] != OpNewRule)
            return form;
        ++i;
    }
}

std::mutex translatorsMutex;
std::vector<QTranslator *> &translators() {
    static std::vector<QTranslator *> list;
    return list;
}

void postLanguageChange() {
    if (QCoreApplication *app = QCoreApplication::instance())
        QCoreApplication::postEvent(app, new QEvent(QEvent::LanguageChange));
}

} // namespace

/*!
    Constructs an empty translator with the given \a parent.
*/
QTranslator::QTranslator(QObject *parent) : QObject(parent) {}

/*!
    Destroys the translator, removing it from the application first if it is
    installed.
*/
QTranslator::~QTranslator() { QCoreApplication::removeTranslator(this); }

bool QTranslator::loadData(QByteArray data, const QString &path) {
    m_data.clear();
    m_hashes = m_messages = -1;
    m_hashesSize = m_messagesSize = 0;
    m_numerusRules.clear();
    m_language.clear();
    m_filePath.clear();
    const auto *p = reinterpret_cast<const uchar *>(data.constData());
    const qsizetype n = data.size();
    if (n < 16 || !std::equal(kMagic, kMagic + 16, p))
        return false;
    qsizetype i = 16;
    while (i + 5 <= n) {
        const uchar tag = p[i];
        const quint32 len = readU32(p + i + 1);
        i += 5;
        if (static_cast<qsizetype>(len) > n - i)
            return false;
        switch (tag) {
        case Hashes:
            m_hashes = i;
            m_hashesSize = len;
            break;
        case Messages:
            m_messages = i;
            m_messagesSize = len;
            break;
        case NumerusRules:
            m_numerusRules = QByteArray(data.constData() + i, static_cast<qsizetype>(len));
            break;
        case Language:
            m_language = QString::fromUtf8(data.constData() + i, static_cast<qsizetype>(len));
            break;
        default:
            break; // Contexts and Dependencies are not needed
        }
        i += len;
    }
    if (m_messages < 0)
        return false;
    m_data = std::move(data);
    m_filePath = path;
    return true;
}

/*!
    Loads the translations from the file \a filename, relative to
    \a directory if it is not absolute, and returns \c true on success.

    If the file does not exist, the function tries the name with the suffix
    \a suffix (\c{".qm"} by default) and without it, then removes the part of
    the name after the last of the \a search_delimiters (\c{"_."} by
    default) and tries again. For \c{"myapp_de_DE"} it tries
    \c{myapp_de_DE.qm}, \c{myapp_de_DE}, \c{myapp_de.qm}, \c{myapp_de},
    \c{myapp.qm} and \c{myapp}.

    The translations previously held by the translator are discarded.

    \sa translate(), isEmpty()
*/
bool QTranslator::load(const QString &filename, const QString &directory, const QString &search_delimiters, const QString &suffix) {
    QString prefix;
    if (!directory.isEmpty() && QFileInfo(filename).isRelative()) {
        prefix = directory;
        if (!prefix.endsWith(QChar(u'/')))
            prefix.append(QChar(u'/'));
    }
    const QString delims = search_delimiters.isNull() ? QString(u"_.") : search_delimiters;
    const QString ext = suffix.isNull() ? QString(u".qm") : suffix;
    QString name = filename;
    for (;;) {
        if (tryFile(prefix + name + ext) || tryFile(prefix + name))
            return true;
        qsizetype cut = -1;
        for (const QChar d : delims)
            cut = std::max(cut, name.lastIndexOf(d));
        if (cut <= 0)
            break;
        name.truncate(cut);
    }
    loadData(QByteArray(), QString());
    return false;
}

/*!
    \overload
    \since 4.8
    Loads the translations for the user interface languages of \a locale and
    returns \c true on success.

    For each language of QLocale::uiLanguages(), with dashes replaced by
    underscores, the function tries
    \c{directory/filename + prefix + language + suffix} and the same without
    \a suffix, then with the language shortened (\c{"fr_CA"} to \c{"fr"}).
    Finally it tries \c{filename + suffix}, \c{filename + prefix} and
    \c{filename}. The default \a suffix is \c{".qm"}.
*/
bool QTranslator::load(const QLocale &locale, const QString &filename, const QString &prefix, const QString &directory, const QString &suffix) {
    QString base;
    if (!directory.isEmpty() && QFileInfo(filename).isRelative()) {
        base = directory;
        if (!base.endsWith(QChar(u'/')))
            base.append(QChar(u'/'));
    }
    base += filename;
    const QString ext = suffix.isNull() ? QString(u".qm") : suffix;
    for (QString lang : locale.uiLanguages()) {
        lang.replace(QChar(u'-'), QChar(u'_'));
        for (;;) {
            for (const QString &candidate : {lang, lang.toLower()}) {
                if (tryFile(base + prefix + candidate + ext) || tryFile(base + prefix + candidate))
                    return true;
            }
            const qsizetype cut = lang.lastIndexOf(QChar(u'_'));
            if (cut <= 0)
                break;
            lang.truncate(cut);
        }
    }
    if (tryFile(base + ext) || (!prefix.isEmpty() && tryFile(base + prefix)) || tryFile(base))
        return true;
    loadData(QByteArray(), QString());
    return false;
}

/*!
    \overload
    \since 4.5
    Loads the translations from the \a len bytes at \a data, which must hold
    the contents of a \c .qm file, and returns \c true on success. The data
    is copied. \a directory is accepted for compatibility; dependencies are
    not loaded.
*/
bool QTranslator::load(const uchar *data, int len, const QString &directory) {
    Q_UNUSED(directory)
    if (!data || len <= 0)
        return loadData(QByteArray(), QString());
    return loadData(QByteArray(reinterpret_cast<const char *>(data), static_cast<qsizetype>(len)), QString());
}

bool QTranslator::tryFile(const QString &path) {
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable())
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return loadData(file.readAll(), info.absoluteFilePath());
}

/*!
    Returns the translation of \a sourceText in \a context with the
    disambiguation comment \a disambiguation, or a null QString if the
    translator has none. If \a n is not negative, the plural form for the
    count \a n is returned; \c{%n} is not replaced here.

    A translation with a disambiguation comment is found only with that
    comment; if there is none, the translation without a comment is used.

    \sa QCoreApplication::translate()
*/
QString QTranslator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const {
    if (m_messages < 0 || !sourceText)
        return QString();
    const auto *base = reinterpret_cast<const uchar *>(m_data.constData());
    const uchar *messages = base + m_messages;
    const QByteArray ctx(context ? context : "");
    const QByteArray src(sourceText);
    QByteArray comment(disambiguation ? disambiguation : "");
    const int form = n >= 0 ? numerusForm(m_numerusRules, n) : 0;

    // Returns the translation of the message at offset, if it matches.
    auto tryMessage = [&](qsizetype offset, QString *out) {
        qsizetype i = offset;
        QByteArray msgContext, msgSource, msgComment;
        bool hasContext = false, hasSource = false, hasComment = false;
        std::vector<QString> forms;
        while (i < m_messagesSize) {
            const uchar tag = messages[i++];
            if (tag == TagEnd)
                break;
            if (tag == TagObsolete1) {
                i += 4;
                continue;
            }
            if (i + 4 > m_messagesSize)
                return false;
            const quint32 len = readU32(messages + i);
            i += 4;
            if (tag == TagTranslation && len == 0xffffffffu) {
                forms.emplace_back();
                continue;
            }
            if (static_cast<qsizetype>(len) > m_messagesSize - i)
                return false;
            const auto *field = reinterpret_cast<const char *>(messages + i);
            switch (tag) {
            case TagTranslation:
                forms.push_back(fromUtf16BE(messages + i, len));
                break;
            case TagSourceText:
                msgSource = QByteArray(field, static_cast<qsizetype>(len));
                hasSource = true;
                break;
            case TagSourceText16:
                msgSource = fromUtf16BE(messages + i, len).toUtf8();
                hasSource = true;
                break;
            case TagContext:
                msgContext = QByteArray(field, static_cast<qsizetype>(len));
                hasContext = true;
                break;
            case TagContext16:
                msgContext = fromUtf16BE(messages + i, len).toUtf8();
                hasContext = true;
                break;
            case TagComment:
                msgComment = QByteArray(field, static_cast<qsizetype>(len));
                hasComment = true;
                break;
            default:
                return false;
            }
            i += len;
        }
        // lrelease writes the comment field empty and keeps the disambiguation only in the hash, so an
        // empty stored comment matches any comment.
        if ((hasContext && msgContext != ctx) || (hasSource && msgSource != src) ||
            (hasComment && !msgComment.isEmpty() && msgComment != comment))
            return false;
        if (forms.empty())
            return false;
        const QString &t = forms[static_cast<std::size_t>(std::min<int>(form, static_cast<int>(forms.size()) - 1))];
        if (t.isNull())
            return false;
        *out = t;
        return true;
    };

    for (;;) {
        QString result;
        if (m_hashes >= 0) {
            const quint32 h = elfHash(src + comment);
            const uchar *table = base + m_hashes;
            const qsizetype count = m_hashesSize / 8;
            qsizetype lo = 0, hi = count;
            while (lo < hi) { // first entry with a hash >= h
                const qsizetype mid = (lo + hi) / 2;
                if (readU32(table + mid * 8) < h)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            for (qsizetype k = lo; k < count && readU32(table + k * 8) == h; ++k)
                if (tryMessage(static_cast<qsizetype>(readU32(table + k * 8 + 4)), &result))
                    return result;
        } else {
            // No hash table: scan all messages.
            qsizetype i = 0;
            while (i < m_messagesSize) {
                if (tryMessage(i, &result))
                    return result;
                // Skip to the next message.
                while (i < m_messagesSize) {
                    const uchar tag = messages[i++];
                    if (tag == TagEnd)
                        break;
                    if (tag == TagObsolete1) {
                        i += 4;
                        continue;
                    }
                    if (i + 4 > m_messagesSize)
                        return QString();
                    const quint32 len = readU32(messages + i);
                    i += 4 + (len == 0xffffffffu ? 0 : static_cast<qsizetype>(len));
                }
            }
        }
        if (comment.isEmpty())
            return QString();
        comment.clear(); // try again without the disambiguation comment
    }
}

/*!
    Returns \c true if the translator holds no translations; otherwise
    returns \c false.
*/
bool QTranslator::isEmpty() const { return m_messages < 0 || m_messagesSize == 0; }

/*!
    \since 5.15
    Returns the target language stored in the loaded file, such as
    \c{"de_DE"}, or an empty string.
*/
QString QTranslator::language() const { return m_language; }

/*!
    \since 5.15
    Returns the absolute path of the loaded file, or an empty string if the
    translations were loaded from memory or nothing is loaded.
*/
QString QTranslator::filePath() const { return m_filePath; }

/*!
    Adds \a translationFile to the translators that tr() and translate()
    use, and returns \c true. Translators installed later are searched first.
    A QEvent::LanguageChange event is posted to the application object.
    Returns \c false if \a translationFile is \c nullptr or empty.

    \sa removeTranslator(), translate()
*/
bool QCoreApplication::installTranslator(QTranslator *translationFile) {
    if (!translationFile || translationFile->isEmpty())
        return false;
    {
        std::lock_guard<std::mutex> lock(translatorsMutex);
        auto &list = translators();
        list.erase(std::remove(list.begin(), list.end(), translationFile), list.end());
        list.push_back(translationFile);
    }
    postLanguageChange();
    return true;
}

/*!
    Removes \a translationFile from the installed translators and returns
    \c true, or returns \c false if it was not installed. A
    QEvent::LanguageChange event is posted to the application object.

    \sa installTranslator()
*/
bool QCoreApplication::removeTranslator(QTranslator *translationFile) {
    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(translatorsMutex);
        auto &list = translators();
        const auto it = std::find(list.begin(), list.end(), translationFile);
        if (it != list.end()) {
            list.erase(it);
            removed = true;
        }
    }
    if (removed)
        postLanguageChange();
    return removed;
}

/*!
    Returns the translation of the UTF-8 text \a sourceText in \a context,
    with the disambiguation comment \a disambiguation, from the installed
    translators, most recently installed first. If none has a translation,
    \a sourceText is returned. If \a n is not negative, the plural form for
    \a n is used and every \c{%n} in the result is replaced by \a n.

    This function is thread-safe.

    \sa installTranslator()
*/
QString QCoreApplication::translate(const char *context, const char *sourceText, const char *disambiguation, int n) {
    QString result;
    {
        std::lock_guard<std::mutex> lock(translatorsMutex);
        const auto &list = translators();
        for (auto it = list.rbegin(); it != list.rend() && result.isNull(); ++it)
            result = (*it)->translate(context, sourceText, disambiguation, n);
    }
    if (result.isNull())
        result = QString::fromUtf8(sourceText);
    if (n >= 0)
        result.replace(QString(u"%n"), QString::number(n));
    return result;
}

QString QtPrivate::translate(const char *context, const char *sourceText, const char *disambiguation, int n) {
    return QCoreApplication::translate(context, sourceText, disambiguation, n);
}

#include "moc_qtranslator.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
