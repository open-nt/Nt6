// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcborcommon.h>
#include <QtCore/qfloat16.h>
#include <QtCore/qstring.h>

#include <memory>

class QIODevice;

class QCborStreamReader {
public:
    /*!
        \enum QCborStreamReader::Type

        The types of item that the reader can be at. The values are those of the first byte of
        the item in CBOR.

        \value UnsignedInteger An integer from 0 to 2^64-1.
        \value NegativeInteger An integer from -1 to -2^64.
        \value ByteString A string of bytes.
        \value ByteArray The same as ByteString.
        \value TextString A string of text in UTF-8.
        \value String The same as TextString.
        \value Array An array.
        \value Map A map of keys to values.
        \value Tag A tag, which says how the item that follows is to be understood.
        \value SimpleType A simple type, among them \c false, \c true, \c null and \c undefined.
        \value HalfFloat A floating-point number of 16 bits.
        \value Float16 The same as HalfFloat.
        \value Float A floating-point number of 32 bits.
        \value Double A floating-point number of 64 bits.
        \value Invalid No item: the end of a container, the end of the data, or an error.
    */
    enum Type : quint8 {
        UnsignedInteger = 0x00,
        NegativeInteger = 0x20,
        ByteString = 0x40,
        ByteArray = ByteString,
        TextString = 0x60,
        String = TextString,
        Array = 0x80,
        Map = 0xa0,
        Tag = 0xc0,
        SimpleType = 0xe0,
        HalfFloat = 0xf9,
        Float16 = HalfFloat,
        Float = 0xfa,
        Double = 0xfb,
        Invalid = 0xff,
    };

    /*!
        \enum QCborStreamReader::StringResultCode

        The status of a chunk of a string.

        \value EndOfString The string has ended: there was no more data, and the reader has moved on to the next item.
        \value Ok A chunk was read.
        \value Error The chunk could not be read; see lastError().
    */
    enum StringResultCode { EndOfString = 0, Ok = 1, Error = -1 };

    /*!
        \class QCborStreamReader::StringResult
        \brief The StringResult class holds a chunk of a string that QCborStreamReader read.
        \since 5.12

        \differences
        None.
        \enddifferences
    */
    template <typename Container> struct StringResult {
        /*!
            The chunk that was read, if status is \c Ok.
        */
        Container data;
        /*!
            Whether a chunk was read, the string ended, or there was an error.
        */
        StringResultCode status = Error;
    };

    QCborStreamReader();
    QCborStreamReader(const char *data, qsizetype len);
    QCborStreamReader(const quint8 *data, qsizetype len);
    explicit QCborStreamReader(const QByteArray &data);
    explicit QCborStreamReader(QIODevice *device);
    ~QCborStreamReader();
    QCborStreamReader(const QCborStreamReader &) = delete;
    QCborStreamReader &operator=(const QCborStreamReader &) = delete;

    void setDevice(QIODevice *device);
    QIODevice *device() const;
    void addData(const QByteArray &data);
    void addData(const char *data, qsizetype len);
    void addData(const quint8 *data, qsizetype len);
    void reparse();
    void clear();
    void reset();

    QCborError lastError() const;
    qint64 currentOffset() const;

    bool isValid() const;
    int containerDepth() const;
    Type parentContainerType() const;
    bool hasNext() const noexcept;
    bool next(int maxRecursion = 10000);

    Type type() const;
    /*!
        Returns \c true if the current item is an unsigned integer.
    */
    bool isUnsignedInteger() const { return type() == UnsignedInteger; }
    /*!
        Returns \c true if the current item is a negative integer.
    */
    bool isNegativeInteger() const { return type() == NegativeInteger; }
    /*!
        Returns \c true if the current item is an integer, unsigned or negative.
    */
    bool isInteger() const { return isUnsignedInteger() || isNegativeInteger(); }
    /*!
        Returns \c true if the current item is a byte string.
    */
    bool isByteArray() const { return type() == ByteArray; }
    /*!
        Returns \c true if the current item is a text string.
    */
    bool isString() const { return type() == String; }
    /*!
        Returns \c true if the current item is an array.
    */
    bool isArray() const { return type() == Array; }
    /*!
        Returns \c true if the current item is a map.
    */
    bool isMap() const { return type() == Map; }
    /*!
        Returns \c true if the current item is a tag.
    */
    bool isTag() const { return type() == Tag; }
    /*!
        Returns \c true if the current item is a simple type, among them \c false, \c true,
        \c null and \c undefined.
    */
    bool isSimpleType() const { return type() == SimpleType; }
    /*!
        \overload
        Returns \c true if the current item is the simple type \a st.
    */
    bool isSimpleType(QCborSimpleType st) const { return isSimpleType() && toSimpleType() == st; }
    /*!
        Returns \c true if the current item is a floating-point number of 16 bits.
    */
    bool isFloat16() const { return type() == Float16; }
    /*!
        Returns \c true if the current item is a floating-point number of 32 bits.
    */
    bool isFloat() const { return type() == Float; }
    /*!
        Returns \c true if the current item is a floating-point number of 64 bits.
    */
    bool isDouble() const { return type() == Double; }
    /*!
        Returns \c true if the current item is the value \c false.
    */
    bool isFalse() const { return isSimpleType(QCborSimpleType::False); }
    /*!
        Returns \c true if the current item is the value \c true.
    */
    bool isTrue() const { return isSimpleType(QCborSimpleType::True); }
    /*!
        Returns \c true if the current item is \c false or \c true.
    */
    bool isBool() const { return isFalse() || isTrue(); }
    /*!
        Returns \c true if the current item is \c null.
    */
    bool isNull() const { return isSimpleType(QCborSimpleType::Null); }
    /*!
        Returns \c true if the current item is \c undefined.
    */
    bool isUndefined() const { return isSimpleType(QCborSimpleType::Undefined); }
    /*!
        Returns \c true if the current item is an array or a map.

        \sa enterContainer()
    */
    bool isContainer() const { return isArray() || isMap(); }

    bool isLengthKnown() const noexcept;
    quint64 length() const;

    bool enterContainer();
    bool leaveContainer();

    StringResult<QString> readString();
    StringResult<QByteArray> readUtf8String();
    StringResult<QByteArray> readByteArray();
    QString readAllString();
    QByteArray readAllUtf8String();
    QByteArray readAllByteArray();

    /*!
        Returns the value of the current \c true or \c false item.
    */
    bool toBool() const { return isTrue(); }
    QCborTag toTag() const;
    quint64 toUnsignedInteger() const;
    QCborNegativeInteger toNegativeInteger() const;
    QCborSimpleType toSimpleType() const;
    qfloat16 toFloat16() const;
    float toFloat() const;
    double toDouble() const;
    qint64 toInteger() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
