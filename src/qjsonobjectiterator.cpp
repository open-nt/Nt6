// MIT License - see LICENSE
#include <QtCore/qjsonobject.h>

#include "qjson_p.h"

/*!
    \class QJsonObject::iterator
    \brief The QJsonObject::iterator class is an STL-style non-const iterator for QJsonObject.

    A random access iterator over the entries of an object, in the sorted order of their
    keys. key() returns the key and the dereferenced value is a QJsonValueRef.

    \sa QJsonObject::const_iterator
*/

/*!
    \typedef QJsonObject::iterator::iterator_category

    Marks the iterator as a random access iterator.
*/

/*!
    \typedef QJsonObject::iterator::difference_type

    Alias for \c qsizetype.
*/

/*!
    \typedef QJsonObject::iterator::value_type

    Alias for QJsonValue.
*/

/*!
    \typedef QJsonObject::iterator::reference

    Alias for QJsonValueRef.
*/

/*!
    \typedef QJsonObject::iterator::pointer

    The type \c{operator->()} returns.
*/

/*!
    \fn QJsonObject::iterator::iterator()

    Creates an iterator that refers to no object.
*/

/*!
    \fn QJsonObject::iterator::iterator(QJsonObject *object, qsizetype index)

    Creates an iterator that refers to entry \a index of \a object.
*/

/*!
    Returns the key of the current entry.
*/
QString QJsonObject::iterator::key() const {
    return QJsonPrivateAccess::data(*o)->items[static_cast<std::size_t>(i)].key;
}

/*!
    \fn QJsonValueRef QJsonObject::iterator::value() const

    Returns a reference to the value of the current entry.
*/

/*!
    \fn QJsonValueRef QJsonObject::iterator::operator*() const

    Returns a reference to the value of the current entry.
*/

/*!
    \fn QJsonRefArrow<QJsonValueRef> QJsonObject::iterator::operator->() const

    Returns a pointer-like helper for the value of the current entry.
*/

/*!
    \fn QJsonValueRef QJsonObject::iterator::operator[](qsizetype j) const

    Returns a reference to the value of the entry \a j positions after the current one.
*/

/*!
    \fn bool QJsonObject::iterator::operator==(const iterator &other) const

    Returns \c true if \a other refers to the same entry.
*/

/*!
    \fn bool QJsonObject::iterator::operator!=(const iterator &other) const

    Returns \c true if \a other refers to another entry.
*/

/*!
    \fn bool QJsonObject::iterator::operator<(const iterator &other) const

    Returns \c true if this iterator is before \a other.
*/

/*!
    \fn bool QJsonObject::iterator::operator<=(const iterator &other) const

    Returns \c true if this iterator is before or at the same position as \a other.
*/

/*!
    \fn bool QJsonObject::iterator::operator>(const iterator &other) const

    Returns \c true if this iterator is after \a other.
*/

/*!
    \fn bool QJsonObject::iterator::operator>=(const iterator &other) const

    Returns \c true if this iterator is after or at the same position as \a other.
*/

/*!
    \fn bool QJsonObject::iterator::operator==(const const_iterator &other) const

    \overload
*/

/*!
    \fn bool QJsonObject::iterator::operator!=(const const_iterator &other) const

    \overload
*/

/*!
    \fn bool QJsonObject::iterator::operator<(const const_iterator &other) const

    \overload
*/

/*!
    \fn bool QJsonObject::iterator::operator<=(const const_iterator &other) const

    \overload
*/

/*!
    \fn bool QJsonObject::iterator::operator>(const const_iterator &other) const

    \overload
*/

/*!
    \fn bool QJsonObject::iterator::operator>=(const const_iterator &other) const

    \overload
*/

/*!
    \fn QJsonObject::iterator &QJsonObject::iterator::operator++()

    Advances the iterator to the next entry and returns it.
*/

/*!
    \fn QJsonObject::iterator QJsonObject::iterator::operator++(int)

    \overload

    Advances the iterator and returns its previous position.
*/

/*!
    \fn QJsonObject::iterator &QJsonObject::iterator::operator--()

    Moves the iterator to the previous entry and returns it.
*/

/*!
    \fn QJsonObject::iterator QJsonObject::iterator::operator--(int)

    \overload

    Moves the iterator back and returns its previous position.
*/

/*!
    \fn QJsonObject::iterator &QJsonObject::iterator::operator+=(qsizetype j)

    Advances the iterator by \a j entries and returns it.
*/

/*!
    \fn QJsonObject::iterator &QJsonObject::iterator::operator-=(qsizetype j)

    Moves the iterator back by \a j entries and returns it.
*/

/*!
    \fn QJsonObject::iterator QJsonObject::iterator::operator+(qsizetype j) const

    Returns an iterator \a j entries after this one.
*/

/*!
    \fn QJsonObject::iterator QJsonObject::iterator::operator-(qsizetype j) const

    \overload

    Returns an iterator \a j entries before this one.
*/

/*!
    \fn qsizetype QJsonObject::iterator::operator-(const iterator &other) const

    \overload

    Returns the number of entries between \a other and this iterator.
*/

/*!
    \class QJsonObject::const_iterator
    \brief The QJsonObject::const_iterator class is an STL-style const iterator for QJsonObject.

    A random access iterator over the entries of an object, in the sorted order of their
    keys. key() returns the key and the dereferenced value is a QJsonValueConstRef.

    \sa QJsonObject::iterator
*/

/*!
    \typedef QJsonObject::const_iterator::iterator_category

    Marks the iterator as a random access iterator.
*/

/*!
    \typedef QJsonObject::const_iterator::difference_type

    Alias for \c qsizetype.
*/

/*!
    \typedef QJsonObject::const_iterator::value_type

    Alias for QJsonValue.
*/

/*!
    \typedef QJsonObject::const_iterator::reference

    Alias for QJsonValueConstRef.
*/

/*!
    \typedef QJsonObject::const_iterator::pointer

    The type \c{operator->()} returns.
*/

/*!
    \fn QJsonObject::const_iterator::const_iterator()

    Creates an iterator that refers to no object.
*/

/*!
    \fn QJsonObject::const_iterator::const_iterator(const QJsonObject *object, qsizetype index)

    Creates an iterator that refers to entry \a index of \a object.
*/

/*!
    \fn QJsonObject::const_iterator::const_iterator(const iterator &other)

    Creates a const iterator that refers to the entry of \a other.
*/

/*!
    Returns the key of the current entry.
*/
QString QJsonObject::const_iterator::key() const {
    return QJsonPrivateAccess::data(*o)->items[static_cast<std::size_t>(i)].key;
}

/*!
    \fn QJsonValueConstRef QJsonObject::const_iterator::value() const

    Returns a reference to the value of the current entry.
*/

/*!
    \fn QJsonValueConstRef QJsonObject::const_iterator::operator*() const

    Returns a reference to the value of the current entry.
*/

/*!
    \fn QJsonRefArrow<QJsonValueConstRef> QJsonObject::const_iterator::operator->() const

    Returns a pointer-like helper for the value of the current entry.
*/

/*!
    \fn QJsonValueConstRef QJsonObject::const_iterator::operator[](qsizetype j) const

    Returns a reference to the value of the entry \a j positions after the current one.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator==(const const_iterator &other) const

    Returns \c true if \a other refers to the same entry.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator!=(const const_iterator &other) const

    Returns \c true if \a other refers to another entry.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator<(const const_iterator &other) const

    Returns \c true if this iterator is before \a other.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator<=(const const_iterator &other) const

    Returns \c true if this iterator is before or at the same position as \a other.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator>(const const_iterator &other) const

    Returns \c true if this iterator is after \a other.
*/

/*!
    \fn bool QJsonObject::const_iterator::operator>=(const const_iterator &other) const

    Returns \c true if this iterator is after or at the same position as \a other.
*/

/*!
    \fn QJsonObject::const_iterator &QJsonObject::const_iterator::operator++()

    Advances the iterator to the next entry and returns it.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::const_iterator::operator++(int)

    \overload

    Advances the iterator and returns its previous position.
*/

/*!
    \fn QJsonObject::const_iterator &QJsonObject::const_iterator::operator--()

    Moves the iterator to the previous entry and returns it.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::const_iterator::operator--(int)

    \overload

    Moves the iterator back and returns its previous position.
*/

/*!
    \fn QJsonObject::const_iterator &QJsonObject::const_iterator::operator+=(qsizetype j)

    Advances the iterator by \a j entries and returns it.
*/

/*!
    \fn QJsonObject::const_iterator &QJsonObject::const_iterator::operator-=(qsizetype j)

    Moves the iterator back by \a j entries and returns it.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::const_iterator::operator+(qsizetype j) const

    Returns an iterator \a j entries after this one.
*/

/*!
    \fn QJsonObject::const_iterator QJsonObject::const_iterator::operator-(qsizetype j) const

    \overload

    Returns an iterator \a j entries before this one.
*/

/*!
    \fn qsizetype QJsonObject::const_iterator::operator-(const const_iterator &other) const

    \overload

    Returns the number of entries between \a other and this iterator.
*/
