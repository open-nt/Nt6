// MIT License - see LICENSE
#include <QtCore/qassociativeiterable.h>
#include <QtCore/qsequentialiterable.h>
#include <QtCore/qvariant.h>

namespace {

// Reads a value of `type` out of the container with `fill`, which constructs it in place.
template <typename Fill> QVariant variantFrom(const QMetaType &type, Fill fill) {
    void *tmp = QtPrivate::allocateValue(type.iface());
    fill(tmp);
    QVariant v(type, tmp);
    type.destruct(tmp);
    QtPrivate::freeValue(type.iface(), tmp);
    return v;
}

} // namespace

/*!
    \class QSequentialIterable
    \brief The QSequentialIterable class is an iterable interface for a container in a QVariant.

    A QVariant that holds a sequential container, such as \c{QList<int>},
    \c{std::vector<QString>}, \c QSet or \c QStringList, converts to a
    QSequentialIterable. It reads the elements as QVariant objects without
    knowing the container type:

    \code
    QVariant v = QVariant::fromValue(QList<int>{1, 2, 3});
    if (v.canConvert<QSequentialIterable>()) {
        QSequentialIterable it = v.value<QSequentialIterable>();
        for (const QVariant &e : it)
            qDebug() << e.toInt();
    }
    \endcode

    The element type must be registered with QMetaType, which happens
    automatically for the types Nt knows and for types that are declared with
    \c Q_DECLARE_METATYPE().

    \differences
    The iterable works on a copy of the variant, so it is read-only: there is
    no \c addValue(), \c removeValue() or \c mutableBegin(). There is no
    \c metaContainer(), and \c canReverseIterate() is always \c true.
    \enddifferences

    \sa QAssociativeIterable, QVariant, QMetaType
*/

/*!
    Constructs an iterable for the container that \a variant holds. The result is not valid if
    the variant does not hold a sequential container.

    \sa isValid()
*/
QSequentialIterable::QSequentialIterable(const QVariant &variant)
    : m_variant(variant), m_iface(variant.sequenceInterface()) {}

/*!
    Returns \c true if the iterable refers to a container.
*/
bool QSequentialIterable::isValid() const { return m_iface != nullptr; }

/*!
    Returns the number of elements, or 0 if the iterable is not valid.
*/
qsizetype QSequentialIterable::size() const { return m_iface ? m_iface->size(m_variant.constData()) : 0; }

/*!
    Returns the element at the position \a idx as a QVariant, or an invalid variant if \a idx
    is out of range.
*/
QVariant QSequentialIterable::at(qsizetype idx) const {
    if (!m_iface || idx < 0 || idx >= size())
        return QVariant();
    const QMetaType type(m_iface->valueType());
    return variantFrom(type, [&](void *tmp) { m_iface->valueAt(m_variant.constData(), idx, tmp); });
}

/*!
    Returns an iterator to the first element.

    \sa end(), constBegin()
*/
QSequentialIterable::const_iterator QSequentialIterable::begin() const {
    const_iterator it;
    it.m_owner = this;
    it.m_index = 0;
    return it;
}

/*!
    Returns an iterator past the last element.

    \sa begin(), constEnd()
*/
QSequentialIterable::const_iterator QSequentialIterable::end() const {
    const_iterator it;
    it.m_owner = this;
    it.m_index = size();
    return it;
}

/*!
    Returns the QMetaType of the elements, or an invalid QMetaType if the iterable is not valid.
*/
QMetaType QSequentialIterable::valueMetaType() const { return m_iface ? QMetaType(m_iface->valueType()) : QMetaType(); }

/*!
    Returns \c true if the container can be iterated backwards. Every container Nt supports can.
*/
bool QSequentialIterable::canReverseIterate() const { return m_iface != nullptr; }

/*!
    Returns the element that the iterator refers to.
*/
QVariant QSequentialIterable::const_iterator::operator*() const { return m_owner ? m_owner->at(m_index) : QVariant(); }

/*!
    \class QAssociativeIterable
    \brief The QAssociativeIterable class is an iterable interface for an associative container in a QVariant.

    A QVariant that holds an associative container, such as
    \c{QMap<QString, int>}, \c QHash or \c{std::map}, converts to a
    QAssociativeIterable, which reads the entries as QVariant objects:

    \code
    QVariant v = QVariant::fromValue(QMap<QString, int>{{"a", 1}, {"b", 2}});
    QAssociativeIterable it = v.value<QAssociativeIterable>();
    for (auto i = it.begin(); i != it.end(); ++i)
        qDebug() << i.key().toString() << i.value().toInt();
    \endcode

    \differences
    The iterable works on a copy of the variant, so it is read-only: there is
    no \c insert() or \c remove(). There is no \c metaContainer() and no
    \c operator[]; use value().
    \enddifferences

    \sa QSequentialIterable, QVariant, QMetaType
*/

/*!
    Constructs an iterable for the container that \a variant holds. The result is not valid if
    the variant does not hold an associative container.

    \sa isValid()
*/
QAssociativeIterable::QAssociativeIterable(const QVariant &variant)
    : m_variant(variant), m_iface(variant.associativeInterface()) {}

/*!
    Returns \c true if the iterable refers to a container.
*/
bool QAssociativeIterable::isValid() const { return m_iface != nullptr; }

/*!
    Returns the number of entries, or 0 if the iterable is not valid.
*/
qsizetype QAssociativeIterable::size() const { return m_iface ? m_iface->size(m_variant.constData()) : 0; }

/*!
    Returns an iterator to the first entry.

    \sa end(), constBegin()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::begin() const {
    const_iterator it;
    it.m_owner = this;
    it.m_index = 0;
    return it;
}

/*!
    Returns an iterator past the last entry.

    \sa begin(), constEnd()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::end() const {
    const_iterator it;
    it.m_owner = this;
    it.m_index = size();
    return it;
}

/*!
    Returns an iterator to the entry with \a key, or end() if there is none or \a key cannot be
    converted to the key type.

    \sa constFind(), containsKey()
*/
QAssociativeIterable::const_iterator QAssociativeIterable::find(const QVariant &key) const {
    if (!m_iface)
        return end();
    QVariant k = key;
    const QMetaType keyType(m_iface->keyType());
    if (!k.convert(keyType))
        return end();
    const qsizetype pos = m_iface->find(m_variant.constData(), k.constData());
    if (pos < 0)
        return end();
    const_iterator it;
    it.m_owner = this;
    it.m_index = pos;
    return it;
}

/*!
    Returns \c true if the container has an entry with \a key.

    \sa find(), value()
*/
bool QAssociativeIterable::containsKey(const QVariant &key) const { return find(key) != end(); }

/*!
    Returns the mapped value of the entry with \a key, or an invalid variant if there is none.

    \sa find(), containsKey()
*/
QVariant QAssociativeIterable::value(const QVariant &key) const {
    const const_iterator it = find(key);
    return it == end() ? QVariant() : it.value();
}

/*!
    Returns the QMetaType of the keys, or an invalid QMetaType if the iterable is not valid.
*/
QMetaType QAssociativeIterable::keyMetaType() const { return m_iface ? QMetaType(m_iface->keyType()) : QMetaType(); }

/*!
    Returns the QMetaType of the mapped values, or an invalid QMetaType if the iterable is not
    valid.
*/
QMetaType QAssociativeIterable::mappedMetaType() const {
    return m_iface ? QMetaType(m_iface->mappedType()) : QMetaType();
}

/*!
    Returns the key of the entry that the iterator refers to.

    \sa value()
*/
QVariant QAssociativeIterable::const_iterator::key() const {
    if (!m_owner || m_index < 0 || m_index >= m_owner->size())
        return QVariant();
    const QMetaType type(m_owner->m_iface->keyType());
    return variantFrom(type, [&](void *tmp) { m_owner->m_iface->keyAt(m_owner->m_variant.constData(), m_index, tmp); });
}

/*!
    Returns the mapped value of the entry that the iterator refers to.

    \sa key()
*/
QVariant QAssociativeIterable::const_iterator::value() const {
    if (!m_owner || m_index < 0 || m_index >= m_owner->size())
        return QVariant();
    const QMetaType type(m_owner->m_iface->mappedType());
    return variantFrom(type,
                       [&](void *tmp) { m_owner->m_iface->mappedAt(m_owner->m_variant.constData(), m_index, tmp); });
}
