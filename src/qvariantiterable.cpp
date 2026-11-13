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

// The value of `value` converted to `type`, or an invalid variant if it does not convert.
QVariant convertedTo(const QVariant &value, const QMetaType &type) {
    QVariant v = value;
    if (!type.isValid() || !v.convert(type))
        return QVariant();
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
    An iterable made from a QVariant reads a copy of the container and cannot change it; QVariant::view() and
    the constructor that takes a pointer give one that changes the container. Iterators are addressed by
    position, and canReverseIterate() is always \c true. Dereferencing a mutable iterator gives a
    QSequentialIterable::iterator::reference rather than a \c QVariantRef.
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
    \since 6.0
    Constructs an iterable for the container at \a iterable, of the type \a metaType, through which the container
    can also be changed. \a metaSequence must be the meta-sequence of that type, as
    QMetaSequence::fromContainer() returns it. The container must outlive the iterable.

    \sa canModify()
*/
QSequentialIterable::QSequentialIterable(const QMetaSequence &metaSequence, QMetaType metaType, void *iterable)
    : m_iface(metaType.isValid() ? metaType.iface()->sequence : nullptr), m_data(iterable) {
    if (!iterable || (m_iface && m_iface->metaSequence && QMetaSequence(m_iface->metaSequence()) != metaSequence))
        m_iface = nullptr, m_data = nullptr;
}

/*!
    \since 6.0
    \overload
    Constructs a read-only iterable for the container at \a iterable, of the type \a metaType, whose
    meta-sequence is \a metaSequence. The container must outlive the iterable.
*/
QSequentialIterable::QSequentialIterable(const QMetaSequence &metaSequence, QMetaType metaType, const void *iterable)
    : m_iface(metaType.isValid() ? metaType.iface()->sequence : nullptr), m_constData(iterable) {
    if (!iterable || (m_iface && m_iface->metaSequence && QMetaSequence(m_iface->metaSequence()) != metaSequence))
        m_iface = nullptr, m_constData = nullptr;
}

/*!
    Returns \c true if the iterable refers to a container.
*/
bool QSequentialIterable::isValid() const { return m_iface != nullptr; }

/*!
    Returns the number of elements, or 0 if the iterable is not valid.
*/
qsizetype QSequentialIterable::size() const { return m_iface ? m_iface->size(container()) : 0; }

/*!
    Returns the element at the position \a idx as a QVariant, or an invalid variant if \a idx
    is out of range.
*/
QVariant QSequentialIterable::at(qsizetype idx) const {
    if (!m_iface || idx < 0 || idx >= size())
        return QVariant();
    const QMetaType type(m_iface->valueType());
    return variantFrom(type, [&](void *tmp) { m_iface->valueAt(container(), idx, tmp); });
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
    \since 6.0
    Returns the QMetaSequence of the container type, which gives access to all of its operations, or a
    meta-sequence of no type if the iterable is not valid.

    \sa QMetaSequence::fromContainer()
*/
QMetaSequence QSequentialIterable::metaContainer() const {
    return m_iface && m_iface->metaSequence ? QMetaSequence(m_iface->metaSequence()) : QMetaSequence();
}

/*!
    \since 6.0
    Adds \a value, converted to the element type, to the container at \a position. Does nothing if the iterable
    cannot change the container, the container cannot add values there, or \a value does not convert.

    \sa removeValue(), canModify()
*/
void QSequentialIterable::addValue(const QVariant &value, Position position) {
    const QMetaSequence meta = metaContainer();
    const QVariant v = convertedTo(value, meta.valueMetaType());
    if (!m_data || !v.isValid())
        return;
    switch (position) {
    case AtBegin:
        meta.addValueAtBegin(m_data, v.constData());
        break;
    case AtEnd:
        meta.addValueAtEnd(m_data, v.constData());
        break;
    case Unspecified:
        meta.addValue(m_data, v.constData());
        break;
    }
}

/*!
    \since 6.0
    Removes the element at \a position of the container. Does nothing if the iterable cannot change the
    container, the container is empty or it cannot remove values there.

    \sa addValue(), canModify()
*/
void QSequentialIterable::removeValue(Position position) {
    const QMetaSequence meta = metaContainer();
    if (!m_data)
        return;
    switch (position) {
    case AtBegin:
        meta.removeValueAtBegin(m_data);
        break;
    case AtEnd:
        meta.removeValueAtEnd(m_data);
        break;
    case Unspecified:
        meta.removeValue(m_data);
        break;
    }
}

/*!
    \since 6.0
    Replaces the element at \a idx with \a value, converted to the element type. Does nothing if the iterable
    cannot change the container, \a idx is out of range, the elements cannot be replaced (those of a set) or
    \a value does not convert.

    \sa at(), canModify()
*/
void QSequentialIterable::set(qsizetype idx, const QVariant &value) {
    const QMetaSequence meta = metaContainer();
    const QVariant v = convertedTo(value, meta.valueMetaType());
    if (!m_data || !v.isValid() || idx < 0 || idx >= size())
        return;
    if (meta.canSetValueAtIndex()) {
        meta.setValueAtIndex(m_data, idx, v.constData());
    } else if (meta.canSetValueAtIterator()) {
        void *it = meta.begin(m_data);
        meta.advanceIterator(it, idx);
        meta.setValueAtIterator(it, v.constData());
        meta.destroyIterator(it);
    }
}

/*!
    \since 6.0
    Returns an iterator to the first element, through which the elements can be replaced. An iterable that cannot
    change the container returns an iterator whose assignments do nothing.

    \sa mutableEnd(), begin()
*/
QSequentialIterable::iterator QSequentialIterable::mutableBegin() {
    iterator it;
    it.m_owner = this;
    it.m_index = 0;
    return it;
}

/*!
    \since 6.0
    Returns an iterator past the last element.

    \sa mutableBegin(), end()
*/
QSequentialIterable::iterator QSequentialIterable::mutableEnd() {
    iterator it;
    it.m_owner = this;
    it.m_index = size();
    return it;
}

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
    An iterable made from a QVariant reads a copy of the container and cannot change it; QVariant::view() and
    the constructor that takes a pointer give one that changes the container. Iterators are addressed by
    position. There is no \c operator[]; use value() and setValue().
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
    \since 6.0
    Constructs an iterable for the container at \a iterable, of the type \a metaType, through which the container
    can also be changed. \a metaAssociation must be the meta-association of that type, as
    QMetaAssociation::fromContainer() returns it. The container must outlive the iterable.

    \sa canModify()
*/
QAssociativeIterable::QAssociativeIterable(const QMetaAssociation &metaAssociation, QMetaType metaType, void *iterable)
    : m_iface(metaType.isValid() ? metaType.iface()->associative : nullptr), m_data(iterable) {
    if (!iterable ||
        (m_iface && m_iface->metaAssociation && QMetaAssociation(m_iface->metaAssociation()) != metaAssociation))
        m_iface = nullptr, m_data = nullptr;
}

/*!
    \since 6.0
    \overload
    Constructs a read-only iterable for the container at \a iterable, of the type \a metaType, whose
    meta-association is \a metaAssociation. The container must outlive the iterable.
*/
QAssociativeIterable::QAssociativeIterable(const QMetaAssociation &metaAssociation, QMetaType metaType,
                                           const void *iterable)
    : m_iface(metaType.isValid() ? metaType.iface()->associative : nullptr), m_constData(iterable) {
    if (!iterable ||
        (m_iface && m_iface->metaAssociation && QMetaAssociation(m_iface->metaAssociation()) != metaAssociation))
        m_iface = nullptr, m_constData = nullptr;
}

/*!
    Returns \c true if the iterable refers to a container.
*/
bool QAssociativeIterable::isValid() const { return m_iface != nullptr; }

/*!
    Returns the number of entries, or 0 if the iterable is not valid.
*/
qsizetype QAssociativeIterable::size() const { return m_iface ? m_iface->size(container()) : 0; }

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
    const qsizetype pos = m_iface->find(container(), k.constData());
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
    \since 6.0
    Returns the QMetaAssociation of the container type, which gives access to all of its operations, or a
    meta-association of no type if the iterable is not valid.

    \sa QMetaAssociation::fromContainer()
*/
QMetaAssociation QAssociativeIterable::metaContainer() const {
    return m_iface && m_iface->metaAssociation ? QMetaAssociation(m_iface->metaAssociation()) : QMetaAssociation();
}

/*!
    \since 6.0
    Inserts \a key, converted to the key type, with a default-constructed mapped value, unless the container has it
    already. Does nothing if the iterable cannot change the container or \a key does not convert.

    \sa setValue(), removeKey(), canModify()
*/
void QAssociativeIterable::insertKey(const QVariant &key) {
    const QMetaAssociation meta = metaContainer();
    const QVariant k = convertedTo(key, meta.keyMetaType());
    if (m_data && k.isValid())
        meta.insertKey(m_data, k.constData());
}

/*!
    \since 6.0
    Removes \a key and its mapped value from the container. Does nothing if the iterable cannot change the
    container or \a key does not convert to the key type.

    \sa insertKey(), canModify()
*/
void QAssociativeIterable::removeKey(const QVariant &key) {
    const QMetaAssociation meta = metaContainer();
    const QVariant k = convertedTo(key, meta.keyMetaType());
    if (m_data && k.isValid())
        meta.removeKey(m_data, k.constData());
}

/*!
    \since 6.0
    Makes \a key map to \a mapped, both converted to the types of the container, inserting the key if needed.
    Does nothing if the iterable cannot change the container or a value does not convert.

    \sa value(), insertKey(), canModify()
*/
void QAssociativeIterable::setValue(const QVariant &key, const QVariant &mapped) {
    const QMetaAssociation meta = metaContainer();
    const QVariant k = convertedTo(key, meta.keyMetaType());
    const QVariant m = convertedTo(mapped, meta.mappedMetaType());
    if (m_data && k.isValid() && m.isValid())
        meta.setMappedAtKey(m_data, k.constData(), m.constData());
}

/*!
    \since 6.0
    Returns an iterator to the first entry, through which the mapped values can be replaced. An iterable that
    cannot change the container returns an iterator whose assignments do nothing.

    \sa mutableEnd(), mutableFind(), begin()
*/
QAssociativeIterable::iterator QAssociativeIterable::mutableBegin() {
    iterator it;
    it.m_owner = this;
    it.m_index = 0;
    return it;
}

/*!
    \since 6.0
    Returns an iterator past the last entry.

    \sa mutableBegin()
*/
QAssociativeIterable::iterator QAssociativeIterable::mutableEnd() {
    iterator it;
    it.m_owner = this;
    it.m_index = size();
    return it;
}

/*!
    \since 6.0
    Returns a mutable iterator to the entry with \a key, or mutableEnd() if there is none.

    \sa find()
*/
QAssociativeIterable::iterator QAssociativeIterable::mutableFind(const QVariant &key) {
    const const_iterator found = find(key);
    iterator it;
    it.m_owner = this;
    it.m_index = found == end() ? size() : found.m_index;
    return it;
}

/*!
    Returns the key of the entry that the iterator refers to.
*/
QVariant QAssociativeIterable::iterator::key() const {
    if (!m_owner || m_index < 0 || m_index >= m_owner->size())
        return QVariant();
    const QMetaType type(m_owner->m_iface->keyType());
    return variantFrom(type, [&](void *tmp) { m_owner->m_iface->keyAt(m_owner->container(), m_index, tmp); });
}

/*!
    Returns the key of the entry that the iterator refers to.

    \sa value()
*/
QVariant QAssociativeIterable::const_iterator::key() const {
    if (!m_owner || m_index < 0 || m_index >= m_owner->size())
        return QVariant();
    const QMetaType type(m_owner->m_iface->keyType());
    return variantFrom(type, [&](void *tmp) { m_owner->m_iface->keyAt(m_owner->container(), m_index, tmp); });
}

/*!
    Returns the mapped value of the entry that the iterator refers to.

    \sa key()
*/
QVariant QAssociativeIterable::const_iterator::value() const {
    if (!m_owner || m_index < 0 || m_index >= m_owner->size())
        return QVariant();
    const QMetaType type(m_owner->m_iface->mappedType());
    return variantFrom(type, [&](void *tmp) { m_owner->m_iface->mappedAt(m_owner->container(), m_index, tmp); });
}
