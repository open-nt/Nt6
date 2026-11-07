// MIT License - see LICENSE
#include <QtCore/qdebug.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace {

QtMessageHandler g_handler = nullptr;

void dispatch(QtMsgType type, const std::string &text) {
    if (g_handler) {
        g_handler(type, QMessageLogContext(), QString::fromStdString(text));
    } else {
        std::fprintf(stderr, "%s\n", text.c_str());
        std::fflush(stderr);
    }
    if (type == QtFatalMsg)
        std::abort();
}

std::string vformat(const char *fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    std::string s(static_cast<std::size_t>(n < 0 ? 0 : n), '\0');
    if (n > 0)
        std::vsnprintf(s.data(), s.size() + 1, fmt, ap2);
    va_end(ap2);
    return s;
}

std::string escaped(const std::string &s) {
    std::string r;
    for (char c : s) {
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default: r.push_back(c);
        }
    }
    return r;
}

} // namespace

QtMessageHandler qInstallMessageHandler(QtMessageHandler handler) {
    QtMessageHandler old = g_handler;
    g_handler = handler;
    return old;
}

QDebug::QDebug(QtMsgType type) : s(std::make_shared<State>()) { s->type = type; }

QDebug::~QDebug() {
    if (s.use_count() != 1)
        return;
    if (s->space && !s->buf.empty() && s->buf.back() == ' ')
        s->buf.pop_back();
    dispatch(s->type, s->buf);
}

QDebug &QDebug::operator<<(char v) {
    maybeQuote('\'');
    s->buf.push_back(v);
    maybeQuote('\'');
    return maybeSpace();
}

QDebug &QDebug::operator<<(const void *p) {
    char b[32];
    std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p)));
    return put(b);
}

QDebug &QDebug::operator<<(QChar c) { return *this << QString(c); }

QDebug &QDebug::operator<<(const QString &v) {
    maybeQuote();
    s->buf += s->quote ? escaped(v.toStdString()) : v.toStdString();
    maybeQuote();
    return maybeSpace();
}

QDebug &QDebug::operator<<(const QByteArray &v) {
    maybeQuote();
    s->buf += s->quote ? escaped(v.toStdString()) : v.toStdString();
    maybeQuote();
    return maybeSpace();
}

QDebug &QDebug::operator<<(const QObject *o) {
    if (!o)
        return put("QObject(0x0)");
    char b[64];
    std::snprintf(b, sizeof b, "QObject(0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(o)));
    std::string r = b;
    if (!o->objectName().isEmpty())
        r += ", name = \"" + escaped(o->objectName().toStdString()) + "\"";
    return put(r + ")");
}

QDebug qDebug() { return QDebug(QtDebugMsg); }
QDebug qInfo() { return QDebug(QtInfoMsg); }
QDebug qWarning() { return QDebug(QtWarningMsg); }
QDebug qCritical() { return QDebug(QtCriticalMsg); }

#define NT_PRINTF_LOGGER(name, type)           \
    void name(const char *fmt, ...) {          \
        va_list ap;                            \
        va_start(ap, fmt);                     \
        dispatch(type, vformat(fmt, ap));      \
        va_end(ap);                            \
    }
NT_PRINTF_LOGGER(qDebug, QtDebugMsg)
NT_PRINTF_LOGGER(qInfo, QtInfoMsg)
NT_PRINTF_LOGGER(qWarning, QtWarningMsg)
NT_PRINTF_LOGGER(qCritical, QtCriticalMsg)
#undef NT_PRINTF_LOGGER

void qFatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    dispatch(QtFatalMsg, vformat(fmt, ap));
    va_end(ap);
    std::abort();
}
