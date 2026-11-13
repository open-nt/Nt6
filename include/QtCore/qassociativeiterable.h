// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetacontainer.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>

#include <cstddef>
#include <iterator>

class QAssociativeIterable {
public:
    /*!
        \class QAssociativeIterable::const_iterator
        \brief The const_iterator class is a read-only iterator over a QAssociativeIterable.

        key() and value() return the key and the mapped value of the current entry as
        QVariant objects.

        \differences
        The iterator is addressed by position, so advancing an iterator over a hash
        takes time proportional to the position.
        \enddifferences
    */
    class const_iterator {
    public:
        /*!
            The category of the iterator for the standard library.
        */
        using iterator_category = std::bidirectional_iterator_tag;
        /*!
            The type of the difference between two iterators.
        */
        using difference_type = qsizetype;
        /*!
            The type of the values: QVariant.
        */
        using value_type = QVariant;
        /*!
            The pointer type of the values.
        */
        using pointer = const QVariant *;
        /*!
            The type that dereferencing the iterator returns: a QVariant by value.
        */
        using reference = QVariant;

        /*!
            Constructs an iterator that does not refer to a container.
        */
        const_iterator() = default;
        QVariant key() const;
        QVariant value() const;
        /*!
            Returns the mapped value of the current entry. This is the same as value().
        */
        QVariant operator*() const { return value(); }
        /*!
            Returns \c true if \a o refers to the same entry of the same container.
        */
        bool operator==(const const_iterator &o) const { return m_owner == o.m_owner && m_index == o.m_index; }
        /*!
            Returns \c true if \a o refers to a different entry or container.
        */
        bool operator!=(const const_iterator &o) const { return !(*this == o); }
        /*!
            Advances the iterator by one entry and returns a reference to it.
        */
        const_iterator &operator++() {
            ++m_index;
            return *this;
        }
        /*!
            \overload
            Advances the iterator by one entry and returns its previous position.
        */
        const_iterator operator++(int) {
            const_iterator r = *this;
            ++m_index;
            return r;
        }
        /*!
            Moves the iterator back by one entry and returns a reference to it.
        */
        const_iterator &operator--() {
            --m_index;
            return *this;
        }
        /*!
            \overload
            Moves the iterator back by one entry and returns its previous position.
        */
        const_iterator operator--(int) {
            const_iterator r = *this;
            --m_index;
            return r;
        }

    private:
        friend class QAssociativeIterable;
        const QAssociativeIterable *m_owner = nullptr;
        qsizetype m_index = 0;
    };

    /*!
        \class QAssociativeIterable::iterator
        \since 6.0
        \brief The iterator class is an iterator over a QAssociativeIterable that can change the mapped values.

        key() returns the key of the current entry; value() and dereferencing give a reference to its mapped value
        that converts to a QVariant and can be assigned a new value.

        \differences
        The mapped value is a QAssociativeIterable::iterator::reference rather than a \c QVariantRef.
        \enddifferences
    */
    class iterator {
    public:
        /*!
            \class QAssociativeIterable::iterator::reference
            \brief The reference class refers to one mapped value of a container through a QAssociativeIterable.
        */
        class reference {
        public:
            /*!
                Returns the mapped value as a QVariant.
            */
            operator QVariant() const { return m_owner ? m_owner->value(m_key) : QVariant(); }
            /*!
                Replaces the mapped value with \a value, converted to the mapped type, and returns a reference to
                this reference.
            */
            reference &operator=(const QVariant &value) {
                if (m_owner)
                    m_owner->setValue(m_key, value);
                return *this;
            }

        private:
            friend class iterator;
            reference(QAssociativeIterable *owner, const QVariant &key) : m_owner(owner), m_key(key) {}
            QAssociativeIterable *m_owner;
            QVariant m_key;
        };

        /*!
            Constructs an iterator that does not refer to a container.
        */
        iterator() = default;
        QVariant key() const;
        /*!
            Returns a reference to the mapped value of the current entry.
        */
        reference value() const { return reference(m_owner, key()); }
        /*!
            Returns a reference to the mapped value of the current entry. This is the same as value().
        */
        reference operator*() const { return value(); }
        /*!
            Returns \c true if \a o refers to the same entry of the same container.
        */
        bool operator==(const iterator &o) const { return m_owner == o.m_owner && m_index == o.m_index; }
        /*!
            Returns \c true if \a o refers to a different entry or container.
        */
        bool operator!=(const iterator &o) const { return !(*this == o); }
        /*!
            Advances the iterator by one entry and returns a reference to it.
        */
        iterator &operator++() {
            ++m_index;
            return *this;
        }
        /*!
            Moves the iterator back by one entry and returns a reference to it.
        */
        iterator &operator--() {
            --m_index;
            return *this;
        }

    private:
        friend class QAssociativeIterable;
        QAssociativeIterable *m_owner = nullptr;
        qsizetype m_index = 0;
    };

    /*!
        \since 6.0
        Constructs an iterable that refers to no container.
    */
    QAssociativeIterable() = default;
    explicit QAssociativeIterable(const QVariant &variant);
    QAssociativeIterable(const QMetaAssociation &metaAssociation, QMetaType metaType, void *iterable);
    QAssociativeIterable(const QMetaAssociation &metaAssociation, QMetaType metaType, const void *iterable);
    /*!
        Constructs a copy of \a other.
    */
    QAssociativeIterable(const QAssociativeIterable &other) = default;
    /*!
        Assigns \a other to this iterable and returns a reference to it.
    */
    QAssociativeIterable &operator=(const QAssociativeIterable &other) = default;
    /*!
        Destroys the iterable.
    */
    ~QAssociativeIterable() = default;

    bool isValid() const;
    qsizetype size() const;
    const_iterator begin() const;
    const_iterator end() const;
    /*!
        Returns an iterator to the first entry.

        \sa begin(), constEnd()
    */
    const_iterator constBegin() const { return begin(); }
    /*!
        Returns an iterator past the last entry.

        \sa end(), constBegin()
    */
    const_iterator constEnd() const { return end(); }
    const_iterator find(const QVariant &key) const;
    /*!
        Returns an iterator to the entry with \a key, or end() if there is none.

        \sa find()
    */
    const_iterator constFind(const QVariant &key) const { return find(key); }
    bool containsKey(const QVariant &key) const;
    QVariant value(const QVariant &key) const;
    QMetaType keyMetaType() const;
    QMetaType mappedMetaType() const;
    QMetaAssociation metaContainer() const;

    void insertKey(const QVariant &key);
    void removeKey(const QVariant &key);
    void setValue(const QVariant &key, const QVariant &mapped);
    iterator mutableBegin();
    iterator mutableEnd();
    iterator mutableFind(const QVariant &key);
    /*!
        \since 6.0
        Returns \c true if the iterable can change the container: it was made by QVariant::view() or from a
        pointer to a container that is not const; otherwise returns \c false.
    */
    bool canModify() const { return m_data != nullptr; }

private:
    const void *container() const { return m_data ? m_data : m_constData ? m_constData : m_variant.constData(); }

    QVariant m_variant;
    const QtPrivate::AssociativeInterface *m_iface = nullptr;
    void *m_data = nullptr;
    const void *m_constData = nullptr;
};
