// MIT License - see LICENSE
#include <QtCore/qdebug.h>

#include "qjson_p.h"

#include <charconv>
#include <cmath>

namespace {

constexpr qint64 kMaxExactInteger = qint64(1) << 53;

// Writes a double the way Qt does: whole numbers up to 2^53 as integers, everything else as the shortest text
// that reads back as the same double, in the style of printf's %g.
void appendDouble(std::string &out, double d) {
    if (d == std::floor(d) && std::fabs(d) <= static_cast<double>(kMaxExactInteger)) {
        out += std::to_string(static_cast<qint64>(d));
        return;
    }
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, d, std::chars_format::scientific);
    const std::string sci(buf, r.ptr);
    std::size_t pos = 0;
    if (sci[0] == '-') {
        out += '-';
        pos = 1;
    }
    const std::size_t e = sci.find('e');
    std::string digits;
    for (std::size_t i = pos; i < e; ++i) {
        if (sci[i] != '.')
            digits += sci[i];
    }
    const int exp10 = std::stoi(sci.substr(e + 1));
    const int count = static_cast<int>(digits.size());
    if (exp10 < -4 || exp10 >= count) {
        out += digits[0];
        if (count > 1) {
            out += '.';
            out.append(digits, 1, std::string::npos);
        }
        out += 'e';
        out += exp10 < 0 ? '-' : '+';
        const int a = std::abs(exp10);
        if (a < 10)
            out += '0';
        out += std::to_string(a);
    } else if (exp10 >= 0) {
        out.append(digits, 0, static_cast<std::size_t>(exp10) + 1);
        if (count > exp10 + 1) {
            out += '.';
            out.append(digits, static_cast<std::size_t>(exp10) + 1, std::string::npos);
        }
    } else {
        out += "0.";
        out.append(static_cast<std::size_t>(-exp10 - 1), '0');
        out += digits;
    }
}

void appendUtf8(std::string &out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// An unpaired surrogate cannot be written as UTF-8; it becomes U+FFFD.
void appendString(std::string &out, const QString &s) {
    static const char hex[] = "0123456789abcdef";
    out += '"';
    const qsizetype n = s.size();
    for (qsizetype i = 0; i < n; ++i) {
        const char16_t u = s.at(i).unicode();
        switch (u) {
        case u'"':
            out += "\\\"";
            break;
        case u'\\':
            out += "\\\\";
            break;
        case u'\b':
            out += "\\b";
            break;
        case u'\f':
            out += "\\f";
            break;
        case u'\n':
            out += "\\n";
            break;
        case u'\r':
            out += "\\r";
            break;
        case u'\t':
            out += "\\t";
            break;
        default:
            if (u < 0x20) {
                out += "\\u00";
                out += hex[u >> 4];
                out += hex[u & 0xF];
            } else if (u >= 0xD800 && u <= 0xDBFF && i + 1 < n && s.at(i + 1).unicode() >= 0xDC00 &&
                       s.at(i + 1).unicode() <= 0xDFFF) {
                const char32_t cp = 0x10000 + ((static_cast<char32_t>(u) - 0xD800) << 10) +
                                    (static_cast<char32_t>(s.at(i + 1).unicode()) - 0xDC00);
                appendUtf8(out, cp);
                ++i;
            } else if (u >= 0xD800 && u <= 0xDFFF) {
                appendUtf8(out, 0xFFFD);
            } else {
                appendUtf8(out, u);
            }
        }
    }
    out += '"';
}

void appendIndent(std::string &out, int level) { out.append(static_cast<std::size_t>(level) * 4, ' '); }

void writeValue(std::string &out, const QJsonValue &v, bool compact, int level);

void writeArray(std::string &out, const QJsonArray &a, bool compact, int level) {
    out += '[';
    if (!compact)
        out += '\n';
    const qsizetype n = a.size();
    for (qsizetype i = 0; i < n; ++i) {
        if (!compact)
            appendIndent(out, level + 1);
        writeValue(out, a.at(i), compact, level + 1);
        if (i + 1 < n)
            out += ',';
        if (!compact)
            out += '\n';
    }
    if (!compact)
        appendIndent(out, level);
    out += ']';
}

void writeObject(std::string &out, const QJsonObject &o, bool compact, int level) {
    out += '{';
    if (!compact)
        out += '\n';
    const auto &items = QJsonPrivateAccess::data(o);
    const std::size_t n = items ? items->items.size() : 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!compact)
            appendIndent(out, level + 1);
        appendString(out, items->items[i].key);
        out += compact ? ":" : ": ";
        writeValue(out, items->items[i].value, compact, level + 1);
        if (i + 1 < n)
            out += ',';
        if (!compact)
            out += '\n';
    }
    if (!compact)
        appendIndent(out, level);
    out += '}';
}

void writeValue(std::string &out, const QJsonValue &v, bool compact, int level) {
    switch (v.type()) {
    case QJsonValue::Null:
    case QJsonValue::Undefined:
        out += "null";
        break;
    case QJsonValue::Bool:
        out += v.toBool() ? "true" : "false";
        break;
    case QJsonValue::Double:
        if (QJsonPrivateAccess::isInteger(v))
            out += std::to_string(QJsonPrivateAccess::integer(v));
        else
            appendDouble(out, v.toDouble());
        break;
    case QJsonValue::String:
        appendString(out, QJsonPrivateAccess::string(v));
        break;
    case QJsonValue::Array:
        writeArray(out, v.toArray(), compact, level);
        break;
    case QJsonValue::Object:
        writeObject(out, v.toObject(), compact, level);
        break;
    }
}

} // namespace

namespace QJsonPrivate {

std::string write(const QJsonValue &root, bool compact) {
    std::string out;
    writeValue(out, root, compact, 0);
    return out;
}

} // namespace QJsonPrivate

// The operators below follow the convention of the other QDebug operators: write without spaces, then restore
// them and add the trailing one. Nested values are therefore formatted here rather than through nested operators.
QDebug operator<<(QDebug dbg, const QJsonValue &value) {
    dbg.nospace();
    switch (value.type()) {
    case QJsonValue::Undefined:
        dbg << "QJsonValue(undefined)";
        break;
    case QJsonValue::Null:
        dbg << "QJsonValue(null)";
        break;
    case QJsonValue::Bool:
        dbg << "QJsonValue(bool, " << value.toBool() << ")";
        break;
    case QJsonValue::Double:
        dbg << "QJsonValue(double, " << value.toDouble() << ")";
        break;
    case QJsonValue::String:
        dbg << "QJsonValue(string, " << value.toString() << ")";
        break;
    case QJsonValue::Array:
        dbg << "QJsonValue(array, QJsonArray(" << QJsonPrivate::write(value, true).c_str() << "))";
        break;
    case QJsonValue::Object:
        dbg << "QJsonValue(object, QJsonObject(" << QJsonPrivate::write(value, true).c_str() << "))";
        break;
    }
    dbg.space();
    return dbg.maybeSpace();
}

QDebug operator<<(QDebug dbg, const QJsonObject &object) {
    dbg.nospace() << "QJsonObject(" << QJsonPrivate::write(QJsonValue(object), true).c_str() << ")";
    dbg.space();
    return dbg.maybeSpace();
}

QDebug operator<<(QDebug dbg, const QJsonArray &array) {
    dbg.nospace() << "QJsonArray(" << QJsonPrivate::write(QJsonValue(array), true).c_str() << ")";
    dbg.space();
    return dbg.maybeSpace();
}
