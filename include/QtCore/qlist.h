// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

class QByteArray;
class QString;

/*!
    \internal
    The functions that QList<T> has for one item type only: join() and its relatives for strings and
    byte arrays. The specializations are QStringList and QByteArrayList.
*/
template <typename T> struct QListSpecialMethods {
protected:
    ~QListSpecialMethods() = default;
    // Never called: QList brings these names into its scope, where the specializations add overloads.
    void contains() const = delete;
    void indexOf() const = delete;
    void lastIndexOf() const = delete;
};
template <> struct QListSpecialMethods<QByteArray>;
template <> struct QListSpecialMethods<QString>;

namespace QtPrivate {

// What QList<bool> keeps its items in: std::vector<bool> packs its items into bits, so it has no bool &
// to give and no array of bool behind it. This has the part of the interface of std::vector that QList
// uses, over one byte for each item.
class BoolVector {
    static_assert(sizeof(bool) == 1 && alignof(bool) == 1, "a bool is expected to be one byte");
    using Cell = unsigned char; // always 0 or 1
    static constexpr Cell cell(bool v) noexcept { return v ? Cell(1) : Cell(0); }

public:
    using value_type = bool;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = bool &;
    using const_reference = const bool &;
    using pointer = bool *;
    using const_pointer = const bool *;
    using iterator = bool *;
    using const_iterator = const bool *;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    BoolVector() = default;
    explicit BoolVector(size_type n) : s(n, Cell(0)) {}
    BoolVector(size_type n, bool v) : s(n, cell(v)) {}
    BoolVector(std::initializer_list<bool> list) { assign(list.begin(), list.end()); }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    BoolVector(It first, It last) {
        assign(first, last);
    }

    // The bytes are only ever written as bool, through these pointers or as 0 and 1.
    bool *data() noexcept { return reinterpret_cast<bool *>(s.data()); }
    const bool *data() const noexcept { return reinterpret_cast<const bool *>(s.data()); }
    size_type size() const noexcept { return s.size(); }
    size_type capacity() const noexcept { return s.capacity(); }
    size_type max_size() const noexcept { return s.max_size(); }
    bool empty() const noexcept { return s.empty(); }
    void reserve(size_type n) { s.reserve(n); }
    void shrink_to_fit() { s.shrink_to_fit(); }
    void clear() noexcept { s.clear(); }
    void resize(size_type n) { s.resize(n, Cell(0)); }
    void resize(size_type n, bool v) { s.resize(n, cell(v)); }
    void swap(BoolVector &other) noexcept { s.swap(other.s); }

    iterator begin() noexcept { return data(); }
    iterator end() noexcept { return data() + size(); }
    const_iterator begin() const noexcept { return data(); }
    const_iterator end() const noexcept { return data() + size(); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const noexcept { return rbegin(); }
    const_reverse_iterator crend() const noexcept { return rend(); }

    bool &operator[](size_type i) { return data()[i]; }
    const bool &operator[](size_type i) const { return data()[i]; }
    bool &front() { return data()[0]; }
    const bool &front() const { return data()[0]; }
    bool &back() { return data()[size() - 1]; }
    const bool &back() const { return data()[size() - 1]; }

    void assign(size_type n, bool v) { s.assign(n, cell(v)); }
    void assign(std::initializer_list<bool> list) { assign(list.begin(), list.end()); }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    void assign(It first, It last) {
        s = cells(first, last);
    }
    void push_back(bool v) { s.push_back(cell(v)); }
    void pop_back() { s.pop_back(); }
    template <typename... Args> bool &emplace_back(Args &&...args) {
        s.push_back(cell(bool(std::forward<Args>(args)...)));
        return back();
    }
    template <typename... Args> iterator emplace(const_iterator pos, Args &&...args) {
        return insert(pos, bool(std::forward<Args>(args)...));
    }
    iterator insert(const_iterator pos, bool v) { return insert(pos, size_type(1), v); }
    iterator insert(const_iterator pos, size_type n, bool v) {
        const difference_type at = pos - cbegin();
        s.insert(s.begin() + at, n, cell(v));
        return begin() + at;
    }
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    iterator insert(const_iterator pos, It first, It last) {
        const difference_type at = pos - cbegin();
        // Copied first: the range may be in this vector.
        const std::vector<Cell> items = cells(first, last);
        s.insert(s.begin() + at, items.begin(), items.end());
        return begin() + at;
    }
    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
    iterator erase(const_iterator first, const_iterator last) {
        const difference_type at = first - cbegin();
        s.erase(s.begin() + at, s.begin() + (last - cbegin()));
        return begin() + at;
    }

    // The bytes are 0 and 1, so they compare as the items do.
    friend bool operator==(const BoolVector &a, const BoolVector &b) { return a.s == b.s; }
    friend bool operator!=(const BoolVector &a, const BoolVector &b) { return a.s != b.s; }
    friend bool operator<(const BoolVector &a, const BoolVector &b) { return a.s < b.s; }
    friend bool operator<=(const BoolVector &a, const BoolVector &b) { return a.s <= b.s; }
    friend bool operator>(const BoolVector &a, const BoolVector &b) { return a.s > b.s; }
    friend bool operator>=(const BoolVector &a, const BoolVector &b) { return a.s >= b.s; }

    operator std::vector<bool>() const { return std::vector<bool>(begin(), end()); }

private:
    template <typename It> static std::vector<Cell> cells(It first, It last) {
        std::vector<Cell> items;
        for (; first != last; ++first)
            items.push_back(cell(static_cast<bool>(*first)));
        return items;
    }

    std::vector<Cell> s;
};

} // namespace QtPrivate

/*!
    \class QList
    \brief The QList class is a template class that provides a dynamic array.

    QList<T> stores its items contiguously and gives fast index-based access,
    fast appends and fast iteration. It is the container to reach for first.

    \code
    QList<QString> names = {"Ada", "Grace"};
    names << "Linus";
    names.removeAll("Grace");
    for (const QString &n : names)
        use(n);
    \endcode

    \differences
    The list is a thin wrapper over \c std::vector, so copies are deep and it
    is not implicitly shared. contains(), indexOf(), lastIndexOf(), count(),
    removeOne() and removeAll() take a value of any type that can be compared
    with the items, as in Qt. A \c QList<bool> keeps a byte for each item,
    as Qt does, and not a bit as \c{std::vector<bool>} does. The \c Qt::Uninitialized constructor and
    resizeForOverwrite() value-initialize the new items. Indexes are not range-checked except by value().
    \c QVector is an alias of QList, so toList(), toVector(), fromList()
    and fromVector() return a copy of the same type.
    \enddifferences

    \sa QQueue, QStack, QVarLengthArray
*/
template <typename T> class QList : public QListSpecialMethods<T> {
public:
    using QListSpecialMethods<T>::contains;
    using QListSpecialMethods<T>::indexOf;
    using QListSpecialMethods<T>::lastIndexOf;
    /*! \typedef QList::Container

        The container that stores the items: \c{std::vector<T>}, except
        for a list of \c bool, which has a container of its own, as
        \c{std::vector<bool>} is not an array of \c bool. */
    using Container = std::conditional_t<std::is_same_v<T, bool>, QtPrivate::BoolVector, std::vector<T>>;
    /*! \typedef QList::iterator

        The non-const iterator type, a random access iterator over \c T. */
    using iterator = typename Container::iterator;
    /*! \typedef QList::const_iterator

        The const iterator type, a random access iterator over \c{const T}. */
    using const_iterator = typename Container::const_iterator;
    /*! \typedef QList::reverse_iterator

        The non-const reverse iterator type. */
    using reverse_iterator = typename Container::reverse_iterator;
    /*! \typedef QList::const_reverse_iterator

        The const reverse iterator type. */
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    /*! \typedef QList::value_type

        The type of the items stored in the list, \c T. Provided for STL compatibility. */
    using value_type = T;
    /*! \typedef QList::reference

        Alias for \c{T &}. Provided for STL compatibility. */
    using reference = T &;
    /*! \typedef QList::const_reference

        Alias for \c{const T &}. Provided for STL compatibility. */
    using const_reference = const T &;
    /*! \typedef QList::size_type

        Alias for qsizetype. Provided for STL compatibility. */
    using size_type = qsizetype;
    /*! \typedef QList::difference_type

        Alias for qsizetype. Provided for STL compatibility. */
    using difference_type = qsizetype;
    /*! \typedef QList::pointer

        Alias for \c{T *}. Provided for STL compatibility. */
    using pointer = T *;
    /*! \typedef QList::const_pointer

        Alias for \c{const T *}. Provided for STL compatibility. */
    using const_pointer = const T *;
    /*! \typedef QList::Iterator

        Qt-style synonym for QList::iterator. */
    using Iterator = iterator;
    /*! \typedef QList::ConstIterator

        Qt-style synonym for QList::const_iterator. */
    using ConstIterator = const_iterator;

    /*! Constructs an empty list. */
    QList() = default;
    /*!
        Constructs a list with \a size default-constructed items.
    */
    explicit QList(qsizetype size) : d(static_cast<std::size_t>(size)) {}
    /*! \overload

        Constructs a list with \a size copies of \a value. */
    QList(qsizetype size, const T &value) : d(static_cast<std::size_t>(size), value) {}
    /*! \overload
        \since 6.8

        Constructs a list with \a size items whose values the caller is about
        to assign. Pass \c Qt::Uninitialized as the second argument. */
    QList(qsizetype size, Qt::Initialization) : d(static_cast<std::size_t>(size)) {}
    /*! Constructs a list from the items in the initializer list \a args. */
    QList(std::initializer_list<T> args) : d(args) {}
    /*!
        \overload

        Constructs a list with the single string \a str, which is a QString
        or converts to one. This constructor exists for a list of strings
        only, as \c{QStringList(str)}.
    */
    template <typename String = QString,
              typename = std::enable_if_t<std::is_same_v<T, QString> && std::is_convertible_v<const String &, QString>>>
    explicit QList(const String &str) {
        d.emplace_back(str);
    }
    /*! Constructs a list with a copy of the items in the range [\a first, \a last). */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QList(It first, It last) : d(first, last) {}

    /*!
        Returns the number of items in the list.

        \sa isEmpty(), resize()
    */
    qsizetype size() const noexcept { return static_cast<qsizetype>(d.size()); }
    /*! Same as size(). */
    qsizetype count() const noexcept { return size(); }
    /*! Same as size(). */
    qsizetype length() const noexcept { return size(); }
    /*!
        Returns the maximum number of items the list can hold without
        reallocating.

        \sa reserve(), squeeze()
    */
    qsizetype capacity() const noexcept { return static_cast<qsizetype>(d.capacity()); }
    /*!
        Returns \c true if the list has size 0; otherwise returns \c false.

        \sa size(), clear()
    */
    bool isEmpty() const noexcept { return d.empty(); }
    /*! Same as isEmpty(). This function is provided for STL compatibility. */
    bool empty() const noexcept { return d.empty(); }
    /*!
        Removes all the items from the list.

        \sa resize(), removeAll()
    */
    void clear() { d.clear(); }
    /*!
        Attempts to allocate memory for at least \a n items, so that later
        appends do not reallocate.

        \sa capacity(), squeeze()
    */
    void reserve(qsizetype n) { d.reserve(static_cast<std::size_t>(n)); }
    /*!
        Releases any memory not required to store the items.

        \sa reserve(), capacity()
    */
    void squeeze() { d.shrink_to_fit(); }
    /*! Same as squeeze(). This function is provided for STL compatibility. */
    void shrink_to_fit() { d.shrink_to_fit(); }
    /*!
        Returns the largest number of items the list can theoretically hold.
        This function is provided for STL compatibility.
    */
    qsizetype max_size() const noexcept {
        const std::size_t n = d.max_size();
        constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<qsizetype>::max());
        return static_cast<qsizetype>(n < limit ? n : limit);
    }
    /*!
        \since 6.8
        Returns the largest number of items a list can theoretically hold.

        \sa max_size()
    */
    static constexpr qsizetype maxSize() noexcept {
        return static_cast<qsizetype>(std::numeric_limits<qsizetype>::max() / static_cast<qsizetype>(sizeof(T)));
    }
    /*!
        Sets the size of the list to \a n. If \a n is larger than the current
        size, the new items are default-constructed; if it is smaller, items
        are removed from the end.

        \sa size(), chop()
    */
    void resize(qsizetype n) { d.resize(static_cast<std::size_t>(n)); }
    /*! \overload

        Sets the size of the list to \a n. New items are copies of \a v. */
    void resize(qsizetype n, const T &v) { d.resize(static_cast<std::size_t>(n), v); }
    /*!
        \since 6.8
        Sets the size of the list to \a size, for a caller that is about to
        assign every new item.

        \sa resize()
    */
    void resizeForOverwrite(qsizetype size) { d.resize(static_cast<std::size_t>(size)); }
    /*!
        Assigns \a v to every item in the list. If \a n is not negative, the
        list is first resized to \a n items.

        \sa resize()
    */
    void fill(const T &v, qsizetype n = -1) {
        if (n >= 0)
            d.assign(static_cast<std::size_t>(n), v);
        else
            std::fill(d.begin(), d.end(), v);
    }

    /*!
        Returns a reference to the item at index position \a i, which can be
        assigned to. \a i must be a valid index.

        \sa at(), value()
    */
    T &operator[](qsizetype i) { return d[static_cast<std::size_t>(i)]; }
    /*! \overload

        Same as at(\a i). */
    const T &operator[](qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    /*!
        Returns the item at index position \a i. \a i must be a valid index.

        \sa value(), front(), back()
    */
    const T &at(qsizetype i) const { return d[static_cast<std::size_t>(i)]; }
    /*!
        Returns the item at index position \a i, or a default-constructed
        value if \a i is out of range.

        \sa at()
    */
    T value(qsizetype i) const { return (i >= 0 && i < size()) ? d[static_cast<std::size_t>(i)] : T(); }
    /*! \overload

        Returns the item at index position \a i, or \a def if \a i is out of range. */
    T value(qsizetype i, const T &def) const { return (i >= 0 && i < size()) ? d[static_cast<std::size_t>(i)] : def; }
    /*!
        Returns a reference to the first item in the list. The list must not
        be empty.

        \sa last(), front(), isEmpty()
    */
    T &first() { return d.front(); }
    /*! \overload

        Returns a const reference to the first item in the list. */
    const T &first() const { return d.front(); }
    /*!
        Returns a reference to the last item in the list. The list must not
        be empty.

        \sa first(), back(), isEmpty()
    */
    T &last() { return d.back(); }
    /*! \overload

        Returns a const reference to the last item in the list. */
    const T &last() const { return d.back(); }
    /*!
        Returns a const reference to the first item in the list. The list must
        not be empty.

        \sa constLast(), first()
    */
    const T &constFirst() const { return d.front(); }
    /*!
        Returns a const reference to the last item in the list. The list must
        not be empty.

        \sa constFirst(), last()
    */
    const T &constLast() const { return d.back(); }
    /*! Same as first(). This function is provided for STL compatibility. */
    T &front() { return d.front(); }
    /*! \overload

        Same as first() const. */
    const T &front() const { return d.front(); }
    /*! Same as last(). This function is provided for STL compatibility. */
    T &back() { return d.back(); }
    /*! \overload

        Same as last() const. */
    const T &back() const { return d.back(); }
    /*!
        Returns a pointer to the data stored in the list. The pointer can be
        used to access and modify the items, and stays valid until the list
        is resized or destroyed.

        \sa constData()
    */
    T *data() { return d.data(); }
    /*! \overload

        Returns a const pointer to the data stored in the list. */
    const T *data() const { return d.data(); }
    /*!
        Returns a const pointer to the data stored in the list.

        \sa data()
    */
    const T *constData() const { return d.data(); }

    /*!
        Inserts \a t at the end of the list.

        \sa prepend(), insert(), push_back()
    */
    void append(const T &t) { d.push_back(t); }
    /*! \overload

        Moves \a t to the end of the list. */
    void append(T &&t) { d.push_back(std::move(t)); }
    /*! \overload

        Appends the items of the list \a l to this list. */
    void append(const QList &l) { d.insert(d.end(), l.d.begin(), l.d.end()); }
    /*! Same as append(\a t). This function is provided for STL compatibility. */
    void push_back(const T &t) { d.push_back(t); }
    /*! \overload

        Same as append(\a t) for an rvalue. */
    void push_back(T &&t) { d.push_back(std::move(t)); }
    /*!
        Inserts \a t at the beginning of the list.

        \sa append(), insert()
    */
    void prepend(const T &t) { d.insert(d.begin(), t); }
    /*! \overload

        Moves \a t to the beginning of the list. */
    void prepend(T &&t) { d.insert(d.begin(), std::move(t)); }
    /*! Same as prepend(\a t). This function is provided for STL compatibility. */
    void push_front(const T &t) { prepend(t); }
    /*! \overload

        Same as prepend(\a t). This function is provided for STL compatibility. */
    void push_front(T &&t) { prepend(std::move(t)); }
    /*!
        Constructs a new item from \a args and inserts it at index position
        \a i. Returns an iterator to the new item. \a i must be between 0
        and size().

        \sa emplaceBack(), insert()
    */
    template <typename... Args> iterator emplace(qsizetype i, Args &&...args) {
        return d.emplace(d.begin() + i, std::forward<Args>(args)...);
    }
    /*! \overload

        Constructs a new item from \a args and inserts it before the item pointed to by \a before. Returns an iterator to the new item. */
    template <typename... Args> iterator emplace(const_iterator before, Args &&...args) {
        return d.emplace(before, std::forward<Args>(args)...);
    }
    /*!
        Constructs a new item at the end of the list from \a args and returns
        a reference to it.

        \sa append()
    */
    template <typename... Args>
    T &emplaceBack(Args &&...args) { d.emplace_back(std::forward<Args>(args)...); return d.back(); }
    /*! Same as emplaceBack(\a args). This function is provided for STL compatibility. */
    template <typename... Args>
    T &emplace_back(Args &&...args) { return emplaceBack(std::forward<Args>(args)...); }
    /*!
        Constructs a new item at the beginning of the list from \a args and
        returns a reference to it.

        \sa emplaceBack(), prepend()
    */
    template <typename... Args> T &emplaceFront(Args &&...args) {
        return *d.emplace(d.begin(), std::forward<Args>(args)...);
    }
    /*! Same as emplaceFront(\a args). This function is provided for STL compatibility. */
    template <typename... Args> T &emplace_front(Args &&...args) { return emplaceFront(std::forward<Args>(args)...); }
    /*!
        Inserts \a t at index position \a i and returns an iterator to the
        inserted item. \a i must be between 0 and size().

        \sa append(), prepend(), remove()
    */
    iterator insert(qsizetype i, const T &t) { return d.insert(d.begin() + i, t); }
    /*! \overload

        Inserts \a n copies of \a t at index position \a i and returns an iterator to the first inserted item. */
    iterator insert(qsizetype i, qsizetype n, const T &t) { return d.insert(d.begin() + i, static_cast<std::size_t>(n), t); }
    /*! \overload

        Inserts \a t before the item pointed to by the iterator \a before and returns an iterator to the inserted item. */
    iterator insert(const_iterator before, const T &t) { return d.insert(before, t); }
    /*! \overload

        Moves \a t to index position \a i and returns an iterator to the inserted item. */
    iterator insert(qsizetype i, T &&t) { return d.insert(d.begin() + i, std::move(t)); }
    /*! \overload

        Moves \a t before the item pointed to by the iterator \a before and returns an iterator to the inserted item. */
    iterator insert(const_iterator before, T &&t) { return d.insert(before, std::move(t)); }
    /*! \overload

        Inserts \a n copies of \a t before the item pointed to by the iterator \a before and returns an iterator to the first inserted item. */
    iterator insert(const_iterator before, qsizetype n, const T &t) {
        return d.insert(before, static_cast<std::size_t>(n), t);
    }
    /*!
        \since 6.6
        Replaces the contents of the list with \a n copies of \a t and
        returns a reference to the list.

        \sa fill()
    */
    QList &assign(qsizetype n, const T &t) {
        d.assign(static_cast<std::size_t>(n), t);
        return *this;
    }
    /*! \overload
        \since 6.6

        Replaces the contents of the list with a copy of the items in the range [\a first, \a last) and returns a reference to the list. */
    template <typename It, typename = typename std::iterator_traits<It>::iterator_category>
    QList &assign(It first, It last) {
        d.assign(first, last);
        return *this;
    }
    /*! \overload
        \since 6.6

        Replaces the contents of the list with the items in \a l and returns a reference to the list. */
    QList &assign(std::initializer_list<T> l) {
        d.assign(l);
        return *this;
    }
    /*!
        Replaces the item at index position \a i with \a t. \a i must be a
        valid index.

        \sa remove()
    */
    void replace(qsizetype i, const T &t) { d[static_cast<std::size_t>(i)] = t; }
    /*!
        Removes \a n items starting at index position \a i. The range must be
        valid.

        \sa removeAt(), insert(), takeAt()
    */
    void remove(qsizetype i, qsizetype n = 1) { d.erase(d.begin() + i, d.begin() + i + n); }
    /*!
        Removes the item at index position \a i. \a i must be a valid index.

        \sa remove(), takeAt(), removeOne()
    */
    void removeAt(qsizetype i) { d.erase(d.begin() + i); }
    /*!
        Removes the first item in the list. The list must not be empty.

        \sa removeLast(), takeFirst()
    */
    void removeFirst() { d.erase(d.begin()); }
    /*!
        Removes the last item in the list. The list must not be empty.

        \sa removeFirst(), takeLast()
    */
    void removeLast() { d.pop_back(); }
    /*! Same as removeLast(). This function is provided for STL compatibility. */
    void pop_back() { d.pop_back(); }
    /*! Same as removeFirst(). This function is provided for STL compatibility. */
    void pop_front() { removeFirst(); }
    /*!
        Removes the item pointed to by the iterator \a pos and returns an
        iterator to the item that followed it.

        \sa removeAt()
    */
    iterator erase(const_iterator pos) { return d.erase(pos); }
    /*! \overload

        Removes the items in the range [\a first, \a last) and returns an iterator to the item that followed the range. */
    iterator erase(const_iterator first, const_iterator last) { return d.erase(first, last); }
    /*!
        Removes the item at index position \a i and returns it. \a i must be
        a valid index.

        \sa removeAt(), takeFirst(), takeLast()
    */
    T takeAt(qsizetype i) { T t = std::move(d[static_cast<std::size_t>(i)]); removeAt(i); return t; }
    /*!
        Removes the first item in the list and returns it. The list must not
        be empty.

        \sa takeLast(), removeFirst()
    */
    T takeFirst() { return takeAt(0); }
    /*!
        Removes the last item in the list and returns it. The list must not
        be empty.

        \sa takeFirst(), removeLast()
    */
    T takeLast() { T t = std::move(d.back()); d.pop_back(); return t; }
    /*!
        Removes the first item equal to \a t and returns \c true; returns
        \c false if there is no such item.

        \sa removeAll(), removeAt()
    */
    template <typename AT = T> bool removeOne(const AT &t) {
        auto it = std::find(d.begin(), d.end(), t);
        if (it == d.end())
            return false;
        d.erase(it);
        return true;
    }
    /*!
        Removes all items equal to \a t and returns the number of items
        removed.

        \sa removeOne(), removeIf()
    */
    template <typename AT = T> qsizetype removeAll(const AT &t) {
        const auto before = d.size();
        // t may be an item of the list, which the removal would overwrite.
        const AT copy = t; // NOLINT(performance-unnecessary-copy-initialization): see above
        d.erase(std::remove(d.begin(), d.end(), copy), d.end());
        return static_cast<qsizetype>(before - d.size());
    }
    /*!
        Removes all items for which the predicate \a p returns \c true and
        returns the number of items removed.

        \sa removeAll()
    */
    template <typename Pred>
    qsizetype removeIf(Pred p) {
        const auto before = d.size();
        d.erase(std::remove_if(d.begin(), d.end(), p), d.end());
        return static_cast<qsizetype>(before - d.size());
    }
    /*!
        Moves the item at index position \a from to index position \a to.
        Both must be valid indexes.

        \sa swapItemsAt()
    */
    void move(qsizetype from, qsizetype to) {
        if (from == to)
            return;
        T t = std::move(d[static_cast<std::size_t>(from)]);
        d.erase(d.begin() + from);
        d.insert(d.begin() + to, std::move(t));
    }
    /*!
        Exchanges the items at index positions \a i and \a j. Both must be
        valid indexes.

        \sa move()
    */
    void swapItemsAt(qsizetype i, qsizetype j) { std::swap(d[static_cast<std::size_t>(i)], d[static_cast<std::size_t>(j)]); }
    /*! Swaps this list with \a o. This operation is very fast and never fails. */
    void swap(QList &o) noexcept { d.swap(o.d); }

    /*!
        Returns \c true if the list contains an item equal to \a t; otherwise
        returns \c false.

        \sa indexOf(), count()
    */
    template <typename AT = T> bool contains(const AT &t) const { return std::find(d.begin(), d.end(), t) != d.end(); }
    /*!
        Returns the index position of the first item equal to \a t, searching
        forward from index position \a from. A negative \a from counts from
        the end. Returns -1 if no item matches.

        \sa lastIndexOf(), contains()
    */
    template <typename AT = T> qsizetype indexOf(const AT &t, qsizetype from = 0) const {
        if (from < 0)
            from = qMax<qsizetype>(0, from + size());
        for (qsizetype i = from; i < size(); ++i)
            if (d[static_cast<std::size_t>(i)] == t)
                return i;
        return -1;
    }
    /*!
        Returns the index position of the last item equal to \a t, searching
        backward from index position \a from. A negative \a from counts from
        the end, so the default of -1 starts at the last item. Returns -1 if
        no item matches.

        \sa indexOf(), contains()
    */
    template <typename AT = T> qsizetype lastIndexOf(const AT &t, qsizetype from = -1) const {
        qsizetype i = (from < 0) ? size() + from : qMin(from, size() - 1);
        for (; i >= 0; --i)
            if (d[static_cast<std::size_t>(i)] == t)
                return i;
        return -1;
    }
    /*!
        \overload

        Returns the number of items in the list equal to \a t.

        \sa contains(), indexOf()
    */
    template <typename AT = T> qsizetype count(const AT &t) const {
        return static_cast<qsizetype>(std::count(d.begin(), d.end(), t));
    }
    /*!
        Returns \c true if the list is not empty and its first item is equal
        to \a t; otherwise returns \c false.

        \sa endsWith()
    */
    bool startsWith(const T &t) const { return !d.empty() && d.front() == t; }
    /*!
        Returns \c true if the list is not empty and its last item is equal
        to \a t; otherwise returns \c false.

        \sa startsWith()
    */
    bool endsWith(const T &t) const { return !d.empty() && d.back() == t; }

    /*!
        Returns a sublist containing up to \a len items starting at index
        position \a pos. If \a len is negative or reaches past the end, all
        items from \a pos to the end are returned. A negative \a pos is
        treated as 0; if \a pos is beyond the end, an empty list is returned.

        \sa first(), last(), sliced()
    */
    QList mid(qsizetype pos, qsizetype len = -1) const {
        if (pos < 0) pos = 0;
        if (pos >= size()) return {};
        const qsizetype end = (len < 0 || pos + len > size()) ? size() : pos + len;
        return QList(d.begin() + pos, d.begin() + end);
    }
    /*! \overload

        Returns a sublist containing the first \a n items of this list. */
    QList first(qsizetype n) const { return mid(0, n); }
    /*! \overload

        Returns a sublist containing the last \a n items of this list. If \a n is not less than size(), the whole list is returned. */
    QList last(qsizetype n) const { return n >= size() ? *this : mid(size() - n, n); }
    /*!
        Returns a sublist containing \a n items starting at index position
        \a pos. Same as mid(\a pos, \a n).

        \sa mid(), first(), last()
    */
    QList sliced(qsizetype pos, qsizetype n) const { return mid(pos, n); }
    /*! \overload

        Returns a sublist containing all items from index position \a pos to the end. */
    QList sliced(qsizetype pos) const { return mid(pos); }
    /*!
        Returns a sublist containing this list without its last \a n items.

        \sa chop(), first()
    */
    QList chopped(qsizetype n) const { return mid(0, size() - n); }
    /*!
        Removes \a n items from the end of the list. \a n must not be greater
        than size().

        \sa chopped(), resize()
    */
    void chop(qsizetype n) { d.resize(d.size() - static_cast<std::size_t>(n)); }
    /*! Reverses the order of the items in the list in place. */
    void reverse() { std::reverse(d.begin(), d.end()); }
    /*!
        Returns the items as a \c{std::vector<T>}: a const reference to the
        vector that the list keeps them in, or, for a list of \c bool, a new
        \c{std::vector<bool>} with a copy of them.
    */
    std::conditional_t<std::is_same_v<T, bool>, std::vector<bool>, const std::vector<T> &> toStdVector() const {
        return d;
    }
    /*! Returns a copy of this list. Provided for compatibility with code that converts between QList and QVector. */
    QList toList() const { return *this; }
    /*! Returns a copy of this list. Provided for compatibility with code that converts between QList and QVector. */
    QList toVector() const { return *this; }
    /*! Returns a copy of \a list. Provided for compatibility with code that converts between QList and QVector. */
    static QList fromList(const QList &list) { return list; }
    /*! Returns a copy of \a vector. Provided for compatibility with code that converts between QList and QVector. */
    static QList fromVector(const QList &vector) { return vector; }

    /*! Returns an iterator pointing to the first item in the list. */
    iterator begin() { return d.begin(); }
    /*! Returns an iterator pointing just after the last item in the list. */
    iterator end() { return d.end(); }
    /*! \overload

        Returns a const iterator pointing to the first item in the list. */
    const_iterator begin() const noexcept { return d.begin(); }
    /*! \overload

        Returns a const iterator pointing just after the last item in the list. */
    const_iterator end() const noexcept { return d.end(); }
    /*! Returns a const iterator pointing to the first item in the list. */
    const_iterator cbegin() const noexcept { return d.cbegin(); }
    /*! Returns a const iterator pointing just after the last item in the list. */
    const_iterator cend() const noexcept { return d.cend(); }
    /*! Same as cbegin(). */
    const_iterator constBegin() const noexcept { return d.cbegin(); }
    /*! Same as cend(). */
    const_iterator constEnd() const noexcept { return d.cend(); }
    /*! Returns a reverse iterator pointing to the last item in the list. */
    reverse_iterator rbegin() { return d.rbegin(); }
    /*! Returns a reverse iterator pointing just before the first item in the list. */
    reverse_iterator rend() { return d.rend(); }
    /*! \overload

        Returns a const reverse iterator pointing to the last item in the list. */
    const_reverse_iterator rbegin() const noexcept { return d.rbegin(); }
    /*! \overload

        Returns a const reverse iterator pointing just before the first item in the list. */
    const_reverse_iterator rend() const noexcept { return d.rend(); }
    /*! Returns a const reverse iterator pointing to the last item in the list. */
    const_reverse_iterator crbegin() const noexcept { return d.crbegin(); }
    /*! Returns a const reverse iterator pointing just before the first item in the list. */
    const_reverse_iterator crend() const noexcept { return d.crend(); }

    /*! Appends the items of the list \a l to this list and returns a reference to this list. */
    QList &operator+=(const QList &l) { append(l); return *this; }
    /*! \overload

        Appends \a t to this list and returns a reference to this list. */
    QList &operator+=(const T &t) { append(t); return *this; }
    /*! \overload

        Moves \a t to the end of this list and returns a reference to this list. */
    QList &operator+=(T &&t) {
        append(std::move(t));
        return *this;
    }
    /*! Appends \a t to this list and returns a reference to this list, so that calls can be chained. */
    QList &operator<<(const T &t) { append(t); return *this; }
    /*! \overload

        Appends the items of the list \a l to this list and returns a reference to this list. */
    QList &operator<<(const QList &l) { append(l); return *this; }
    /*! \overload

        Moves \a t to the end of this list and returns a reference to this list. */
    QList &operator<<(T &&t) {
        append(std::move(t));
        return *this;
    }
    friend QList operator+(const QList &a, const QList &b) { QList r(a); r.append(b); return r; }
    friend bool operator==(const QList &a, const QList &b) { return a.d == b.d; }
    friend bool operator!=(const QList &a, const QList &b) { return a.d != b.d; }
    // The relational operators exist only for element types that have operator<, so that type
    // traits (such as the ones QMetaType uses) see whether a list can be ordered.
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator<(const QList &a, const QList &b) {
        return a.d < b.d;
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator<=(const QList &a, const QList &b) {
        return a.d <= b.d;
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator>(const QList &a, const QList &b) {
        return a.d > b.d;
    }
    template <typename U = T, typename = decltype(std::declval<const U &>() < std::declval<const U &>())>
    friend bool operator>=(const QList &a, const QList &b) {
        return a.d >= b.d;
    }

private:
    Container d;
};

template <typename T>
using QVector = QList<T>;

/*!
    \class QQueue
    \brief The QQueue class is a template class that provides a queue.

    QQueue<T> is a QList<T> with first-in, first-out operations: items are
    added with enqueue() and taken from the front with dequeue(). All of
    QList's functions are available.

    \code
    QQueue<int> queue;
    queue.enqueue(1);
    queue.enqueue(2);
    int first = queue.dequeue(); // 1
    \endcode

    \differences
    The class is a plain subclass of QList that adds only enqueue(),
    dequeue() and head(). It has no further members.
    \enddifferences

    \sa QList, QStack
*/
template <typename T>
class QQueue : public QList<T> {
public:
    using QList<T>::QList;
    /*!
        Adds \a t to the end of the queue.

        \sa dequeue(), head()
    */
    void enqueue(const T &t) { this->append(t); }
    /*!
        Removes the head item in the queue and returns it. The queue must
        not be empty.

        \sa enqueue(), head()
    */
    T dequeue() { return this->takeFirst(); }
    /*!
        Returns a reference to the item at the head of the queue. The queue
        must not be empty.

        \sa enqueue(), dequeue()
    */
    T &head() { return this->first(); }
    /*! \overload

        Returns a const reference to the item at the head of the queue. */
    const T &head() const { return this->first(); }
};

/*!
    \class QStack
    \brief The QStack class is a template class that provides a stack.

    QStack<T> is a QList<T> with last-in, first-out operations: items are
    added with push() and taken from the top with pop(). All of QList's
    functions are available.

    \code
    QStack<int> stack;
    stack.push(1);
    stack.push(2);
    int top = stack.pop(); // 2
    \endcode

    \differences
    The class is a plain subclass of QList that adds only push(), pop() and
    top(). It has no further members.
    \enddifferences

    \sa QList, QQueue
*/
template <typename T>
class QStack : public QList<T> {
public:
    using QList<T>::QList;
    /*!
        Adds \a t to the top of the stack.

        \sa pop(), top()
    */
    void push(const T &t) { this->append(t); }
    /*!
        Removes the top item from the stack and returns it. The stack must
        not be empty.

        \sa push(), top()
    */
    T pop() { return this->takeLast(); }
    /*!
        Returns a reference to the item at the top of the stack. The stack
        must not be empty.

        \sa push(), pop()
    */
    T &top() { return this->last(); }
    /*! \overload

        Returns a const reference to the item at the top of the stack. */
    const T &top() const { return this->last(); }
};
