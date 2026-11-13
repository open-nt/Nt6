// MIT License - see LICENSE
#pragma once

#include <QtCore/qcborvalue.h>
#include <QtCore/qkeyvalueiterator.h>
#include <QtCore/qlist.h>
#include <QtCore/qvariant.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <utility>

class QJsonObject;

class QCborMap {
public:
    /*! A pair of a key and a value. */
    using value_type = std::pair<QCborValue, QCborValue>;
    /*! The type of the keys. */
    using key_type = QCborValue;
    /*! The type of the values. */
    using mapped_type = QCborValue;
    /*! The type of a size. */
    using size_type = qsizetype;

    class ConstIterator;
    class Iterator {
    public:
        /*! The category of the iterator. */
        using iterator_category = std::random_access_iterator_tag;
        /*! The type of a distance between iterators. */
        using difference_type = qsizetype;
        /*! The type of a pair. */
        using value_type = std::pair<QCborValueConstRef, QCborValueRef>;
        /*! The type that dereferencing gives: the key, and the value that can be assigned to. */
        using reference = std::pair<QCborValueConstRef, QCborValueRef>;
        /*! A pointer to a reference to the value. */
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
            Returns the key and a reference to the value of the pair.
        */
        value_type operator*() const { return {key(), value()}; }
        /*!
            Returns the key and a reference to the value of the pair \a j places on.
        */
        value_type operator[](qsizetype j) const { return *(*this + j); }
        /*!
            Returns a pointer to a reference to the value of the pair.
        */
        QCborValueRef *operator->() {
            QtPrivate::CborRefAccess::pointer(current) = QtPrivate::CborRefAccess::pointer(item) + 1;
            return &current;
        }
        /*!
            Returns a pointer to a reference to the value of the pair that cannot be assigned through.
        */
        const QCborValueConstRef *operator->() const {
            QtPrivate::CborRefAccess::pointer(current) = QtPrivate::CborRefAccess::pointer(item) + 1;
            return &current;
        }
        /*!
            Returns a reference to the value of the pair; assigning to it changes the map.
        */
        QCborValueRef value() const {
            return QtPrivate::CborRefAccess::ref(QtPrivate::CborRefAccess::pointer(item) + 1);
        }
        /*!
            \overload
            Returns \c true if \a other points to the same pair.
        */
        bool operator==(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            \overload
            Returns \c true if \a other points to another pair.
        */
        bool operator!=(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item);
        }

        /*!
            Returns a reference to the key of the pair.
        */
        QCborValueConstRef key() const {
            return QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(item));
        }
        /*!
            Returns \c true if \a other points to the same pair.
        */
        bool operator==(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if \a other points to another pair.
        */
        bool operator!=(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points before \a other.
        */
        bool operator<(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) < QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points before \a other or to the same pair.
        */
        bool operator<=(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) <= QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points after \a other.
        */
        bool operator>(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) > QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points after \a other or to the same pair.
        */
        bool operator>=(const Iterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) >= QtPrivate::CborRefAccess::pointer(other.item);
        }

        /*!
            Moves to the next pair and returns this iterator.
        */
        Iterator &operator++() {
            QtPrivate::CborRefAccess::pointer(item) += 2;
            return *this;
        }
        /*!
            Moves to the next pair and returns an iterator to the pair before.
        */
        Iterator operator++(int) {
            Iterator old = *this;
            QtPrivate::CborRefAccess::pointer(item) += 2;
            return old;
        }
        /*!
            Moves to the pair before and returns this iterator.
        */
        Iterator &operator--() {
            QtPrivate::CborRefAccess::pointer(item) -= 2;
            return *this;
        }
        /*!
            Moves to the pair before and returns an iterator to the pair it was at.
        */
        Iterator operator--(int) {
            Iterator old = *this;
            QtPrivate::CborRefAccess::pointer(item) -= 2;
            return old;
        }
        /*!
            Moves \a j pairs on and returns this iterator.
        */
        Iterator &operator+=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) += 2 * j;
            return *this;
        }
        /*!
            Moves \a j pairs back and returns this iterator.
        */
        Iterator &operator-=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) -= 2 * j;
            return *this;
        }
        /*!
            Returns an iterator \a j pairs on.
        */
        Iterator operator+(qsizetype j) const { return Iterator(QtPrivate::CborRefAccess::pointer(item) + 2 * j); }
        /*!
            Returns an iterator \a j pairs back.
        */
        Iterator operator-(qsizetype j) const { return Iterator(QtPrivate::CborRefAccess::pointer(item) - 2 * j); }
        /*!
            Returns the number of pairs from \a j to this iterator.
        */
        qsizetype operator-(Iterator j) const {
            return (QtPrivate::CborRefAccess::pointer(item) - QtPrivate::CborRefAccess::pointer(j.item)) / 2;
        }

    private:
        friend class QCborMap;
        friend class ConstIterator;
        explicit Iterator(QCborValue *key) : item(QtPrivate::CborRefAccess::ref(key)) {}
        QCborValueRef item = QtPrivate::CborRefAccess::ref(nullptr);            // the key; the value follows it
        mutable QCborValueRef current = QtPrivate::CborRefAccess::ref(nullptr); // what operator->() points to
    };

    class ConstIterator {
    public:
        /*! The category of the iterator. */
        using iterator_category = std::random_access_iterator_tag;
        /*! The type of a distance between iterators. */
        using difference_type = qsizetype;
        /*! The type of a pair. */
        using value_type = std::pair<QCborValueConstRef, QCborValueConstRef>;
        /*! The type that dereferencing gives: the key and the value. */
        using reference = std::pair<QCborValueConstRef, QCborValueConstRef>;
        /*! A pointer to a reference to the value. */
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
        ConstIterator(const Iterator &other)
            : item(QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(other.item))) {}
        /*!
            Makes this iterator point where \a other does.
        */
        ConstIterator &operator=(const ConstIterator &other) {
            QtPrivate::CborRefAccess::pointer(item) = QtPrivate::CborRefAccess::pointer(other.item);
            return *this;
        }

        /*!
            Returns the key and the value of the pair.
        */
        value_type operator*() const { return {key(), value()}; }
        /*!
            Returns the key and the value of the pair \a j places on.
        */
        value_type operator[](qsizetype j) const { return *(*this + j); }
        /*!
            Returns a pointer to a reference to the value of the pair.
        */
        const QCborValueConstRef *operator->() const {
            QtPrivate::CborRefAccess::pointer(current) = QtPrivate::CborRefAccess::pointer(item) + 1;
            return &current;
        }
        /*!
            Returns a reference to the value of the pair.
        */
        QCborValueConstRef value() const {
            return QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(item) + 1);
        }
        /*!
            Returns a reference to the key of the pair.
        */
        QCborValueConstRef key() const {
            return QtPrivate::CborRefAccess::constRef(QtPrivate::CborRefAccess::pointer(item));
        }
        /*!
            Returns \c true if \a other points to the same pair.
        */
        bool operator==(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) == QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if \a other points to another pair.
        */
        bool operator!=(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) != QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points before \a other.
        */
        bool operator<(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) < QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points before \a other or to the same pair.
        */
        bool operator<=(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) <= QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points after \a other.
        */
        bool operator>(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) > QtPrivate::CborRefAccess::pointer(other.item);
        }
        /*!
            Returns \c true if this iterator points after \a other or to the same pair.
        */
        bool operator>=(const ConstIterator &other) const {
            return QtPrivate::CborRefAccess::pointer(item) >= QtPrivate::CborRefAccess::pointer(other.item);
        }

        /*!
            Moves to the next pair and returns this iterator.
        */
        ConstIterator &operator++() {
            QtPrivate::CborRefAccess::pointer(item) += 2;
            return *this;
        }
        /*!
            Moves to the next pair and returns an iterator to the pair before.
        */
        ConstIterator operator++(int) {
            ConstIterator old = *this;
            QtPrivate::CborRefAccess::pointer(item) += 2;
            return old;
        }
        /*!
            Moves to the pair before and returns this iterator.
        */
        ConstIterator &operator--() {
            QtPrivate::CborRefAccess::pointer(item) -= 2;
            return *this;
        }
        /*!
            Moves to the pair before and returns an iterator to the pair it was at.
        */
        ConstIterator operator--(int) {
            ConstIterator old = *this;
            QtPrivate::CborRefAccess::pointer(item) -= 2;
            return old;
        }
        /*!
            Moves \a j pairs on and returns this iterator.
        */
        ConstIterator &operator+=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) += 2 * j;
            return *this;
        }
        /*!
            Moves \a j pairs back and returns this iterator.
        */
        ConstIterator &operator-=(qsizetype j) {
            QtPrivate::CborRefAccess::pointer(item) -= 2 * j;
            return *this;
        }
        /*!
            Returns an iterator \a j pairs on.
        */
        ConstIterator operator+(qsizetype j) const {
            return ConstIterator(QtPrivate::CborRefAccess::pointer(item) + 2 * j);
        }
        /*!
            Returns an iterator \a j pairs back.
        */
        ConstIterator operator-(qsizetype j) const {
            return ConstIterator(QtPrivate::CborRefAccess::pointer(item) - 2 * j);
        }
        /*!
            Returns the number of pairs from \a j to this iterator.
        */
        qsizetype operator-(ConstIterator j) const {
            return (QtPrivate::CborRefAccess::pointer(item) - QtPrivate::CborRefAccess::pointer(j.item)) / 2;
        }

    private:
        friend class QCborMap;
        friend class Iterator;
        explicit ConstIterator(QCborValue *key) : item(QtPrivate::CborRefAccess::constRef(key)) {}
        QCborValueConstRef item = QtPrivate::CborRefAccess::constRef(nullptr); // the key; the value follows it
        mutable QCborValueConstRef current = QtPrivate::CborRefAccess::constRef(nullptr); // what operator->() points to
    };

    /*! The same as Iterator. */
    using iterator = Iterator;
    /*! The same as ConstIterator. */
    using const_iterator = ConstIterator;

    QCborMap() noexcept;
    QCborMap(std::initializer_list<value_type> args);
    QCborMap(const QCborMap &other);
    QCborMap(QCborMap &&other) noexcept;
    QCborMap &operator=(const QCborMap &other);
    QCborMap &operator=(QCborMap &&other) noexcept;
    ~QCborMap();

    void swap(QCborMap &other) noexcept;
    QCborValue toCborValue() const;

    qsizetype size() const noexcept;
    /*!
        Returns \c true if the map has no pairs.
    */
    bool isEmpty() const { return size() == 0; }
    /*!
        Returns \c true if the map has no pairs.
    */
    bool empty() const { return isEmpty(); }
    void clear();
    QList<QCborValue> keys() const;

    QCborValue value(qint64 key) const;
    QCborValue value(QLatin1StringView key) const;
    QCborValue value(const QString &key) const;
    QCborValue value(const QCborValue &key) const;
    const QCborValue operator[](qint64 key) const;
    const QCborValue operator[](QLatin1StringView key) const;
    const QCborValue operator[](const QString &key) const;
    const QCborValue operator[](const QCborValue &key) const;
    QCborValueRef operator[](qint64 key);
    QCborValueRef operator[](QLatin1StringView key);
    QCborValueRef operator[](const QString &key);
    QCborValueRef operator[](const QCborValue &key);

    QCborValue take(qint64 key);
    QCborValue take(QLatin1StringView key);
    QCborValue take(const QString &key);
    QCborValue take(const QCborValue &key);
    void remove(qint64 key);
    void remove(QLatin1StringView key);
    void remove(const QString &key);
    void remove(const QCborValue &key);
    bool contains(qint64 key) const;
    bool contains(QLatin1StringView key) const;
    bool contains(const QString &key) const;
    bool contains(const QCborValue &key) const;

    int compare(const QCborMap &other) const noexcept;
    /*!
        Returns \c true if \a a and \a b have equal pairs in the same order.
    */
    friend bool operator==(const QCborMap &a, const QCborMap &b) noexcept { return a.compare(b) == 0; }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QCborMap &a, const QCborMap &b) noexcept { return a.compare(b) != 0; }
    /*!
        Returns \c true if \a a comes before \a b in the order of compare().
    */
    friend bool operator<(const QCborMap &a, const QCborMap &b) noexcept { return a.compare(b) < 0; }

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

    /*!
        \typedef QCborMap::key_value_iterator
        \since 6.4
        An iterator over the entries that gives each one as a pair of its key and its value.
    */
    using key_value_iterator = QKeyValueIterator<QCborValueConstRef, QCborValueRef, Iterator>;
    /*!
        \typedef QCborMap::const_key_value_iterator
        \since 6.4
        A const iterator over the entries that gives each one as a pair of its key and its value.
    */
    using const_key_value_iterator = QKeyValueIterator<QCborValueConstRef, QCborValueConstRef, ConstIterator>;
    /*!
        \since 6.4
        Returns an iterator over the pairs of keys and values, at the first entry.

        \sa keyValueEnd(), asKeyValueRange()
    */
    key_value_iterator keyValueBegin() { return key_value_iterator(begin()); }
    /*!
        \since 6.4
        Returns an iterator over the pairs of keys and values, past the last entry.
    */
    key_value_iterator keyValueEnd() { return key_value_iterator(end()); }
    /*!
        \since 6.4
        \overload
    */
    const_key_value_iterator keyValueBegin() const { return const_key_value_iterator(begin()); }
    /*!
        \since 6.4
        \overload
    */
    const_key_value_iterator keyValueEnd() const { return const_key_value_iterator(end()); }
    /*!
        \since 6.4
        Returns a const iterator over the pairs of keys and values, at the first entry.
    */
    const_key_value_iterator constKeyValueBegin() const { return keyValueBegin(); }
    /*!
        \since 6.4
        Returns a const iterator over the pairs of keys and values, past the last entry.
    */
    const_key_value_iterator constKeyValueEnd() const { return keyValueEnd(); }
    /*!
        \since 6.4
        Returns a range for a range-based \c for loop over the pairs of keys and values, which can be taken apart
        with a structured binding:

        \code
        for (auto [key, value] : map.asKeyValueRange())
            qDebug() << key.toString() << value.toInteger();
        \endcode
    */
    auto asKeyValueRange() & { return QtPrivate::KeyValueRange<QCborMap &, key_value_iterator>(*this); }
    /*!
        \since 6.4
        \overload
    */
    auto asKeyValueRange() const & {
        return QtPrivate::KeyValueRange<const QCborMap &, const_key_value_iterator>(*this);
    }
    /*!
        \since 6.4
        \overload
    */
    auto asKeyValueRange() && { return QtPrivate::KeyValueRange<QCborMap, key_value_iterator>(std::move(*this)); }

    Iterator erase(Iterator it);
    Iterator erase(ConstIterator it);
    QCborValue extract(Iterator it);
    QCborValue extract(ConstIterator it);

    Iterator find(qint64 key);
    Iterator find(QLatin1StringView key);
    Iterator find(const QString &key);
    Iterator find(const QCborValue &key);
    ConstIterator find(qint64 key) const;
    ConstIterator find(QLatin1StringView key) const;
    ConstIterator find(const QString &key) const;
    ConstIterator find(const QCborValue &key) const;
    ConstIterator constFind(qint64 key) const;
    ConstIterator constFind(QLatin1StringView key) const;
    ConstIterator constFind(const QString &key) const;
    ConstIterator constFind(const QCborValue &key) const;

    Iterator insert(qint64 key, const QCborValue &value);
    Iterator insert(QLatin1StringView key, const QCborValue &value);
    Iterator insert(const QString &key, const QCborValue &value);
    Iterator insert(const QCborValue &key, const QCborValue &value);
    Iterator insert(const value_type &v);

    static QCborMap fromVariantMap(const QVariantMap &map);
    static QCborMap fromVariantHash(const QVariantHash &hash);
    static QCborMap fromJsonObject(const QJsonObject &o);
    QVariantMap toVariantMap() const;
    QVariantHash toVariantHash() const;
    QJsonObject toJsonObject() const;

private:
    qsizetype indexOf(const QCborValue &key) const; // of the value, or -1
    QList<QCborValue> m_items;                      // keys and values, one after the other
};
