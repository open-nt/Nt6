// MIT License - see LICENSE
#pragma once

#include <QtCore/qjsonvalue.h>
#include <QtCore/qstringlist.h>

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>

class QJsonObject {
public:
    QJsonObject();
    QJsonObject(std::initializer_list<std::pair<QString, QJsonValue>> args);
    QJsonObject(const QJsonObject &other);
    QJsonObject(QJsonObject &&other) noexcept;
    ~QJsonObject();
    QJsonObject &operator=(const QJsonObject &other);
    QJsonObject &operator=(QJsonObject &&other) noexcept;

    void swap(QJsonObject &other) noexcept { d.swap(other.d); }

    qsizetype size() const;
    qsizetype count() const { return size(); }
    qsizetype length() const { return size(); }
    bool isEmpty() const { return size() == 0; }
    bool empty() const { return isEmpty(); }

    QStringList keys() const;

    QJsonValue value(const QString &key) const;
    QJsonValue value(QStringView key) const;
    QJsonValue value(QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> QJsonValue value(K &&key) const {
        return value(QLatin1StringView(key));
    }
    QJsonValue operator[](const QString &key) const { return value(key); }
    QJsonValue operator[](QStringView key) const { return value(key); }
    QJsonValue operator[](QLatin1StringView key) const { return value(key); }
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>>
    QJsonValue operator[](K &&key) const {
        return value(QLatin1StringView(key));
    }
    QJsonValueRef operator[](const QString &key);
    QJsonValueRef operator[](QStringView key);
    QJsonValueRef operator[](QLatin1StringView key);
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> QJsonValueRef operator[](K &&key) {
        return (*this)[QLatin1StringView(key)];
    }

    void remove(const QString &key);
    void remove(QStringView key);
    void remove(QLatin1StringView key);
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> void remove(K &&key) {
        remove(QLatin1StringView(key));
    }
    QJsonValue take(const QString &key);
    QJsonValue take(QStringView key);
    QJsonValue take(QLatin1StringView key);
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> QJsonValue take(K &&key) {
        return take(QLatin1StringView(key));
    }
    bool contains(const QString &key) const;
    bool contains(QStringView key) const;
    bool contains(QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> bool contains(K &&key) const {
        return contains(QLatin1StringView(key));
    }

    friend bool operator==(const QJsonObject &a, const QJsonObject &b) { return a.equals(b); }
    friend bool operator!=(const QJsonObject &a, const QJsonObject &b) { return !a.equals(b); }

    class const_iterator;
    class iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = qsizetype;
        using value_type = QJsonValue;
        using reference = QJsonValueRef;
        using pointer = QJsonRefArrow<QJsonValueRef>;

        iterator() = default;
        explicit iterator(QJsonObject *object, qsizetype index) : o(object), i(index) {}

        QString key() const;
        QJsonValueRef value() const { return QJsonValueRef(o, i); }
        QJsonValueRef operator*() const { return QJsonValueRef(o, i); }
        QJsonRefArrow<QJsonValueRef> operator->() const { return QJsonRefArrow<QJsonValueRef>(**this); }
        QJsonValueRef operator[](qsizetype j) const { return QJsonValueRef(o, i + j); }

        bool operator==(const iterator &other) const { return o == other.o && i == other.i; }
        bool operator!=(const iterator &other) const { return !(*this == other); }
        bool operator<(const iterator &other) const { return i < other.i; }
        bool operator<=(const iterator &other) const { return i <= other.i; }
        bool operator>(const iterator &other) const { return i > other.i; }
        bool operator>=(const iterator &other) const { return i >= other.i; }
        inline bool operator==(const const_iterator &other) const;
        inline bool operator!=(const const_iterator &other) const;
        inline bool operator<(const const_iterator &other) const;
        inline bool operator<=(const const_iterator &other) const;
        inline bool operator>(const const_iterator &other) const;
        inline bool operator>=(const const_iterator &other) const;

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
        iterator operator+(qsizetype j) const { return iterator(o, i + j); }
        iterator operator-(qsizetype j) const { return iterator(o, i - j); }
        friend iterator operator+(qsizetype j, const iterator &it) { return it + j; }
        qsizetype operator-(const iterator &other) const { return i - other.i; }

    private:
        friend class QJsonObject;
        friend class const_iterator;
        QJsonObject *o = nullptr;
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
        explicit const_iterator(const QJsonObject *object, qsizetype index) : o(object), i(index) {}
        const_iterator(const iterator &other) : o(other.o), i(other.i) {}

        QString key() const;
        QJsonValueConstRef value() const { return QJsonValueConstRef(o, i); }
        QJsonValueConstRef operator*() const { return QJsonValueConstRef(o, i); }
        QJsonRefArrow<QJsonValueConstRef> operator->() const { return QJsonRefArrow<QJsonValueConstRef>(**this); }
        QJsonValueConstRef operator[](qsizetype j) const { return QJsonValueConstRef(o, i + j); }

        bool operator==(const const_iterator &other) const { return o == other.o && i == other.i; }
        bool operator!=(const const_iterator &other) const { return !(*this == other); }
        bool operator<(const const_iterator &other) const { return i < other.i; }
        bool operator<=(const const_iterator &other) const { return i <= other.i; }
        bool operator>(const const_iterator &other) const { return i > other.i; }
        bool operator>=(const const_iterator &other) const { return i >= other.i; }

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
        const_iterator operator+(qsizetype j) const { return const_iterator(o, i + j); }
        const_iterator operator-(qsizetype j) const { return const_iterator(o, i - j); }
        friend const_iterator operator+(qsizetype j, const const_iterator &it) { return it + j; }
        qsizetype operator-(const const_iterator &other) const { return i - other.i; }

    private:
        friend class QJsonObject;
        const QJsonObject *o = nullptr;
        qsizetype i = 0;
    };

    using Iterator = iterator;
    using ConstIterator = const_iterator;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using key_type = QString;
    using mapped_type = QJsonValue;
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

    iterator insert(const QString &key, const QJsonValue &value);
    iterator erase(iterator it);

    iterator find(const QString &key);
    iterator find(QStringView key);
    iterator find(QLatin1StringView key);
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> iterator find(K &&key) {
        return find(QLatin1StringView(key));
    }
    const_iterator find(const QString &key) const { return constFind(key); }
    const_iterator find(QStringView key) const { return constFind(key); }
    const_iterator find(QLatin1StringView key) const { return constFind(key); }
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>> const_iterator find(K &&key) const {
        return constFind(QLatin1StringView(key));
    }
    const_iterator constFind(const QString &key) const;
    const_iterator constFind(QStringView key) const;
    const_iterator constFind(QLatin1StringView key) const;
    template <typename K, typename = std::enable_if_t<QtPrivate::IsJsonCString<K>>>
    const_iterator constFind(K &&key) const {
        return constFind(QLatin1StringView(key));
    }

private:
    friend class QJsonPrivateAccess;
    bool equals(const QJsonObject &other) const;
    // Copies the shared data if another object refers to it, and creates it when the object is empty.
    QJsonObjectPrivate &detached();

    std::shared_ptr<QJsonObjectPrivate> d;
};

QDebug operator<<(QDebug dbg, const QJsonObject &object);

inline bool QJsonObject::iterator::operator==(const const_iterator &other) const {
    return const_iterator(*this) == other;
}
inline bool QJsonObject::iterator::operator!=(const const_iterator &other) const {
    return const_iterator(*this) != other;
}
inline bool QJsonObject::iterator::operator<(const const_iterator &other) const {
    return const_iterator(*this) < other;
}
inline bool QJsonObject::iterator::operator<=(const const_iterator &other) const {
    return const_iterator(*this) <= other;
}
inline bool QJsonObject::iterator::operator>(const const_iterator &other) const {
    return const_iterator(*this) > other;
}
inline bool QJsonObject::iterator::operator>=(const const_iterator &other) const {
    return const_iterator(*this) >= other;
}
