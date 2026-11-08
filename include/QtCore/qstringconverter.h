// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

#include <optional>

/*!
    \class QStringConverterBase
    \since 6.0
    \brief The QStringConverterBase class provides the flags shared by the string converters.

    QStringConverterBase is the base class of QStringConverter. It holds the
    Flag enum, which controls how byte order marks and invalid input are
    handled.

    \differences
    The \c State structure is not part of the API; each converter keeps its
    state privately. \c Flag::UsesIcu is accepted but ignored.
    \enddifferences

    \sa QStringEncoder, QStringDecoder
*/
class QStringConverterBase {
public:
    /*!
        \enum QStringConverterBase::Flag

        \value Default Byte order marks at the start of decoded data are
               skipped, encoders do not write one, and invalid input is
               replaced by the replacement character.
        \value Stateless Each call converts its input on its own: an
               incomplete sequence at the end of the input is treated as
               invalid instead of being kept for the next call.
        \value ConvertInvalidToNull Invalid input is converted to U+0000 (or a
               null byte) instead of the replacement character.
        \value WriteBom Encoders write a byte order mark before the first
               encoded character.
        \value ConvertInitialBom Decoders keep a byte order mark at the start
               of the data as U+FEFF instead of skipping it.
        \value UsesIcu Accepted for compatibility; Nt does not use ICU.
    */
    enum class Flag { Default = 0, Stateless = 0x1, ConvertInvalidToNull = 0x2, WriteBom = 0x4, ConvertInitialBom = 0x8, UsesIcu = 0x10 };
    Q_DECLARE_FLAGS(Flags, Flag)
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QStringConverterBase::Flags)

/*!
    \class QStringConverter
    \since 6.0
    \brief The QStringConverter class provides a base class for encoding and decoding text.

    QStringConverter holds the encoding and the conversion state shared by
    QStringEncoder and QStringDecoder, and the static functions that look up
    encodings by name or detect them from data.

    \differences
    Only the encodings of the Encoding enum are supported; there is no ICU
    backend, so names such as \c{"Shift_JIS"} give an invalid converter. The
    System encoding is UTF-8. Utf16 and Utf32 without a byte order mark use
    the host byte order. Converters can be moved but not copied.
    \enddifferences

    \sa QStringEncoder, QStringDecoder
*/
class QStringConverter : public QStringConverterBase {
public:
    /*!
        \enum QStringConverter::Encoding

        \value Utf8 UTF-8.
        \value Utf16 UTF-16. Decoding detects the byte order from a byte order
               mark and otherwise uses the host order; encoding uses the host
               order.
        \value Utf16LE UTF-16, little endian.
        \value Utf16BE UTF-16, big endian.
        \value Utf32 UTF-32, with the byte order handled like Utf16.
        \value Utf32LE UTF-32, little endian.
        \value Utf32BE UTF-32, big endian.
        \value Latin1 ISO-8859-1. Characters above U+00FF are encoded as \c{'?'}.
        \value System The encoding of the system, which is UTF-8 in Nt.
        \omitvalue LastEncoding
    */
    enum Encoding { Utf8, Utf16, Utf16LE, Utf16BE, Utf32, Utf32LE, Utf32BE, Latin1, System, LastEncoding = System };

    QStringConverter(const QStringConverter &) = delete;
    QStringConverter &operator=(const QStringConverter &) = delete;
    /*!
        Move-constructs a converter from \a other.
    */
    QStringConverter(QStringConverter &&other) noexcept = default;
    /*!
        Move-assigns \a other to this converter and returns a reference to it.
    */
    QStringConverter &operator=(QStringConverter &&other) noexcept = default;

    /*!
        Returns \c true if the converter has a supported encoding; otherwise
        returns \c false.
    */
    bool isValid() const noexcept { return m_valid; }
    void resetState() noexcept;
    /*!
        Returns \c true if invalid input was seen since the converter was
        created or resetState() was called; otherwise returns \c false.
    */
    bool hasError() const noexcept { return m_invalid > 0; }
    const char *name() const noexcept;

    static std::optional<Encoding> encodingForName(QAnyStringView name) noexcept;
    static const char *nameForEncoding(Encoding e) noexcept;
    static std::optional<Encoding> encodingForData(QByteArrayView data, char16_t expectedFirstCharacter = 0) noexcept;
    static std::optional<Encoding> encodingForHtml(QByteArrayView data);
    static QStringList availableCodecs();

protected:
    /*!
        Constructs an invalid converter.
    */
    QStringConverter() noexcept = default;
    /*!
        Constructs a converter for \a encoding with the flags \a f.
    */
    QStringConverter(Encoding encoding, Flags f) noexcept : m_encoding(encoding == System ? Utf8 : encoding), m_flags(f), m_valid(true), m_system(encoding == System) {}
    QStringConverter(QAnyStringView name, Flags f) noexcept;
    /*!
        Destroys the converter.
    */
    ~QStringConverter() = default;

    /*! \internal */
    Encoding m_encoding = Utf8;
    /*! \internal */
    Flags m_flags;
    /*! \internal */
    bool m_valid = false;
    /*! \internal */
    bool m_system = false;
    /*! \internal Set once the start of the data (and its byte order mark) has been handled. */
    bool m_started = false;
    /*! \internal Byte order chosen by a Utf16 or Utf32 decoder: 0 unknown, 1 little, 2 big endian. */
    int m_order = 0;
    /*! \internal Number of invalid sequences seen. */
    qsizetype m_invalid = 0;
    /*! \internal Bytes of an incomplete sequence kept by a decoder, or the pending high surrogate of an encoder. */
    unsigned char m_pending[4] = {0, 0, 0, 0};
    /*! \internal */
    int m_pendingCount = 0;
};

/*!
    \class QStringEncoder
    \since 6.0
    \brief The QStringEncoder class encodes QString data into bytes.

    \code
    QStringEncoder toUtf16(QStringConverter::Utf16LE);
    QByteArray bytes = toUtf16(u"text");
    \endcode

    A stateful encoder keeps a high surrogate that ends one call for the
    next, so text can be encoded in pieces.

    \differences
    operator()() and encode() return a QByteArray directly instead of a
    lazily converted \c EncodedData object.
    \enddifferences

    \sa QStringDecoder, QStringConverter
*/
class QStringEncoder : public QStringConverter {
public:
    /*!
        Constructs an invalid encoder.
    */
    QStringEncoder() noexcept = default;
    /*!
        Constructs an encoder for \a encoding with the flags \a flags.
    */
    explicit QStringEncoder(Encoding encoding, Flags flags = Flag::Default) noexcept : QStringConverter(encoding, flags) {}
    /*!
        Constructs an encoder for the encoding called \a name with the flags
        \a flags. The encoder is invalid if the name is not supported.

        \sa encodingForName()
    */
    explicit QStringEncoder(QAnyStringView name, Flags flags = Flag::Default) noexcept : QStringConverter(name, flags) {}

    /*!
        Encodes \a in and returns the bytes. Same as encode().
    */
    QByteArray operator()(QStringView in) { return encode(in); }
    /*!
        \overload
        Encodes \a in and returns the bytes.
    */
    QByteArray operator()(const QString &in) { return encode(QStringView(in)); }
    QByteArray encode(QStringView in);
    qsizetype requiredSpace(qsizetype inputLength) const noexcept;
    char *appendToBuffer(char *out, QStringView in);
};

/*!
    \class QStringDecoder
    \since 6.0
    \brief The QStringDecoder class decodes bytes into a QString.

    \code
    QStringDecoder fromUtf8(QStringConverter::Utf8);
    QString text = fromUtf8(chunk1);
    text += fromUtf8(chunk2); // a sequence split between chunks is joined
    \endcode

    \differences
    operator()() and decode() return a QString directly instead of a lazily
    converted \c DecodedData object.
    \enddifferences

    \sa QStringEncoder, QStringConverter
*/
class QStringDecoder : public QStringConverter {
public:
    /*!
        Constructs an invalid decoder.
    */
    QStringDecoder() noexcept = default;
    /*!
        Constructs a decoder for \a encoding with the flags \a flags.
    */
    explicit QStringDecoder(Encoding encoding, Flags flags = Flag::Default) noexcept : QStringConverter(encoding, flags) {}
    /*!
        Constructs a decoder for the encoding called \a name with the flags
        \a flags. The decoder is invalid if the name is not supported.

        \sa encodingForName()
    */
    explicit QStringDecoder(QAnyStringView name, Flags flags = Flag::Default) noexcept : QStringConverter(name, flags) {}

    /*!
        Decodes \a ba and returns the text. Same as decode().
    */
    QString operator()(QByteArrayView ba) { return decode(ba); }
    /*!
        \overload
        Decodes \a ba and returns the text.
    */
    QString operator()(const QByteArray &ba) { return decode(QByteArrayView(ba)); }
    QString decode(QByteArrayView ba);
    qsizetype requiredSpace(qsizetype inputLength) const noexcept;
    QChar *appendToBuffer(QChar *out, QByteArrayView ba);
    /*!
        \overload
        Decodes \a ba into the buffer \a out and returns a pointer just past
        the last character written.
    */
    char16_t *appendToBuffer(char16_t *out, QByteArrayView ba) { return reinterpret_cast<char16_t *>(appendToBuffer(reinterpret_cast<QChar *>(out), ba)); }
    static QStringDecoder decoderForHtml(QByteArrayView data);
};
