// MIT License - see LICENSE
#include <QtCore/qcbormap.h>

#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>

#include <utility>

/*!
    \class QCborMap
    \brief The QCborMap class is used to hold an associative container representable in CBOR.
    \since 5.12

    A QCborMap holds pairs of a key and a value, both QCborValue. Keys are usually strings or
    integers but may be of any type. The pairs keep the order in which they were added, and a key
    is found by going through them, so a map is meant for the small objects of a data format and
    not as a large dictionary.

    \code
    QCborMap map{{u"name"_s, u"Nt"_s}, {1, true}};
    map[u"version"_s] = 6;
    for (auto [key, value] : map)
        qDebug() << key.toDiagnosticNotation() << value.toDiagnosticNotation();
    \endcode

    Reading a key that the map does not have gives \c undefined; the operator[] that is not const
    adds it.

    \differences
    Copies are deep: a copy of a map copies its pairs, which share their own strings and
    containers. Keys are compared with QCborValue::compare(), so the integer 1 and the
    floating-point number 1.0 are different keys. There is no \c QDebug operator, and no
    \c QDataStream operators.
    \enddifferences

    \sa QCborArray, QCborValue
*/

/*!
    \class QCborMap::Iterator
    \brief The Iterator class is the iterator of QCborMap that can change values.
    \since 5.12

    Dereferencing it gives the key and a QCborValueRef to the value, so that the pairs can be
    taken apart in a range-based \c for loop. value() and the operator-> refer to the value.

    \differences
    The iterator is invalid once the map gets or loses pairs.
    \enddifferences
*/

/*!
    \class QCborMap::ConstIterator
    \brief The ConstIterator class is the iterator of QCborMap that reads pairs.
    \since 5.12

    \differences
    The iterator is invalid once the map gets or loses pairs.
    \enddifferences
*/

/*!
    Constructs an empty map.
*/
QCborMap::QCborMap() noexcept = default;

/*!
    Constructs a map of the pairs in \a args. A key that comes twice keeps the value of the
    first pair.
*/
QCborMap::QCborMap(std::initializer_list<value_type> args) {
    for (const value_type &pair : args) {
        if (indexOf(pair.first) < 0) {
            m_items.append(pair.first);
            m_items.append(pair.second);
        }
    }
}

/*!
    Constructs a copy of \a other.
*/
QCborMap::QCborMap(const QCborMap &other) = default;

/*!
    Move-constructs a map from \a other, which becomes empty.
*/
QCborMap::QCborMap(QCborMap &&other) noexcept : m_items(std::move(other.m_items)) {}

/*!
    Assigns \a other to this map and returns a reference to it.
*/
QCborMap &QCborMap::operator=(const QCborMap &other) = default;

/*!
    Move-assigns \a other to this map and returns a reference to it.
*/
QCborMap &QCborMap::operator=(QCborMap &&other) noexcept {
    m_items = std::move(other.m_items);
    return *this;
}

/*!
    Destroys the map.
*/
QCborMap::~QCborMap() = default;

/*!
    Exchanges this map with \a other.
*/
void QCborMap::swap(QCborMap &other) noexcept { m_items.swap(other.m_items); }

/*!
    Returns the map as a QCborValue.
*/
QCborValue QCborMap::toCborValue() const { return QCborValue(*this); }

/*!
    Returns the number of pairs.
*/
qsizetype QCborMap::size() const noexcept { return m_items.size() / 2; }

/*!
    Removes all pairs.
*/
void QCborMap::clear() { m_items.clear(); }

/*!
    Returns the keys, in the order of the pairs.
*/
QList<QCborValue> QCborMap::keys() const {
    QList<QCborValue> result;
    for (qsizetype i = 0; i < m_items.size(); i += 2)
        result.append(m_items.at(i));
    return result;
}

qsizetype QCborMap::indexOf(const QCborValue &key) const {
    for (qsizetype i = 0; i < m_items.size(); i += 2) {
        if (m_items.at(i) == key)
            return i + 1;
    }
    return -1;
}

/*!
    Returns the value of \a key, or \c undefined if the map has no such key.
*/
QCborValue QCborMap::value(qint64 key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? QCborValue() : m_items.at(i);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
QCborValue QCborMap::value(QLatin1StringView key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? QCborValue() : m_items.at(i);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
QCborValue QCborMap::value(const QString &key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? QCborValue() : m_items.at(i);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
QCborValue QCborMap::value(const QCborValue &key) const {
    const qsizetype i = indexOf(key);
    return i < 0 ? QCborValue() : m_items.at(i);
}

/*!
    Returns the value of \a key, or \c undefined if the map has no such key.
*/
const QCborValue QCborMap::operator[](qint64 key) const {
    return value(key);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
const QCborValue QCborMap::operator[](QLatin1StringView key) const {
    return value(key);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
const QCborValue QCborMap::operator[](const QString &key) const {
    return value(key);
}

/*!
    \overload
    Returns the value of \a key, or \c undefined.
*/
const QCborValue QCborMap::operator[](const QCborValue &key) const {
    return value(key);
}

/*!
    Returns a reference to the value of \a key. If the map has no such key, it is added with an \c undefined value.
*/
QCborValueRef QCborMap::operator[](qint64 key) {
    qsizetype i = indexOf(QCborValue(key));
    if (i < 0) {
        m_items.append(QCborValue(key));
        m_items.append(QCborValue());
        i = m_items.size() - 1;
    }
    return QCborValueRef(m_items.data() + i);
}

/*!
    \overload
    Returns a reference to the value of \a key, which is added if the map has none.
*/
QCborValueRef QCborMap::operator[](QLatin1StringView key) {
    qsizetype i = indexOf(QCborValue(key));
    if (i < 0) {
        m_items.append(QCborValue(key));
        m_items.append(QCborValue());
        i = m_items.size() - 1;
    }
    return QCborValueRef(m_items.data() + i);
}

/*!
    \overload
    Returns a reference to the value of \a key, which is added if the map has none.
*/
QCborValueRef QCborMap::operator[](const QString &key) {
    qsizetype i = indexOf(QCborValue(key));
    if (i < 0) {
        m_items.append(QCborValue(key));
        m_items.append(QCborValue());
        i = m_items.size() - 1;
    }
    return QCborValueRef(m_items.data() + i);
}

/*!
    \overload
    Returns a reference to the value of \a key, which is added if the map has none.
*/
QCborValueRef QCborMap::operator[](const QCborValue &key) {
    qsizetype i = indexOf(key);
    if (i < 0) {
        m_items.append(key);
        m_items.append(QCborValue());
        i = m_items.size() - 1;
    }
    return QCborValueRef(m_items.data() + i);
}

/*!
    Removes \a key and returns its value, or returns \c undefined if the map has no such key.
*/
QCborValue QCborMap::take(qint64 key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i < 0)
        return QCborValue();
    QCborValue v = m_items.at(i);
    m_items.remove(i - 1, 2);
    return v;
}

/*!
    \overload
    Removes \a key and returns its value.
*/
QCborValue QCborMap::take(QLatin1StringView key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i < 0)
        return QCborValue();
    QCborValue v = m_items.at(i);
    m_items.remove(i - 1, 2);
    return v;
}

/*!
    \overload
    Removes \a key and returns its value.
*/
QCborValue QCborMap::take(const QString &key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i < 0)
        return QCborValue();
    QCborValue v = m_items.at(i);
    m_items.remove(i - 1, 2);
    return v;
}

/*!
    \overload
    Removes \a key and returns its value.
*/
QCborValue QCborMap::take(const QCborValue &key) {
    const qsizetype i = indexOf(key);
    if (i < 0)
        return QCborValue();
    QCborValue v = m_items.at(i);
    m_items.remove(i - 1, 2);
    return v;
}

/*!
    Removes \a key and its value, if the map has the key.
*/
void QCborMap::remove(qint64 key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i >= 0)
        m_items.remove(i - 1, 2);
}

/*!
    \overload
    Removes \a key and its value.
*/
void QCborMap::remove(QLatin1StringView key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i >= 0)
        m_items.remove(i - 1, 2);
}

/*!
    \overload
    Removes \a key and its value.
*/
void QCborMap::remove(const QString &key) {
    const qsizetype i = indexOf(QCborValue(key));
    if (i >= 0)
        m_items.remove(i - 1, 2);
}

/*!
    \overload
    Removes \a key and its value.
*/
void QCborMap::remove(const QCborValue &key) {
    const qsizetype i = indexOf(key);
    if (i >= 0)
        m_items.remove(i - 1, 2);
}

/*!
    Returns \c true if the map has \a key.
*/
bool QCborMap::contains(qint64 key) const {
    return indexOf(QCborValue(key)) >= 0;
}

/*!
    \overload
    Returns \c true if the map has \a key.
*/
bool QCborMap::contains(QLatin1StringView key) const {
    return indexOf(QCborValue(key)) >= 0;
}

/*!
    \overload
    Returns \c true if the map has \a key.
*/
bool QCborMap::contains(const QString &key) const {
    return indexOf(QCborValue(key)) >= 0;
}

/*!
    \overload
    Returns \c true if the map has \a key.
*/
bool QCborMap::contains(const QCborValue &key) const {
    return indexOf(key) >= 0;
}

/*!
    Returns an iterator to the pair of \a key, or end() if the map has no such key.
*/
QCborMap::Iterator QCborMap::find(qint64 key) {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? end() : Iterator(m_items.data() + i - 1);
}

/*!
    \overload
    Returns an iterator to the pair of \a key, or end().
*/
QCborMap::Iterator QCborMap::find(QLatin1StringView key) {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? end() : Iterator(m_items.data() + i - 1);
}

/*!
    \overload
    Returns an iterator to the pair of \a key, or end().
*/
QCborMap::Iterator QCborMap::find(const QString &key) {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? end() : Iterator(m_items.data() + i - 1);
}

/*!
    \overload
    Returns an iterator to the pair of \a key, or end().
*/
QCborMap::Iterator QCborMap::find(const QCborValue &key) {
    const qsizetype i = indexOf(key);
    return i < 0 ? end() : Iterator(m_items.data() + i - 1);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or end().
*/
QCborMap::ConstIterator QCborMap::find(qint64 key) const {
    return constFind(key);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or end().
*/
QCborMap::ConstIterator QCborMap::find(QLatin1StringView key) const {
    return constFind(key);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or end().
*/
QCborMap::ConstIterator QCborMap::find(const QString &key) const {
    return constFind(key);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or end().
*/
QCborMap::ConstIterator QCborMap::find(const QCborValue &key) const {
    return constFind(key);
}

/*!
    Returns a const iterator to the pair of \a key, or constEnd() if the map has no such key.
*/
QCborMap::ConstIterator QCborMap::constFind(qint64 key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? constEnd() : ConstIterator(const_cast<QCborValue *>(m_items.constData()) + i - 1);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or constEnd().
*/
QCborMap::ConstIterator QCborMap::constFind(QLatin1StringView key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? constEnd() : ConstIterator(const_cast<QCborValue *>(m_items.constData()) + i - 1);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or constEnd().
*/
QCborMap::ConstIterator QCborMap::constFind(const QString &key) const {
    const qsizetype i = indexOf(QCborValue(key));
    return i < 0 ? constEnd() : ConstIterator(const_cast<QCborValue *>(m_items.constData()) + i - 1);
}

/*!
    \overload
    Returns a const iterator to the pair of \a key, or constEnd().
*/
QCborMap::ConstIterator QCborMap::constFind(const QCborValue &key) const {
    const qsizetype i = indexOf(key);
    return i < 0 ? constEnd() : ConstIterator(const_cast<QCborValue *>(m_items.constData()) + i - 1);
}

/*!
    Sets the value of \a key to \a value, adding the key if the map has none, and returns an iterator to the pair.
*/
QCborMap::Iterator QCborMap::insert(qint64 key, const QCborValue &value) {
    QCborValueRef ref = (*this)[key];
    ref = value;
    return find(key);
}

/*!
    \overload
    Sets the value of \a key to \a value and returns an iterator to the pair.
*/
QCborMap::Iterator QCborMap::insert(QLatin1StringView key, const QCborValue &value) {
    QCborValueRef ref = (*this)[key];
    ref = value;
    return find(key);
}

/*!
    \overload
    Sets the value of \a key to \a value and returns an iterator to the pair.
*/
QCborMap::Iterator QCborMap::insert(const QString &key, const QCborValue &value) {
    QCborValueRef ref = (*this)[key];
    ref = value;
    return find(key);
}

/*!
    \overload
    Sets the value of \a key to \a value and returns an iterator to the pair.
*/
QCborMap::Iterator QCborMap::insert(const QCborValue &key, const QCborValue &value) {
    QCborValueRef ref = (*this)[key];
    ref = value;
    return find(key);
}

/*!
    \overload
    Adds the pair \a v, or sets the value of its key if the map has the key, and returns an
    iterator to the pair.
*/
QCborMap::Iterator QCborMap::insert(value_type v) { return insert(v.first, v.second); }

/*!
    Compares this map with \a other: a smaller map comes first, and maps of the same size are
    compared pair by pair, key and then value. Returns a negative number, 0 or a positive number.

    \sa QCborValue::compare()
*/
int QCborMap::compare(const QCborMap &other) const noexcept {
    if (size() != other.size())
        return size() < other.size() ? -1 : 1;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        const int c = m_items.at(i).compare(other.m_items.at(i));
        if (c != 0)
            return c;
    }
    return 0;
}

/*!
    Returns an iterator to the first pair.
*/
QCborMap::Iterator QCborMap::begin() { return Iterator(m_items.data()); }

/*!
    \overload
*/
QCborMap::ConstIterator QCborMap::begin() const { return constBegin(); }

/*!
    Returns a const iterator to the first pair.
*/
QCborMap::ConstIterator QCborMap::constBegin() const { return ConstIterator(const_cast<QCborValue *>(m_items.constData())); }

/*!
    Returns an iterator past the last pair.
*/
QCborMap::Iterator QCborMap::end() { return Iterator(m_items.data() + m_items.size()); }

/*!
    \overload
*/
QCborMap::ConstIterator QCborMap::end() const { return constEnd(); }

/*!
    Returns a const iterator past the last pair.
*/
QCborMap::ConstIterator QCborMap::constEnd() const {
    return ConstIterator(const_cast<QCborValue *>(m_items.constData()) + m_items.size());
}

/*!
    Removes the pair at \a it and returns an iterator to the pair after it.
*/
QCborMap::Iterator QCborMap::erase(Iterator it) {
    const qsizetype i = it - begin();
    m_items.remove(2 * i, 2);
    return begin() + i;
}

/*!
    \overload
    Removes the pair at \a it and returns an iterator to the pair after it.
*/
QCborMap::Iterator QCborMap::erase(ConstIterator it) {
    const qsizetype i = it - constBegin();
    m_items.remove(2 * i, 2);
    return begin() + i;
}

/*!
    Removes the pair at \a it and returns its value.
*/
QCborValue QCborMap::extract(Iterator it) {
    QCborValue v = it.value();
    erase(it);
    return v;
}

/*!
    \overload
    Removes the pair at \a it and returns its value.
*/
QCborValue QCborMap::extract(ConstIterator it) {
    QCborValue v = it.value();
    erase(it);
    return v;
}

/*!
    Returns a map of the pairs in \a map, with the values converted by QCborValue::fromVariant().
*/
QCborMap QCborMap::fromVariantMap(const QVariantMap &map) {
    QCborMap result;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it)
        result.insert(it.key(), QCborValue::fromVariant(it.value()));
    return result;
}

/*!
    Returns a map of the pairs in \a hash, with the values converted by
    QCborValue::fromVariant(). The order of the pairs is that of the hash.
*/
QCborMap QCborMap::fromVariantHash(const QVariantHash &hash) {
    QCborMap result;
    for (auto it = hash.constBegin(); it != hash.constEnd(); ++it)
        result.insert(it.key(), QCborValue::fromVariant(it.value()));
    return result;
}

/*!
    Returns a map of the pairs in \a o, with the values converted by
    QCborValue::fromJsonValue().
*/
QCborMap QCborMap::fromJsonObject(const QJsonObject &o) {
    QCborMap result;
    const QStringList keys = o.keys();
    for (const QString &key : keys)
        result.insert(key, QCborValue::fromJsonValue(o.value(key)));
    return result;
}

namespace {

// A key as text: a string as it is, an integer in decimal, a byte array in Base64url, anything else in the
// diagnostic notation.
QString keyText(const QCborValue &key) {
    if (key.isString())
        return key.toString();
    if (key.isInteger())
        return QString::number(key.toInteger());
    if (key.isByteArray())
        return QString::fromLatin1(key.toByteArray().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    return key.toDiagnosticNotation();
}

} // namespace

/*!
    Returns the pairs as a QVariantMap, with the values converted by QCborValue::toVariant() and
    keys that are not strings converted to text: an integer in decimal, a byte array in
    Base64url, and anything else in the diagnostic notation. Of keys that give the same text the
    last one counts.
*/
QVariantMap QCborMap::toVariantMap() const {
    QVariantMap map;
    for (qsizetype i = 0; i < m_items.size(); i += 2)
        map.insert(keyText(m_items.at(i)), m_items.at(i + 1).toVariant());
    return map;
}

/*!
    Returns the pairs as a QVariantHash, converted as toVariantMap() does.
*/
QVariantHash QCborMap::toVariantHash() const {
    QVariantHash hash;
    for (qsizetype i = 0; i < m_items.size(); i += 2)
        hash.insert(keyText(m_items.at(i)), m_items.at(i + 1).toVariant());
    return hash;
}

/*!
    Returns the pairs as a QJsonObject, with the values converted by QCborValue::toJsonValue() and
    the keys converted to text as toVariantMap() does.
*/
QJsonObject QCborMap::toJsonObject() const {
    QJsonObject object;
    for (qsizetype i = 0; i < m_items.size(); i += 2)
        object.insert(keyText(m_items.at(i)), m_items.at(i + 1).toJsonValue());
    return object;
}
