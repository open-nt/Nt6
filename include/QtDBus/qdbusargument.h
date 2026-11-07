// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtDBus/qdbusextratypes.h>
#include <QtDBus/qdbusunixfiledescriptor.h>

#include <memory>
#include <tuple>
#include <utility>

class QDBusArgumentPrivate;

class QDBusArgument {
public:
    enum ElementType { BasicType, VariantType, ArrayType, StructureType, MapType, MapEntryType, UnknownType = -1 };

    QDBusArgument();
    QDBusArgument(const QDBusArgument &other);
    QDBusArgument(QDBusArgument &&other) noexcept;
    QDBusArgument &operator=(const QDBusArgument &other);
    QDBusArgument &operator=(QDBusArgument &&other) noexcept;
    ~QDBusArgument();
    void swap(QDBusArgument &other) noexcept;

    QDBusArgument &operator<<(uchar arg);
    QDBusArgument &operator<<(bool arg);
    QDBusArgument &operator<<(short arg);
    QDBusArgument &operator<<(ushort arg);
    QDBusArgument &operator<<(int arg);
    QDBusArgument &operator<<(uint arg);
    QDBusArgument &operator<<(qlonglong arg);
    QDBusArgument &operator<<(qulonglong arg);
    QDBusArgument &operator<<(long arg);
    QDBusArgument &operator<<(unsigned long arg);
    QDBusArgument &operator<<(double arg);
    QDBusArgument &operator<<(const QString &arg);
    QDBusArgument &operator<<(const QDBusVariant &arg);
    QDBusArgument &operator<<(const QDBusObjectPath &arg);
    QDBusArgument &operator<<(const QDBusSignature &arg);
    QDBusArgument &operator<<(const QDBusUnixFileDescriptor &arg);
    QDBusArgument &operator<<(const QStringList &arg);
    QDBusArgument &operator<<(const QByteArray &arg);

    void beginStructure();
    void endStructure();
    void beginArray(QMetaType elementMetaType);
    /*!
        \overload
        Opens an array whose elements have the meta-type id \a elementMetaTypeId.
    */
    void beginArray(int elementMetaTypeId) { beginArray(QMetaType(elementMetaTypeId)); }
    void endArray();
    void beginMap(QMetaType keyMetaType, QMetaType valueMetaType);
    /*!
        \overload
        Opens a map whose keys and values have the meta-type ids \a keyMetaTypeId and
        \a valueMetaTypeId.
    */
    void beginMap(int keyMetaTypeId, int valueMetaTypeId) {
        beginMap(QMetaType(keyMetaTypeId), QMetaType(valueMetaTypeId));
    }
    void endMap();
    void beginMapEntry();
    void endMapEntry();
    void appendVariant(const QVariant &value);

    QString currentSignature() const;
    ElementType currentType() const;

    const QDBusArgument &operator>>(uchar &arg) const;
    const QDBusArgument &operator>>(bool &arg) const;
    const QDBusArgument &operator>>(short &arg) const;
    const QDBusArgument &operator>>(ushort &arg) const;
    const QDBusArgument &operator>>(int &arg) const;
    const QDBusArgument &operator>>(uint &arg) const;
    const QDBusArgument &operator>>(qlonglong &arg) const;
    const QDBusArgument &operator>>(qulonglong &arg) const;
    const QDBusArgument &operator>>(long &arg) const;
    const QDBusArgument &operator>>(unsigned long &arg) const;
    const QDBusArgument &operator>>(double &arg) const;
    const QDBusArgument &operator>>(QString &arg) const;
    const QDBusArgument &operator>>(QDBusVariant &arg) const;
    const QDBusArgument &operator>>(QDBusObjectPath &arg) const;
    const QDBusArgument &operator>>(QDBusSignature &arg) const;
    const QDBusArgument &operator>>(QDBusUnixFileDescriptor &arg) const;
    const QDBusArgument &operator>>(QStringList &arg) const;
    const QDBusArgument &operator>>(QByteArray &arg) const;

    void beginStructure() const;
    void endStructure() const;
    void beginArray() const;
    void endArray() const;
    void beginMap() const;
    void endMap() const;
    void beginMapEntry() const;
    void endMapEntry() const;
    bool atEnd() const;
    QVariant asVariant() const;

private:
    friend class QDBusArgumentPrivate;
    explicit QDBusArgument(std::shared_ptr<QDBusArgumentPrivate> d);
    QDBusArgumentPrivate *reading() const;
    QDBusArgumentPrivate *writing() const;

    mutable std::shared_ptr<QDBusArgumentPrivate> d;
};

Q_DECLARE_METATYPE(QDBusArgument)

// Extracts a value of type T from arg and returns it.
template <typename T> T qdbus_cast(const QDBusArgument &arg) {
    T item;
    arg >> item;
    return item;
}

// Returns the value of v as a T. A QDBusArgument in v is demarshalled; any other value
// is converted with QVariant::value().
template <typename T> T qdbus_cast(const QVariant &v) {
    if (v.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<T>(v.value<QDBusArgument>());
    return v.value<T>();
}

QDBusArgument &operator<<(QDBusArgument &arg, const QVariant &value);
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariant &value);
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantList &list);
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantList &list);
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantMap &map);
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantMap &map);
QDBusArgument &operator<<(QDBusArgument &arg, const QVariantHash &map);
const QDBusArgument &operator>>(const QDBusArgument &arg, QVariantHash &map);

// Writes list to arg as an array.
template <typename T> QDBusArgument &operator<<(QDBusArgument &arg, const QList<T> &list) {
    arg.beginArray(QMetaType::fromType<T>());
    for (const T &item : list)
        arg << item;
    arg.endArray();
    return arg;
}

// Reads an array from arg into list.
template <typename T> const QDBusArgument &operator>>(const QDBusArgument &arg, QList<T> &list) {
    arg.beginArray();
    list.clear();
    while (!arg.atEnd()) {
        T item;
        arg >> item;
        list.append(std::move(item));
    }
    arg.endArray();
    return arg;
}

// Writes map to arg as a dictionary.
template <typename K, typename V> QDBusArgument &operator<<(QDBusArgument &arg, const QMap<K, V> &map) {
    arg.beginMap(QMetaType::fromType<K>(), QMetaType::fromType<V>());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << it.key() << it.value();
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Reads a dictionary from arg into map.
template <typename K, typename V> const QDBusArgument &operator>>(const QDBusArgument &arg, QMap<K, V> &map) {
    arg.beginMap();
    map.clear();
    while (!arg.atEnd()) {
        K key;
        V value;
        arg.beginMapEntry();
        arg >> key >> value;
        map.insert(key, value);
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Writes map to arg as a dictionary.
template <typename K, typename V> QDBusArgument &operator<<(QDBusArgument &arg, const QHash<K, V> &map) {
    arg.beginMap(QMetaType::fromType<K>(), QMetaType::fromType<V>());
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        arg.beginMapEntry();
        arg << it.key() << it.value();
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Reads a dictionary from arg into map.
template <typename K, typename V> const QDBusArgument &operator>>(const QDBusArgument &arg, QHash<K, V> &map) {
    arg.beginMap();
    map.clear();
    while (!arg.atEnd()) {
        K key;
        V value;
        arg.beginMapEntry();
        arg >> key >> value;
        map.insert(key, value);
        arg.endMapEntry();
    }
    arg.endMap();
    return arg;
}

// Writes pair to arg as a structure of two members.
template <typename A, typename B> QDBusArgument &operator<<(QDBusArgument &arg, const std::pair<A, B> &pair) {
    arg.beginStructure();
    arg << pair.first << pair.second;
    arg.endStructure();
    return arg;
}

// Reads a structure of two members from arg into pair.
template <typename A, typename B> const QDBusArgument &operator>>(const QDBusArgument &arg, std::pair<A, B> &pair) {
    arg.beginStructure();
    arg >> pair.first >> pair.second;
    arg.endStructure();
    return arg;
}

// Writes tuple to arg as a structure.
template <typename... T> QDBusArgument &operator<<(QDBusArgument &arg, const std::tuple<T...> &tuple) {
    arg.beginStructure();
    std::apply([&arg](const auto &...item) { (void)(arg << ... << item); }, tuple);
    arg.endStructure();
    return arg;
}

// Reads a structure from arg into tuple.
template <typename... T> const QDBusArgument &operator>>(const QDBusArgument &arg, std::tuple<T...> &tuple) {
    arg.beginStructure();
    std::apply([&arg](auto &...item) { (void)(arg >> ... >> item); }, tuple);
    arg.endStructure();
    return arg;
}
