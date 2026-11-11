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
#include <QtCore/qstringview.h>
#include <QtCore/qtextstream.h>

#include <cstdio>
#include <memory>
#include <list>
#include <string>
#include <utility>
#include <vector>

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
        \since 6.10
        Returns \c true if strings, characters and byte arrays are written in
        quotes; otherwise returns \c false.

        \sa setQuoteStrings(), quote(), noquote()
    */
    bool quoteStrings() const noexcept { return s->quote; }
    /*!
        \since 6.10
        Turns the quoting of strings, characters and byte arrays on if \a b
        is \c true and off otherwise.

        \sa quoteStrings(), quote(), noquote()
    */
    void setQuoteStrings(bool b) { s->quote = b; }
    /*!
        \since 6.0
        Returns \a object written to a debug stream, as a string. The stream
        does not insert spaces, so the result is what \c{qDebug().nospace()}
        would print.

        \sa toBytes()
    */
    template <typename T> static QString toString(const T &object) {
        QString buffer;
        {
            QDebug stream(&buffer);
            stream.nospace() << object;
        }
        return buffer;
    }
    /*!
        \since 6.9
        Returns \a object written to a debug stream, as UTF-8 encoded bytes.

        \sa toString()
    */
    template <typename T> static QByteArray toBytes(const T &object) { return toString(object).toUtf8(); }
    /*!
        Writes the character \a c to the stream if quoting is enabled, and
        returns a reference to the stream.

        \sa quote(), noquote()
    */
    QDebug &maybeQuote(char c = '"') { if (s->quote) s->buf.push_back(c); return *this; }
    /*!
        \since 5.6
        Returns the verbosity of the stream, from 0 (least verbose) to 7
        (most verbose). The default is 2.

        \sa setVerbosity()
    */
    int verbosity() const { return s->verbosity; }
    /*!
        \since 5.6
        Sets the verbosity of the stream to \a verbosityLevel, which is
        clamped to the range 0 to 7. Streaming operators may use it to decide
        how much detail to write.

        \sa verbosity()
    */
    void setVerbosity(int verbosityLevel) {
        s->verbosity = verbosityLevel < 0 ? 0 : (verbosityLevel > 7 ? 7 : verbosityLevel);
    }
    /*!
        \overload
        \since 5.13
        Sets the verbosity of the stream to \a verbosityLevel and returns a
        reference to the stream.
    */
    QDebug &verbosity(int verbosityLevel) {
        setVerbosity(verbosityLevel);
        return *this;
    }
    /*!
        \since 5.4
        Restores the default formatting: automatic spaces, quoting and
        verbosity 2. Returns a reference to the stream.

        \sa space(), quote(), setVerbosity()
    */
    QDebug &resetFormat() {
        s->space = true;
        s->quote = true;
        s->verbosity = DefaultVerbosity;
        return *this;
    }
    /*!
        \since 5.0
        Swaps this stream with \a other.
    */
    void swap(QDebug &other) noexcept { s.swap(other.s); }

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
    QDebug &operator<<(signed char v) { return putNumber(static_cast<int>(v)); }
    /*!
        \overload

        Writes the unsigned character \a v to the debug stream as a number.
    */
    QDebug &operator<<(unsigned char v) { return putNumber(static_cast<unsigned>(v)); }
    /*!
        \overload

        Writes the short integer \a v to the debug stream.
    */
    QDebug &operator<<(short v) { return putNumber(v); }
    /*!
        \overload

        Writes the unsigned short integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned short v) { return putNumber(v); }
    /*!
        \overload

        Writes the integer \a v to the debug stream.
    */
    QDebug &operator<<(int v) { return putNumber(v); }
    /*!
        \overload

        Writes the unsigned integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned int v) { return putNumber(v); }
    /*!
        \overload

        Writes the long integer \a v to the debug stream.
    */
    QDebug &operator<<(long v) { return putNumber(v); }
    /*!
        \overload

        Writes the unsigned long integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned long v) { return putNumber(v); }
    /*!
        \overload

        Writes the long long integer \a v to the debug stream.
    */
    QDebug &operator<<(long long v) { return putNumber(v); }
    /*!
        \overload

        Writes the unsigned long long integer \a v to the debug stream.
    */
    QDebug &operator<<(unsigned long long v) { return putNumber(v); }
    /*!
        \overload

        Writes the float \a v to the debug stream, using the shortest
        representation that reads back unchanged.
    */
    QDebug &operator<<(float v) {
        return s->format.isDefault() ? put(QString::number(static_cast<double>(v), 'g', -1).toStdString())
                                     : putNumber(v);
    }
    /*!
        \overload

        Writes the double \a v to the debug stream, using the shortest
        representation that reads back unchanged.
    */
    QDebug &operator<<(double v) {
        return s->format.isDefault() ? put(QString::number(v, 'g', -1).toStdString()) : putNumber(v);
    }
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
        \since 5.10
        Writes the string view \a v to the debug stream, quoted and escaped
        like a QString, and returns a reference to the stream.
    */
    QDebug &operator<<(QStringView v) { return *this << v.toString(); }
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
    QDebug &operator<<(QTextStreamFunction f);
    QDebug &operator<<(const QTextStreamManipulator &m);

private:
    friend class QMessageLogger;
    QDebug &put(const std::string &text) {
        s->buf += text;
        return maybeSpace();
    }
    // The QTextStream settings that the manipulators change, applied to numbers.
    struct Format {
        int base = 10;
        QTextStream::NumberFlags flags;
        QTextStream::RealNumberNotation notation = QTextStream::SmartNotation;
        int precision = 6;
        int width = 0;
        QChar pad = QChar(u' ');
        QTextStream::FieldAlignment align = QTextStream::AlignRight;
        bool changed = false; // set once a manipulator ran; until then numbers take the fast path
        bool isDefault() const { return !changed; }
    };
    static void applyFormat(QTextStream &ts, const Format &f);
    static void readFormat(const QTextStream &ts, Format &f);
    template <typename T> QDebug &putNumber(T v) {
        if (s->format.isDefault()) {
            if constexpr (std::is_floating_point_v<T>)
                return put(QString::number(static_cast<double>(v), 'g', -1).toStdString());
            else
                return put(std::to_string(v));
        }
        QString out;
        QTextStream ts(&out);
        applyFormat(ts, s->format);
        ts << v;
        ts.flush();
        return put(out.toStdString());
    }
    friend class QDebugStateSaver;
    static constexpr int DefaultVerbosity = 2;
    struct State {
        std::string buf;
        QtMsgType type = QtDebugMsg;
        bool space = true;
        bool quote = true;
        bool output = true;
        int verbosity = DefaultVerbosity;
        const char *file = nullptr;
        int line = 0;
        const char *function = nullptr;
        std::string category;
        QString *string = nullptr;
        QIODevice *device = nullptr;
        Format format;
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

namespace QtPrivate {
template <typename It> QDebug printSequence(QDebug d, const char *name, It first, It last) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << name << "(";
    for (It it = first; it != last; ++it) {
        if (it != first)
            d << ", ";
        d << *it;
    }
    d << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}
template <typename Map> QDebug printAssociative(QDebug d, const char *name, const Map &m) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << name << "(";
    for (auto it = m.cbegin(); it != m.cend(); ++it)
        d << "(" << it.key() << ", " << it.value() << ")";
    d << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}
} // namespace QtPrivate

template <typename T> QDebug operator<<(QDebug d, const QSet<T> &set) {
    return QtPrivate::printSequence(d, "QSet", set.cbegin(), set.cend());
}

template <typename T, typename Alloc> QDebug operator<<(QDebug d, const std::vector<T, Alloc> &v) {
    return QtPrivate::printSequence(d, "std::vector", v.cbegin(), v.cend());
}

template <typename T, typename Alloc> QDebug operator<<(QDebug d, const std::list<T, Alloc> &l) {
    return QtPrivate::printSequence(d, "std::list", l.cbegin(), l.cend());
}

template <typename T1, typename T2> QDebug operator<<(QDebug d, const std::pair<T1, T2> &pair) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "std::pair(" << pair.first << ", " << pair.second << ")";
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

template <typename K, typename V> QDebug operator<<(QDebug d, const QMultiMap<K, V> &m) {
    return QtPrivate::printAssociative(d, "QMultiMap", m);
}

template <typename K, typename V> QDebug operator<<(QDebug d, const QMultiHash<K, V> &m) {
    return QtPrivate::printAssociative(d, "QMultiHash", m);
}

template <typename Enum> QDebug operator<<(QDebug d, QFlags<Enum> flags) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace() << "QFlags(";
    bool needSeparator = false;
    using U = std::make_unsigned_t<typename QFlags<Enum>::Int>;
    const auto bits = static_cast<U>(flags.toInt());
    for (unsigned i = 0; i < sizeof(U) * 8; ++i) {
        const U bit = static_cast<U>(U(1) << i);
        if ((bits & bit) == 0)
            continue;
        if (needSeparator)
            d << "|";
        needSeparator = true;
        char buf[24];
        std::snprintf(buf, sizeof buf, "0x%llx", static_cast<unsigned long long>(bit));
        d << buf;
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
    int m_verbosity;
    QDebug::Format m_format;
};

#define qPrintable(string) (string).toLocal8Bit().constData()
#define qUtf8Printable(string) (string).toUtf8().constData()
