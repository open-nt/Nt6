// MIT License - see LICENSE
#include <QtCore/qcborarray.h>

#include <QtCore/qjsonarray.h>
#include <QtCore/qjsonvalue.h>

#include <utility>

/*!
    \class QCborArray
    \brief The QCborArray class is used to hold an array of CBOR elements.
    \since 5.12

    A QCborArray is a list of QCborValue, which may be of any type, nested arrays and maps among
    them. It is what QCborValue::toArray() returns, and becomes a QCborValue again with
    toCborValue() or the QCborValue constructor.

    \code
    QCborArray array{1, u"two"_s, 3.5};
    array.append(QCborMap{{u"key"_s, true}});
    array[0] = 10;
    \endcode

    The operator[] that is not const and the iterators give a QCborValueRef, through which an
    element is changed in place. An element that is read past the end is \c undefined.

    \differences
    Copies are deep: a copy of an array copies its elements, which share their own strings and
    containers. There is no \c QDebug operator, and no \c QDataStream operators.
    \enddifferences

    \sa QCborMap, QCborValue
*/

/*!
    \class QCborArray::Iterator
    \brief The Iterator class is the iterator of QCborArray that can change elements.
    \since 5.12

    Dereferencing it gives a QCborValueRef; assigning to that changes the element.

    \differences
    The iterator is invalid once the array gets or loses elements.
    \enddifferences
*/

/*!
    \class QCborArray::ConstIterator
    \brief The ConstIterator class is the iterator of QCborArray that reads elements.
    \since 5.12

    \differences
    The iterator is invalid once the array gets or loses elements.
    \enddifferences
*/

/*!
    Constructs an empty array.
*/
QCborArray::QCborArray() noexcept = default;

/*!
    Constructs an array of the values in \a args.
*/
QCborArray::QCborArray(std::initializer_list<QCborValue> args) {
    for (const QCborValue &value : args)
        m_items.append(value);
}

/*!
    Constructs a copy of \a other.
*/
QCborArray::QCborArray(const QCborArray &other) = default;

/*!
    Move-constructs an array from \a other, which becomes empty.
*/
QCborArray::QCborArray(QCborArray &&other) noexcept : m_items(std::move(other.m_items)) {}

/*!
    Assigns \a other to this array and returns a reference to it.
*/
QCborArray &QCborArray::operator=(const QCborArray &other) = default;

/*!
    Move-assigns \a other to this array and returns a reference to it.
*/
QCborArray &QCborArray::operator=(QCborArray &&other) noexcept {
    m_items = std::move(other.m_items);
    return *this;
}

/*!
    Destroys the array.
*/
QCborArray::~QCborArray() = default;

/*!
    Exchanges this array with \a other.
*/
void QCborArray::swap(QCborArray &other) noexcept { m_items.swap(other.m_items); }

/*!
    Returns the array as a QCborValue.
*/
QCborValue QCborArray::toCborValue() const { return QCborValue(*this); }

/*!
    Returns the number of elements.
*/
qsizetype QCborArray::size() const noexcept { return m_items.size(); }

/*!
    Removes all elements.
*/
void QCborArray::clear() { m_items.clear(); }

/*!
    Returns the element at \a i, or \c undefined if there is none.
*/
QCborValue QCborArray::at(qsizetype i) const { return i >= 0 && i < size() ? m_items.at(i) : QCborValue(); }

/*!
    Returns the first element, or \c undefined if the array is empty.
*/
QCborValue QCborArray::first() const { return at(0); }

/*!
    Returns the last element, or \c undefined if the array is empty.
*/
QCborValue QCborArray::last() const { return at(size() - 1); }

/*!
    Returns the element at \a i, or \c undefined if there is none.
*/
const QCborValue QCborArray::operator[](qsizetype i) const { return at(i); }

/*!
    \overload
    Returns a reference to the first element. The array must not be empty.
*/
QCborValueRef QCborArray::first() { return QCborValueRef(m_items.data()); }

/*!
    \overload
    Returns a reference to the last element. The array must not be empty.
*/
QCborValueRef QCborArray::last() { return QCborValueRef(m_items.data() + size() - 1); }

/*!
    \overload
    Returns a reference to the element at \a i. An array that is shorter grows to \a i + 1
    elements, with \c undefined in between.
*/
QCborValueRef QCborArray::operator[](qsizetype i) {
    while (size() <= i)
        m_items.append(QCborValue());
    return QCborValueRef(m_items.data() + i);
}

/*!
    Inserts \a value at \a i. With a negative \a i, or one at the end, \a value is appended; an
    \a i beyond the end makes the array grow with \c undefined elements up to it.

    \sa append(), prepend()
*/
void QCborArray::insert(qsizetype i, const QCborValue &value) {
    if (i < 0 || i >= size()) {
        while (i > size())
            m_items.append(QCborValue());
        m_items.append(value);
        return;
    }
    m_items.insert(i, value);
}

/*!
    Removes the element at \a it and returns it.
*/
QCborValue QCborArray::extract(ConstIterator it) { return takeAt(it - constBegin()); }

/*!
    \overload
    Removes the element at \a it and returns it.
*/
QCborValue QCborArray::extract(Iterator it) { return takeAt(it - begin()); }

/*!
    Removes the element at \a i, if there is one.
*/
void QCborArray::removeAt(qsizetype i) {
    if (i >= 0 && i < size())
        m_items.removeAt(i);
}

/*!
    Removes the element at \a i and returns it, or returns \c undefined if there is none.
*/
QCborValue QCborArray::takeAt(qsizetype i) {
    if (i < 0 || i >= size())
        return QCborValue();
    QCborValue value = m_items.at(i);
    m_items.removeAt(i);
    return value;
}

/*!
    Returns \c true if an element is equal to \a value.
*/
bool QCborArray::contains(const QCborValue &value) const {
    for (const QCborValue &element : m_items) {
        if (element == value)
            return true;
    }
    return false;
}

/*!
    Compares this array with \a other: a shorter array comes first, and arrays of the same length
    are compared element by element. Returns a negative number, 0 or a positive number.

    \sa QCborValue::compare()
*/
int QCborArray::compare(const QCborArray &other) const noexcept {
    if (size() != other.size())
        return size() < other.size() ? -1 : 1;
    for (qsizetype i = 0; i < size(); ++i) {
        const int c = m_items.at(i).compare(other.m_items.at(i));
        if (c != 0)
            return c;
    }
    return 0;
}

/*!
    Returns an iterator to the first element.
*/
QCborArray::Iterator QCborArray::begin() { return Iterator(m_items.data()); }

/*!
    \overload
*/
QCborArray::ConstIterator QCborArray::begin() const { return constBegin(); }

/*!
    Returns a const iterator to the first element.
*/
QCborArray::ConstIterator QCborArray::constBegin() const {
    return ConstIterator(const_cast<QCborValue *>(m_items.constData()));
}

/*!
    Returns an iterator past the last element.
*/
QCborArray::Iterator QCborArray::end() { return Iterator(m_items.data() + size()); }

/*!
    \overload
*/
QCborArray::ConstIterator QCborArray::end() const { return constEnd(); }

/*!
    Returns a const iterator past the last element.
*/
QCborArray::ConstIterator QCborArray::constEnd() const {
    return ConstIterator(const_cast<QCborValue *>(m_items.constData()) + size());
}

/*!
    Inserts \a value before \a before and returns an iterator to it.
*/
QCborArray::Iterator QCborArray::insert(Iterator before, const QCborValue &value) {
    const qsizetype i = before - begin();
    m_items.insert(i, value);
    return begin() + i;
}

/*!
    \overload
    Inserts \a value before \a before and returns an iterator to it.
*/
QCborArray::Iterator QCborArray::insert(ConstIterator before, const QCborValue &value) {
    const qsizetype i = before - constBegin();
    m_items.insert(i, value);
    return begin() + i;
}

/*!
    Removes the element at \a it and returns an iterator to the element after it.
*/
QCborArray::Iterator QCborArray::erase(Iterator it) {
    const qsizetype i = it - begin();
    removeAt(i);
    return begin() + i;
}

/*!
    \overload
    Removes the element at \a it and returns an iterator to the element after it.
*/
QCborArray::Iterator QCborArray::erase(ConstIterator it) {
    const qsizetype i = it - constBegin();
    removeAt(i);
    return begin() + i;
}

/*!
    Returns an array of the strings in \a list.
*/
QCborArray QCborArray::fromStringList(const QStringList &list) {
    QCborArray array;
    for (const QString &s : list)
        array.append(QCborValue(s));
    return array;
}

/*!
    Returns an array of the values in \a list, each converted by QCborValue::fromVariant().
*/
QCborArray QCborArray::fromVariantList(const QVariantList &list) {
    QCborArray array;
    for (const QVariant &v : list)
        array.append(QCborValue::fromVariant(v));
    return array;
}

/*!
    Returns an array of the values in \a array, each converted by QCborValue::fromJsonValue().
*/
QCborArray QCborArray::fromJsonArray(const QJsonArray &array) {
    QCborArray result;
    for (qsizetype i = 0; i < array.size(); ++i)
        result.append(QCborValue::fromJsonValue(array.at(i)));
    return result;
}

/*!
    Returns the elements converted by QCborValue::toVariant().
*/
QVariantList QCborArray::toVariantList() const {
    QVariantList list;
    for (const QCborValue &v : m_items)
        list.append(v.toVariant());
    return list;
}

/*!
    Returns the elements converted by QCborValue::toJsonValue().
*/
QJsonArray QCborArray::toJsonArray() const {
    QJsonArray array;
    for (const QCborValue &v : m_items)
        array.append(v.toJsonValue());
    return array;
}
