// MIT License - see LICENSE
#pragma once

#include <QtCore/qmetacontainer.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>

#include <cstddef>
#include <iterator>

class QSequentialIterable {
public:
    /*!
        \class QSequentialIterable::const_iterator
        \brief The const_iterator class is a read-only iterator over a QSequentialIterable.

        Dereferencing it returns each element as a QVariant.

        \differences
        The iterator is addressed by index, so advancing an iterator over a
        std::list or a QSet takes time proportional to the position.
        \enddifferences
    */
    class const_iterator {
    public:
        /*!
            The category of the iterator for the standard library.
        */
        using iterator_category = std::random_access_iterator_tag;
        /*!
            The type of the difference between two iterators.
        */
        using difference_type = qsizetype;
        /*!
            The type of the elements: QVariant.
        */
        using value_type = QVariant;
        /*!
            The pointer type of the elements.
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
        QVariant operator*() const;
        /*!
            Returns the element \a j positions after the one this iterator refers to.
        */
        QVariant operator[](qsizetype j) const { return *(*this + j); }
        /*!
            Returns \c true if \a o refers to the same position in the same container.
        */
        bool operator==(const const_iterator &o) const { return m_owner == o.m_owner && m_index == o.m_index; }
        /*!
            Returns \c true if \a o refers to a different position or container.
        */
        bool operator!=(const const_iterator &o) const { return !(*this == o); }
        /*!
            Returns \c true if this iterator is before \a o.
        */
        bool operator<(const const_iterator &o) const { return m_index < o.m_index; }
        /*!
            Returns \c true if this iterator is before or at the position of \a o.
        */
        bool operator<=(const const_iterator &o) const { return m_index <= o.m_index; }
        /*!
            Returns \c true if this iterator is after \a o.
        */
        bool operator>(const const_iterator &o) const { return m_index > o.m_index; }
        /*!
            Returns \c true if this iterator is after or at the position of \a o.
        */
        bool operator>=(const const_iterator &o) const { return m_index >= o.m_index; }
        /*!
            Advances the iterator by one element and returns a reference to it.
        */
        const_iterator &operator++() {
            ++m_index;
            return *this;
        }
        /*!
            \overload
            Advances the iterator by one element and returns its previous position.
        */
        const_iterator operator++(int) {
            const_iterator r = *this;
            ++m_index;
            return r;
        }
        /*!
            Moves the iterator back by one element and returns a reference to it.
        */
        const_iterator &operator--() {
            --m_index;
            return *this;
        }
        /*!
            \overload
            Moves the iterator back by one element and returns its previous position.
        */
        const_iterator operator--(int) {
            const_iterator r = *this;
            --m_index;
            return r;
        }
        /*!
            Advances the iterator by \a j elements and returns a reference to it.
        */
        const_iterator &operator+=(qsizetype j) {
            m_index += j;
            return *this;
        }
        /*!
            Moves the iterator back by \a j elements and returns a reference to it.
        */
        const_iterator &operator-=(qsizetype j) {
            m_index -= j;
            return *this;
        }
        /*!
            Returns an iterator \a j elements after this one.
        */
        const_iterator operator+(qsizetype j) const {
            const_iterator r = *this;
            r.m_index += j;
            return r;
        }
        /*!
            Returns an iterator \a j elements before this one.
        */
        const_iterator operator-(qsizetype j) const {
            const_iterator r = *this;
            r.m_index -= j;
            return r;
        }
        /*!
            Returns the number of elements between \a j and this iterator.
        */
        qsizetype operator-(const const_iterator &j) const { return m_index - j.m_index; }

    private:
        friend class QSequentialIterable;
        const QSequentialIterable *m_owner = nullptr;
        qsizetype m_index = 0;
    };

    /*!
        \since 6.0
        This enum says where addValue() and removeValue() work.

        \value Unspecified Where the container adds or removes values: at the back of a list, anywhere in a set.
        \value AtBegin At the front.
        \value AtEnd At the back.
    */
    enum Position : quint8 { Unspecified, AtBegin, AtEnd };

    /*!
        \class QSequentialIterable::iterator
        \since 6.0
        \brief The iterator class is an iterator over a QSequentialIterable that can change the elements.

        Dereferencing it gives a reference that converts to a QVariant and can be assigned a new value.

        \differences
        Dereferencing gives a QSequentialIterable::iterator::reference rather than a \c QVariantRef.
        \enddifferences
    */
    class iterator {
    public:
        /*!
            \class QSequentialIterable::iterator::reference
            \brief The reference class refers to one element of a container through a QSequentialIterable.
        */
        class reference {
        public:
            /*!
                Returns the element as a QVariant.
            */
            operator QVariant() const { return m_owner ? m_owner->at(m_index) : QVariant(); }
            /*!
                Replaces the element with \a value, converted to the element type, and returns a reference to
                this reference.
            */
            reference &operator=(const QVariant &value) {
                if (m_owner)
                    m_owner->set(m_index, value);
                return *this;
            }

        private:
            friend class iterator;
            reference(QSequentialIterable *owner, qsizetype index) : m_owner(owner), m_index(index) {}
            QSequentialIterable *m_owner;
            qsizetype m_index;
        };

        /*!
            Constructs an iterator that does not refer to a container.
        */
        iterator() = default;
        /*!
            Returns a reference to the element that the iterator refers to.
        */
        reference operator*() const { return reference(m_owner, m_index); }
        /*!
            Returns \c true if \a o refers to the same position in the same container.
        */
        bool operator==(const iterator &o) const { return m_owner == o.m_owner && m_index == o.m_index; }
        /*!
            Returns \c true if \a o refers to a different position or container.
        */
        bool operator!=(const iterator &o) const { return !(*this == o); }
        /*!
            Advances the iterator by one element and returns a reference to it.
        */
        iterator &operator++() {
            ++m_index;
            return *this;
        }
        /*!
            Moves the iterator back by one element and returns a reference to it.
        */
        iterator &operator--() {
            --m_index;
            return *this;
        }
        /*!
            Advances the iterator by \a j elements and returns a reference to it.
        */
        iterator &operator+=(qsizetype j) {
            m_index += j;
            return *this;
        }
        /*!
            Returns the number of elements between \a j and this iterator.
        */
        qsizetype operator-(const iterator &j) const { return m_index - j.m_index; }

    private:
        friend class QSequentialIterable;
        QSequentialIterable *m_owner = nullptr;
        qsizetype m_index = 0;
    };

    /*!
        \since 6.0
        Constructs an iterable that refers to no container.
    */
    QSequentialIterable() = default;
    explicit QSequentialIterable(const QVariant &variant);
    QSequentialIterable(const QMetaSequence &metaSequence, QMetaType metaType, void *iterable);
    QSequentialIterable(const QMetaSequence &metaSequence, QMetaType metaType, const void *iterable);
    /*!
        Constructs a copy of \a other.
    */
    QSequentialIterable(const QSequentialIterable &other) = default;
    /*!
        Assigns \a other to this iterable and returns a reference to it.
    */
    QSequentialIterable &operator=(const QSequentialIterable &other) = default;
    /*!
        Destroys the iterable.
    */
    ~QSequentialIterable() = default;

    bool isValid() const;
    qsizetype size() const;
    QVariant at(qsizetype idx) const;
    const_iterator begin() const;
    const_iterator end() const;
    /*!
        Returns an iterator to the first element.

        \sa begin(), constEnd()
    */
    const_iterator constBegin() const { return begin(); }
    /*!
        Returns an iterator past the last element.

        \sa end(), constBegin()
    */
    const_iterator constEnd() const { return end(); }
    QMetaType valueMetaType() const;
    bool canReverseIterate() const;
    QMetaSequence metaContainer() const;

    void addValue(const QVariant &value, Position position = Unspecified);
    void removeValue(Position position = Unspecified);
    void set(qsizetype idx, const QVariant &value);
    iterator mutableBegin();
    iterator mutableEnd();
    /*!
        \since 6.0
        Returns \c true if the iterable can change the container: it was made by QVariant::view() or from a
        pointer to a container that is not const; otherwise returns \c false.
    */
    bool canModify() const { return m_data != nullptr; }

private:
    const void *container() const { return m_data ? m_data : m_constData ? m_constData : m_variant.constData(); }

    QVariant m_variant;
    const QtPrivate::SequenceInterface *m_iface = nullptr;
    void *m_data = nullptr;
    const void *m_constData = nullptr;
};
