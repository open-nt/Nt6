// MIT License - see LICENSE
#include <QtCore/qjsonobject.h>

#include "qjson_p.h"

#include <algorithm>

namespace {
using Entry = QJsonObjectPrivate::Entry;

qsizetype lowerBound(const QJsonObjectPrivate *d, const QString &key) {
    if (!d)
        return 0;
    const auto it = std::lower_bound(d->items.begin(), d->items.end(), key,
                                     [](const Entry &e, const QString &k) { return e.key < k; });
    return static_cast<qsizetype>(it - d->items.begin());
}

// Returns the index of key, or -1.
qsizetype indexOfKey(const QJsonObjectPrivate *d, const QString &key) {
    if (!d)
        return -1;
    const qsizetype i = lowerBound(d, key);
    return i < static_cast<qsizetype>(d->items.size()) && d->items[static_cast<std::size_t>(i)].key == key ? i : -1;
}
} // namespace

QJsonObject QJsonPrivateAccess::objectFromData(std::shared_ptr<QJsonObjectPrivate> d) {
    QJsonObject o;
    o.d = std::move(d);
    return o;
}

QJsonObject QJsonPrivateAccess::makeObject(std::vector<Entry> &&entries) {
    QJsonObject o;
    if (!entries.empty()) {
        o.d = std::make_shared<QJsonObjectPrivate>();
        o.d->items = std::move(entries);
    }
    return o;
}

QJsonObject QJsonPrivateAccess::makeObjectSorting(std::vector<Entry> &&entries) {
    std::stable_sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) { return a.key < b.key; });
    // For equal keys the last one wins, as if each had been inserted in turn.
    std::vector<Entry> unique;
    unique.reserve(entries.size());
    for (Entry &e : entries) {
        if (!unique.empty() && unique.back().key == e.key)
            unique.back().value = std::move(e.value);
        else
            unique.push_back(std::move(e));
    }
    return makeObject(std::move(unique));
}

void QJsonPrivateAccess::setObjectValue(QJsonObject &o, qsizetype index, const QJsonValue &v) {
    if (index < 0 || index >= o.size())
        return;
    o.detached().items[static_cast<std::size_t>(index)].value = v;
}

/*!
    \class QJsonObject
    \brief The QJsonObject class encapsulates a JSON object.

    A JSON object is a list of key-value pairs, where the keys are unique
    strings and the values are QJsonValue. A QJsonObject can be converted to
    and from a QJsonDocument.

    \code
    QJsonObject o;
    o.insert("name", "Nt");
    o["version"] = 6;
    for (auto it = o.begin(); it != o.end(); ++it)
        use(it.key(), it.value());
    \endcode

    The keys are kept sorted, so keys(), the iterators and the JSON text a
    document writes all list them in that order, not in the order in which they
    were inserted. A key is a QString and is compared by UTF-16 code unit.

    QJsonObject is implicitly shared: copies are cheap and the data is copied
    when one of them is modified. It is reentrant, but one object must not be
    modified while another thread reads it or a copy that shares its data.

    Inserting or removing a key invalidates the iterators, because the
    entries after it move; so does anything else that changes the number of
    keys. Assigning through an iterator, or through the QJsonValueRef that
    the \c{operator[]} functions return, changes a value in place and does not.

    \differences
    There is no \c fromVariantMap(), \c fromVariantHash(), \c toVariantMap()
    or \c toVariantHash() until QVariant exists. The iterators hold the object
    and an index instead of a pointer, so a copy made while one is alive is
    harmless: writes through a non-const iterator go to the object it came
    from. Assigning an undefined value through \c{operator[]} or an iterator
    stores it, and it is written as null; only insert() removes the key.
    \enddifferences

    \sa QJsonValue, QJsonArray, QJsonDocument
*/

/*!
    \typedef QJsonObject::Iterator

    Qt-style synonym for QJsonObject::iterator.
*/

/*!
    \typedef QJsonObject::ConstIterator

    Qt-style synonym for QJsonObject::const_iterator.
*/

/*!
    \typedef QJsonObject::reverse_iterator

    A reverse iterator over the entries, for STL compatibility.
*/

/*!
    \typedef QJsonObject::const_reverse_iterator

    A const reverse iterator over the entries, for STL compatibility.
*/

/*!
    \typedef QJsonObject::key_type

    Alias for QString, for STL compatibility.
*/

/*!
    \typedef QJsonObject::mapped_type

    Alias for QJsonValue, for STL compatibility.
*/

/*!
    \typedef QJsonObject::size_type

    Alias for \c qsizetype, for STL compatibility.
*/

/*!
    \typedef QJsonObject::difference_type

    Alias for \c qsizetype, for STL compatibility.
*/

/*!
    \typedef QJsonObject::value_type

    Alias for QJsonValue, for STL compatibility.
*/

/*!
    \typedef QJsonObject::reference

    Alias for QJsonValueRef, for STL compatibility.
*/

/*!
    \typedef QJsonObject::const_reference

    Alias for QJsonValueConstRef, for STL compatibility.
*/

/*!
    \typedef QJsonObject::pointer

    The type \c{iterator::operator->()} returns, for STL compatibility.
*/

/*!
    \typedef QJsonObject::const_pointer

    The type \c{const_iterator::operator->()} returns, for STL compatibility.
*/

/*!
    Creates an empty object.
*/
QJsonObject::QJsonObject() = default;

/*!
    Creates an object with the key-value pairs in \a args. If a key occurs
    more than once, the last value is kept.
*/
QJsonObject::QJsonObject(std::initializer_list<std::pair<QString, QJsonValue>> args) {
    std::vector<Entry> entries;
    entries.reserve(args.size());
    for (const auto &p : args) {
        if (!p.second.isUndefined())
            entries.push_back(Entry{p.first, p.second});
    }
    *this = QJsonPrivateAccess::makeObjectSorting(std::move(entries));
}

/*!
    Creates a copy of \a other. The two share their data until one of them is
    modified.
*/
QJsonObject::QJsonObject(const QJsonObject &other) = default;

/*!
    Moves \a other into the new object, which leaves \a other empty.
*/
QJsonObject::QJsonObject(QJsonObject &&other) noexcept = default;

/*!
    Destroys the object.
*/
QJsonObject::~QJsonObject() = default;

/*!
    Assigns \a other to this object and returns a reference to it.
*/
QJsonObject &QJsonObject::operator=(const QJsonObject &other) = default;

/*!
    Moves \a other into this object and returns a reference to it.
*/
QJsonObject &QJsonObject::operator=(QJsonObject &&other) noexcept = default;

/*!
    \fn void QJsonObject::swap(QJsonObject &other)

    Swaps this object with \a other.
*/

QJsonObjectPrivate &QJsonObject::detached() {
    if (!d)
        d = std::make_shared<QJsonObjectPrivate>();
    else if (d.use_count() > 1)
        d = std::make_shared<QJsonObjectPrivate>(*d);
    return *d;
}

/*!
    Returns the number of key-value pairs in the object.
*/
qsizetype QJsonObject::size() const { return d ? static_cast<qsizetype>(d->items.size()) : 0; }

/*!
    \fn qsizetype QJsonObject::count() const

    Same as size().
*/

/*!
    \fn qsizetype QJsonObject::length() const

    Same as size().
*/

/*!
    \fn bool QJsonObject::isEmpty() const

    Returns \c true if the object has no keys.
*/

/*!
    \fn bool QJsonObject::empty() const

    Same as isEmpty(), for STL compatibility.
*/

/*!
    Returns the keys of the object in sorted order.

    \sa contains()
*/
QStringList QJsonObject::keys() const {
    QStringList list;
    if (d) {
        for (const Entry &e : d->items)
            list.append(e.key);
    }
    return list;
}

/*!
    Returns the value for \a key, or an undefined value if the object has no
    such key.

    \sa contains()
*/
QJsonValue QJsonObject::value(const QString &key) const {
    const qsizetype i = indexOfKey(d.get(), key);
    return i < 0 ? QJsonValue(QJsonValue::Undefined) : d->items[static_cast<std::size_t>(i)].value;
}

/*!
    \overload
*/
QJsonValue QJsonObject::value(QStringView key) const { return value(key.toString()); }

/*!
    \overload
*/
QJsonValue QJsonObject::value(QLatin1StringView key) const { return value(QString(key)); }

/*!
    \fn template <typename K, typename> QJsonValue QJsonObject::value(K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \fn QJsonValue QJsonObject::operator[](const QString &key) const

    \overload

    Same as value().
*/

/*!
    \fn QJsonValue QJsonObject::operator[](QStringView key) const

    \overload
*/

/*!
    \fn QJsonValue QJsonObject::operator[](QLatin1StringView key) const

    \overload
*/

/*!
    \fn template <typename K, typename> QJsonValue QJsonObject::operator[](K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    Returns a reference to the value for \a key, which can be assigned to. If
    the object has no such key it is inserted with a null value first.

    \sa value(), insert()
*/
QJsonValueRef QJsonObject::operator[](const QString &key) {
    qsizetype i = indexOfKey(d.get(), key);
    if (i < 0) {
        auto &items = detached().items;
        i = lowerBound(&*d, key);
        items.insert(items.begin() + i, Entry{key, QJsonValue()});
    }
    return QJsonValueRef(this, i);
}

/*!
    \overload
*/
QJsonValueRef QJsonObject::operator[](QStringView key) { return (*this)[key.toString()]; }

/*!
    \overload
*/
QJsonValueRef QJsonObject::operator[](QLatin1StringView key) { return (*this)[QString(key)]; }

/*!
    \fn template <typename K, typename> QJsonValueRef QJsonObject::operator[](K &&key)

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    Removes \a key and its value from the object. Nothing happens if the
    object has no such key.

    \sa take(), insert()
*/
void QJsonObject::remove(const QString &key) {
    const qsizetype i = indexOfKey(d.get(), key);
    if (i < 0)
        return;
    auto &items = detached().items;
    items.erase(items.begin() + i);
}

/*!
    \overload
*/
void QJsonObject::remove(QStringView key) { remove(key.toString()); }

/*!
    \overload
*/
void QJsonObject::remove(QLatin1StringView key) { remove(QString(key)); }

/*!
    \fn template <typename K, typename> void QJsonObject::remove(K &&key)

    \overload

    Removes \a key when it is a C string, such as a string literal.
*/

/*!
    Removes \a key from the object and returns its value, or returns an
    undefined value if the object has no such key.

    \sa remove()
*/
QJsonValue QJsonObject::take(const QString &key) {
    const qsizetype i = indexOfKey(d.get(), key);
    if (i < 0)
        return QJsonValue(QJsonValue::Undefined);
    auto &items = detached().items;
    QJsonValue v = std::move(items[static_cast<std::size_t>(i)].value);
    items.erase(items.begin() + i);
    return v;
}

/*!
    \overload
*/
QJsonValue QJsonObject::take(QStringView key) { return take(key.toString()); }

/*!
    \overload
*/
QJsonValue QJsonObject::take(QLatin1StringView key) { return take(QString(key)); }

/*!
    \fn template <typename K, typename> QJsonValue QJsonObject::take(K &&key)

    \overload

    Takes \a key when it is a C string, such as a string literal.
*/

/*!
    Returns \c true if the object contains \a key.

    \sa value(), keys()
*/
bool QJsonObject::contains(const QString &key) const { return indexOfKey(d.get(), key) >= 0; }

/*!
    \overload
*/
bool QJsonObject::contains(QStringView key) const { return contains(key.toString()); }

/*!
    \overload
*/
bool QJsonObject::contains(QLatin1StringView key) const { return contains(QString(key)); }

/*!
    \fn template <typename K, typename> bool QJsonObject::contains(K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

bool QJsonObject::equals(const QJsonObject &other) const {
    if (d == other.d)
        return true;
    if (size() != other.size())
        return false;
    if (size() == 0)
        return true;
    return std::equal(d->items.begin(), d->items.end(), other.d->items.begin(),
                      [](const Entry &a, const Entry &b) { return a.key == b.key && a.value == b.value; });
}

/*!
    Inserts \a value under \a key and returns an iterator to the new entry. If
    \a key exists, its value is replaced. If \a value is undefined the key is
    removed instead, and the returned iterator is end().

    \sa remove()
*/
QJsonObject::iterator QJsonObject::insert(const QString &key, const QJsonValue &value) {
    if (value.isUndefined()) {
        remove(key);
        return end();
    }
    auto &items = detached().items;
    const qsizetype i = lowerBound(&*d, key);
    if (i < static_cast<qsizetype>(items.size()) && items[static_cast<std::size_t>(i)].key == key)
        items[static_cast<std::size_t>(i)].value = value;
    else
        items.insert(items.begin() + i, Entry{key, value});
    return iterator(this, i);
}

/*!
    Removes the entry \a it refers to and returns an iterator to the entry
    that followed it.

    \sa remove(), insert()
*/
QJsonObject::iterator QJsonObject::erase(iterator it) {
    const qsizetype i = it.i;
    if (i >= 0 && i < size()) {
        auto &items = detached().items;
        items.erase(items.begin() + i);
    }
    return iterator(this, i);
}

/*!
    Returns an iterator to the entry for \a key, or end() if the object has no
    such key.

    \sa constFind(), contains()
*/
QJsonObject::iterator QJsonObject::find(const QString &key) {
    const qsizetype i = indexOfKey(d.get(), key);
    return i < 0 ? end() : iterator(this, i);
}

/*!
    \overload
*/
QJsonObject::iterator QJsonObject::find(QStringView key) { return find(key.toString()); }

/*!
    \overload
*/
QJsonObject::iterator QJsonObject::find(QLatin1StringView key) { return find(QString(key)); }

/*!
    \fn template <typename K, typename> QJsonObject::iterator QJsonObject::find(K &&key)

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::find(const QString &key) const

    \overload
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::find(QStringView key) const

    \overload
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::find(QLatin1StringView key) const

    \overload
*/

/*!
    \fn template <typename K, typename> QJsonObject::const_iterator QJsonObject::find(K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    Returns a const iterator to the entry for \a key, or constEnd() if the
    object has no such key.

    \sa find(), contains()
*/
QJsonObject::const_iterator QJsonObject::constFind(const QString &key) const {
    const qsizetype i = indexOfKey(d.get(), key);
    return i < 0 ? end() : const_iterator(this, i);
}

/*!
    \overload
*/
QJsonObject::const_iterator QJsonObject::constFind(QStringView key) const { return constFind(key.toString()); }

/*!
    \overload
*/
QJsonObject::const_iterator QJsonObject::constFind(QLatin1StringView key) const { return constFind(QString(key)); }

/*!
    \fn template <typename K, typename> QJsonObject::const_iterator QJsonObject::constFind(K &&key) const

    \overload

    Looks \a key up when it is a C string, such as a string literal.
*/

/*!
    \fn QJsonObject::iterator QJsonObject::begin()

    Returns an iterator to the first entry, which has the smallest key.

    \sa end(), constBegin()
*/

/*!
    \fn QJsonObject::iterator QJsonObject::end()

    Returns an iterator to the position after the last entry.

    \sa begin(), constEnd()
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::begin() const

    \overload
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::end() const

    \overload
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::cbegin() const

    Returns a const iterator to the first entry.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::cend() const

    Returns a const iterator to the position after the last entry.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::constBegin() const

    Returns a const iterator to the first entry.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::constEnd() const

    Returns a const iterator to the position after the last entry.
*/

/*!
    \fn QJsonObject::reverse_iterator QJsonObject::rbegin()

    Returns a reverse iterator to the last entry.
*/

/*!
    \fn QJsonObject::reverse_iterator QJsonObject::rend()

    Returns a reverse iterator to the position before the first entry.
*/

/*!
    \fn QJsonObject::const_reverse_iterator QJsonObject::rbegin() const

    \overload
*/

/*!
    \fn QJsonObject::const_reverse_iterator QJsonObject::rend() const

    \overload
*/

/*!
    \fn QJsonObject::const_reverse_iterator QJsonObject::crbegin() const

    Returns a const reverse iterator to the last entry.
*/

/*!
    \fn QJsonObject::const_reverse_iterator QJsonObject::crend() const

    Returns a const reverse iterator to the position before the first entry.
*/
