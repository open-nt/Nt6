// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qbytearrayview.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <optional>

/*!
    \class QStringConverterBase
    \brief The QStringConverterBase class holds the flags and the state shared by QStringEncoder and QStringDecoder.

    The class only defines the Flag enumeration and the State structure that
    QStringConverter, QStringEncoder and QStringDecoder use. It is not
    instantiated on its own.

    \differences
    The members of State that Qt reserves for ICU are not present, and
    \c UsesIcu is accepted but never reported because Nt does not use ICU.
    \enddifferences

    \sa QStringConverter, QStringEncoder, QStringDecoder
*/
class QStringConverterBase {
public:
    /*!
        \enum QStringConverterBase::Flag

        This enum selects how a converter treats its input and output. The
        Flags type is a typedef for \c{QFlags<Flag>}.

        \value Default The default behaviour: the converter keeps state between calls, a leading byte order mark is consumed when decoding and none is written when encoding.
        \value Stateless Every call starts from the initial state and nothing is kept for the next call. An incomplete sequence at the end of the input counts as an invalid one.
        \value ConvertInitialBom When decoding, an initial byte order mark is converted to U+FEFF instead of being skipped.
        \value WriteBom When encoding, a byte order mark is written before the first character of a UTF encoding.
        \value ConvertInvalidToNull Invalid input is converted to a null character instead of U+FFFD when decoding and to a null byte instead of \c{'?'} when encoding to an 8-bit encoding.
        \value UsesIcu Accepted for compatibility with Qt; it has no effect because Nt does not use ICU.
    */
    enum class Flag {
        Default = 0x0,
        Stateless = 0x1,
        ConvertInitialBom = 0x2,
        WriteBom = 0x4,
        ConvertInvalidToNull = 0x8,
        UsesIcu = 0x10
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    /*!
        \struct QStringConverterBase::State

        The State structure holds what a converter remembers between calls:
        the flags, how many invalid characters were met, and the bytes or
        surrogates of an incomplete sequence at the end of the previous chunk.
    */
    struct State {
        /*!
            Constructs a state with the given \a f flags.
        */
        constexpr State(Flags f = Flag::Default) noexcept : flags(f) {}
        /*!
            Destroys the state.
        */
        ~State() = default;
        /*!
            Constructs a state by moving \a other, which is reset.
        */
        State(State &&other) noexcept { *this = std::move(other); }
        /*!
            Moves \a other into this state; \a other is reset. Returns a
            reference to this state.
        */
        State &operator=(State &&other) noexcept {
            if (this != &other) {
                flags = other.flags;
                internalState = other.internalState;
                remainingChars = other.remainingChars;
                invalidChars = other.invalidChars;
                for (int i = 0; i < 4; ++i)
                    state_data[i] = other.state_data[i];
                other.clear();
            }
            return *this;
        }
        State(const State &) = delete;
        State &operator=(const State &) = delete;
        /*!
            Resets the state to its initial value, keeping the flags.
        */
        void clear() noexcept {
            internalState = 0;
            remainingChars = 0;
            invalidChars = 0;
            for (unsigned &v : state_data)
                v = 0;
        }
        /*!
            Resets the state to its initial value, keeping the flags. This is
            the same as clear().
        */
        void reset() noexcept { clear(); }

        /*!
            The flags the converter was created with.
        */
        Flags flags;
        /*!
            Converter private data: which header work is done and, for UTF-16
            and UTF-32, the byte order that was detected.
        */
        int internalState = 0;
        /*!
            The number of bytes (when decoding) or UTF-16 code units (when
            encoding) kept from the previous call because they start a
            sequence that is not complete yet.
        */
        qsizetype remainingChars = 0;
        /*!
            The number of invalid characters or sequences met since the last
            reset.
        */
        qsizetype invalidChars = 0;
        /*!
            Converter private storage for the incomplete sequence.
        */
        unsigned state_data[4] = {0, 0, 0, 0};
    };
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QStringConverterBase::Flags)

/*!
    \class QStringConverter
    \brief The QStringConverter class is the base class of QStringEncoder and QStringDecoder.

    A converter is created for one encoding and keeps the state needed to
    convert a text that arrives in several chunks. Use QStringDecoder to turn
    bytes into a QString and QStringEncoder to turn a QString into bytes.

    The encodings named by the Encoding enumeration are always available. Other
    encodings can be selected by name, for example
    \c{QStringDecoder("ISO-8859-2")}: the ISO 8859 parts 2 to 11 and 13 to 16,
    the Windows code pages 1250 to 1258, KOI8-R, KOI8-U, IBM437, IBM850, IBM866
    and Mac Roman, ASCII and ISO-8859-1. Use isValid() to find out whether a
    name was understood.

    \differences
    Encodings other than those of the Encoding enumeration are limited to the
    single-byte code pages listed above, which come from built-in tables. There
    is no ICU or iconv back end, so multibyte legacy encodings (Shift JIS, GBK,
    EUC-KR, Big5 and the like) and \c{"Locale"} other than UTF-8 are not
    available, and the \c UsesIcu flag has no effect. encodingForName() returns
    an empty value for names that are not in the Encoding enumeration. An
    invalid UTF-8, UTF-16 or UTF-32 sequence is replaced by one U+FFFD for each
    maximal invalid subpart, as the Unicode standard recommends. Characters
    that cannot be encoded become \c{'?'}, or \c{0xEF 0xBF 0xBD} in UTF-8.
    \enddifferences

    \sa QStringEncoder, QStringDecoder
*/
class QStringConverter : public QStringConverterBase {
public:
    /*!
        \enum QStringConverter::Encoding

        This enum names the encodings every converter supports.

        \value Utf8 UTF-8.
        \value Utf16 UTF-16 in the byte order of the system. When decoding, a byte order mark selects the byte order.
        \value Utf16LE UTF-16, little endian.
        \value Utf16BE UTF-16, big endian.
        \value Utf32 UTF-32 in the byte order of the system. When decoding, a byte order mark selects the byte order.
        \value Utf32LE UTF-32, little endian.
        \value Utf32BE UTF-32, big endian.
        \value Latin1 ISO 8859-1.
        \value System The encoding of the system, which is UTF-8 in Nt.
    */
    enum Encoding {
        Utf8,
        Utf16,
        Utf16LE,
        Utf16BE,
        Utf32,
        Utf32LE,
        Utf32BE,
        Latin1,
        System
    };

    /*!
        Returns \c true if the converter has an encoding it can use; returns
        \c false if it was default-constructed or created with a name that is
        not known.

        \sa name()
    */
    bool isValid() const noexcept { return iface != nullptr; }
    /*!
        Resets the state of the converter to its initial state. Any partial
        input kept from earlier calls is dropped and hasError() becomes
        \c false.

        \sa hasError()
    */
    void resetState() noexcept { state.clear(); }
    /*!
        Returns \c true if an invalid character or sequence was met since the
        converter was created or last reset.

        \sa resetState()
    */
    bool hasError() const noexcept { return state.invalidChars != 0; }
    const char *name() const noexcept;

    static std::optional<Encoding> encodingForName(const char *name, qsizetype nameLength = -1) noexcept;
    static const char *nameForEncoding(Encoding e) noexcept;
    static std::optional<Encoding> encodingForData(QByteArrayView data, char16_t expectedFirstCharacter = 0) noexcept;
    static std::optional<Encoding> encodingForHtml(QByteArrayView data) noexcept;

    /*!
        \internal
    */
    struct Interface;

protected:
    QStringConverter() noexcept = default;
    QStringConverter(Encoding encoding, Flags f) noexcept;
    QStringConverter(const char *name, Flags f) noexcept;
    ~QStringConverter() = default;
    QStringConverter(QStringConverter &&) noexcept = default;
    QStringConverter &operator=(QStringConverter &&) noexcept = default;

    const Interface *iface = nullptr;
    State state;
};

/*!
    \class QStringEncoder
    \brief The QStringEncoder class converts a QString to a byte array in a given encoding.

    Create an encoder for an encoding and call it with a string. The result
    converts to a QByteArray:

    \code
    QStringEncoder toLatin1(QStringConverter::Latin1);
    QByteArray bytes = toLatin1(u"caf\u00e9");
    \endcode

    The same encoder can be called with consecutive chunks of one text, and
    for the UTF encodings a surrogate pair that is split between two chunks is
    encoded correctly. Characters that the encoding cannot represent are
    replaced and make hasError() return \c true.

    \differences
    See QStringConverter.
    \enddifferences

    \sa QStringDecoder, QStringConverter
*/
class QStringEncoder : public QStringConverter {
    template <typename View>
    struct EncodedData {
        QStringEncoder *encoder;
        View data;
        operator QByteArray() const {
            if (data.isNull())
                return QByteArray();
            QByteArray ba(encoder->requiredSpace(data.size()), '\0');
            char *begin = ba.data();
            char *end = encoder->appendToBuffer(begin, data);
            ba.truncate(end - begin);
            return ba;
        }
    };

public:
    /*!
        Constructs an invalid encoder. isValid() returns \c false.
    */
    QStringEncoder() = default;
    /*!
        Constructs an encoder for \a encoding with the given flags \a f.
    */
    QStringEncoder(Encoding encoding, Flags f = Flag::Default) noexcept : QStringConverter(encoding, f) {}
    /*!
        \overload

        Constructs an encoder for the encoding called \a name, with the flags
        \a f. The encoder is invalid if \a name is not a known encoding name.

        \sa isValid()
    */
    QStringEncoder(const char *name, Flags f = Flag::Default) noexcept : QStringConverter(name, f) {}

    /*!
        Encodes \a in and returns an object that converts to a QByteArray. The
        encoder must outlive the returned object, and so must the data of
        \a in.
    */
    EncodedData<QStringView> operator()(QStringView in) { return EncodedData<QStringView>{this, in}; }
    /*!
        \overload

        Encodes the \a length characters starting at \a in.
    */
    EncodedData<QStringView> operator()(const QChar *in, qsizetype length) { return EncodedData<QStringView>{this, QStringView(in, length)}; }
    /*!
        Returns the number of bytes that appendToBuffer() can write for
        \a inputLength UTF-16 code units, an upper bound that includes a byte
        order mark and a surrogate kept from the previous call.

        \sa appendToBuffer()
    */
    qsizetype requiredSpace(qsizetype inputLength) const noexcept;
    char *appendToBuffer(char *out, QStringView in);
};

/*!
    \class QStringDecoder
    \brief The QStringDecoder class converts a byte array in a given encoding to a QString.

    Create a decoder for an encoding and call it with bytes. The result
    converts to a QString:

    \code
    QStringDecoder fromUtf8(QStringConverter::Utf8);
    QString text = fromUtf8("caf\xc3\xa9");
    \endcode

    The same decoder can be called with consecutive chunks of one text. A
    character whose bytes are split between two chunks is decoded when the
    second chunk arrives, unless the Stateless flag is set. Invalid input is
    replaced by U+FFFD and makes hasError() return \c true.

    \differences
    See QStringConverter.
    \enddifferences

    \sa QStringEncoder, QStringConverter
*/
class QStringDecoder : public QStringConverter {
    template <typename View>
    struct DecodedData {
        QStringDecoder *decoder;
        View data;
        operator QString() const {
            if (data.isNull())
                return QString();
            QString s(decoder->requiredSpace(data.size()), QChar());
            QChar *begin = s.data();
            QChar *end = decoder->appendToBuffer(begin, data);
            s.truncate(end - begin);
            return s;
        }
    };

public:
    /*!
        Constructs an invalid decoder. isValid() returns \c false.
    */
    QStringDecoder() = default;
    /*!
        Constructs a decoder for \a encoding with the given flags \a f.
    */
    QStringDecoder(Encoding encoding, Flags f = Flag::Default) noexcept : QStringConverter(encoding, f) {}
    /*!
        \overload

        Constructs a decoder for the encoding called \a name, with the flags
        \a f. The decoder is invalid if \a name is not a known encoding name.

        \sa isValid()
    */
    QStringDecoder(const char *name, Flags f = Flag::Default) noexcept : QStringConverter(name, f) {}

    /*!
        Decodes \a ba and returns an object that converts to a QString. The
        decoder must outlive the returned object, and so must the data of
        \a ba.
    */
    DecodedData<QByteArrayView> operator()(QByteArrayView ba) { return DecodedData<QByteArrayView>{this, ba}; }
    /*!
        \overload

        Decodes the \a length bytes starting at \a chars.
    */
    DecodedData<QByteArrayView> operator()(const char *chars, qsizetype length) { return DecodedData<QByteArrayView>{this, QByteArrayView(chars, length)}; }
    /*!
        Returns the number of UTF-16 code units that appendToBuffer() can
        write for \a inputLength bytes, an upper bound that includes bytes kept
        from the previous call.

        \sa appendToBuffer()
    */
    qsizetype requiredSpace(qsizetype inputLength) const noexcept;
    QChar *appendToBuffer(QChar *out, QByteArrayView ba);
};
