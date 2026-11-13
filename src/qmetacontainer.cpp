// MIT License - see LICENSE
#include <QtCore/qmetacontainer.h>

/*!
    \class QMetaContainer
    \since 6.0
    \brief The QMetaContainer class is the common part of QMetaSequence and QMetaAssociation: what a container
    of a type that is not known at compile time can do, and its iterators.

    A QMetaContainer holds the functions for one container type, made by QMetaSequence::fromContainer() or
    QMetaAssociation::fromContainer(). It works on containers passed as \c{void *}, and hands out iterators
    as \c{void *} that must be destroyed with destroyIterator() or destroyConstIterator():

    \code
    QList<int> list = {1, 2, 3};
    const QMetaSequence meta = QMetaSequence::fromContainer<QList<int>>();
    void *it = meta.constBegin(&list);
    void *end = meta.constEnd(&list);
    int value = 0;
    for (; !meta.compareConstIterator(it, end); meta.advanceConstIterator(it, 1)) {
        meta.valueAtConstIterator(it, &value);
        qDebug() << value;
    }
    meta.destroyConstIterator(it);
    meta.destroyConstIterator(end);
    \endcode

    Each operation has a function that tells whether the container has it, such as hasSize() for size(). Calling
    an operation that the container does not have does nothing and returns a default value.

    \differences
    The functions of a container type are found by looking at what the container can do (\c size(),
    \c clear(), \c push_back(), \c insert() at an iterator and so on) rather than from a fixed list of
    containers, so any container with \c begin(), \c end() and \c value_type works.
    \enddifferences

    \sa QMetaSequence, QMetaAssociation, QSequentialIterable, QAssociativeIterable
*/

/*!
    \fn QMetaContainer::QMetaContainer()
    Constructs a meta-container that stands for no container type. It has no operations.
*/

/*!
    \fn QMetaContainer::QMetaContainer(const QtMetaContainerPrivate::QMetaContainerInterface *d)
    \internal
*/

namespace {

using Interface = QtMetaContainerPrivate::QMetaContainerInterface;

bool hasCapability(const Interface *d, QtMetaContainerPrivate::IteratorCapability capability) {
    return d && (d->iteratorCapabilities & capability);
}

} // namespace

/*!
    Returns \c true if the container has iterators that can be read once, from the begin to the end, which every
    container with iterators has; otherwise returns \c false.

    \sa hasForwardIterator()
*/
bool QMetaContainer::hasInputIterator() const { return hasCapability(d_ptr, QtMetaContainerPrivate::InputCapability); }

/*!
    Returns \c true if the iterators of the container can go over it several times; otherwise returns \c false.

    \sa hasBidirectionalIterator()
*/
bool QMetaContainer::hasForwardIterator() const {
    return hasCapability(d_ptr, QtMetaContainerPrivate::ForwardCapability);
}

/*!
    Returns \c true if the iterators of the container can also move backward, with a negative step in
    advanceIterator(); otherwise returns \c false.

    \sa hasRandomAccessIterator()
*/
bool QMetaContainer::hasBidirectionalIterator() const {
    return hasCapability(d_ptr, QtMetaContainerPrivate::BiDirectionalCapability);
}

/*!
    Returns \c true if the iterators of the container jump by any step in constant time, as those of a QList do;
    otherwise returns \c false.

    \sa hasBidirectionalIterator()
*/
bool QMetaContainer::hasRandomAccessIterator() const {
    return hasCapability(d_ptr, QtMetaContainerPrivate::RandomAccessCapability);
}

/*!
    Returns \c true if the container can tell its size; otherwise returns \c false.

    \sa size()
*/
bool QMetaContainer::hasSize() const { return d_ptr && d_ptr->sizeFn; }

/*!
    Returns the number of values in \a container, or -1 if the container cannot tell it.

    \sa hasSize()
*/
qsizetype QMetaContainer::size(const void *container) const { return hasSize() ? d_ptr->sizeFn(container) : -1; }

/*!
    Returns \c true if the container can be cleared; otherwise returns \c false.

    \sa clear()
*/
bool QMetaContainer::canClear() const { return d_ptr && d_ptr->clearFn; }

/*!
    Removes all values from \a container, if it can be cleared.

    \sa canClear()
*/
void QMetaContainer::clear(void *container) const {
    if (canClear())
        d_ptr->clearFn(container);
}

/*!
    Returns \c true if the container has iterators that can change it; otherwise returns \c false.

    \sa begin(), hasConstIterator()
*/
bool QMetaContainer::hasIterator() const {
    return d_ptr && d_ptr->createIteratorFn && d_ptr->destroyIteratorFn && d_ptr->compareIteratorFn &&
           d_ptr->copyIteratorFn && d_ptr->advanceIteratorFn && d_ptr->diffIteratorFn;
}

/*!
    Returns a new iterator to the first value of \a container, or \c nullptr if the container has no iterators.
    Destroy it with destroyIterator().

    \sa end(), constBegin()
*/
void *QMetaContainer::begin(void *container) const {
    return hasIterator() ? d_ptr->createIteratorFn(container, Interface::AtBegin) : nullptr;
}

/*!
    Returns a new iterator past the last value of \a container, or \c nullptr if the container has no iterators.
    Destroy it with destroyIterator().

    \sa begin(), constEnd()
*/
void *QMetaContainer::end(void *container) const {
    return hasIterator() ? d_ptr->createIteratorFn(container, Interface::AtEnd) : nullptr;
}

/*!
    Destroys \a iterator, which begin(), end() or a function that creates an iterator returned.

    \sa begin()
*/
void QMetaContainer::destroyIterator(const void *iterator) const {
    if (hasIterator())
        d_ptr->destroyIteratorFn(iterator);
}

/*!
    Returns \c true if the iterators \a i and \a j point to the same place; otherwise returns \c false.

    \sa compareConstIterator()
*/
bool QMetaContainer::compareIterator(const void *i, const void *j) const {
    return i == j || (hasIterator() && d_ptr->compareIteratorFn(i, j));
}

/*!
    Makes the iterator \a target point where \a source points.

    \sa copyConstIterator()
*/
void QMetaContainer::copyIterator(void *target, const void *source) const {
    if (hasIterator())
        d_ptr->copyIteratorFn(target, source);
}

/*!
    Moves \a iterator by \a step values: forward, or backward for a negative step if the container has
    bidirectional iterators.

    \sa hasBidirectionalIterator(), advanceConstIterator()
*/
void QMetaContainer::advanceIterator(void *iterator, qsizetype step) const {
    if (hasIterator())
        d_ptr->advanceIteratorFn(iterator, step);
}

/*!
    Returns the number of values from \a j to \a i: \c{i - j}. For iterators that are not random access, \a i must
    be reachable from \a j.

    \sa diffConstIterator()
*/
qsizetype QMetaContainer::diffIterator(const void *i, const void *j) const {
    return hasIterator() ? d_ptr->diffIteratorFn(i, j) : 0;
}

/*!
    Returns \c true if the container has iterators that only read it; otherwise returns \c false.

    \sa constBegin(), hasIterator()
*/
bool QMetaContainer::hasConstIterator() const {
    return d_ptr && d_ptr->createConstIteratorFn && d_ptr->destroyConstIteratorFn && d_ptr->compareConstIteratorFn &&
           d_ptr->copyConstIteratorFn && d_ptr->advanceConstIteratorFn && d_ptr->diffConstIteratorFn;
}

/*!
    Returns a new const iterator to the first value of \a container, or \c nullptr if the container has no const
    iterators. Destroy it with destroyConstIterator().

    \sa constEnd(), begin()
*/
void *QMetaContainer::constBegin(const void *container) const {
    return hasConstIterator() ? d_ptr->createConstIteratorFn(container, Interface::AtBegin) : nullptr;
}

/*!
    Returns a new const iterator past the last value of \a container, or \c nullptr if the container has no const
    iterators. Destroy it with destroyConstIterator().

    \sa constBegin(), end()
*/
void *QMetaContainer::constEnd(const void *container) const {
    return hasConstIterator() ? d_ptr->createConstIteratorFn(container, Interface::AtEnd) : nullptr;
}

/*!
    Destroys the const iterator \a iterator.

    \sa constBegin()
*/
void QMetaContainer::destroyConstIterator(const void *iterator) const {
    if (hasConstIterator())
        d_ptr->destroyConstIteratorFn(iterator);
}

/*!
    Returns \c true if the const iterators \a i and \a j point to the same place; otherwise returns \c false.

    \sa compareIterator()
*/
bool QMetaContainer::compareConstIterator(const void *i, const void *j) const {
    return i == j || (hasConstIterator() && d_ptr->compareConstIteratorFn(i, j));
}

/*!
    Makes the const iterator \a target point where \a source points.

    \sa copyIterator()
*/
void QMetaContainer::copyConstIterator(void *target, const void *source) const {
    if (hasConstIterator())
        d_ptr->copyConstIteratorFn(target, source);
}

/*!
    Moves the const iterator \a iterator by \a step values.

    \sa advanceIterator()
*/
void QMetaContainer::advanceConstIterator(void *iterator, qsizetype step) const {
    if (hasConstIterator())
        d_ptr->advanceConstIteratorFn(iterator, step);
}

/*!
    Returns the number of values from the const iterator \a j to \a i.

    \sa diffIterator()
*/
qsizetype QMetaContainer::diffConstIterator(const void *i, const void *j) const {
    return hasConstIterator() ? d_ptr->diffConstIteratorFn(i, j) : 0;
}

/*!
    \class QMetaSequence
    \since 6.0
    \brief The QMetaSequence class gives access to a sequential container, such as a QList or a \c std::vector,
    whose type is not known at compile time.

    Make one with fromContainer(), or get the one of a QVariant through QSequentialIterable::metaContainer().
    Values are passed by pointer and copied by assignment: a pointer to a result must point to a constructed value
    of valueMetaType().

    \code
    std::vector<QString> names;
    const QMetaSequence meta = QMetaSequence::fromContainer<std::vector<QString>>();
    const QString name = QStringLiteral("Ada");
    meta.addValueAtEnd(&names, &name);
    QString first;
    meta.valueAtIndex(&names, 0, &first);
    \endcode

    \differences
    The operations come from what the container can do: \c push_front() and \c push_back(), \c pop_front()
    and \c pop_back(), \c insert() and \c erase() at an iterator, assignment through an iterator. A set adds
    values with \c insert() when addValue() is called.
    \enddifferences

    \sa QMetaAssociation, QSequentialIterable
*/

/*!
    \fn QMetaSequence::QMetaSequence()
    Constructs a meta-sequence that stands for no container type.
*/

/*!
    \fn QMetaSequence::QMetaSequence(const QtMetaContainerPrivate::QMetaSequenceInterface *d)
    \internal
*/

/*!
    \fn template <typename T> QMetaSequence QMetaSequence::fromContainer()
    Returns the meta-sequence of the container type \c T, which needs \c begin(), \c end() and \c value_type.
*/

/*!
    \fn const QtMetaContainerPrivate::QMetaSequenceInterface *QMetaSequence::iface() const
    \internal
*/

/*!
    Returns the type of the values in the container, or an invalid QMetaType for a meta-sequence of no type.
*/
QMetaType QMetaSequence::valueMetaType() const {
    return d() && d()->valueMetaType ? QMetaType(d()->valueMetaType()) : QMetaType();
}

/*!
    Returns \c true if the values can be sorted in place: the container has random access iterators through
    which values can be read and written; otherwise returns \c false.
*/
bool QMetaSequence::isSortable() const {
    return hasRandomAccessIterator() && canGetValueAtIterator() && canSetValueAtIterator();
}

/*!
    Returns \c true if values can be added at the front of the container; otherwise returns \c false.

    \sa addValueAtBegin()
*/
bool QMetaSequence::canAddValueAtBegin() const {
    return d() && d()->addValueFn && (d()->addRemoveCapabilities & QtMetaContainerPrivate::CanAddAtBegin);
}

/*!
    Adds a copy of \a value at the front of \a container.

    \sa canAddValueAtBegin()
*/
void QMetaSequence::addValueAtBegin(void *container, const void *value) const {
    if (canAddValueAtBegin())
        d()->addValueFn(container, value, Interface::AtBegin);
}

/*!
    Returns \c true if values can be added at the back of the container; otherwise returns \c false.

    \sa addValueAtEnd()
*/
bool QMetaSequence::canAddValueAtEnd() const {
    return d() && d()->addValueFn && (d()->addRemoveCapabilities & QtMetaContainerPrivate::CanAddAtEnd);
}

/*!
    Adds a copy of \a value at the back of \a container.

    \sa canAddValueAtEnd()
*/
void QMetaSequence::addValueAtEnd(void *container, const void *value) const {
    if (canAddValueAtEnd())
        d()->addValueFn(container, value, Interface::AtEnd);
}

/*!
    Returns \c true if the first value can be removed; otherwise returns \c false.

    \sa removeValueAtBegin()
*/
bool QMetaSequence::canRemoveValueAtBegin() const {
    return d() && d()->removeValueFn && (d()->addRemoveCapabilities & QtMetaContainerPrivate::CanRemoveAtBegin);
}

/*!
    Removes the first value of \a container, if it has one.

    \sa canRemoveValueAtBegin()
*/
void QMetaSequence::removeValueAtBegin(void *container) const {
    if (canRemoveValueAtBegin())
        d()->removeValueFn(container, Interface::AtBegin);
}

/*!
    Returns \c true if the last value can be removed; otherwise returns \c false.

    \sa removeValueAtEnd()
*/
bool QMetaSequence::canRemoveValueAtEnd() const {
    return d() && d()->removeValueFn && (d()->addRemoveCapabilities & QtMetaContainerPrivate::CanRemoveAtEnd);
}

/*!
    Removes the last value of \a container, if it has one.

    \sa canRemoveValueAtEnd()
*/
void QMetaSequence::removeValueAtEnd(void *container) const {
    if (canRemoveValueAtEnd())
        d()->removeValueFn(container, Interface::AtEnd);
}

/*!
    Returns \c true if values can be read by their index; otherwise returns \c false.

    \sa valueAtIndex()
*/
bool QMetaSequence::canGetValueAtIndex() const { return d() && d()->valueAtIndexFn; }

/*!
    Assigns the value at \a index of \a container to the value that \a result points to. \a index must be valid.

    \sa canGetValueAtIndex(), setValueAtIndex()
*/
void QMetaSequence::valueAtIndex(const void *container, qsizetype index, void *result) const {
    if (canGetValueAtIndex())
        d()->valueAtIndexFn(container, index, result);
}

/*!
    Returns \c true if values can be replaced by their index; otherwise returns \c false.

    \sa setValueAtIndex()
*/
bool QMetaSequence::canSetValueAtIndex() const { return d() && d()->setValueAtIndexFn; }

/*!
    Replaces the value at \a index of \a container with a copy of \a value. \a index must be valid.

    \sa canSetValueAtIndex()
*/
void QMetaSequence::setValueAtIndex(void *container, qsizetype index, const void *value) const {
    if (canSetValueAtIndex())
        d()->setValueAtIndexFn(container, index, value);
}

/*!
    Returns \c true if values can be added somewhere in the container; otherwise returns \c false.

    \sa addValue()
*/
bool QMetaSequence::canAddValue() const { return d() && d()->addValueFn; }

/*!
    Adds a copy of \a value to \a container where the container puts new values: at the back of a list, or
    with \c insert() into a set.

    \sa canAddValue(), addValueAtEnd()
*/
void QMetaSequence::addValue(void *container, const void *value) const {
    if (canAddValue())
        d()->addValueFn(container, value, Interface::Unspecified);
}

/*!
    Returns \c true if a value can be removed from an end of the container; otherwise returns \c false.

    \sa removeValue()
*/
bool QMetaSequence::canRemoveValue() const { return d() && d()->removeValueFn; }

/*!
    Removes a value from \a container: the last one, or the first if only that one can be removed.

    \sa canRemoveValue()
*/
void QMetaSequence::removeValue(void *container) const {
    if (canRemoveValue())
        d()->removeValueFn(container, Interface::Unspecified);
}

/*!
    Returns \c true if values can be read through iterators; otherwise returns \c false.

    \sa valueAtIterator()
*/
bool QMetaSequence::canGetValueAtIterator() const { return d() && d()->valueAtIteratorFn; }

/*!
    Assigns the value that \a iterator points to to the value that \a result points to.

    \sa canGetValueAtIterator()
*/
void QMetaSequence::valueAtIterator(const void *iterator, void *result) const {
    if (canGetValueAtIterator())
        d()->valueAtIteratorFn(iterator, result);
}

/*!
    Returns \c true if values can be replaced through iterators; otherwise returns \c false. The values of a set
    cannot.

    \sa setValueAtIterator()
*/
bool QMetaSequence::canSetValueAtIterator() const { return d() && d()->setValueAtIteratorFn; }

/*!
    Replaces the value that \a iterator points to with a copy of \a value.

    \sa canSetValueAtIterator()
*/
void QMetaSequence::setValueAtIterator(const void *iterator, const void *value) const {
    if (canSetValueAtIterator())
        d()->setValueAtIteratorFn(iterator, value);
}

/*!
    Returns \c true if values can be inserted before an iterator; otherwise returns \c false.

    \sa insertValueAtIterator()
*/
bool QMetaSequence::canInsertValueAtIterator() const { return d() && d()->insertValueAtIteratorFn; }

/*!
    Inserts a copy of \a value into \a container before \a iterator. Iterators of the container may no longer be
    valid afterward, as with the container's own \c insert().

    \sa canInsertValueAtIterator()
*/
void QMetaSequence::insertValueAtIterator(void *container, const void *iterator, const void *value) const {
    if (canInsertValueAtIterator())
        d()->insertValueAtIteratorFn(container, iterator, value);
}

/*!
    Returns \c true if the value at an iterator can be erased; otherwise returns \c false.

    \sa eraseValueAtIterator()
*/
bool QMetaSequence::canEraseValueAtIterator() const { return d() && d()->eraseValueAtIteratorFn; }

/*!
    Erases the value that \a iterator points to from \a container.

    \sa canEraseValueAtIterator(), eraseRangeAtIterator()
*/
void QMetaSequence::eraseValueAtIterator(void *container, const void *iterator) const {
    if (canEraseValueAtIterator())
        d()->eraseValueAtIteratorFn(container, iterator);
}

/*!
    Returns \c true if a range of values between two iterators can be erased; otherwise returns \c false.

    \sa eraseRangeAtIterator()
*/
bool QMetaSequence::canEraseRangeAtIterator() const { return d() && d()->eraseRangeAtIteratorFn; }

/*!
    Erases the values of \a container from \a iterator1 up to, but not including, \a iterator2.

    \sa canEraseRangeAtIterator()
*/
void QMetaSequence::eraseRangeAtIterator(void *container, const void *iterator1, const void *iterator2) const {
    if (canEraseRangeAtIterator())
        d()->eraseRangeAtIteratorFn(container, iterator1, iterator2);
}

/*!
    Returns \c true if values can be read through const iterators; otherwise returns \c false.

    \sa valueAtConstIterator()
*/
bool QMetaSequence::canGetValueAtConstIterator() const { return d() && d()->valueAtConstIteratorFn; }

/*!
    Assigns the value that the const iterator \a iterator points to to the value that \a result points to.

    \sa canGetValueAtConstIterator()
*/
void QMetaSequence::valueAtConstIterator(const void *iterator, void *result) const {
    if (canGetValueAtConstIterator())
        d()->valueAtConstIteratorFn(iterator, result);
}

/*!
    \class QMetaAssociation
    \since 6.0
    \brief The QMetaAssociation class gives access to an associative container, such as a QMap or a
    \c std::unordered_map, whose type is not known at compile time.

    Make one with fromContainer(), or get the one of a QVariant through QAssociativeIterable::metaContainer().
    Keys and mapped values are passed by pointer and copied by assignment: a pointer to a result must point to
    a constructed value of keyMetaType() or mappedMetaType().

    \code
    QHash<QString, int> ages;
    const QMetaAssociation meta = QMetaAssociation::fromContainer<QHash<QString, int>>();
    const QString key = QStringLiteral("Ada");
    const int age = 36;
    meta.setMappedAtKey(&ages, &key, &age);
    int found = 0;
    meta.mappedAtKey(&ages, &key, &found);
    \endcode

    \differences
    The operations come from what the container can do: \c find(), \c operator[] and \c remove() or
    \c erase(). mappedAtKey() gives a default-constructed value for a key that is not there.
    \enddifferences

    \sa QMetaSequence, QAssociativeIterable
*/

/*!
    \fn QMetaAssociation::QMetaAssociation()
    Constructs a meta-association that stands for no container type.
*/

/*!
    \fn QMetaAssociation::QMetaAssociation(const QtMetaContainerPrivate::QMetaAssociationInterface *d)
    \internal
*/

/*!
    \fn template <typename T> QMetaAssociation QMetaAssociation::fromContainer()
    Returns the meta-association of the container type \c T, which needs \c key_type and \c mapped_type.
*/

/*!
    \fn const QtMetaContainerPrivate::QMetaAssociationInterface *QMetaAssociation::iface() const
    \internal
*/

/*!
    Returns the type of the keys, or an invalid QMetaType for a meta-association of no type.

    \sa mappedMetaType()
*/
QMetaType QMetaAssociation::keyMetaType() const {
    return d() && d()->keyMetaType ? QMetaType(d()->keyMetaType()) : QMetaType();
}

/*!
    Returns the type of the mapped values, or an invalid QMetaType for a meta-association of no type.

    \sa keyMetaType()
*/
QMetaType QMetaAssociation::mappedMetaType() const {
    return d() && d()->mappedMetaType ? QMetaType(d()->mappedMetaType()) : QMetaType();
}

/*!
    Returns \c true if keys can be inserted; otherwise returns \c false.

    \sa insertKey()
*/
bool QMetaAssociation::canInsertKey() const { return d() && d()->insertKeyFn; }

/*!
    Inserts \a key into \a container with a default-constructed mapped value, unless it is there already.

    \sa canInsertKey(), setMappedAtKey()
*/
void QMetaAssociation::insertKey(void *container, const void *key) const {
    if (canInsertKey())
        d()->insertKeyFn(container, key);
}

/*!
    Returns \c true if keys can be removed; otherwise returns \c false.

    \sa removeKey()
*/
bool QMetaAssociation::canRemoveKey() const { return d() && d()->removeKeyFn; }

/*!
    Removes \a key and its mapped value from \a container.

    \sa canRemoveKey()
*/
void QMetaAssociation::removeKey(void *container, const void *key) const {
    if (canRemoveKey())
        d()->removeKeyFn(container, key);
}

/*!
    Returns \c true if the container can tell whether it has a key; otherwise returns \c false.

    \sa containsKey()
*/
bool QMetaAssociation::canContainsKey() const { return d() && d()->containsKeyFn; }

/*!
    Returns \c true if \a container has \a key; otherwise returns \c false.

    \sa canContainsKey()
*/
bool QMetaAssociation::containsKey(const void *container, const void *key) const {
    return canContainsKey() && d()->containsKeyFn(container, key);
}

/*!
    Returns \c true if mapped values can be read by their key; otherwise returns \c false.

    \sa mappedAtKey()
*/
bool QMetaAssociation::canGetMappedAtKey() const { return d() && d()->mappedAtKeyFn; }

/*!
    Assigns the value that \a key maps to in \a container to the value that \a mapped points to, or a
    default-constructed value if the key is not there.

    \sa canGetMappedAtKey()
*/
void QMetaAssociation::mappedAtKey(const void *container, const void *key, void *mapped) const {
    if (canGetMappedAtKey())
        d()->mappedAtKeyFn(container, key, mapped);
}

/*!
    Returns \c true if mapped values can be set by their key; otherwise returns \c false.

    \sa setMappedAtKey()
*/
bool QMetaAssociation::canSetMappedAtKey() const { return d() && d()->setMappedAtKeyFn; }

/*!
    Makes \a key map to a copy of \a mapped in \a container, inserting the key if needed.

    \sa canSetMappedAtKey()
*/
void QMetaAssociation::setMappedAtKey(void *container, const void *key, const void *mapped) const {
    if (canSetMappedAtKey())
        d()->setMappedAtKeyFn(container, key, mapped);
}

/*!
    Returns \c true if keys can be read through iterators; otherwise returns \c false.

    \sa keyAtIterator()
*/
bool QMetaAssociation::canGetKeyAtIterator() const { return d() && d()->keyAtIteratorFn; }

/*!
    Assigns the key that \a iterator points to to the value that \a key points to.

    \sa canGetKeyAtIterator()
*/
void QMetaAssociation::keyAtIterator(const void *iterator, void *key) const {
    if (canGetKeyAtIterator())
        d()->keyAtIteratorFn(iterator, key);
}

/*!
    Returns \c true if keys can be read through const iterators; otherwise returns \c false.

    \sa keyAtConstIterator()
*/
bool QMetaAssociation::canGetKeyAtConstIterator() const { return d() && d()->keyAtConstIteratorFn; }

/*!
    Assigns the key that the const iterator \a iterator points to to the value that \a key points to.

    \sa canGetKeyAtConstIterator()
*/
void QMetaAssociation::keyAtConstIterator(const void *iterator, void *key) const {
    if (canGetKeyAtConstIterator())
        d()->keyAtConstIteratorFn(iterator, key);
}

/*!
    Returns \c true if mapped values can be read through iterators; otherwise returns \c false.

    \sa mappedAtIterator()
*/
bool QMetaAssociation::canGetMappedAtIterator() const { return d() && d()->mappedAtIteratorFn; }

/*!
    Assigns the mapped value that \a iterator points to to the value that \a mapped points to.

    \sa canGetMappedAtIterator()
*/
void QMetaAssociation::mappedAtIterator(const void *iterator, void *mapped) const {
    if (canGetMappedAtIterator())
        d()->mappedAtIteratorFn(iterator, mapped);
}

/*!
    Returns \c true if mapped values can be read through const iterators; otherwise returns \c false.

    \sa mappedAtConstIterator()
*/
bool QMetaAssociation::canGetMappedAtConstIterator() const { return d() && d()->mappedAtConstIteratorFn; }

/*!
    Assigns the mapped value that the const iterator \a iterator points to to the value that \a mapped points to.

    \sa canGetMappedAtConstIterator()
*/
void QMetaAssociation::mappedAtConstIterator(const void *iterator, void *mapped) const {
    if (canGetMappedAtConstIterator())
        d()->mappedAtConstIteratorFn(iterator, mapped);
}

/*!
    Returns \c true if mapped values can be replaced through iterators; otherwise returns \c false.

    \sa setMappedAtIterator()
*/
bool QMetaAssociation::canSetMappedAtIterator() const { return d() && d()->setMappedAtIteratorFn; }

/*!
    Replaces the mapped value that \a iterator points to with a copy of \a mapped.

    \sa canSetMappedAtIterator()
*/
void QMetaAssociation::setMappedAtIterator(const void *iterator, const void *mapped) const {
    if (canSetMappedAtIterator())
        d()->setMappedAtIteratorFn(iterator, mapped);
}

/*!
    Returns \c true if an iterator can be made for a key; otherwise returns \c false.

    \sa createIteratorAtKey()
*/
bool QMetaAssociation::canCreateIteratorAtKey() const { return d() && d()->createIteratorAtKeyFn && hasIterator(); }

/*!
    Returns a new iterator to \a key in \a container, or to the end if the key is not there. Destroy it with
    destroyIterator().

    \sa canCreateIteratorAtKey(), createConstIteratorAtKey()
*/
void *QMetaAssociation::createIteratorAtKey(void *container, const void *key) const {
    return canCreateIteratorAtKey() ? d()->createIteratorAtKeyFn(container, key) : nullptr;
}

/*!
    Returns \c true if a const iterator can be made for a key; otherwise returns \c false.

    \sa createConstIteratorAtKey()
*/
bool QMetaAssociation::canCreateConstIteratorAtKey() const {
    return d() && d()->createConstIteratorAtKeyFn && hasConstIterator();
}

/*!
    Returns a new const iterator to \a key in \a container, or to the end if the key is not there. Destroy it with
    destroyConstIterator().

    \sa canCreateConstIteratorAtKey()
*/
void *QMetaAssociation::createConstIteratorAtKey(const void *container, const void *key) const {
    return canCreateConstIteratorAtKey() ? d()->createConstIteratorAtKeyFn(container, key) : nullptr;
}
