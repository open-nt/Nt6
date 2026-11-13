// MIT License - see LICENSE
#pragma once

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

    explicit QAssociativeIterable(const QVariant &variant);
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

private:
    QVariant m_variant;
    const QtPrivate::AssociativeInterface *m_iface = nullptr;
};
