// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>
#include <QtCore/qstring.h>

class QDebug;
class QVariant;

struct QJsonParseError {
    enum ParseError {
        NoError = 0,
        UnterminatedObject,
        MissingNameSeparator,
        UnterminatedArray,
        MissingValueSeparator,
        IllegalValue,
        TerminationByNumber,
        IllegalNumber,
        IllegalEscapeSequence,
        IllegalUTF8String,
        UnterminatedString,
        MissingObject,
        DeepNesting,
        DocumentTooLarge,
        GarbageAtEnd
    };

    QString errorString() const;

    int offset = 0;
    ParseError error = NoError;
};

class QJsonDocument {
public:
    enum JsonFormat { Indented, Compact };

    QJsonDocument();
    explicit QJsonDocument(const QJsonObject &object);
    explicit QJsonDocument(const QJsonArray &array);
    ~QJsonDocument();
    QJsonDocument(const QJsonDocument &other);
    QJsonDocument(QJsonDocument &&other) noexcept;
    QJsonDocument &operator=(const QJsonDocument &other);
    QJsonDocument &operator=(QJsonDocument &&other) noexcept;

    void swap(QJsonDocument &other) noexcept;

    static QJsonDocument fromVariant(const QVariant &variant);
    QVariant toVariant() const;

    static QJsonDocument fromJson(const QByteArray &json, QJsonParseError *error = nullptr);
    QByteArray toJson(JsonFormat format = Indented) const;

    bool isEmpty() const;
    bool isNull() const;
    bool isArray() const;
    bool isObject() const;

    QJsonObject object() const;
    QJsonArray array() const;
    void setObject(const QJsonObject &object);
    void setArray(const QJsonArray &array);

    QJsonValue operator[](const QString &key) const;
    QJsonValue operator[](QStringView key) const;
    QJsonValue operator[](QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>>
    QJsonValue operator[](K &&key) const {
        return (*this)[QLatin1StringView(key)];
    }
    QJsonValue operator[](qsizetype i) const;

    friend bool operator==(const QJsonDocument &a, const QJsonDocument &b) { return a.equals(b); }
    friend bool operator!=(const QJsonDocument &a, const QJsonDocument &b) { return !a.equals(b); }

private:
    bool equals(const QJsonDocument &other) const;

    // A document holds an array or an object; Null means the document is null.
    QJsonValue m_root;
};

QDebug operator<<(QDebug dbg, const QJsonDocument &doc);
