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
    /*!
        The name of the source file the message came from, or \c nullptr.
        The default message handler context always leaves it unset.
    */
    const char *file = nullptr;
    /*!
        The line number the message came from, or \c 0. The default message
        handler context always leaves it unset.
    */
    int line = 0;
    /*!
        The name of the function the message came from, or \c nullptr. The
        default message handler context always leaves it unset.
    */
    const char *function = nullptr;
    /*!
        The logging category of the message. It is \c{"default"}.
    */
    const char *category = "default";
};

using QtMessageHandler = void (*)(QtMsgType, const QMessageLogContext &, const QString &);
QtMessageHandler qInstallMessageHandler(QtMessageHandler handler);

class QDebug {
public:
    explicit QDebug(QtMsgType type);
    /*!
        Constructs a copy of another stream. The copies share one buffer, and
        the message is emitted when the last copy is destroyed.
    */
    QDebug(const QDebug &) = default;
    /*!
        Assigns another stream to this stream and returns a reference to this
        stream. The two streams then share one buffer.
    */
    QDebug &operator=(const QDebug &) = default;
    ~QDebug();

    /*!
        Writes a space character after each value written to the stream, and
        returns a reference to the stream. This is the default.

        \sa nospace(), maybeSpace()
    */
    QDebug &space() { s->space = true; return *this; }
    /*!
        Disables the automatic insertion of spaces between values, and
        returns a reference to the stream.

        \sa space(), maybeSpace()
    */
    QDebug &nospace() { s->space = false; return *this; }
    /*!
        Writes a space character to the stream if spaces are enabled, and
        returns a reference to the stream.

        \sa space(), nospace()
    */
    QDebug &maybeSpace() { if (s->space) s->buf.push_back(' '); return *this; }
    /*!
        Enables quoting of strings and characters, and returns a reference to
        the stream. This is the default.

        \sa noquote(), maybeQuote()
    */
    QDebug &quote() { s->quote = true; return *this; }
    /*!
        Disables quoting of strings and characters, and returns a reference
        to the stream. Strings are then written without escaping.

        \sa quote(), maybeQuote()
    */
    QDebug &noquote() { s->quote = false; return *this; }
    /*!
        Writes the character \a c to the stream if quoting is enabled, and
        returns a reference to the stream.

        \sa quote(), noquote()
    */
    QDebug &maybeQuote(char c = '"') { if (s->quote) s->buf.push_back(c); return *this; }

    /*!
        Writes the boolean value \a v to the debug stream as \c true or
        \c false, and returns a reference to the stream.
    */
    QDebug &operator<<(bool v) { return put(v ? "true" : "false"); }
    /*!
        \overload

        Writes the character \a v to the debug stream, in single quotes if
        quoting is enabled.
    */
    QDebug &operator<<(char v);
    /*!
        \overload

        Writes the signed character \a v to the debug stream as a number.
    */
    QDebug &operator<<(signed char v) { return put(std::to_string(static_cast<int>(v))); }
    /*!
        \overload

        Writes the unsigned character \a v to the debug stream as a number.
    */
    QDebug &operator<<(unsigned char v) { return put(std::to_string(static_cast<unsigned>(v))); }
    /*!
        \overload

        Writes the short integer \a v to the debug stream.
    */
    QDebug &operator<<(short v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the unsigned short integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned short v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the integer \a v to the debug stream.
    */
    QDebug &operator<<(int v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the unsigned integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned int v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the long integer \a v to the debug stream.
    */
    QDebug &operator<<(long v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the unsigned long integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned long v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the long long integer \a v to the debug stream.
    */
    QDebug &operator<<(long long v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the unsigned long long integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned long long v) { return put(std::to_string(v)); }
    /*!
        \overload

        Writes the float \a v to the debug stream, using the shortest
        representation that reads back unchanged.
    */
    QDebug &operator<<(float v) { return put(QString::number(static_cast<double>(v), 'g', -1).toStdString()); }
    /*!
        \overload

        Writes the double \a v to the debug stream, using the shortest
        representation that reads back unchanged.
    */
    QDebug &operator<<(double v) { return put(QString::number(v, 'g', -1).toStdString()); }
    /*!
        \overload

        Writes the zero-terminated string \a v to the debug stream, without
        quotes or escaping. A null pointer is written as \c{(null)}.
    */
    QDebug &operator<<(const char *v) { return put(v ? v : "(null)"); }
    /*!
        \overload

        Writes the string \a v to the debug stream, without quotes or
        escaping.
    */
    QDebug &operator<<(const std::string &v) { return put(v); }
    /*!
        \overload

        Writes the pointer \a p to the debug stream as a hexadecimal address.
    */
    QDebug &operator<<(const void *p);
    /*!
        \overload

        Writes \c{(nullptr)} to the debug stream.
    */
    QDebug &operator<<(std::nullptr_t) { return put("(nullptr)"); }
    /*!
        \overload

        Writes the character \a c to the debug stream as a one-character
        string.
    */
    QDebug &operator<<(QChar c);
    /*!
        \overload

        Writes the string \a v to the debug stream, in double quotes and with
        special characters escaped if quoting is enabled.

        \sa quote(), noquote()
    */
    QDebug &operator<<(const QString &v);
    /*!
        \overload

        Writes the Latin-1 string \a v to the debug stream, as for a QString.
    */
    QDebug &operator<<(QLatin1StringView v) { return *this << QString(v); }
    /*!
        \overload

        Writes the byte array \a v to the debug stream, in double quotes and
        with special characters escaped if quoting is enabled.
    */
    QDebug &operator<<(const QByteArray &v);
    /*!
        \overload

        Writes the object \a o to the debug stream as its address and, if set,
        its object name, for example \c{QObject(0x1234, name = "x")}. A null
        pointer is written as \c{QObject(0x0)}.
    */
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
