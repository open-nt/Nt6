// MIT License - see LICENSE
// The INI format of QSettings and the text encoding of QVariant values in it.

#include "qsettings_p.h"

#include <QtCore/qdatastream.h>
#include <QtCore/qdebug.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>

#include <map>

namespace NtSettings {
namespace {

constexpr char kHexUpper[] = "0123456789ABCDEF";

bool isAsciiAlnum(unsigned ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool isSpace(char c) { return c == ' ' || c == '\t'; }

bool isNewline(char c) { return c == '\n' || c == '\r'; }

void appendHexLower(QByteArray &out, unsigned value) {
    char digits[8];
    int n = 0;
    do {
        digits[n++] = "0123456789abcdef"[value & 0xF];
        value >>= 4;
    } while (value != 0);
    while (n > 0)
        out.append(digits[--n]);
}

void appendUtf8(QByteArray &out, char32_t cp) {
    if (cp < 0x80) {
        out.append(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.append(static_cast<char>(0xC0 | (cp >> 6)));
        out.append(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.append(static_cast<char>(0xE0 | (cp >> 12)));
        out.append(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.append(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.append(static_cast<char>(0xF0 | (cp >> 18)));
        out.append(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.append(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.append(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Section and key names: '/' becomes '\', and everything that is not a letter, a digit or one of
// "_-." is written as %XX or %UXXXX.
void iniEscapedKey(const QString &key, QByteArray &result) {
    for (qsizetype i = 0; i < key.size(); ++i) {
        const unsigned ch = key.at(i).unicode();
        if (ch == '/') {
            result.append('\\');
        } else if (isAsciiAlnum(ch) || ch == '_' || ch == '-' || ch == '.') {
            result.append(static_cast<char>(ch));
        } else if (ch <= 0xFF) {
            result.append('%');
            result.append(kHexUpper[ch / 16]);
            result.append(kHexUpper[ch % 16]);
        } else {
            result.append("%U");
            for (int shift = 12; shift >= 0; shift -= 4)
                result.append(kHexUpper[(ch >> shift) & 0xF]);
        }
    }
}

QString iniUnescapedKey(const QByteArray &key) {
    QString result;
    const qsizetype n = key.size();
    qsizetype i = 0;
    while (i < n) {
        const char ch = key.at(i);
        if (ch == '\\') {
            result.append(QChar(u'/'));
            ++i;
            continue;
        }
        if (ch == '%' && i + 1 < n) {
            qsizetype first = i + 1;
            qsizetype digits = 2;
            if (key.at(first) == 'U') {
                ++first;
                digits = 4;
            }
            if (first + digits <= n) {
                unsigned value = 0;
                bool ok = true;
                for (qsizetype k = 0; k < digits; ++k) {
                    const int h = hexValue(key.at(first + k));
                    if (h < 0) {
                        ok = false;
                        break;
                    }
                    value = value * 16 + static_cast<unsigned>(h);
                }
                if (ok) {
                    result.append(QChar(static_cast<char16_t>(value)));
                    i = first + digits;
                    continue;
                }
            }
        }
        qsizetype j = i + 1;
        while (j < n && key.at(j) != '\\' && key.at(j) != '%')
            ++j;
        result.append(QString::fromUtf8(key.constData() + i, j - i));
        i = j;
    }
    return result;
}

// Strings that carry binary data are stored as Latin-1, everything else as UTF-8.
bool usesUtf8(const QString &s) {
    return !(s.startsWith(QString::fromLatin1("@ByteArray(")) || s.startsWith(QString::fromLatin1("@Variant(")) ||
             s.startsWith(QString::fromLatin1("@DateTime(")));
}

void iniEscapedString(const QString &str, QByteArray &result) {
    bool needsQuotes = false;
    bool escapeNextIfDigit = false;
    const bool utf8 = usesUtf8(str);
    const qsizetype startPos = result.size();

    for (qsizetype i = 0; i < str.size(); ++i) {
        const unsigned ch = str.at(i).unicode();
        if (ch == ';' || ch == ',' || ch == '=')
            needsQuotes = true;

        if (escapeNextIfDigit && ch < 0x80 && hexValue(static_cast<char>(ch)) >= 0) {
            result.append("\\x");
            appendHexLower(result, ch);
            continue;
        }
        escapeNextIfDigit = false;

        switch (ch) {
        case 0:
            result.append("\\0");
            escapeNextIfDigit = true;
            break;
        case '\a':
            result.append("\\a");
            break;
        case '\b':
            result.append("\\b");
            break;
        case '\f':
            result.append("\\f");
            break;
        case '\n':
            result.append("\\n");
            break;
        case '\r':
            result.append("\\r");
            break;
        case '\t':
            result.append("\\t");
            break;
        case '\v':
            result.append("\\v");
            break;
        case '"':
        case '\\':
            result.append('\\');
            result.append(static_cast<char>(ch));
            break;
        default:
            if (ch <= 0x1F || (ch >= 0x80 && !utf8)) {
                result.append("\\x");
                appendHexLower(result, ch);
                escapeNextIfDigit = true;
            } else if (ch >= 0x80) {
                char32_t cp = ch;
                if (ch >= 0xD800 && ch < 0xDC00 && i + 1 < str.size() && str.at(i + 1).unicode() >= 0xDC00 &&
                    str.at(i + 1).unicode() < 0xE000) {
                    cp = 0x10000 + ((ch - 0xD800) << 10) + (str.at(i + 1).unicode() - 0xDC00);
                    ++i;
                } else if (ch >= 0xD800 && ch < 0xE000) {
                    cp = 0xFFFD;
                }
                appendUtf8(result, cp);
            } else {
                result.append(static_cast<char>(ch));
            }
        }
    }

    if (needsQuotes ||
        (startPos < result.size() && (result.at(startPos) == ' ' || result.at(result.size() - 1) == ' '))) {
        result.insert(startPos, '"');
        result.append('"');
    }
}

void iniEscapedStringList(const QStringList &strs, QByteArray &result) {
    // An empty list needs a spelling of its own, as a list of one empty string is an empty value.
    if (strs.isEmpty()) {
        result.append("@Invalid()");
        return;
    }
    for (qsizetype i = 0; i < strs.size(); ++i) {
        if (i != 0)
            result.append(", ");
        iniEscapedString(strs.at(i), result);
    }
}

QStringList variantListToStringList(const QVariantList &list) {
    QStringList result;
    for (const QVariant &v : list)
        result.append(variantToString(v));
    return result;
}

// Returns true if the value is a list. The items are in listResult, or the single value in
// stringResult.
bool iniUnescapedStringList(const QByteArray &str, QString &stringResult, QStringList &listResult) {
    const qsizetype n = str.size();
    qsizetype i = 0;
    while (i < n && isSpace(str.at(i)))
        ++i;
    const bool utf8 = usesUtf8(QString::fromLatin1(str.constData() + i, qMin<qsizetype>(n - i, 11)));

    bool isList = false;
    bool inQuotes = false;
    bool itemQuoted = false;
    QString cur;
    qsizetype chop = 0; // length of cur without trailing blanks that were neither quoted nor escaped

    const auto skipSpaces = [&] {
        while (i < n && isSpace(str.at(i)))
            ++i;
    };

    while (i < n) {
        const char c = str.at(i);
        if (c == '\\') {
            ++i;
            if (i >= n)
                break;
            const char e = str.at(i++);
            char16_t simple = 0;
            switch (e) {
            case 'a':
                simple = u'\a';
                break;
            case 'b':
                simple = u'\b';
                break;
            case 'f':
                simple = u'\f';
                break;
            case 'n':
                simple = u'\n';
                break;
            case 'r':
                simple = u'\r';
                break;
            case 't':
                simple = u'\t';
                break;
            case 'v':
                simple = u'\v';
                break;
            case '"':
            case '?':
            case '\'':
            case '\\':
                simple = static_cast<char16_t>(e);
                break;
            default:
                break;
            }
            if (simple != 0) {
                cur.append(QChar(simple));
                chop = cur.size();
            } else if (e == 'x') {
                int h = i < n ? hexValue(str.at(i)) : -1;
                if (h < 0)
                    continue;
                unsigned value = 0;
                while (h >= 0) {
                    value = (value << 4) + static_cast<unsigned>(h);
                    ++i;
                    h = i < n ? hexValue(str.at(i)) : -1;
                }
                cur.append(QChar(static_cast<char16_t>(value & 0xFFFF)));
                chop = cur.size();
            } else if (e >= '0' && e <= '7') {
                unsigned value = static_cast<unsigned>(e - '0');
                while (i < n && str.at(i) >= '0' && str.at(i) <= '7')
                    value = (value << 3) + static_cast<unsigned>(str.at(i++) - '0');
                cur.append(QChar(static_cast<char16_t>(value & 0xFFFF)));
                chop = cur.size();
            } else if (isNewline(e)) {
                // A backslash before a line break continues the value on the next line.
                if (i < n && isNewline(str.at(i)) && str.at(i) != e)
                    ++i;
                chop = cur.size();
            }
            // Any other escaped character is dropped.
            continue;
        }
        if (c == '"') {
            ++i;
            itemQuoted = true;
            inQuotes = !inQuotes;
            if (inQuotes)
                chop = cur.size();
            else
                skipSpaces();
            continue;
        }
        if (c == ',' && !inQuotes) {
            if (!itemQuoted)
                cur.truncate(chop);
            isList = true;
            listResult.append(cur);
            cur.clear();
            chop = 0;
            itemQuoted = false;
            ++i;
            skipSpaces();
            continue;
        }
        qsizetype j = i + 1;
        while (j < n && str.at(j) != '\\' && str.at(j) != '"' && str.at(j) != ',')
            ++j;
        const QString piece =
            utf8 ? QString::fromUtf8(str.constData() + i, j - i) : QString::fromLatin1(str.constData() + i, j - i);
        cur.append(piece);
        if (inQuotes) {
            chop = cur.size();
        } else {
            qsizetype end = piece.size();
            while (end > 0 && (piece.at(end - 1) == QChar(u' ') || piece.at(end - 1) == QChar(u'\t')))
                --end;
            if (end > 0)
                chop = cur.size() - (piece.size() - end);
        }
        i = j;
    }
    if (!itemQuoted)
        cur.truncate(chop);
    if (isList)
        listResult.append(cur);
    else
        stringResult = cur;
    return isList;
}

QVariant stringListToVariantList(const QStringList &list) {
    QStringList out = list;
    for (qsizetype i = 0; i < out.size(); ++i) {
        const QString &s = out.at(i);
        if (!s.startsWith(QChar(u'@')))
            continue;
        if (s.size() >= 2 && s.at(1) == QChar(u'@')) {
            out[i].remove(0, 1);
        } else {
            QVariantList variants;
            for (const QString &item : list)
                variants.append(stringToVariant(item));
            return QVariant(variants);
        }
    }
    return QVariant(out);
}

QStringList splitArgs(const QString &s, qsizetype open) {
    return s.mid(open + 1, s.size() - open - 2).split(QChar(u' '));
}

// Returns the next logical line of the file. A backslash keeps the line break that follows it in
// the line, and a semicolon outside quotes starts a comment.
bool nextLine(const QByteArray &data, qsizetype &pos, QByteArray &line) {
    line.clear();
    const qsizetype n = data.size();
    if (pos >= n)
        return false;
    bool inQuotes = false;
    while (pos < n) {
        const char c = data.at(pos);
        if (isNewline(c)) {
            ++pos;
            if (pos < n && isNewline(data.at(pos)) && data.at(pos) != c)
                ++pos;
            return true;
        }
        if (c == '\\' && pos + 1 < n) {
            const char d = data.at(pos + 1);
            line.append(c);
            line.append(d);
            pos += 2;
            if (isNewline(d) && pos < n && isNewline(data.at(pos)) && data.at(pos) != d)
                line.append(data.at(pos++));
            continue;
        }
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ';' && !inQuotes) {
            while (pos < n && !isNewline(data.at(pos)))
                ++pos;
            continue;
        }
        line.append(c);
        ++pos;
    }
    return true;
}

QByteArray trimmedBlanks(const QByteArray &s) {
    qsizetype b = 0;
    qsizetype e = s.size();
    while (b < e && isSpace(s.at(b)))
        ++b;
    while (e > b && isSpace(s.at(e - 1)))
        --e;
    return s.mid(b, e - b);
}

} // namespace

QString variantToString(const QVariant &v) {
    QString result;
    switch (v.typeId()) {
    case QMetaType::UnknownType:
        return QString::fromLatin1("@Invalid()");
    case QMetaType::QByteArray:
        return QString::fromLatin1("@ByteArray(") + QString::fromLatin1(v.toByteArray()) + QChar(u')');
    case QMetaType::QString:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Bool:
    case QMetaType::Float:
    case QMetaType::Double:
        result = v.toString();
        if (result.contains(QChar(u'\0')))
            result = QString::fromLatin1("@String(") + result + QChar(u')');
        else if (result.startsWith(QChar(u'@')))
            result.prepend(QChar(u'@'));
        return result;
    case QMetaType::QRect: {
        const QRect r = v.value<QRect>();
        return QString::fromLatin1("@Rect(") + QString::number(r.x()) + QChar(u' ') + QString::number(r.y()) +
               QChar(u' ') + QString::number(r.width()) + QChar(u' ') + QString::number(r.height()) + QChar(u')');
    }
    case QMetaType::QSize: {
        const QSize s = v.value<QSize>();
        return QString::fromLatin1("@Size(") + QString::number(s.width()) + QChar(u' ') + QString::number(s.height()) +
               QChar(u')');
    }
    case QMetaType::QPoint: {
        const QPoint p = v.value<QPoint>();
        return QString::fromLatin1("@Point(") + QString::number(p.x()) + QChar(u' ') + QString::number(p.y()) +
               QChar(u')');
    }
    default:
        break;
    }

    // A date and time needs the newer stream version to keep its time spec; the rest is streamed in
    // the format of Qt 4.0, which Qt reads and writes for every other type.
    const bool dateTime = v.typeId() == QMetaType::QDateTime;
    QByteArray bytes;
    bool ok = false;
    {
        QDataStream s(&bytes, QIODevice::WriteOnly);
        s.setVersion(dateTime ? QDataStream::Qt_5_6 : QDataStream::Qt_4_0);
        s << v;
        ok = s.status() == QDataStream::Ok;
    }
    if (!ok) {
        qWarning("QSettings: cannot store a value of type %s", v.typeName());
        return QString::fromLatin1("@Invalid()");
    }
    return QString::fromLatin1(dateTime ? "@DateTime(" : "@Variant(") + QString::fromLatin1(bytes) + QChar(u')');
}

QVariant stringToVariant(const QString &s) {
    if (s.startsWith(QChar(u'@'))) {
        if (s.endsWith(QChar(u')'))) {
            if (s.startsWith(QString::fromLatin1("@ByteArray(")))
                return QVariant(s.mid(11, s.size() - 12).toLatin1());
            if (s.startsWith(QString::fromLatin1("@String(")))
                return QVariant(s.mid(8, s.size() - 9));
            if (s.startsWith(QString::fromLatin1("@Variant(")) || s.startsWith(QString::fromLatin1("@DateTime("))) {
                const bool dateTime = s.at(1) == QChar(u'D');
                const QByteArray bytes = s.mid(dateTime ? 10 : 9, s.size() - (dateTime ? 11 : 10)).toLatin1();
                QDataStream stream(bytes);
                stream.setVersion(dateTime ? QDataStream::Qt_5_6 : QDataStream::Qt_4_0);
                QVariant result;
                stream >> result;
                return result;
            }
            if (s.startsWith(QString::fromLatin1("@Rect("))) {
                const QStringList args = splitArgs(s, 5);
                if (args.size() == 4)
                    return QVariant(QRect(args[0].toInt(), args[1].toInt(), args[2].toInt(), args[3].toInt()));
            } else if (s.startsWith(QString::fromLatin1("@Size("))) {
                const QStringList args = splitArgs(s, 5);
                if (args.size() == 2)
                    return QVariant(QSize(args[0].toInt(), args[1].toInt()));
            } else if (s.startsWith(QString::fromLatin1("@Point("))) {
                const QStringList args = splitArgs(s, 6);
                if (args.size() == 2)
                    return QVariant(QPoint(args[0].toInt(), args[1].toInt()));
            } else if (s == QString::fromLatin1("@Invalid()")) {
                return QVariant();
            }
        }
        if (s.startsWith(QString::fromLatin1("@@")))
            return QVariant(s.mid(1));
    }
    return QVariant(s);
}

bool readIniFile(QIODevice &device, QSettings::SettingsMap &map) {
    const QByteArray data = device.readAll();
    qsizetype pos = data.startsWith("\xEF\xBB\xBF") ? 3 : 0;

    bool ok = true;
    QString section;
    QByteArray line;
    while (nextLine(data, pos, line)) {
        line = trimmedBlanks(line);
        if (line.isEmpty() || line.at(0) == '#')
            continue;
        if (line.at(0) == '[') {
            const qsizetype close = line.indexOf(']');
            if (close < 0) {
                ok = false;
                continue;
            }
            const QByteArray raw = trimmedBlanks(line.mid(1, close - 1));
            const QByteArray lower = raw.toLower();
            if (raw.isEmpty() || lower == "general")
                section.clear();
            else if (lower == "%general")
                section = QString::fromLatin1("General");
            else
                section = iniUnescapedKey(raw);
            continue;
        }
        const qsizetype eq = line.indexOf('=');
        if (eq < 0) {
            ok = false;
            continue;
        }
        const QString key = iniUnescapedKey(trimmedBlanks(line.left(eq)));
        if (key.isEmpty() && section.isEmpty()) {
            ok = false;
            continue;
        }
        QString text;
        QStringList list;
        const bool isList = iniUnescapedStringList(line.mid(eq + 1), text, list);
        const QVariant value = isList ? stringListToVariantList(list) : stringToVariant(text);
        map.insert(section.isEmpty() ? key : section + QChar(u'/') + key, value);
    }
    return ok;
}

bool writeIniFile(QIODevice &device, const QSettings::SettingsMap &map) {
    std::map<QString, std::map<QString, QVariant>> sections;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        const QString &key = it.key();
        const qsizetype slash = key.lastIndexOf(QChar(u'/'));
        sections[slash < 0 ? QString() : key.left(slash)][key.mid(slash + 1)] = it.value();
    }

    QByteArray out;
    bool first = true;
    for (const auto &section : sections) {
        if (!first)
            out.append('\n');
        first = false;
        out.append('[');
        if (section.first.isEmpty())
            out.append("General");
        else if (section.first == QString::fromLatin1("General"))
            out.append("%General");
        else if (section.first.compare(QString::fromLatin1("General"), Qt::CaseInsensitive) == 0)
            out.append(QString::number(section.first.at(0).unicode(), 16).prepend(QChar(u'%')).toLatin1() +
                       section.first.mid(1).toLatin1());
        else
            iniEscapedKey(section.first, out);
        out.append("]\n");
        for (const auto &entry : section.second) {
            iniEscapedKey(entry.first, out);
            out.append('=');
            const QVariant &value = entry.second;
            const int id = value.typeId();
            if (id == QMetaType::QStringList || (id == QMetaType::QVariantList && value.toList().size() != 1))
                iniEscapedStringList(variantListToStringList(value.toList()), out);
            else
                iniEscapedString(variantToString(value), out);
            out.append('\n');
        }
    }
    return device.write(out) == out.size();
}

} // namespace NtSettings
