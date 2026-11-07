// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>

#include <memory>
#include <string>
#include <vector>

struct QJsonArrayPrivate {
    std::vector<QJsonValue> items;
};

struct QJsonObjectPrivate {
    struct Entry {
        QString key;
        QJsonValue value;
    };
    // Sorted by key, without duplicates.
    std::vector<Entry> items;
};

// The classes keep their representation private; this is the one place that reaches into all of them.
class QJsonPrivateAccess {
public:
    static const std::shared_ptr<QJsonArrayPrivate> &data(const QJsonArray &a) { return a.d; }
    static const std::shared_ptr<QJsonObjectPrivate> &data(const QJsonObject &o) { return o.d; }
    static QJsonArray arrayFromData(std::shared_ptr<QJsonArrayPrivate> d);
    static QJsonObject objectFromData(std::shared_ptr<QJsonObjectPrivate> d);
    static QJsonArray makeArray(std::vector<QJsonValue> &&items);
    static QJsonObject makeObject(std::vector<QJsonObjectPrivate::Entry> &&entries);
    // Takes entries in any order and keeps the last one of each key.
    static QJsonObject makeObjectSorting(std::vector<QJsonObjectPrivate::Entry> &&entries);
    static void setObjectValue(QJsonObject &o, qsizetype index, const QJsonValue &v);

    static const std::shared_ptr<void> &payload(const QJsonValue &v) { return v.m_data; }
    static bool isInteger(const QJsonValue &v) { return v.m_type == QJsonValue::Double && v.m_isInt; }
    static qint64 integer(const QJsonValue &v) { return v.m_int; }
    static double number(const QJsonValue &v) { return v.m_isInt ? static_cast<double>(v.m_int) : v.m_double; }
    static bool boolean(const QJsonValue &v) { return v.m_bool; }
    static const QString &string(const QJsonValue &v);
    static QJsonValue fromArrayData(std::shared_ptr<QJsonArrayPrivate> d);
    static QJsonValue fromObjectData(std::shared_ptr<QJsonObjectPrivate> d);
};

namespace QJsonPrivate {

// Parses a document and returns its root, an array or an object. On failure returns a Null value and sets
// *error when it is not null.
QJsonValue parse(const QByteArray &json, QJsonParseError *error);

// Writes the value as JSON text without a trailing line feed.
std::string write(const QJsonValue &root, bool compact);

} // namespace QJsonPrivate
