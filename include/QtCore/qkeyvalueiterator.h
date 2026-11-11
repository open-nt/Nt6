// MIT License - see LICENSE
#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

/*!
    \class QKeyValueIterator
    \since 5.10
    \brief The QKeyValueIterator class is an iterator over the key and value pairs of an associative container.

    keyValueBegin() and keyValueEnd() of QMap, QMultiMap, QHash and QMultiHash
    return this iterator. Dereferencing it gives a \c std::pair of the key and
    the value, so standard algorithms see both:

    \code
    QMap<QString, int> ages{{u"ada"_s, 36}, {u"alan"_s, 41}};
    auto oldest = std::max_element(ages.keyValueBegin(), ages.keyValueEnd(),
                                   [](const auto &a, const auto &b) { return a.second < b.second; });
    \endcode

    \c asKeyValueRange() wraps the same iterators in a range for a range-based
    \c for loop with structured bindings:

    \code
    for (auto [name, age] : ages.asKeyValueRange())
        qDebug() << name << age;
    \endcode

    \differences
    The iterator is a forward and backward iterator for every container,
    whatever the category of the container's own iterator. \c operator->()
    returns a proxy that holds the pair.
    \enddifferences

    \sa QMap, QHash
*/
template <typename Key, typename T, typename Iterator> class QKeyValueIterator {
public:
    /*! \typedef QKeyValueIterator::iterator_category

        The iterator category, \c{std::bidirectional_iterator_tag}. */
    using iterator_category = std::bidirectional_iterator_tag;
    /*! \typedef QKeyValueIterator::difference_type

        The type of the distance between two iterators. */
    using difference_type = std::ptrdiff_t;
    /*! \typedef QKeyValueIterator::value_type

        The type of an item, \c{std::pair<Key, T>}. */
    using value_type = std::pair<Key, T>;
    /*! \typedef QKeyValueIterator::reference

        The type returned by operator*(), the same as value_type. */
    using reference = value_type;

    /*!
        A proxy returned by operator->() that holds the pair of the current
        item.
    */
    struct pointer {
        /*! The pair of the item that the proxy was created for. */
        value_type pair;
        /*!
            Returns a pointer to the pair.
        */
        value_type *operator->() noexcept { return &pair; }
    };

    /*!
        Constructs a default iterator, which must not be dereferenced.
    */
    QKeyValueIterator() = default;
    /*!
        Constructs an iterator over the item that \a o refers to.
    */
    constexpr explicit QKeyValueIterator(Iterator o) : i(o) {}

    /*!
        Returns the key and the value of the current item.
    */
    value_type operator*() const { return value_type(keyOf(i, 0), i.value()); }
    /*!
        Returns a proxy for accessing the key and the value of the current
        item through \c{->first} and \c{->second}.
    */
    pointer operator->() const { return pointer{**this}; }

    /*!
        Moves the iterator to the next item and returns it.
    */
    QKeyValueIterator &operator++() {
        ++i;
        return *this;
    }
    /*!
        \overload
        Moves the iterator to the next item and returns the previous position.
    */
    QKeyValueIterator operator++(int) {
        QKeyValueIterator old = *this;
        ++i;
        return old;
    }
    /*!
        Moves the iterator to the previous item and returns it.
    */
    QKeyValueIterator &operator--() {
        --i;
        return *this;
    }
    /*!
        \overload
        Moves the iterator to the previous item and returns the previous
        position.
    */
    QKeyValueIterator operator--(int) {
        QKeyValueIterator old = *this;
        --i;
        return old;
    }

    /*!
        Returns the iterator of the container that this iterator wraps.
    */
    Iterator base() const { return i; }

    /*!
        Returns \c true if \a a and \a b refer to the same item.
    */
    friend bool operator==(const QKeyValueIterator &a, const QKeyValueIterator &b) { return a.i == b.i; }
    /*!
        Returns \c true if \a a and \a b refer to different items.
    */
    friend bool operator!=(const QKeyValueIterator &a, const QKeyValueIterator &b) { return a.i != b.i; }

private:
    // A container whose key type is a view (QJsonObject) has an iterator with a keyView() that
    // refers to the stored key; key() returns a copy, which a view must not outlive.
    template <typename I>
    static auto keyOf(const I &it, int) -> std::enable_if_t<std::is_same_v<decltype(it.keyView()), Key>, Key> {
        return it.keyView();
    }
    template <typename I> static decltype(auto) keyOf(const I &it, long) { return it.key(); }

    Iterator i{};
};

namespace QtPrivate {

// The range returned by asKeyValueRange(): a reference to the container, or the container itself
// when it was a temporary.
template <typename Map, typename It> class KeyValueRange {
public:
    explicit KeyValueRange(Map m) : map(std::forward<Map>(m)) {}
    It begin() const { return It(map.begin()); }
    It end() const { return It(map.end()); }
    It begin() { return It(map.begin()); }
    It end() { return It(map.end()); }

private:
    Map map;
};

} // namespace QtPrivate
