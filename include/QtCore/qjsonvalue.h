// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <memory>
#include <type_traits>

class QDebug;
class QJsonArray;
class QJsonObject;
class QJsonValueConstRef;
class QJsonValueRef;
struct QJsonArrayPrivate;
struct QJsonObjectPrivate;
class QJsonPrivateAccess;

namespace QtPrivate {
// Selects the overloads that take a C string, so that a literal key is not ambiguous between QString and
// QLatin1StringView, and so that the literal 0 still means an index.
template <typename T>
inline constexpr bool IsJsonCString =
    std::is_same_v<std::decay_t<T>, const char *> || std::is_same_v<std::decay_t<T>, char *>;
} // namespace QtPrivate

// What operator-> of the iterators returns: they have no element to point at, only a reference proxy.
template <typename Ref> class QJsonRefArrow {
public:
    explicit QJsonRefArrow(const Ref &r) : m_ref(r) {}
    Ref *operator->() const { return &m_ref; }

private:
    mutable Ref m_ref;
};

class QJsonValue {
public:
    enum Type { Null = 0x0, Bool = 0x1, Double = 0x2, String = 0x3, Array = 0x4, Object = 0x5, Undefined = 0x80 };

    QJsonValue(Type type = Null);
    QJsonValue(bool b);
    QJsonValue(double n);
    QJsonValue(int n);
    QJsonValue(qint64 v);
    QJsonValue(const QString &s);
    QJsonValue(QLatin1StringView s);
    QJsonValue(const char *s);
    QJsonValue(const QJsonArray &a);
    QJsonValue(const QJsonObject &o);
    QJsonValue(const QJsonValueConstRef &ref);
    QJsonValue(const QJsonValue &other) = default;
    QJsonValue(QJsonValue &&other) noexcept;
    ~QJsonValue() = default;
    QJsonValue &operator=(const QJsonValue &other) = default;
    QJsonValue &operator=(QJsonValue &&other) noexcept;

    void swap(QJsonValue &other) noexcept;

    Type type() const { return m_type; }
    bool isNull() const { return m_type == Null; }
    bool isBool() const { return m_type == Bool; }
    bool isDouble() const { return m_type == Double; }
    bool isString() const { return m_type == String; }
    bool isArray() const { return m_type == Array; }
    bool isObject() const { return m_type == Object; }
    bool isUndefined() const { return m_type == Undefined; }

    bool toBool(bool defaultValue = false) const;
    int toInt(int defaultValue = 0) const;
    qint64 toInteger(qint64 defaultValue = 0) const;
    double toDouble(double defaultValue = 0) const;
    QString toString() const;
    QString toString(const QString &defaultValue) const;
    QJsonArray toArray() const;
    QJsonArray toArray(const QJsonArray &defaultValue) const;
    QJsonObject toObject() const;
    QJsonObject toObject(const QJsonObject &defaultValue) const;

    QJsonValue operator[](const QString &key) const;
    QJsonValue operator[](QStringView key) const;
    QJsonValue operator[](QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>>
    QJsonValue operator[](K &&key) const {
        return (*this)[QLatin1StringView(key)];
    }
    QJsonValue operator[](qsizetype i) const;

    friend bool operator==(const QJsonValue &a, const QJsonValue &b) { return a.equals(b); }
    friend bool operator!=(const QJsonValue &a, const QJsonValue &b) { return !a.equals(b); }

private:
    friend class QJsonPrivateAccess;
    bool equals(const QJsonValue &other) const;

    Type m_type = Null;
    bool m_isInt = false;
    union {
        bool m_bool;
        double m_double;
        qint64 m_int = 0;
    };
    // The QString (String), QJsonArrayPrivate (Array) or QJsonObjectPrivate (Object); null for an empty container.
    std::shared_ptr<void> m_data;
};

class QJsonValueConstRef {
public:
    QJsonValueConstRef(const QJsonValueConstRef &) = default;
    QJsonValueConstRef &operator=(const QJsonValueConstRef &) = delete;

    QJsonValue::Type type() const;
    bool isNull() const { return type() == QJsonValue::Null; }
    bool isBool() const { return type() == QJsonValue::Bool; }
    bool isDouble() const { return type() == QJsonValue::Double; }
    bool isString() const { return type() == QJsonValue::String; }
    bool isArray() const { return type() == QJsonValue::Array; }
    bool isObject() const { return type() == QJsonValue::Object; }
    bool isUndefined() const { return type() == QJsonValue::Undefined; }

    bool toBool(bool defaultValue = false) const;
    int toInt(int defaultValue = 0) const;
    qint64 toInteger(qint64 defaultValue = 0) const;
    double toDouble(double defaultValue = 0) const;
    QString toString() const;
    QString toString(const QString &defaultValue) const;
    QJsonArray toArray() const;
    QJsonArray toArray(const QJsonArray &defaultValue) const;
    QJsonObject toObject() const;
    QJsonObject toObject(const QJsonObject &defaultValue) const;

    QJsonValue operator[](const QString &key) const;
    QJsonValue operator[](QStringView key) const;
    QJsonValue operator[](QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>>
    QJsonValue operator[](K &&key) const {
        return (*this)[QLatin1StringView(key)];
    }
    QJsonValue operator[](qsizetype i) const;

    friend bool operator==(const QJsonValueConstRef &a, const QJsonValueConstRef &b) {
        return QJsonValue(a) == QJsonValue(b);
    }
    friend bool operator!=(const QJsonValueConstRef &a, const QJsonValueConstRef &b) { return !(a == b); }
    friend bool operator==(const QJsonValueConstRef &a, const QJsonValue &b) { return QJsonValue(a) == b; }
    friend bool operator!=(const QJsonValueConstRef &a, const QJsonValue &b) { return !(QJsonValue(a) == b); }
    friend bool operator==(const QJsonValue &a, const QJsonValueConstRef &b) { return a == QJsonValue(b); }
    friend bool operator!=(const QJsonValue &a, const QJsonValueConstRef &b) { return !(a == QJsonValue(b)); }

private:
    friend class QJsonValue;
    friend class QJsonArray;
    friend class QJsonObject;
    friend class QJsonValueRef;
    QJsonValueConstRef(const QJsonArray *array, qsizetype index) : m_array(array), m_index(index) {}
    QJsonValueConstRef(const QJsonObject *object, qsizetype index) : m_object(object), m_index(index) {}
    QJsonValue concrete() const;

    // Exactly one of the two is set, which is how an element knows what it refers to.
    const QJsonArray *m_array = nullptr;
    const QJsonObject *m_object = nullptr;
    qsizetype m_index;
};

class QJsonValueRef : public QJsonValueConstRef {
public:
    QJsonValueRef(QJsonArray *array, qsizetype idx) : QJsonValueConstRef(array, idx) {}
    QJsonValueRef(QJsonObject *object, qsizetype idx) : QJsonValueConstRef(object, idx) {}
    QJsonValueRef(const QJsonValueRef &) = default;

    QJsonValueRef &operator=(const QJsonValue &val);
    QJsonValueRef &operator=(const QJsonValueRef &val);
};

QDebug operator<<(QDebug dbg, const QJsonValue &value);

inline void QJsonValue::swap(QJsonValue &other) noexcept {
    QJsonValue tmp(std::move(other));
    other = std::move(*this);
    *this = std::move(tmp);
}
