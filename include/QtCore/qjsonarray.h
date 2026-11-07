// MIT License - see LICENSE
#pragma once

#include <QtCore/qjsonvalue.h>
#include <QtCore/qstringlist.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>

class QJsonArray {
public:
    QJsonArray();
    QJsonArray(std::initializer_list<QJsonValue> args);
    QJsonArray(const QJsonArray &other);
    QJsonArray(QJsonArray &&other) noexcept;
    ~QJsonArray();
    QJsonArray &operator=(const QJsonArray &other);
    QJsonArray &operator=(QJsonArray &&other) noexcept;

    void swap(QJsonArray &other) noexcept { d.swap(other.d); }

    static QJsonArray fromStringList(const QStringList &list);

    qsizetype size() const;
    qsizetype count() const { return size(); }
    qsizetype length() const { return size(); }
    bool isEmpty() const { return size() == 0; }
    bool empty() const { return isEmpty(); }

    QJsonValue at(qsizetype i) const;
    QJsonValue first() const { return at(0); }
    QJsonValue last() const { return at(size() - 1); }
    QJsonValueRef first() { return (*this)[0]; }
    QJsonValueRef last() { return (*this)[size() - 1]; }

    void prepend(const QJsonValue &value);
    void append(const QJsonValue &value);
    void insert(qsizetype i, const QJsonValue &value);
    void removeAt(qsizetype i);
    QJsonValue takeAt(qsizetype i);
    void removeFirst() { removeAt(0); }
    void removeLast() { removeAt(size() - 1); }
    void replace(qsizetype i, const QJsonValue &value);
    bool contains(const QJsonValue &element) const;

    QJsonValueRef operator[](qsizetype i);
    QJsonValue operator[](qsizetype i) const { return at(i); }

    QJsonArray &operator+=(const QJsonValue &value) {
        append(value);
        return *this;
    }
    QJsonArray operator+(const QJsonValue &value) const {
        QJsonArray n = *this;
        n.append(value);
        return n;
    }
    QJsonArray &operator<<(const QJsonValue &value) {
        append(value);
        return *this;
    }

    friend bool operator==(const QJsonArray &a, const QJsonArray &b) { return a.equals(b); }
    friend bool operator!=(const QJsonArray &a, const QJsonArray &b) { return !a.equals(b); }

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = qsizetype;
        using value_type = QJsonValue;
        using reference = QJsonValueRef;
        using pointer = QJsonRefArrow<QJsonValueRef>;

        iterator() = default;
        explicit iterator(QJsonArray *array, qsizetype index) : a(array), i(index) {}

        QJsonValueRef operator*() const { return QJsonValueRef(a, i); }
        QJsonRefArrow<QJsonValueRef> operator->() const { return QJsonRefArrow<QJsonValueRef>(**this); }
        QJsonValueRef operator[](qsizetype j) const { return QJsonValueRef(a, i + j); }

        bool operator==(const iterator &o) const { return a == o.a && i == o.i; }
        bool operator!=(const iterator &o) const { return !(*this == o); }
        bool operator<(const iterator &o) const { return i < o.i; }
        bool operator<=(const iterator &o) const { return i <= o.i; }
        bool operator>(const iterator &o) const { return i > o.i; }
        bool operator>=(const iterator &o) const { return i >= o.i; }
        inline bool operator==(const const_iterator &o) const;
        inline bool operator!=(const const_iterator &o) const;
        inline bool operator<(const const_iterator &o) const;
        inline bool operator<=(const const_iterator &o) const;
        inline bool operator>(const const_iterator &o) const;
        inline bool operator>=(const const_iterator &o) const;

        iterator &operator++() {
            ++i;
            return *this;
        }
        iterator operator++(int) {
            iterator n = *this;
            ++i;
            return n;
        }
        iterator &operator--() {
            --i;
            return *this;
        }
        iterator operator--(int) {
            iterator n = *this;
            --i;
            return n;
        }
        iterator &operator+=(qsizetype j) {
            i += j;
            return *this;
        }
        iterator &operator-=(qsizetype j) {
            i -= j;
            return *this;
        }
        iterator operator+(qsizetype j) const { return iterator(a, i + j); }
        iterator operator-(qsizetype j) const { return iterator(a, i - j); }
        friend iterator operator+(qsizetype j, const iterator &it) { return it + j; }
        qsizetype operator-(const iterator &o) const { return i - o.i; }

    private:
        friend class QJsonArray;
        friend class const_iterator;
        QJsonArray *a = nullptr;
        qsizetype i = 0;
    };

    class const_iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = qsizetype;
        using value_type = QJsonValue;
        using reference = QJsonValueConstRef;
        using pointer = QJsonRefArrow<QJsonValueConstRef>;

        const_iterator() = default;
        explicit const_iterator(const QJsonArray *array, qsizetype index) : a(array), i(index) {}
        const_iterator(const iterator &o) : a(o.a), i(o.i) {}

        QJsonValueConstRef operator*() const { return QJsonValueConstRef(a, i); }
        QJsonRefArrow<QJsonValueConstRef> operator->() const { return QJsonRefArrow<QJsonValueConstRef>(**this); }
        QJsonValueConstRef operator[](qsizetype j) const { return QJsonValueConstRef(a, i + j); }

        bool operator==(const const_iterator &o) const { return a == o.a && i == o.i; }
        bool operator!=(const const_iterator &o) const { return !(*this == o); }
        bool operator<(const const_iterator &o) const { return i < o.i; }
        bool operator<=(const const_iterator &o) const { return i <= o.i; }
        bool operator>(const const_iterator &o) const { return i > o.i; }
        bool operator>=(const const_iterator &o) const { return i >= o.i; }

        const_iterator &operator++() {
            ++i;
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator n = *this;
            ++i;
            return n;
        }
        const_iterator &operator--() {
            --i;
            return *this;
        }
        const_iterator operator--(int) {
            const_iterator n = *this;
            --i;
            return n;
        }
        const_iterator &operator+=(qsizetype j) {
            i += j;
            return *this;
        }
        const_iterator &operator-=(qsizetype j) {
            i -= j;
            return *this;
        }
        const_iterator operator+(qsizetype j) const { return const_iterator(a, i + j); }
        const_iterator operator-(qsizetype j) const { return const_iterator(a, i - j); }
        friend const_iterator operator+(qsizetype j, const const_iterator &it) { return it + j; }
        qsizetype operator-(const const_iterator &o) const { return i - o.i; }

    private:
        friend class QJsonArray;
        const QJsonArray *a = nullptr;
        qsizetype i = 0;
    };

    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = qsizetype;
    using difference_type = qsizetype;
    using value_type = QJsonValue;
    using reference = QJsonValueRef;
    using const_reference = QJsonValueConstRef;
    using pointer = QJsonRefArrow<QJsonValueRef>;
    using const_pointer = QJsonRefArrow<QJsonValueConstRef>;

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, size()); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size()); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    const_iterator constBegin() const { return begin(); }
    const_iterator constEnd() const { return end(); }
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    const_reverse_iterator crbegin() const { return rbegin(); }
    const_reverse_iterator crend() const { return rend(); }

    iterator insert(iterator before, const QJsonValue &value);
    iterator erase(iterator it);

    void push_back(const QJsonValue &value) { append(value); }
    void push_front(const QJsonValue &value) { prepend(value); }
    void pop_front() { removeFirst(); }
    void pop_back() { removeLast(); }

private:
    friend class QJsonPrivateAccess;
    bool equals(const QJsonArray &other) const;
    // Copies the shared data if another array refers to it, and creates it when the array is empty.
    QJsonArrayPrivate &detached();

    std::shared_ptr<QJsonArrayPrivate> d;
};

QDebug operator<<(QDebug dbg, const QJsonArray &array);

inline bool QJsonArray::iterator::operator==(const const_iterator &o) const { return const_iterator(*this) == o; }
inline bool QJsonArray::iterator::operator!=(const const_iterator &o) const { return const_iterator(*this) != o; }
inline bool QJsonArray::iterator::operator<(const const_iterator &o) const { return const_iterator(*this) < o; }
inline bool QJsonArray::iterator::operator<=(const const_iterator &o) const { return const_iterator(*this) <= o; }
inline bool QJsonArray::iterator::operator>(const const_iterator &o) const { return const_iterator(*this) > o; }
inline bool QJsonArray::iterator::operator>=(const const_iterator &o) const { return const_iterator(*this) >= o; }
