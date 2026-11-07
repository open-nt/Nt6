// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <memory>

// Text is UTF-8 on devices and byte arrays, until QStringConverter exists.
class QTextStream {
public:
    enum RealNumberNotation { SmartNotation, FixedNotation, ScientificNotation };
    enum FieldAlignment { AlignLeft, AlignRight, AlignCenter, AlignAccountingStyle };
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed };
    enum NumberFlag {
        ShowBase = 0x1,
        ForcePoint = 0x2,
        ForceSign = 0x4,
        UppercaseBase = 0x8,
        UppercaseDigits = 0x10
    };
    Q_DECLARE_FLAGS(NumberFlags, NumberFlag)

    QTextStream();
    explicit QTextStream(QIODevice *device);
    explicit QTextStream(QString *string, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    explicit QTextStream(QByteArray *array, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    explicit QTextStream(const QByteArray &array, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadOnly);
    ~QTextStream();
    QTextStream(const QTextStream &) = delete;
    QTextStream &operator=(const QTextStream &) = delete;

    void setDevice(QIODevice *device);
    QIODevice *device() const { return m_dev; }
    void setString(QString *string, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    QString *string() const { return m_str; }
    Status status() const { return m_status; }
    void setStatus(Status status) { if (m_status == Ok) m_status = status; }
    void resetStatus() { m_status = Ok; }
    bool atEnd() const;
    void reset();
    void flush();
    bool seek(qint64 pos);
    qint64 pos() const;
    void skipWhiteSpace();

    QString readLine(qint64 maxlen = 0);
    bool readLineInto(QString *line, qint64 maxlen = 0);
    QString readAll();
    QString read(qint64 maxlen);

    void setFieldAlignment(FieldAlignment a) { m_align = a; }
    FieldAlignment fieldAlignment() const { return m_align; }
    void setPadChar(QChar c) { m_pad = c; }
    QChar padChar() const { return m_pad; }
    void setFieldWidth(int w) { m_width = w; }
    int fieldWidth() const { return m_width; }
    void setNumberFlags(NumberFlags f) { m_flags = f; }
    NumberFlags numberFlags() const { return m_flags; }
    void setIntegerBase(int base) { m_base = base; }
    int integerBase() const { return m_base; }
    void setRealNumberNotation(RealNumberNotation n) { m_notation = n; }
    RealNumberNotation realNumberNotation() const { return m_notation; }
    void setRealNumberPrecision(int p) { m_precision = p; }
    int realNumberPrecision() const { return m_precision; }

    QTextStream &operator>>(QChar &c);
    QTextStream &operator>>(char &c);
    QTextStream &operator>>(short &i) { return readInteger(i); }
    QTextStream &operator>>(unsigned short &i) { return readInteger(i); }
    QTextStream &operator>>(int &i) { return readInteger(i); }
    QTextStream &operator>>(unsigned int &i) { return readInteger(i); }
    QTextStream &operator>>(long &i) { return readInteger(i); }
    QTextStream &operator>>(unsigned long &i) { return readInteger(i); }
    QTextStream &operator>>(long long &i) { return readInteger(i); }
    QTextStream &operator>>(unsigned long long &i) { return readInteger(i); }
    QTextStream &operator>>(float &f);
    QTextStream &operator>>(double &f);
    QTextStream &operator>>(QString &s);
    QTextStream &operator>>(QByteArray &array);

    QTextStream &operator<<(QChar c);
    QTextStream &operator<<(char c) { return *this << QChar::fromLatin1(c); }
    QTextStream &operator<<(short i) { return writeSigned(i); }
    QTextStream &operator<<(unsigned short i) { return writeUnsigned(i); }
    QTextStream &operator<<(int i) { return writeSigned(i); }
    QTextStream &operator<<(unsigned int i) { return writeUnsigned(i); }
    QTextStream &operator<<(long i) { return writeSigned(i); }
    QTextStream &operator<<(unsigned long i) { return writeUnsigned(i); }
    QTextStream &operator<<(long long i) { return writeSigned(i); }
    QTextStream &operator<<(unsigned long long i) { return writeUnsigned(i); }
    QTextStream &operator<<(float f) { return *this << static_cast<double>(f); }
    QTextStream &operator<<(double f);
    QTextStream &operator<<(const QString &s);
    QTextStream &operator<<(QStringView s) { return *this << s.toString(); }
    QTextStream &operator<<(QLatin1StringView s) { return *this << QString(s); }
    QTextStream &operator<<(const QByteArray &array) { return *this << QString::fromUtf8(array); }
    QTextStream &operator<<(const char *s) { return *this << QString::fromUtf8(s); }
    QTextStream &operator<<(const void *ptr);

private:
    template <typename T>
    QTextStream &readInteger(T &out) {
        qlonglong v = 0;
        bool ok = false;
        const QString tok = nextToken();
        if constexpr (std::is_signed_v<T>)
            v = parseSigned(tok, &ok);
        else
            v = static_cast<qlonglong>(parseUnsigned(tok, &ok));
        out = ok ? static_cast<T>(v) : T();
        if (!ok)
            setStatus(tok.isEmpty() ? ReadPastEnd : ReadCorruptData);
        return *this;
    }
    template <typename T>
    QTextStream &writeSigned(T v) { const qlonglong w = v; return writeInteger(w < 0, w < 0 ? 0ULL - static_cast<qulonglong>(w) : static_cast<qulonglong>(w)); }
    template <typename T>
    QTextStream &writeUnsigned(T v) { return writeInteger(false, v); }

    QTextStream &writeInteger(bool negative, qulonglong magnitude);
    void put(const QString &field);
    void fill() const;
    QString nextToken();
    qlonglong parseSigned(const QString &s, bool *ok) const;
    qulonglong parseUnsigned(const QString &s, bool *ok) const;
    void attachOwned(QIODevice *d);

    QIODevice *m_dev = nullptr;
    std::unique_ptr<QIODevice> m_owned;
    QString *m_str = nullptr;
    QIODeviceBase::OpenMode m_strMode = QIODeviceBase::ReadWrite;
    mutable QString m_read;
    mutable qsizetype m_readPos = 0;
    mutable QByteArray m_partial;
    QByteArray m_write;
    Status m_status = Ok;
    FieldAlignment m_align = AlignRight;
    QChar m_pad = QChar(u' ');
    int m_width = 0;
    NumberFlags m_flags;
    int m_base = 10;
    RealNumberNotation m_notation = SmartNotation;
    int m_precision = 6;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QTextStream::NumberFlags)

using QTextStreamFunction = QTextStream &(*)(QTextStream &);
inline QTextStream &operator<<(QTextStream &s, QTextStreamFunction f) { return f(s); }
inline QTextStream &operator>>(QTextStream &s, QTextStreamFunction f) { return f(s); }

class QTextStreamManipulator {
public:
    using Setter = void (QTextStream::*)(int);
    using CharSetter = void (QTextStream::*)(QChar);
    constexpr QTextStreamManipulator(Setter m, int a) noexcept : setter(m), arg(a) {}
    constexpr QTextStreamManipulator(CharSetter m, QChar c) noexcept : charSetter(m), ch(c) {}
    void exec(QTextStream &s) const {
        if (setter)
            (s.*setter)(arg);
        else
            (s.*charSetter)(ch);
    }

private:
    Setter setter = nullptr;
    CharSetter charSetter = nullptr;
    int arg = 0;
    QChar ch;
};
inline QTextStream &operator<<(QTextStream &s, const QTextStreamManipulator &m) { m.exec(s); return s; }
inline QTextStream &operator>>(QTextStream &s, const QTextStreamManipulator &m) { m.exec(s); return s; }

inline QTextStreamManipulator qSetFieldWidth(int width) { return QTextStreamManipulator(&QTextStream::setFieldWidth, width); }
inline QTextStreamManipulator qSetPadChar(QChar ch) { return QTextStreamManipulator(&QTextStream::setPadChar, ch); }
inline QTextStreamManipulator qSetRealNumberPrecision(int precision) { return QTextStreamManipulator(&QTextStream::setRealNumberPrecision, precision); }

namespace Qt {
QTextStream &bin(QTextStream &s);
QTextStream &oct(QTextStream &s);
QTextStream &dec(QTextStream &s);
QTextStream &hex(QTextStream &s);
QTextStream &showbase(QTextStream &s);
QTextStream &noshowbase(QTextStream &s);
QTextStream &forcesign(QTextStream &s);
QTextStream &noforcesign(QTextStream &s);
QTextStream &uppercasebase(QTextStream &s);
QTextStream &lowercasebase(QTextStream &s);
QTextStream &uppercasedigits(QTextStream &s);
QTextStream &lowercasedigits(QTextStream &s);
QTextStream &fixed(QTextStream &s);
QTextStream &scientific(QTextStream &s);
QTextStream &left(QTextStream &s);
QTextStream &right(QTextStream &s);
QTextStream &center(QTextStream &s);
QTextStream &endl(QTextStream &s);
QTextStream &flush(QTextStream &s);
QTextStream &reset(QTextStream &s);
QTextStream &ws(QTextStream &s);
}
