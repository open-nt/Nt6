// MIT License - see LICENSE
#include <QtCore/qjsonarray.h>

#include "qjson_p.h"

#include <algorithm>

QJsonArray QJsonPrivateAccess::arrayFromData(std::shared_ptr<QJsonArrayPrivate> d) {
    QJsonArray a;
    a.d = std::move(d);
    return a;
}

QJsonArray QJsonPrivateAccess::makeArray(std::vector<QJsonValue> &&items) {
    QJsonArray a;
    if (!items.empty()) {
        a.d = std::make_shared<QJsonArrayPrivate>();
        a.d->items = std::move(items);
    }
    return a;
}

/*!
    \class QJsonArray
    \brief The QJsonArray class encapsulates a JSON array.

    A JSON array is a list of values. You can append and remove values from
    the array, and read or replace the value at any index.

    \code
    QJsonArray array = {1, 2.5, "three"};
    array.append(true);
    array.insert(0, QJsonValue::Null);
    for (const QJsonValue &v : array)
        use(v);
    \endcode

    QJsonArray is implicitly shared: copies are cheap and the data is copied
    when one of them is modified. It is reentrant, so separate arrays can be
    used from separate threads, but one array must not be modified while
    another thread reads it or a copy that shares its data.

    The iterators are index based. Reading or assigning through an iterator
    is valid for as long as the array has at least that many elements, and an
    insertion or removal before the position moves the element the iterator
    refers to, so, as with QList, treat any iterator at or after a modified
    position as invalid. Assigning through \c{*it} does not invalidate other
    iterators.

    \differences
    There is no \c fromVariantList() or \c toVariantList() until QVariant
    exists. Out-of-range indexes never assert: \l at() returns an undefined
    value, and insert(), replace() and removeAt() do nothing. The iterators
    hold the array and an index instead of a pointer, so copying the array
    while an iterator is alive is harmless: writes through a non-const iterator
    go to the array it came from.
    \enddifferences

    \sa QJsonValue, QJsonObject, QJsonDocument
*/

/*!
    \typedef QJsonArray::Iterator

    Qt-style synonym for QJsonArray::iterator.
*/

/*!
    \typedef QJsonArray::ConstIterator

    Qt-style synonym for QJsonArray::const_iterator.
*/

/*!
    \typedef QJsonArray::reverse_iterator

    A reverse iterator over the elements, for STL compatibility.
*/

/*!
    \typedef QJsonArray::const_reverse_iterator

    A const reverse iterator over the elements, for STL compatibility.
*/

/*!
    \typedef QJsonArray::size_type

    Alias for \c qsizetype, for STL compatibility.
*/

/*!
    \typedef QJsonArray::difference_type

    Alias for \c qsizetype, for STL compatibility.
*/

/*!
    \typedef QJsonArray::value_type

    Alias for QJsonValue, for STL compatibility.
*/

/*!
    \typedef QJsonArray::reference

    Alias for QJsonValueRef, for STL compatibility.
*/

/*!
    \typedef QJsonArray::const_reference

    Alias for QJsonValueConstRef, for STL compatibility.
*/

/*!
    \typedef QJsonArray::pointer

    The type \c{iterator::operator->()} returns, for STL compatibility.
*/

/*!
    \typedef QJsonArray::const_pointer

    The type \c{const_iterator::operator->()} returns, for STL compatibility.
*/

/*!
    Creates an empty array.
*/
QJsonArray::QJsonArray() = default;

/*!
    Creates an array with the values in \a args.
*/
QJsonArray::QJsonArray(std::initializer_list<QJsonValue> args) {
    if (args.size() == 0)
        return;
    d = std::make_shared<QJsonArrayPrivate>();
    d->items.assign(args.begin(), args.end());
}

/*!
    Creates a copy of \a other. The two share their data until one of them is
    modified.
*/
QJsonArray::QJsonArray(const QJsonArray &other) = default;

/*!
    Moves \a other into the new array, which leaves \a other empty.
*/
QJsonArray::QJsonArray(QJsonArray &&other) noexcept = default;

/*!
    Destroys the array.
*/
QJsonArray::~QJsonArray() = default;

/*!
    Assigns \a other to this array and returns a reference to it.
*/
QJsonArray &QJsonArray::operator=(const QJsonArray &other) = default;

/*!
    Moves \a other into this array and returns a reference to it.
*/
QJsonArray &QJsonArray::operator=(QJsonArray &&other) noexcept = default;

/*!
    \fn void QJsonArray::swap(QJsonArray &other)

    Swaps this array with \a other.
*/

/*!
    Converts \a list to an array of strings.
*/
QJsonArray QJsonArray::fromStringList(const QStringList &list) {
    std::vector<QJsonValue> items;
    items.reserve(static_cast<std::size_t>(list.size()));
    for (const QString &s : list)
        items.emplace_back(s);
    return QJsonPrivateAccess::makeArray(std::move(items));
}

QJsonArrayPrivate &QJsonArray::detached() {
    if (!d)
        d = std::make_shared<QJsonArrayPrivate>();
    else if (d.use_count() > 1)
        d = std::make_shared<QJsonArrayPrivate>(*d);
    return *d;
}

/*!
    Returns the number of values in the array.
*/
qsizetype QJsonArray::size() const { return d ? static_cast<qsizetype>(d->items.size()) : 0; }

/*!
    \fn qsizetype QJsonArray::count() const

    Same as size().
*/

/*!
    \fn qsizetype QJsonArray::length() const

    Same as size().
*/

/*!
    \fn bool QJsonArray::isEmpty() const

    Returns \c true if the array has no values.
*/

/*!
    \fn bool QJsonArray::empty() const

    Same as isEmpty(), for STL compatibility.
*/

/*!
    Returns the value at index \a i, or an undefined value if \a i is out of
    range.
*/
QJsonValue QJsonArray::at(qsizetype i) const {
    if (i < 0 || i >= size())
        return QJsonValue(QJsonValue::Undefined);
    return d->items[static_cast<std::size_t>(i)];
}

/*!
    \fn QJsonValue QJsonArray::first() const

    Returns the first value, or an undefined value if the array is empty.

    \sa last(), at()
*/

/*!
    \fn QJsonValue QJsonArray::last() const

    Returns the last value, or an undefined value if the array is empty.

    \sa first(), at()
*/

/*!
    \fn QJsonValueRef QJsonArray::first()

    \overload

    Returns a reference to the first value, which can be assigned to.
*/

/*!
    \fn QJsonValueRef QJsonArray::last()

    \overload

    Returns a reference to the last value, which can be assigned to.
*/

/*!
    Inserts \a value at the beginning of the array.

    \sa append(), insert()
*/
void QJsonArray::prepend(const QJsonValue &value) { insert(0, value); }

/*!
    Inserts \a value at the end of the array.

    \sa prepend(), insert()
*/
void QJsonArray::append(const QJsonValue &value) { detached().items.push_back(value); }

/*!
    Inserts \a value at index \a i, which must be between 0 and size().
    Nothing happens for another index.

    \sa append(), removeAt()
*/
void QJsonArray::insert(qsizetype i, const QJsonValue &value) {
    if (i < 0 || i > size())
        return;
    auto &items = detached().items;
    items.insert(items.begin() + i, value);
}

/*!
    Removes the value at index \a i. Nothing happens if \a i is out of range.

    \sa takeAt(), removeFirst(), removeLast()
*/
void QJsonArray::removeAt(qsizetype i) {
    if (i < 0 || i >= size())
        return;
    auto &items = detached().items;
    items.erase(items.begin() + i);
}

/*!
    Removes the value at index \a i and returns it, or returns an undefined
    value if \a i is out of range.

    \sa removeAt()
*/
QJsonValue QJsonArray::takeAt(qsizetype i) {
    if (i < 0 || i >= size())
        return QJsonValue(QJsonValue::Undefined);
    auto &items = detached().items;
    QJsonValue v = std::move(items[static_cast<std::size_t>(i)]);
    items.erase(items.begin() + i);
    return v;
}

/*!
    \fn void QJsonArray::removeFirst()

    Removes the first value. Nothing happens if the array is empty.
*/

/*!
    \fn void QJsonArray::removeLast()

    Removes the last value. Nothing happens if the array is empty.
*/

/*!
    Replaces the value at index \a i with \a value. Nothing happens if \a i is
    out of range.

    \sa insert()
*/
void QJsonArray::replace(qsizetype i, const QJsonValue &value) {
    if (i < 0 || i >= size())
        return;
    detached().items[static_cast<std::size_t>(i)] = value;
}

/*!
    Returns \c true if the array contains a value equal to \a element.
*/
bool QJsonArray::contains(const QJsonValue &element) const {
    return d && std::find(d->items.begin(), d->items.end(), element) != d->items.end();
}

/*!
    Returns a reference to the value at index \a i, which can be assigned to.
    Reading through a reference to an index that is out of range gives an
    undefined value.
*/
QJsonValueRef QJsonArray::operator[](qsizetype i) { return QJsonValueRef(this, i); }

/*!
    \fn QJsonValue QJsonArray::operator[](qsizetype i) const

    \overload

    Same as at().
*/

/*!
    \fn QJsonArray &QJsonArray::operator+=(const QJsonValue &value)

    Appends \a value to the array and returns a reference to it.
*/

/*!
    \fn QJsonArray QJsonArray::operator+(const QJsonValue &value) const

    Returns a copy of the array with \a value appended.
*/

/*!
    \fn QJsonArray &QJsonArray::operator<<(const QJsonValue &value)

    Appends \a value to the array and returns a reference to it.
*/

bool QJsonArray::equals(const QJsonArray &other) const {
    if (d == other.d)
        return true;
    if (size() != other.size())
        return false;
    if (size() == 0)
        return true;
    return std::equal(d->items.begin(), d->items.end(), other.d->items.begin());
}

/*!
    Inserts \a value before the position \a before and returns an iterator to
    the new value.

    \sa erase()
*/
QJsonArray::iterator QJsonArray::insert(iterator before, const QJsonValue &value) {
    const qsizetype i = before.i;
    insert(i, value);
    return iterator(this, i);
}

/*!
    Removes the value \a it refers to and returns an iterator to the value
    that followed it.

    \sa insert(), removeAt()
*/
QJsonArray::iterator QJsonArray::erase(iterator it) {
    const qsizetype i = it.i;
    removeAt(i);
    return iterator(this, i);
}

/*!
    \fn void QJsonArray::push_back(const QJsonValue &value)

    Appends \a value to the array. Same as append(), for STL compatibility.
*/

/*!
    \fn void QJsonArray::push_front(const QJsonValue &value)

    Inserts \a value at the beginning of the array. Same as prepend(), for STL compatibility.
*/

/*!
    \fn void QJsonArray::pop_front()

    Same as removeFirst(), for STL compatibility.
*/

/*!
    \fn void QJsonArray::pop_back()

    Same as removeLast(), for STL compatibility.
*/

/*!
    \fn QJsonArray::iterator QJsonArray::begin()

    Returns an iterator to the first value.

    \sa end(), constBegin()
*/

/*!
    \fn QJsonArray::iterator QJsonArray::end()

    Returns an iterator to the position after the last value.

    \sa begin(), constEnd()
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::begin() const

    \overload
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::end() const

    \overload
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::cbegin() const

    Returns a const iterator to the first value.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::cend() const

    Returns a const iterator to the position after the last value.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::constBegin() const

    Returns a const iterator to the first value.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::constEnd() const

    Returns a const iterator to the position after the last value.
*/

/*!
    \fn QJsonArray::reverse_iterator QJsonArray::rbegin()

    Returns a reverse iterator to the last value.
*/

/*!
    \fn QJsonArray::reverse_iterator QJsonArray::rend()

    Returns a reverse iterator to the position before the first value.
*/

/*!
    \fn QJsonArray::const_reverse_iterator QJsonArray::rbegin() const

    \overload
*/

/*!
    \fn QJsonArray::const_reverse_iterator QJsonArray::rend() const

    \overload
*/

/*!
    \fn QJsonArray::const_reverse_iterator QJsonArray::crbegin() const

    Returns a const reverse iterator to the last value.
*/

/*!
    \fn QJsonArray::const_reverse_iterator QJsonArray::crend() const

    Returns a const reverse iterator to the position before the first value.
*/

/*!
    \class QJsonArray::iterator
    \brief The QJsonArray::iterator class is an STL-style non-const iterator for QJsonArray.

    A random access iterator over the values of an array. Dereferencing it
    gives a QJsonValueRef, which can be assigned to.

    \sa QJsonArray::const_iterator
*/

/*!
    \typedef QJsonArray::iterator::iterator_category

    Marks the iterator as a random access iterator.
*/

/*!
    \typedef QJsonArray::iterator::difference_type

    Alias for \c qsizetype.
*/

/*!
    \typedef QJsonArray::iterator::value_type

    Alias for QJsonValue.
*/

/*!
    \typedef QJsonArray::iterator::reference

    Alias for QJsonValueRef.
*/

/*!
    \typedef QJsonArray::iterator::pointer

    The type \c{operator->()} returns.
*/

/*!
    \fn QJsonArray::iterator::iterator()

    Creates an iterator that refers to no array.
*/

/*!
    \fn QJsonArray::iterator::iterator(QJsonArray *array, qsizetype index)

    Creates an iterator that refers to position \a index of \a array.
*/

/*!
    \fn QJsonValueRef QJsonArray::iterator::operator*() const

    Returns a reference to the current value.
*/

/*!
    \fn QJsonRefArrow<QJsonValueRef> QJsonArray::iterator::operator->() const

    Returns a pointer-like helper for the current value.
*/

/*!
    \fn QJsonValueRef QJsonArray::iterator::operator[](qsizetype j) const

    Returns a reference to the value \a j positions after the current one.
*/

/*!
    \fn bool QJsonArray::iterator::operator==(const iterator &o) const

    Returns \c true if \a o refers to the same position.
*/

/*!
    \fn bool QJsonArray::iterator::operator!=(const iterator &o) const

    Returns \c true if \a o refers to another position.
*/

/*!
    \fn bool QJsonArray::iterator::operator<(const iterator &o) const

    Returns \c true if this iterator is before \a o.
*/

/*!
    \fn bool QJsonArray::iterator::operator<=(const iterator &o) const

    Returns \c true if this iterator is before or at the same position as \a o.
*/

/*!
    \fn bool QJsonArray::iterator::operator>(const iterator &o) const

    Returns \c true if this iterator is after \a o.
*/

/*!
    \fn bool QJsonArray::iterator::operator>=(const iterator &o) const

    Returns \c true if this iterator is after or at the same position as \a o.
*/

/*!
    \fn bool QJsonArray::iterator::operator==(const const_iterator &o) const

    \overload
*/

/*!
    \fn bool QJsonArray::iterator::operator!=(const const_iterator &o) const

    \overload
*/

/*!
    \fn bool QJsonArray::iterator::operator<(const const_iterator &o) const

    \overload
*/

/*!
    \fn bool QJsonArray::iterator::operator<=(const const_iterator &o) const

    \overload
*/

/*!
    \fn bool QJsonArray::iterator::operator>(const const_iterator &o) const

    \overload
*/

/*!
    \fn bool QJsonArray::iterator::operator>=(const const_iterator &o) const

    \overload
*/

/*!
    \fn QJsonArray::iterator &QJsonArray::iterator::operator++()

    Advances the iterator to the next value and returns it.
*/

/*!
    \fn QJsonArray::iterator QJsonArray::iterator::operator++(int)

    \overload

    Advances the iterator and returns its previous position.
*/

/*!
    \fn QJsonArray::iterator &QJsonArray::iterator::operator--()

    Moves the iterator to the previous value and returns it.
*/

/*!
    \fn QJsonArray::iterator QJsonArray::iterator::operator--(int)

    \overload

    Moves the iterator back and returns its previous position.
*/

/*!
    \fn QJsonArray::iterator &QJsonArray::iterator::operator+=(qsizetype j)

    Advances the iterator by \a j values and returns it.
*/

/*!
    \fn QJsonArray::iterator &QJsonArray::iterator::operator-=(qsizetype j)

    Moves the iterator back by \a j values and returns it.
*/

/*!
    \fn QJsonArray::iterator QJsonArray::iterator::operator+(qsizetype j) const

    Returns an iterator \a j values after this one.
*/

/*!
    \fn QJsonArray::iterator QJsonArray::iterator::operator-(qsizetype j) const

    \overload

    Returns an iterator \a j values before this one.
*/

/*!
    \fn qsizetype QJsonArray::iterator::operator-(const iterator &o) const

    \overload

    Returns the number of values between \a o and this iterator.
*/

/*!
    \class QJsonArray::const_iterator
    \brief The QJsonArray::const_iterator class is an STL-style const iterator for QJsonArray.

    A random access iterator over the values of an array. Dereferencing it
    gives a QJsonValueConstRef.

    \sa QJsonArray::iterator
*/

/*!
    \typedef QJsonArray::const_iterator::iterator_category

    Marks the iterator as a random access iterator.
*/

/*!
    \typedef QJsonArray::const_iterator::difference_type

    Alias for \c qsizetype.
*/

/*!
    \typedef QJsonArray::const_iterator::value_type

    Alias for QJsonValue.
*/

/*!
    \typedef QJsonArray::const_iterator::reference

    Alias for QJsonValueConstRef.
*/

/*!
    \typedef QJsonArray::const_iterator::pointer

    The type \c{operator->()} returns.
*/

/*!
    \fn QJsonArray::const_iterator::const_iterator()

    Creates an iterator that refers to no array.
*/

/*!
    \fn QJsonArray::const_iterator::const_iterator(const QJsonArray *array, qsizetype index)

    Creates an iterator that refers to position \a index of \a array.
*/

/*!
    \fn QJsonArray::const_iterator::const_iterator(const iterator &o)

    Creates a const iterator that refers to the position of \a o.
*/

/*!
    \fn QJsonValueConstRef QJsonArray::const_iterator::operator*() const

    Returns a reference to the current value.
*/

/*!
    \fn QJsonRefArrow<QJsonValueConstRef> QJsonArray::const_iterator::operator->() const

    Returns a pointer-like helper for the current value.
*/

/*!
    \fn QJsonValueConstRef QJsonArray::const_iterator::operator[](qsizetype j) const

    Returns a reference to the value \a j positions after the current one.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator==(const const_iterator &o) const

    Returns \c true if \a o refers to the same position.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator!=(const const_iterator &o) const

    Returns \c true if \a o refers to another position.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator<(const const_iterator &o) const

    Returns \c true if this iterator is before \a o.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator<=(const const_iterator &o) const

    Returns \c true if this iterator is before or at the same position as \a o.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator>(const const_iterator &o) const

    Returns \c true if this iterator is after \a o.
*/

/*!
    \fn bool QJsonArray::const_iterator::operator>=(const const_iterator &o) const

    Returns \c true if this iterator is after or at the same position as \a o.
*/

/*!
    \fn QJsonArray::const_iterator &QJsonArray::const_iterator::operator++()

    Advances the iterator to the next value and returns it.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::const_iterator::operator++(int)

    \overload

    Advances the iterator and returns its previous position.
*/

/*!
    \fn QJsonArray::const_iterator &QJsonArray::const_iterator::operator--()

    Moves the iterator to the previous value and returns it.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::const_iterator::operator--(int)

    \overload

    Moves the iterator back and returns its previous position.
*/

/*!
    \fn QJsonArray::const_iterator &QJsonArray::const_iterator::operator+=(qsizetype j)

    Advances the iterator by \a j values and returns it.
*/

/*!
    \fn QJsonArray::const_iterator &QJsonArray::const_iterator::operator-=(qsizetype j)

    Moves the iterator back by \a j values and returns it.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::const_iterator::operator+(qsizetype j) const

    Returns an iterator \a j values after this one.
*/

/*!
    \fn QJsonArray::const_iterator QJsonArray::const_iterator::operator-(qsizetype j) const

    \overload

    Returns an iterator \a j values before this one.
*/

/*!
    \fn qsizetype QJsonArray::const_iterator::operator-(const const_iterator &o) const

    \overload

    Returns the number of values between \a o and this iterator.
*/
