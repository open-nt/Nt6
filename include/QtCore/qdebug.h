// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qchar.h>
#include <QtCore/qglobal.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qlogging.h>
#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>
#include <string>
#include <utility>

class QIODevice;

class QDebug {
public:
    explicit QDebug(QtMsgType type);
    explicit QDebug(QString *string);
    explicit QDebug(QIODevice *device);
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
        Returns \c true if spaces are inserted between values automatically.

        \sa setAutoInsertSpaces(), space(), nospace()
    */
    bool autoInsertSpaces() const { return s->space; }
    /*!
        Sets whether spaces are inserted between values automatically to \a b.

        \sa autoInsertSpaces(), space(), nospace()
    */
    void setAutoInsertSpaces(bool b) { s->space = b; }
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
    friend class QDebugStateSaver;
    struct State {
        std::string buf;
        QtMsgType type = QtDebugMsg;
        bool space = true;
        bool quote = true;
        bool output = true;
        const char *file = nullptr;
        int line = 0;
        const char *function = nullptr;
        std::string category;
        QString *string = nullptr;
        QIODevice *device = nullptr;
    };
    std::shared_ptr<State> s;
};

template <typename T>
QDebug operator<<(QDebug d, const QList<T> &l) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "(";
    for (qsizetype i = 0; i < l.size(); ++i) {
        if (i)
            d << ", ";
        d << l.at(i);
    }
    d << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

template <typename K, typename V>
QDebug operator<<(QDebug d, const QMap<K, V> &m) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "QMap(";
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        d << "(" << it.key() << ", " << it.value() << ")";
    d << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

template <typename K, typename V>
QDebug operator<<(QDebug d, const QHash<K, V> &m) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "QHash(";
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        d << "(" << it.key() << ", " << it.value() << ")";
    d << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

class QDate;
class QTime;
class QDateTime;
class QUrl;
class QUuid;
class QVersionNumber;
class QBitArray;
class QPoint;
class QPointF;
class QSize;
class QSizeF;
class QLine;
class QLineF;
class QRect;
class QRectF;
class QMargins;
class QMarginsF;

QDebug operator<<(QDebug d, const QDate &date);
QDebug operator<<(QDebug d, const QTime &time);
QDebug operator<<(QDebug d, const QDateTime &dateTime);
QDebug operator<<(QDebug d, const QUrl &url);
QDebug operator<<(QDebug d, const QUuid &uuid);
QDebug operator<<(QDebug d, const QVersionNumber &version);
QDebug operator<<(QDebug d, const QBitArray &bits);
QDebug operator<<(QDebug d, const QPoint &p);
QDebug operator<<(QDebug d, const QPointF &p);
QDebug operator<<(QDebug d, const QSize &s);
QDebug operator<<(QDebug d, const QSizeF &s);
QDebug operator<<(QDebug d, const QLine &l);
QDebug operator<<(QDebug d, const QLineF &l);
QDebug operator<<(QDebug d, const QRect &r);
QDebug operator<<(QDebug d, const QRectF &r);
QDebug operator<<(QDebug d, const QMargins &m);
QDebug operator<<(QDebug d, const QMarginsF &m);

class QDebugStateSaver {
public:
    explicit QDebugStateSaver(QDebug &dbg);
    ~QDebugStateSaver();
    QDebugStateSaver(const QDebugStateSaver &) = delete;
    QDebugStateSaver &operator=(const QDebugStateSaver &) = delete;

private:
    QDebug &m_debug;
    bool m_space;
    bool m_quote;
};

#define qPrintable(string) (string).toLocal8Bit().constData()
#define qUtf8Printable(string) (string).toUtf8().constData()
