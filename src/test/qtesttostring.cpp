// MIT License - see LICENSE
#include <QTest>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

char *copy(const std::string &text) {
    char *out = new char[text.size() + 1];
    std::memcpy(out, text.c_str(), text.size() + 1);
    return out;
}

// Quotes UTF-8 or binary text, escaping anything that would make a log unreadable.
std::string quoted(const char *data, std::size_t size) {
    std::string out = "\"";
    for (std::size_t i = 0; i < size; ++i) {
        const auto c = static_cast<unsigned char>(data[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20 || c == 0x7f) {
            char escape[8];
            std::snprintf(escape, sizeof escape, "\\x%02x", static_cast<unsigned>(c));
            out += escape;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "\"";
}

std::string quoted(const std::string &text) { return quoted(text.data(), text.size()); }

// The shortest form that reads back as the same value.
template <typename T> char *shortestReal(T value, int maxPrecision) {
    char text[64];
    for (int precision = 6; precision <= maxPrecision; ++precision) {
        std::snprintf(text, sizeof text, "%.*g", precision, static_cast<double>(value));
        if (static_cast<T>(std::strtod(text, nullptr)) == value)
            break;
    }
    return copy(text);
}

template <typename T> bool fuzzy(T a, T b, T factor) {
    if (std::isnan(a) || std::isnan(b))
        return std::isnan(a) && std::isnan(b);
    if (std::isinf(a) || std::isinf(b) || a == b)
        return a == b;
    return std::fabs(a - b) * factor <= std::min(std::fabs(a), std::fabs(b));
}

} // namespace

namespace QTestPrivate {

char *boolToString(bool value) { return copy(value ? "true" : "false"); }
char *charToString(char value) { return copy(quoted(&value, 1)); }
char *integerToString(long long value) { return copy(std::to_string(value)); }
char *unsignedToString(unsigned long long value) { return copy(std::to_string(value)); }
char *realToString(double value) { return shortestReal(value, 17); }
char *realToString(float value) { return shortestReal(value, 9); }

char *pointerToString(const void *value) {
    if (!value)
        return copy("nullptr");
    char text[32];
    std::snprintf(text, sizeof text, "%p", value);
    return copy(text);
}

char *sequenceToString(const char *open, const std::vector<std::string> &items, const char *close) {
    std::string out = open;
    for (std::size_t i = 0; i < items.size(); ++i)
        out += (i ? ", " : "") + items[i];
    return copy(out + close);
}

bool fuzzyEqual(double a, double b) { return fuzzy(a, b, 1000000000000.0); }
bool fuzzyEqual(float a, float b) { return fuzzy(a, b, 100000.0F); }

} // namespace QTestPrivate

namespace QTest {

char *toString(const char *value) { return copy(value ? quoted(value, std::strlen(value)) : "nullptr"); }

char *toString(std::nullptr_t) { return copy("nullptr"); }

char *toString(bool value) { return QTestPrivate::boolToString(value); }

char *toString(char value) { return QTestPrivate::charToString(value); }

char *toString(const QString &value) { return copy(quoted(value.toStdString())); }

char *toString(QStringView value) { return copy(quoted(value.toString().toStdString())); }

char *toString(QLatin1StringView value) { return copy(quoted(QString(value).toStdString())); }

char *toString(const QStringList &value) {
    std::vector<std::string> items;
    for (const QString &item : value)
        items.push_back(quoted(item.toStdString()));
    return QTestPrivate::sequenceToString("QStringList(", items, ")");
}

char *toString(const QByteArray &value) {
    return copy(quoted(value.constData(), static_cast<std::size_t>(value.size())));
}

char *toString(QChar value) { return copy(quoted(QString(value).toStdString())); }

char *toString(const QDate &value) {
    return copy(value.isValid() ? value.toString(Qt::ISODate).toStdString() : "Invalid QDate");
}

char *toString(const QTime &value) {
    return copy(value.isValid() ? value.toString(Qt::ISODateWithMs).toStdString() : "Invalid QTime");
}

char *toString(const QDateTime &value) {
    return copy(value.isValid() ? value.toString(Qt::ISODateWithMs).toStdString() : "Invalid QDateTime");
}

char *toString(const QUrl &value) { return copy(quoted(value.toString().toStdString())); }

char *toString(const QUuid &value) { return copy(value.toString().toStdString()); }

char *toString(const QVersionNumber &value) { return copy(value.toString().toStdString()); }

} // namespace QTest
