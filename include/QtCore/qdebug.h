// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>
#include <string>
#include <utility>

enum QtMsgType { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg, QtSystemMsg = QtCriticalMsg };

class QMessageLogContext {
public:
    const char *file = nullptr;
    int line = 0;
    const char *function = nullptr;
    const char *category = "default";
};

using QtMessageHandler = void (*)(QtMsgType, const QMessageLogContext &, const QString &);
QtMessageHandler qInstallMessageHandler(QtMessageHandler handler);

class QDebug {
public:
    explicit QDebug(QtMsgType type);
    QDebug(const QDebug &) = default;
    QDebug &operator=(const QDebug &) = default;
    ~QDebug();

    QDebug &space() { s->space = true; return *this; }
    QDebug &nospace() { s->space = false; return *this; }
    QDebug &maybeSpace() { if (s->space) s->buf.push_back(' '); return *this; }
    QDebug &quote() { s->quote = true; return *this; }
    QDebug &noquote() { s->quote = false; return *this; }
    QDebug &maybeQuote(char c = '"') { if (s->quote) s->buf.push_back(c); return *this; }

    QDebug &operator<<(bool v) { return put(v ? "true" : "false"); }
    QDebug &operator<<(char v);
    QDebug &operator<<(signed char v) { return put(std::to_string(static_cast<int>(v))); }
    QDebug &operator<<(unsigned char v) { return put(std::to_string(static_cast<unsigned>(v))); }
    QDebug &operator<<(short v) { return put(std::to_string(v)); }
    QDebug &operator<<(unsigned short v) { return put(std::to_string(v)); }
    QDebug &operator<<(int v) { return put(std::to_string(v)); }
    QDebug &operator<<(unsigned int v) { return put(std::to_string(v)); }
    QDebug &operator<<(long v) { return put(std::to_string(v)); }
    QDebug &operator<<(unsigned long v) { return put(std::to_string(v)); }
    QDebug &operator<<(long long v) { return put(std::to_string(v)); }
    QDebug &operator<<(unsigned long long v) { return put(std::to_string(v)); }
    QDebug &operator<<(float v) { return put(QString::number(static_cast<double>(v), 'g', -1).toStdString()); }
    QDebug &operator<<(double v) { return put(QString::number(v, 'g', -1).toStdString()); }
    QDebug &operator<<(const char *v) { return put(v ? v : "(null)"); }
    QDebug &operator<<(const std::string &v) { return put(v); }
    QDebug &operator<<(const void *p);
    QDebug &operator<<(std::nullptr_t) { return put("(nullptr)"); }
    QDebug &operator<<(QChar c);
    QDebug &operator<<(const QString &v);
    QDebug &operator<<(QLatin1StringView v) { return *this << QString(v); }
    QDebug &operator<<(const QByteArray &v);
    QDebug &operator<<(const QObject *o);

private:
    friend class QMessageLogger;
    QDebug &put(const std::string &text) {
        s->buf += text;
        return maybeSpace();
    }
    struct State {
        std::string buf;
        QtMsgType type;
        bool space = true;
        bool quote = true;
    };
    std::shared_ptr<State> s;
};

template <typename T>
QDebug operator<<(QDebug d, const QList<T> &l) {
    d.nospace() << "(";
    for (qsizetype i = 0; i < l.size(); ++i) {
        if (i)
            d << ", ";
        d << l.at(i);
    }
    d << ")";
    d.space();
    return d.maybeSpace();
}

template <typename K, typename V>
QDebug operator<<(QDebug d, const QMap<K, V> &m) {
    d.nospace() << "QMap(";
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        d << "(" << it.key() << ", " << it.value() << ")";
    d << ")";
    d.space();
    return d.maybeSpace();
}

template <typename K, typename V>
QDebug operator<<(QDebug d, const QHash<K, V> &m) {
    d.nospace() << "QHash(";
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        d << "(" << it.key() << ", " << it.value() << ")";
    d << ")";
    d.space();
    return d.maybeSpace();
}

QDebug qDebug();
QDebug qInfo();
QDebug qWarning();
QDebug qCritical();
void qDebug(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void qInfo(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void qWarning(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void qCritical(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
[[noreturn]] void qFatal(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define qPrintable(string) (string).toLocal8Bit().constData()
#define qUtf8Printable(string) (string).toUtf8().constData()
#define Q_FUNC_INFO __PRETTY_FUNCTION__
