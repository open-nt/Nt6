// MIT License - see LICENSE
#pragma once

#include <QtCore/qcborvalue.h>
#include <QtCore/qlist.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>

class QJsonArray;

class QCborArray {
public:
    class ConstIterator;
    class Iterator {
    public:
        /*! The category of the iterator. */
        using iterator_category = std::random_access_iterator_tag;
        /*! The type of a distance between iterators. */
        using difference_type = qsizetype;
        /*! The type of the elements. */
        using value_type = QCborValue;
        /*! The type that dereferencing gives: a reference that can be assigned to. */
        using reference = QCborValueRef;
        /*! A pointer to such a reference. */
        using pointer = QCborValueRef *;

        /*!
            Constructs an iterator that points nowhere.
        */
        Iterator() = default;
        /*!
            Constructs a copy of \a other.
        */
        Iterator(const Iterator &other) = default;
        /*!
            Makes this iterator point where \a other does.
        */
        Iterator &operator=(const Iterator &other) {
            QtPrivate::CborRefAccess::pointer(item) = QtPrivate::CborRefAccess::pointer(other.item);
            return *this;
        }

        /*!
            Returns a reference to the element; assigning to it changes the array.
        */
        QCborValueRef operator*() const { return item; }
        /*!
            Returns a pointer to a reference to the element.
        */
        QCborValueRef *operator->() { return &item; }
        /*!
            Returns a pointer to a reference to the element that cannot be assigned through.
        */
        const QCborValueConstRef *operator->() const { return &item; }
        /*!
            Returns a reference to the element \a j places on.
        */
        QCborValueRef operator[](qsizetype j) const { return QtPrivate::CborRefAccess::ref(QtPrivate::CborRefAccess::pointer(item) + j); }

        /*!
            Returns \c true if \a other points to the same element.
        */
        bool operator==(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if \a other points to another element.
        */
        bool operator!=(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points before \a other.
        */
        bool operator<(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) < QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points before \a other or to the same element.
        */
        bool operator<=(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) <= QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points after \a other.
        */
        bool operator>(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) > QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points after \a other or to the same element.
        */
        bool operator>=(const Iterator &other) const { return QtPrivate::CborRefAccess::pointer(item) >= QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            \overload
            Returns \c true if \a other points to the same element.
        */
        bool operator==(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            \overload
            Returns \c true if \a other points to another element.
        */
        bool operator!=(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item); }

        /*!
            Moves to the next element and returns this iterator.
        */
        Iterator &operator++() {
            ++QtPrivate::CborRefAccess::pointer(item);
            return *this;
        }
        /*!
            Moves to the next element and returns an iterator to the element before.
        */
        Iterator operator++(int) {
            Iterator old = *this;
            ++QtPrivate::CborRefAccess::pointer(item);
            return old;
        }
        /*!
            Moves to the element before and returns this iterator.
        */
        Iterator &operator--() {
            --QtPrivate::CborRefAccess::pointer(item);
            return *this;
        }
        /*!
            Moves to the element before and returns an iterator to the element it was at.
        */
        Iterator operator--(int) {
            Iterator old = *this;
            --QtPrivate::CborRefAccess::pointer(item);
            return old;
        }
        /*!
            Moves \a j elements on and returns this iterator.
        */
        Iterator &operator+=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) += j;
            return *this;
        }
        /*!
            Moves \a j elements back and returns this iterator.
        */
        Iterator &operator-=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) -= j;
            return *this;
        }
        /*!
            Returns an iterator \a j elements on.
        */
        Iterator operator+(qsizetype j) const { return Iterator(QtPrivate::CborRefAccess::pointer(item) + j); }
        /*!
            Returns an iterator \a j elements back.
        */
        Iterator operator-(qsizetype j) const { return Iterator(QtPrivate::CborRefAccess::pointer(item) - j); }
        /*!
            Returns the number of elements from \a j to this iterator.
        */
        qsizetype operator-(Iterator j) const { return QtPrivate::CborRefAccess::pointer(item) - QtPrivate::CborRefAccess::pointer(j.item); }

    private:
        friend class QCborArray;
        friend class ConstIterator;
        explicit Iterator(QCborValue *v) : item(QtPrivate::CborRefAccess::ref(v)) {}
        QCborValueRef item = QtPrivate::CborRefAccess::ref(nullptr);
    };

    class ConstIterator {
    public:
        /*! The category of the iterator. */
        using iterator_category = std::random_access_iterator_tag;
        /*! The type of a distance between iterators. */
        using difference_type = qsizetype;
        /*! The type of the elements. */
        using value_type = const QCborValue;
        /*! The type that dereferencing gives. */
        using reference = const QCborValueConstRef;
        /*! A pointer to such a reference. */
        using pointer = const QCborValueConstRef *;

        /*!
            Constructs an iterator that points nowhere.
        */
        ConstIterator() = default;
        /*!
            Constructs a copy of \a other.
        */
        ConstIterator(const ConstIterator &other) = default;
        /*!
            Constructs a const iterator that points where \a other does.
        */
        ConstIterator(const Iterator &other) : item(QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(other.item))) {}
        /*!
            Makes this iterator point where \a other does.
        */
        ConstIterator &operator=(const ConstIterator &other) {
            QtPrivate::CborRefAccess::pointer(item) = QtPrivate::CborRefAccess::pointer(other.item);
            return *this;
        }

        /*!
            Returns a reference to the element.
        */
        QCborValueConstRef operator*() const { return item; }
        /*!
            Returns a pointer to a reference to the element.
        */
        const QCborValueConstRef *operator->() const { return &item; }
        /*!
            Returns a reference to the element \a j places on.
        */
        QCborValueConstRef operator[](qsizetype j) const { return QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(item) + j); }

        /*!
            Returns \c true if \a other points to the same element.
        */
        bool operator==(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if \a other points to another element.
        */
        bool operator!=(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points before \a other.
        */
        bool operator<(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) < QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points before \a other or to the same element.
        */
        bool operator<=(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) <= QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points after \a other.
        */
        bool operator>(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) > QtPrivate::CborRefAccess::pointer(other.item); }
        /*!
            Returns \c true if this iterator points after \a other or to the same element.
        */
        bool operator>=(const ConstIterator &other) const { return QtPrivate::CborRefAccess::pointer(item) >= QtPrivate::CborRefAccess::pointer(other.item); }

        /*!
            Moves to the next element and returns this iterator.
        */
        ConstIterator &operator++() {
            ++QtPrivate::CborRefAccess::pointer(item);
            return *this;
        }
        /*!
            Moves to the next element and returns an iterator to the element before.
        */
        ConstIterator operator++(int) {
            ConstIterator old = *this;
            ++QtPrivate::CborRefAccess::pointer(item);
            return old;
        }
        /*!
            Moves to the element before and returns this iterator.
        */
        ConstIterator &operator--() {
            --QtPrivate::CborRefAccess::pointer(item);
            return *this;
        }
        /*!
            Moves to the element before and returns an iterator to the element it was at.
        */
        ConstIterator operator--(int) {
            ConstIterator old = *this;
            --QtPrivate::CborRefAccess::pointer(item);
            return old;
        }
        /*!
            Moves \a j elements on and returns this iterator.
        */
        ConstIterator &operator+=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) += j;
            return *this;
        }
        /*!
            Moves \a j elements back and returns this iterator.
        */
        ConstIterator &operator-=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) -= j;
            return *this;
        }
        /*!
            Returns an iterator \a j elements on.
        */
        ConstIterator operator+(qsizetype j) const { return ConstIterator(QtPrivate::CborRefAccess::pointer(item) + j); }
        /*!
            Returns an iterator \a j elements back.
        */
        ConstIterator operator-(qsizetype j) const { return ConstIterator(QtPrivate::CborRefAccess::pointer(item) - j); }
        /*!
            Returns the number of elements from \a j to this iterator.
        */
        qsizetype operator-(ConstIterator j) const { return QtPrivate::CborRefAccess::pointer(item) - QtPrivate::CborRefAccess::pointer(j.item); }

    private:
        friend class QCborArray;
        friend class Iterator;
        explicit ConstIterator(QCborValue *v) : item(QtPrivate::CborRefAccess::constRef(v)) {}
        QCborValueConstRef item = QtPrivate::CborRefAccess::constRef(nullptr);
    };

    /*! The type of a size. */
    using size_type = qsizetype;
    /*! The type of the elements. */
    using value_type = QCborValue;
    /*! A pointer to an element. */
    using pointer = value_type *;
    /*! A pointer to an element that cannot be changed. */
    using const_pointer = const value_type *;
    /*! A reference to an element that can be assigned to. */
    using reference = QCborValueRef;
    /*! A copy of an element. */
    using const_reference = const QCborValue;
    /*! The type of a distance between iterators. */
    using difference_type = qsizetype;
    /*! The same as Iterator. */
    using iterator = Iterator;
    /*! The same as ConstIterator. */
    using const_iterator = ConstIterator;

    QCborArray() noexcept;
    QCborArray(std::initializer_list<QCborValue> args);
    QCborArray(const QCborArray &other);
    QCborArray(QCborArray &&other) noexcept;
    QCborArray &operator=(const QCborArray &other);
    QCborArray &operator=(QCborArray &&other) noexcept;
    ~QCborArray();

    void swap(QCborArray &other) noexcept;
    QCborValue toCborValue() const;

    qsizetype size() const noexcept;
    /*!
        Returns \c true if the array has no elements.
    */
    bool isEmpty() const { return size() == 0; }
    void clear();

    QCborValue at(qsizetype i) const;
    QCborValue first() const;
    QCborValue last() const;
    const QCborValue operator[](qsizetype i) const;
    QCborValueRef first();
    QCborValueRef last();
    QCborValueRef operator[](qsizetype i);

    void insert(qsizetype i, const QCborValue &value);
    QCborValue extract(ConstIterator it);
    QCborValue extract(Iterator it);
    void removeAt(qsizetype i);
    QCborValue takeAt(qsizetype i);
    /*!
        Removes the first element. The array must not be empty.
    */
    void removeFirst() { removeAt(0); }
    /*!
        Removes the last element. The array must not be empty.
    */
    void removeLast() { removeAt(size() - 1); }
    /*!
        Removes the first element and returns it. The array must not be empty.
    */
    QCborValue takeFirst() { return takeAt(0); }
    /*!
        Removes the last element and returns it. The array must not be empty.
    */
    QCborValue takeLast() { return takeAt(size() - 1); }

    bool contains(const QCborValue &value) const;
    int compare(const QCborArray &other) const noexcept;
    /*!
        Returns \c true if \a a and \a b have equal elements in the same order.
    */
    friend bool operator==(const QCborArray &a, const QCborArray &b) noexcept { return a.compare(b) == 0; }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QCborArray &a, const QCborArray &b) noexcept { return a.compare(b) != 0; }
    /*!
        Returns \c true if \a a comes before \a b in the order of compare().
    */
    friend bool operator<(const QCborArray &a, const QCborArray &b) noexcept { return a.compare(b) < 0; }

    Iterator begin();
    ConstIterator begin() const;
    ConstIterator constBegin() const;
    /*!
        Same as constBegin().
    */
    ConstIterator cbegin() const { return constBegin(); }
    Iterator end();
    ConstIterator end() const;
    ConstIterator constEnd() const;
    /*!
        Same as constEnd().
    */
    ConstIterator cend() const { return constEnd(); }
    Iterator insert(Iterator before, const QCborValue &value);
    Iterator insert(ConstIterator before, const QCborValue &value);
    Iterator erase(Iterator it);
    Iterator erase(ConstIterator it);

    /*!
        Appends \a t to the array.
    */
    void push_back(const QCborValue &t) { append(t); }
    /*!
        Puts \a t in front of the array.
    */
    void push_front(const QCborValue &t) { prepend(t); }
    /*!
        Removes the first element.
    */
    void pop_front() { removeFirst(); }
    /*!
        Removes the last element.
    */
    void pop_back() { removeLast(); }
    /*!
        Returns \c true if the array has no elements.
    */
    bool empty() const { return isEmpty(); }

    /*!
        Puts \a value in front of the array.
    */
    void prepend(const QCborValue &value) { insert(0, value); }
    /*!
        Appends \a value to the array.
    */
    void append(const QCborValue &value) { insert(-1, value); }

    /*!
        Returns a copy of the array with \a v appended.
    */
    QCborArray operator+(const QCborValue &v) const {
        QCborArray n = *this;
        n.append(v);
        return n;
    }
    /*!
        Appends \a v and returns a reference to this array.
    */
    QCborArray &operator+=(const QCborValue &v) {
        append(v);
        return *this;
    }
    /*!
        Appends \a v and returns a reference to this array.
    */
    QCborArray &operator<<(const QCborValue &v) {
        append(v);
        return *this;
    }

    static QCborArray fromStringList(const QStringList &list);
    static QCborArray fromVariantList(const QVariantList &list);
    static QCborArray fromJsonArray(const QJsonArray &array);
    QVariantList toVariantList() const;
    QJsonArray toJsonArray() const;

private:
    QList<QCborValue> m_items;
};
