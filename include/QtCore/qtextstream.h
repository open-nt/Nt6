// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlocale.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringconverter.h>
#include <QtCore/qstringview.h>

#include <cstdio>
#include <limits>
#include <memory>
#include <type_traits>

// Text is UTF-8 on devices and byte arrays, until QStringConverter exists.
class QTextStream {
public:
    /*!
        \enum QTextStream::RealNumberNotation

        Selects how floating-point numbers are written.

        \value SmartNotation Fixed or scientific notation, whichever is more
               compact (like \c printf's \c %g).
        \value FixedNotation Fixed-point notation (like \c %f).
        \value ScientificNotation Scientific notation (like \c %e).
    */
    enum RealNumberNotation { SmartNotation, FixedNotation, ScientificNotation };
    /*!
        \enum QTextStream::FieldAlignment

        Selects where padding is inserted when a value is shorter than the
        field width.

        \value AlignLeft Pad on the right side of the field.
        \value AlignRight Pad on the left side of the field. This is the
               default.
        \value AlignCenter Pad on both sides of the field.
        \value AlignAccountingStyle Pad between the sign and the digits of a
               number, and otherwise as \c AlignRight.
    */
    enum FieldAlignment { AlignLeft, AlignRight, AlignCenter, AlignAccountingStyle };
    /*!
        \enum QTextStream::Status

        Describes the state of the stream.

        \value Ok The stream is operating normally.
        \value ReadPastEnd The stream has read past the end of the data in the
               device or string.
        \value ReadCorruptData The stream read data that could not be
               converted to the requested type.
        \value WriteFailed The stream cannot write to the underlying device.
    */
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed };
    /*!
        \enum QTextStream::NumberFlag

        Controls how numbers are written. The NumberFlags type is a typedef for
        \c{QFlags<NumberFlag>}.

        \value ShowBase Show the base as a prefix (\c 0x, \c 0b or \c 0)
               for hexadecimal, binary and octal numbers.
        \value ForcePoint Always show the decimal point for floating-point
               numbers, even if there is no fractional part.
        \value ForceSign Always show the sign of numbers, even if positive.
        \value UppercaseBase Use an uppercase base prefix (\c 0X, \c 0B) and
               uppercase \c INF, \c NAN and exponent letters.
        \value UppercaseDigits Use uppercase letters for digits above 9 in
               hexadecimal numbers.
    */
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
    explicit QTextStream(FILE *fileHandle, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    explicit QTextStream(QString *string, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    explicit QTextStream(QByteArray *array, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    explicit QTextStream(const QByteArray &array, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadOnly);
    ~QTextStream();
    QTextStream(const QTextStream &) = delete;
    QTextStream &operator=(const QTextStream &) = delete;

    void setDevice(QIODevice *device);
    /*!
        Returns the current I/O device, or \c nullptr if none is set or the
        stream operates on a QString.

        \sa setDevice(), string()
    */
    QIODevice *device() const { return m_dev; }
    void setString(QString *string, QIODeviceBase::OpenMode mode = QIODeviceBase::ReadWrite);
    /*!
        Returns the current string assigned to the stream, or \c nullptr if
        the stream uses a device.

        \sa setString(), device()
    */
    QString *string() const { return m_str; }
    /*!
        Returns the status of the text stream.

        \sa Status, setStatus(), resetStatus()
    */
    Status status() const { return m_status; }
    /*!
        Sets the status of the text stream to \a status, unless an error was
        already recorded. The first error is kept until resetStatus() is
        called.

        \sa Status, status(), resetStatus()
    */
    void setStatus(Status status) { if (m_status == Ok) m_status = status; }
    /*!
        Resets the status of the text stream to \c Ok.

        \sa status(), setStatus()
    */
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

    /*!
        Sets the field alignment to \a a. The alignment applies when a field
        width is set.

        \sa fieldAlignment(), setFieldWidth()
    */
    void setFieldAlignment(FieldAlignment a) { m_align = a; }
    /*!
        Returns the current field alignment, which is \c AlignRight by
        default.

        \sa setFieldAlignment()
    */
    FieldAlignment fieldAlignment() const { return m_align; }
    /*!
        Sets the pad character to \a c. The default is a space.

        \sa padChar(), setFieldWidth()
    */
    void setPadChar(QChar c) { m_pad = c; }
    /*!
        Returns the current pad character.

        \sa setPadChar()
    */
    QChar padChar() const { return m_pad; }
    /*!
        Sets the current field width to \a w. If \a w is \c 0, the default,
        no padding is added. The width applies to each value written.

        \sa fieldWidth(), setPadChar(), setFieldAlignment()
    */
    void setFieldWidth(int w) { m_width = w; }
    /*!
        Returns the current field width.

        \sa setFieldWidth()
    */
    int fieldWidth() const { return m_width; }
    /*!
        Sets the current number flags to \a f.

        \sa numberFlags(), NumberFlag
    */
    void setNumberFlags(NumberFlags f) { m_flags = f; }
    /*!
        Returns the current number flags.

        \sa setNumberFlags(), NumberFlag
    */
    NumberFlags numberFlags() const { return m_flags; }
    /*!
        Sets the base of integers to \a base, which can be 2 to 36 when
        writing. When reading, a base of \c 0 detects the base from a prefix
        (\c 0x, \c 0b or a leading \c 0 for octal). Other values are treated as
        10. The default is 10.

        \sa integerBase()
    */
    void setIntegerBase(int base) { m_base = base; }
    /*!
        Returns the current base of integers.

        \sa setIntegerBase()
    */
    int integerBase() const { return m_base; }
    /*!
        Sets the real number notation to \a n.

        \sa realNumberNotation(), setRealNumberPrecision()
    */
    void setRealNumberNotation(RealNumberNotation n) { m_notation = n; }
    /*!
        Returns the current real number notation, which is \c SmartNotation by
        default.

        \sa setRealNumberNotation()
    */
    RealNumberNotation realNumberNotation() const { return m_notation; }
    /*!
        Sets the precision of real numbers to \a p, the number of digits
        written after the decimal point for fixed and scientific notation, or
        significant digits for smart notation. A negative value is treated as
        6, which is the default.

        \sa realNumberPrecision(), setRealNumberNotation()
    */
    void setRealNumberPrecision(int p) { m_precision = p; }
    void setEncoding(QStringConverter::Encoding encoding);
    /*!
        \since 6.0
        Returns the encoding of the stream: the one set with setEncoding(),
        or the one found by Unicode detection. The default is
        QStringConverter::Utf8.

        \sa setEncoding(), autoDetectUnicode()
    */
    QStringConverter::Encoding encoding() const { return m_encoding; }
    /*!
        Sets whether the stream detects the encoding of the data from a byte
        order mark at its start to \a enabled. It is enabled by default.

        \sa autoDetectUnicode(), encoding()
    */
    void setAutoDetectUnicode(bool enabled) { m_autoDetect = enabled; }
    /*!
        Returns \c true if the stream detects the encoding from a byte order
        mark; otherwise returns \c false.

        \sa setAutoDetectUnicode()
    */
    bool autoDetectUnicode() const { return m_autoDetect; }
    /*!
        Sets whether the stream writes a byte order mark before the first text
        it writes, for the Unicode encodings, to \a generate. It is off by
        default.

        \sa generateByteOrderMark(), Qt::bom()
    */
    void setGenerateByteOrderMark(bool generate) { m_writeBom = generate; }
    /*!
        Returns \c true if the stream writes a byte order mark; otherwise
        returns \c false.

        \sa setGenerateByteOrderMark()
    */
    bool generateByteOrderMark() const { return m_writeBom; }
    /*!
        Returns the current real number precision.

        \sa setRealNumberPrecision()
    */
    int realNumberPrecision() const { return m_precision; }
    void setLocale(const QLocale &locale);
    /*!
        Returns the locale used for numbers. The default is QLocale::c().

        \sa setLocale()
    */
    QLocale locale() const { return m_locale; }

    /*!
        Reads a character from the stream into \a c, skipping leading
        whitespace, and returns a reference to the stream. If no character is
        available, \a c is set to a null character and the status is set to
        \c ReadPastEnd.
    */
    QTextStream &operator>>(QChar &c);
    /*!
        \overload

        Reads a character from the stream into \a c, skipping leading
        whitespace. The character is converted to Latin-1.
    */
    QTextStream &operator>>(char &c);
    /*!
        \overload

        Reads a short integer from the stream into \a i, skipping leading
        whitespace and using the current integer base. If the number cannot
        be read or does not fit the type, \a i is set to \c 0 and the status
        is set to \c ReadPastEnd at the end of the data or \c ReadCorruptData
        otherwise. Only the longest valid numeric prefix is consumed.
    */
    QTextStream &operator>>(short &i) { return readInteger(i); }
    /*!
        \overload

        Reads an unsigned short integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(unsigned short &i) { return readInteger(i); }
    /*!
        \overload

        Reads an integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(int &i) { return readInteger(i); }
    /*!
        \overload

        Reads an unsigned integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(unsigned int &i) { return readInteger(i); }
    /*!
        \overload

        Reads a long integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(long &i) { return readInteger(i); }
    /*!
        \overload

        Reads an unsigned long integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(unsigned long &i) { return readInteger(i); }
    /*!
        \overload

        Reads a long long integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(long long &i) { return readInteger(i); }
    /*!
        \overload

        Reads an unsigned long long integer from the stream into \a i; see
        \c{operator>>(short &)}.
    */
    QTextStream &operator>>(unsigned long long &i) { return readInteger(i); }
    /*!
        \overload

        Reads a real number from the stream into \a f, skipping leading
        whitespace. Decimal, exponent, \c inf and \c nan forms are accepted.
        If no number can be read, \a f is set to \c 0 and the status is set
        to \c ReadPastEnd or \c ReadCorruptData.
    */
    QTextStream &operator>>(float &f);
    /*!
        \overload

        Reads a real number from the stream into \a f; see
        \c{operator>>(float &)}.
    */
    QTextStream &operator>>(double &f);
    /*!
        \overload

        Reads a whitespace-delimited word from the stream into \a s,
        skipping leading whitespace. If no word is available, \a s is null and
        the status is set to \c ReadPastEnd.
    */
    QTextStream &operator>>(QString &s);
    /*!
        \overload

        Reads a whitespace-delimited word from the stream into \a array as
        UTF-8, skipping leading whitespace. If no word is available, \a array
        is cleared and the status is set to \c ReadPastEnd.
    */
    QTextStream &operator>>(QByteArray &array);

    /*!
        Writes the character \a c to the stream, padded to the field width,
        and returns a reference to the stream.
    */
    QTextStream &operator<<(QChar c);
    /*!
        \overload

        Writes the Latin-1 character \a c to the stream.
    */
    QTextStream &operator<<(char c) { return *this << QChar::fromLatin1(c); }
    /*!
        \overload

        Writes the short integer \a i to the stream, using the current integer
        base, number flags and field settings.
    */
    QTextStream &operator<<(short i) { return writeSigned(i); }
    /*!
        \overload

        Writes the unsigned short integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(unsigned short i) { return writeUnsigned(i); }
    /*!
        \overload

        Writes the integer \a i to the stream; see \c{operator<<(short)}.
    */
    QTextStream &operator<<(int i) { return writeSigned(i); }
    /*!
        \overload

        Writes the unsigned integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(unsigned int i) { return writeUnsigned(i); }
    /*!
        \overload

        Writes the long integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(long i) { return writeSigned(i); }
    /*!
        \overload

        Writes the unsigned long integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(unsigned long i) { return writeUnsigned(i); }
    /*!
        \overload

        Writes the long long integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(long long i) { return writeSigned(i); }
    /*!
        \overload

        Writes the unsigned long long integer \a i to the stream; see
        \c{operator<<(short)}.
    */
    QTextStream &operator<<(unsigned long long i) { return writeUnsigned(i); }
    /*!
        \overload

        Writes the real number \a f to the stream, as a double.
    */
    QTextStream &operator<<(float f) { return *this << static_cast<double>(f); }
    /*!
        \overload

        Writes the real number \a f to the stream, using the current real
        number notation, precision, number flags and field settings.
    */
    QTextStream &operator<<(double f);
    /*!
        \overload

        Writes the string \a s to the stream, padded to the field width.
    */
    QTextStream &operator<<(const QString &s);
    /*!
        \overload

        Writes the string view \a s to the stream.
    */
    QTextStream &operator<<(QStringView s) { return *this << s.toString(); }
    /*!
        \overload

        Writes the Latin-1 string \a s to the stream.
    */
    QTextStream &operator<<(QLatin1StringView s) { return *this << QString(s); }
    /*!
        \overload

        Writes the UTF-8 byte array \a array to the stream.
    */
    QTextStream &operator<<(const QByteArray &array) { return *this << QString::fromUtf8(array); }
    /*!
        \overload

        Writes the zero-terminated UTF-8 string \a s to the stream.
    */
    QTextStream &operator<<(const char *s) { return *this << QString::fromUtf8(s); }
    /*!
        \overload

        Writes the address \a ptr to the stream as a hexadecimal number with
        a \c 0x prefix.
    */
    QTextStream &operator<<(const void *ptr);

private:
    template <typename T>
    QTextStream &readInteger(T &out) {
        bool ok = false;
        const QString tok = scanInteger();
        out = T();
        if constexpr (std::is_signed_v<T>) {
            const qlonglong v = parseSigned(tok, &ok);
            ok = ok && v >= static_cast<qlonglong>(std::numeric_limits<T>::min())
                && v <= static_cast<qlonglong>(std::numeric_limits<T>::max());
            if (ok)
                out = static_cast<T>(v);
        } else {
            const qulonglong v = parseUnsigned(tok, &ok);
            ok = ok && v <= static_cast<qulonglong>(std::numeric_limits<T>::max());
            if (ok)
                out = static_cast<T>(v);
        }
        if (!ok)
            setStatus(tok.isNull() ? ReadPastEnd : ReadCorruptData);
        return *this;
    }
    template <typename T>
    QTextStream &writeSigned(T v) { const qlonglong w = v; return writeInteger(w < 0, w < 0 ? 0ULL - static_cast<qulonglong>(w) : static_cast<qulonglong>(w)); }
    template <typename T>
    QTextStream &writeUnsigned(T v) { return writeInteger(false, v); }

    QTextStream &writeInteger(bool negative, qulonglong magnitude);
    QString padded(const QString &sign, const QString &body) const;
    void put(const QString &field);
    void rawPut(const QString &text);
    void flushWrite() const;
    bool fill() const;
    bool available(qsizetype n) const;
    QString nextToken();
    QString scanInteger();
    QString scanReal();
    qlonglong parseSigned(const QString &s, bool *ok) const;
    qulonglong parseUnsigned(const QString &s, bool *ok) const;
    void attachOwned(QIODevice *d);

    QIODevice *m_dev = nullptr;
    std::unique_ptr<QIODevice> m_owned;
    QString *m_str = nullptr;
    QIODeviceBase::OpenMode m_strMode = QIODeviceBase::ReadWrite;
    mutable QString m_read;
    mutable qsizetype m_readPos = 0;
    mutable QStringDecoder m_decoder;
    mutable bool m_decoderReady = false;
    mutable QStringEncoder m_encoder;
    mutable bool m_encoderReady = false;
    mutable QStringConverter::Encoding m_encoding = QStringConverter::Utf8; // detection in fill() can change it
    bool m_autoDetect = true;
    bool m_writeBom = false;
    QLocale m_locale = QLocale::c();
    bool m_localized = false; // m_locale is not the C locale
    QChar m_decimal = QChar(u'.');
    QChar m_group; // null when the locale omits group separators
    QString groupDigits(const QString &digits) const;
    mutable QByteArray m_write;
    mutable Status m_status = Ok;
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

/*!
    \class QTextStreamManipulator
    \brief The QTextStreamManipulator class holds a deferred QTextStream setter call.

    The objects returned by \c qSetFieldWidth(), \c qSetPadChar() and
    \c qSetRealNumberPrecision() are QTextStreamManipulator instances. Streaming
    one into a QTextStream with \c{<<} or \c{>>} calls the stored setter on
    that stream.

    \differences
    In Qt this class is an undocumented implementation detail. In Nt it is
    public, and its setters are limited to those taking an \c int or a QChar.
    \enddifferences

    \sa QTextStream
*/
class QTextStreamManipulator {
public:
    /*!
        The type of a QTextStream setter that takes an \c int.
    */
    using Setter = void (QTextStream::*)(int);
    /*!
        The type of a QTextStream setter that takes a QChar.
    */
    using CharSetter = void (QTextStream::*)(QChar);
    /*!
        Constructs a manipulator that calls the \a m setter with the integer
        argument \a a.
    */
    constexpr QTextStreamManipulator(Setter m, int a) noexcept : setter(m), arg(a) {}
    /*!
        Constructs a manipulator that calls the \a m setter with the
        character argument \a c.
    */
    constexpr QTextStreamManipulator(CharSetter m, QChar c) noexcept : charSetter(m), ch(c) {}
    /*!
        Calls the stored setter on the stream \a s.
    */
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
/*!
    Calls QTextStream::setIntegerBase() with base 2 on the stream \a s and
    returns \a s.
*/
QTextStream &bin(QTextStream &s);
/*!
    Calls QTextStream::setIntegerBase() with base 8 on the stream \a s and
    returns \a s.
*/
QTextStream &oct(QTextStream &s);
/*!
    Calls QTextStream::setIntegerBase() with base 10 on the stream \a s and
    returns \a s.
*/
QTextStream &dec(QTextStream &s);
/*!
    Calls QTextStream::setIntegerBase() with base 16 on the stream \a s and
    returns \a s.
*/
QTextStream &hex(QTextStream &s);
/*!
    Calls QTextStream::setGenerateByteOrderMark() with \c true on the
    stream \a s and returns \a s.
*/
QTextStream &bom(QTextStream &s);
/*!
    Sets the \c ShowBase number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &showbase(QTextStream &s);
/*!
    Clears the \c ShowBase number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &noshowbase(QTextStream &s);
/*!
    Sets the \c ForceSign number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &forcesign(QTextStream &s);
/*!
    Clears the \c ForceSign number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &noforcesign(QTextStream &s);
/*!
    Sets the \c ForcePoint number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &forcepoint(QTextStream &s);
/*!
    Clears the \c ForcePoint number flag on the stream \a s and returns \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &noforcepoint(QTextStream &s);
/*!
    Sets the \c UppercaseBase number flag on the stream \a s and returns
    \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &uppercasebase(QTextStream &s);
/*!
    Clears the \c UppercaseBase number flag on the stream \a s and returns
    \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &lowercasebase(QTextStream &s);
/*!
    Sets the \c UppercaseDigits number flag on the stream \a s and returns
    \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &uppercasedigits(QTextStream &s);
/*!
    Clears the \c UppercaseDigits number flag on the stream \a s and returns
    \a s.

    \sa QTextStream::setNumberFlags()
*/
QTextStream &lowercasedigits(QTextStream &s);
/*!
    Sets the real number notation of the stream \a s to
    \c QTextStream::FixedNotation and returns \a s.

    \sa QTextStream::setRealNumberNotation()
*/
QTextStream &fixed(QTextStream &s);
/*!
    Sets the real number notation of the stream \a s to
    \c QTextStream::ScientificNotation and returns \a s.

    \sa QTextStream::setRealNumberNotation()
*/
QTextStream &scientific(QTextStream &s);
/*!
    Sets the field alignment of the stream \a s to \c QTextStream::AlignLeft
    and returns \a s.

    \sa QTextStream::setFieldAlignment()
*/
QTextStream &left(QTextStream &s);
/*!
    Sets the field alignment of the stream \a s to \c QTextStream::AlignRight
    and returns \a s.

    \sa QTextStream::setFieldAlignment()
*/
QTextStream &right(QTextStream &s);
/*!
    Sets the field alignment of the stream \a s to \c QTextStream::AlignCenter
    and returns \a s.

    \sa QTextStream::setFieldAlignment()
*/
QTextStream &center(QTextStream &s);
/*!
    Writes a newline character to the stream \a s, flushes it, and returns
    \a s.

    \sa QTextStream::flush()
*/
QTextStream &endl(QTextStream &s);
/*!
    Flushes the stream \a s and returns \a s.

    \sa QTextStream::flush()
*/
QTextStream &flush(QTextStream &s);
/*!
    Resets the formatting options of the stream \a s to their defaults and
    returns \a s.

    \sa QTextStream::reset()
*/
QTextStream &reset(QTextStream &s);
/*!
    Skips whitespace in the stream \a s and returns \a s.

    \sa QTextStream::skipWhiteSpace()
*/
QTextStream &ws(QTextStream &s);
}
