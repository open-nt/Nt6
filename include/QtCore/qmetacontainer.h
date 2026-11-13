// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>

#include <iterator>
#include <type_traits>
#include <utility>

namespace QtMetaContainerPrivate {

// What the iterators of a container can do.
enum IteratorCapability : quint8 {
    InputCapability = 1 << 0,
    ForwardCapability = 1 << 1,
    BiDirectionalCapability = 1 << 2,
    RandomAccessCapability = 1 << 3,
};
Q_DECLARE_FLAGS(IteratorCapabilities, IteratorCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(IteratorCapabilities)

// Where values can be added to and removed from a sequence.
enum AddRemoveCapability : quint8 {
    CanAddAtBegin = 1 << 0,
    CanRemoveAtBegin = 1 << 1,
    CanAddAtEnd = 1 << 2,
    CanRemoveAtEnd = 1 << 3,
};
Q_DECLARE_FLAGS(AddRemoveCapabilities, AddRemoveCapability)
Q_DECLARE_OPERATORS_FOR_FLAGS(AddRemoveCapabilities)

// The functions that QMetaContainer calls on a container of one type. A null function is one that the container
// does not have. Iterators are allocated by the create functions and freed by the destroy functions.
struct QMetaContainerInterface {
    enum Position : quint8 { AtBegin, AtEnd, Unspecified };

    using SizeFn = qsizetype (*)(const void *);
    using ClearFn = void (*)(void *);
    using CreateIteratorFn = void *(*)(void *, Position);
    using CreateConstIteratorFn = void *(*)(const void *, Position);
    using DestroyIteratorFn = void (*)(const void *);
    using CompareIteratorFn = bool (*)(const void *, const void *);
    using CopyIteratorFn = void (*)(void *, const void *);
    using AdvanceIteratorFn = void (*)(void *, qsizetype);
    using DiffIteratorFn = qsizetype (*)(const void *, const void *);

    IteratorCapabilities iteratorCapabilities = {};
    SizeFn sizeFn = nullptr;
    ClearFn clearFn = nullptr;

    CreateIteratorFn createIteratorFn = nullptr;
    DestroyIteratorFn destroyIteratorFn = nullptr;
    CompareIteratorFn compareIteratorFn = nullptr;
    CopyIteratorFn copyIteratorFn = nullptr;
    AdvanceIteratorFn advanceIteratorFn = nullptr;
    DiffIteratorFn diffIteratorFn = nullptr;

    CreateConstIteratorFn createConstIteratorFn = nullptr;
    DestroyIteratorFn destroyConstIteratorFn = nullptr;
    CompareIteratorFn compareConstIteratorFn = nullptr;
    CopyIteratorFn copyConstIteratorFn = nullptr;
    AdvanceIteratorFn advanceConstIteratorFn = nullptr;
    DiffIteratorFn diffConstIteratorFn = nullptr;
};

struct QMetaSequenceInterface : QMetaContainerInterface {
    using MetaTypeFn = const QMetaTypeInterface *(*)();
    using ValueAtIndexFn = void (*)(const void *, qsizetype, void *);
    using SetValueAtIndexFn = void (*)(void *, qsizetype, const void *);
    using AddValueFn = void (*)(void *, const void *, Position);
    using RemoveValueFn = void (*)(void *, Position);
    using ValueAtIteratorFn = void (*)(const void *, void *);
    using SetValueAtIteratorFn = void (*)(const void *, const void *);
    using InsertValueAtIteratorFn = void (*)(void *, const void *, const void *);
    using EraseValueAtIteratorFn = void (*)(void *, const void *);
    using EraseRangeAtIteratorFn = void (*)(void *, const void *, const void *);

    MetaTypeFn valueMetaType = nullptr;
    AddRemoveCapabilities addRemoveCapabilities = {};
    ValueAtIndexFn valueAtIndexFn = nullptr;
    SetValueAtIndexFn setValueAtIndexFn = nullptr;
    AddValueFn addValueFn = nullptr;
    RemoveValueFn removeValueFn = nullptr;
    ValueAtIteratorFn valueAtIteratorFn = nullptr;
    SetValueAtIteratorFn setValueAtIteratorFn = nullptr;
    InsertValueAtIteratorFn insertValueAtIteratorFn = nullptr;
    ValueAtIteratorFn valueAtConstIteratorFn = nullptr;
    EraseValueAtIteratorFn eraseValueAtIteratorFn = nullptr;
    EraseRangeAtIteratorFn eraseRangeAtIteratorFn = nullptr;
};

struct QMetaAssociationInterface : QMetaContainerInterface {
    using MetaTypeFn = const QMetaTypeInterface *(*)();
    using InsertKeyFn = void (*)(void *, const void *);
    using RemoveKeyFn = void (*)(void *, const void *);
    using ContainsKeyFn = bool (*)(const void *, const void *);
    using MappedAtKeyFn = void (*)(const void *, const void *, void *);
    using SetMappedAtKeyFn = void (*)(void *, const void *, const void *);
    using CreateIteratorAtKeyFn = void *(*)(void *, const void *);
    using CreateConstIteratorAtKeyFn = void *(*)(const void *, const void *);
    using KeyAtIteratorFn = void (*)(const void *, void *);
    using MappedAtIteratorFn = void (*)(const void *, void *);
    using SetMappedAtIteratorFn = void (*)(const void *, const void *);

    MetaTypeFn keyMetaType = nullptr;
    MetaTypeFn mappedMetaType = nullptr;
    InsertKeyFn insertKeyFn = nullptr;
    RemoveKeyFn removeKeyFn = nullptr;
    ContainsKeyFn containsKeyFn = nullptr;
    MappedAtKeyFn mappedAtKeyFn = nullptr;
    SetMappedAtKeyFn setMappedAtKeyFn = nullptr;
    CreateIteratorAtKeyFn createIteratorAtKeyFn = nullptr;
    CreateConstIteratorAtKeyFn createConstIteratorAtKeyFn = nullptr;
    KeyAtIteratorFn keyAtIteratorFn = nullptr;
    KeyAtIteratorFn keyAtConstIteratorFn = nullptr;
    MappedAtIteratorFn mappedAtIteratorFn = nullptr;
    MappedAtIteratorFn mappedAtConstIteratorFn = nullptr;
    SetMappedAtIteratorFn setMappedAtIteratorFn = nullptr;
};

// ---- Detection of what a container can do ----

template <typename C, typename = void> struct HasSize : std::false_type {};
template <typename C> struct HasSize<C, std::void_t<decltype(std::declval<const C &>().size())>> : std::true_type {};
template <typename C, typename = void> struct HasClear : std::false_type {};
template <typename C> struct HasClear<C, std::void_t<decltype(std::declval<C &>().clear())>> : std::true_type {};
template <typename C, typename = void> struct HasIterator : std::false_type {};
template <typename C>
struct HasIterator<C, std::void_t<typename C::iterator, decltype(std::declval<C &>().begin())>> : std::true_type {};
template <typename C, typename = void> struct HasConstIterator : std::false_type {};
template <typename C>
struct HasConstIterator<C, std::void_t<typename C::const_iterator, decltype(std::declval<const C &>().begin())>>
    : std::true_type {};
template <typename C, typename V, typename = void> struct HasPushBack : std::false_type {};
template <typename C, typename V>
struct HasPushBack<C, V, std::void_t<decltype(std::declval<C &>().push_back(std::declval<const V &>()))>>
    : std::true_type {};
template <typename C, typename V, typename = void> struct HasPushFront : std::false_type {};
template <typename C, typename V>
struct HasPushFront<C, V, std::void_t<decltype(std::declval<C &>().push_front(std::declval<const V &>()))>>
    : std::true_type {};
template <typename C, typename V, typename = void> struct HasInsertValue : std::false_type {};
template <typename C, typename V>
struct HasInsertValue<C, V, std::void_t<decltype(std::declval<C &>().insert(std::declval<const V &>()))>>
    : std::true_type {};
template <typename C, typename = void> struct HasPopBack : std::false_type {};
template <typename C> struct HasPopBack<C, std::void_t<decltype(std::declval<C &>().pop_back())>> : std::true_type {};
template <typename C, typename = void> struct HasPopFront : std::false_type {};
template <typename C> struct HasPopFront<C, std::void_t<decltype(std::declval<C &>().pop_front())>> : std::true_type {};
template <typename C, typename It, typename V, typename = void> struct HasInsertAt : std::false_type {};
template <typename C, typename It, typename V>
struct HasInsertAt<C, It, V,
                   std::void_t<decltype(std::declval<C &>().insert(std::declval<It>(), std::declval<const V &>()))>>
    : std::true_type {};
template <typename C, typename It, typename = void> struct HasEraseAt : std::false_type {};
template <typename C, typename It>
struct HasEraseAt<C, It, std::void_t<decltype(std::declval<C &>().erase(std::declval<It>()))>> : std::true_type {};
template <typename C, typename It, typename = void> struct HasEraseRange : std::false_type {};
template <typename C, typename It>
struct HasEraseRange<C, It, std::void_t<decltype(std::declval<C &>().erase(std::declval<It>(), std::declval<It>()))>>
    : std::true_type {};
template <typename C, typename K, typename = void> struct HasRemoveKey : std::false_type {};
template <typename C, typename K>
struct HasRemoveKey<C, K, std::void_t<decltype(std::declval<C &>().remove(std::declval<const K &>()))>>
    : std::true_type {};
template <typename C, typename K, typename = void> struct HasEraseKey : std::false_type {};
template <typename C, typename K>
struct HasEraseKey<C, K, std::void_t<decltype(std::declval<C &>().erase(std::declval<const K &>()))>> : std::true_type {
};
template <typename C, typename K, typename = void> struct HasFind : std::false_type {};
template <typename C, typename K>
struct HasFind<C, K, std::void_t<decltype(std::declval<const C &>().find(std::declval<const K &>()))>>
    : std::true_type {};
template <typename C, typename K, typename = void> struct HasSubscript : std::false_type {};
template <typename C, typename K>
struct HasSubscript<C, K, std::void_t<decltype(std::declval<C &>()[std::declval<const K &>()])>> : std::true_type {};
template <typename C, typename = void> struct IsAssociative : std::false_type {};
template <typename C>
struct IsAssociative<C, std::void_t<typename C::key_type, typename C::mapped_type>> : std::true_type {};

template <typename It> constexpr IteratorCapabilities capabilitiesOf() {
    using Category = typename std::iterator_traits<It>::iterator_category;
    IteratorCapabilities caps = InputCapability;
    if constexpr (std::is_base_of_v<std::forward_iterator_tag, Category>)
        caps |= ForwardCapability;
    if constexpr (std::is_base_of_v<std::bidirectional_iterator_tag, Category>)
        caps |= BiDirectionalCapability;
    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, Category>)
        caps |= RandomAccessCapability;
    return caps;
}

// ---- The functions for a container of type C ----

template <typename C> struct QMetaContainerForContainer {
    template <typename It> static void *create(It it) { return new It(std::move(it)); }
    template <typename It> static void destroy(const void *i) { delete static_cast<const It *>(i); }
    template <typename It> static bool compare(const void *i, const void *j) {
        return *static_cast<const It *>(i) == *static_cast<const It *>(j);
    }
    template <typename It> static void copy(void *target, const void *source) {
        *static_cast<It *>(target) = *static_cast<const It *>(source);
    }
    template <typename It> static void advance(void *i, qsizetype step) {
        std::advance(*static_cast<It *>(i), static_cast<typename std::iterator_traits<It>::difference_type>(step));
    }
    template <typename It> static qsizetype diff(const void *i, const void *j) {
        return static_cast<qsizetype>(std::distance(*static_cast<const It *>(j), *static_cast<const It *>(i)));
    }

    static constexpr void fill(QMetaContainerInterface &d) {
        if constexpr (HasSize<C>::value)
            d.sizeFn = [](const void *c) { return static_cast<qsizetype>(static_cast<const C *>(c)->size()); };
        if constexpr (HasClear<C>::value)
            d.clearFn = [](void *c) { static_cast<C *>(c)->clear(); };
        if constexpr (HasIterator<C>::value) {
            using It = typename C::iterator;
            d.iteratorCapabilities = capabilitiesOf<It>();
            d.createIteratorFn = [](void *c, QMetaContainerInterface::Position p) -> void * {
                C &container = *static_cast<C *>(c);
                return create<It>(p == QMetaContainerInterface::AtEnd ? container.end() : container.begin());
            };
            d.destroyIteratorFn = &destroy<It>;
            d.compareIteratorFn = &compare<It>;
            d.copyIteratorFn = &copy<It>;
            d.advanceIteratorFn = &advance<It>;
            d.diffIteratorFn = &diff<It>;
        }
        if constexpr (HasConstIterator<C>::value) {
            using It = typename C::const_iterator;
            if constexpr (!HasIterator<C>::value)
                d.iteratorCapabilities = capabilitiesOf<It>();
            d.createConstIteratorFn = [](const void *c, QMetaContainerInterface::Position p) -> void * {
                const C &container = *static_cast<const C *>(c);
                return create<It>(p == QMetaContainerInterface::AtEnd ? container.end() : container.begin());
            };
            d.destroyConstIteratorFn = &destroy<It>;
            d.compareConstIteratorFn = &compare<It>;
            d.copyConstIteratorFn = &copy<It>;
            d.advanceConstIteratorFn = &advance<It>;
            d.diffConstIteratorFn = &diff<It>;
        }
    }
};

template <typename C> struct QMetaSequenceForContainer : QMetaContainerForContainer<C> {
    using V = typename C::value_type;
    using Base = QMetaContainerForContainer<C>;

    static constexpr QMetaSequenceInterface make() {
        QMetaSequenceInterface d;
        Base::fill(d);
        d.valueMetaType = [] { return QMetaType::fromType<V>().iface(); };
        constexpr bool assignable = std::is_copy_assignable_v<V>;
        if constexpr (HasPushFront<C, V>::value)
            d.addRemoveCapabilities |= CanAddAtBegin;
        if constexpr (HasPopFront<C>::value)
            d.addRemoveCapabilities |= CanRemoveAtBegin;
        if constexpr (HasPushBack<C, V>::value)
            d.addRemoveCapabilities |= CanAddAtEnd;
        if constexpr (HasPopBack<C>::value)
            d.addRemoveCapabilities |= CanRemoveAtEnd;
        if constexpr (HasPushFront<C, V>::value || HasPushBack<C, V>::value || HasInsertValue<C, V>::value) {
            d.addValueFn = [](void *c, const void *v, QMetaContainerInterface::Position p) {
                C &container = *static_cast<C *>(c);
                const V &value = *static_cast<const V *>(v);
                if (p == QMetaContainerInterface::AtBegin) {
                    if constexpr (HasPushFront<C, V>::value)
                        container.push_front(value);
                } else if constexpr (HasPushBack<C, V>::value) {
                    container.push_back(value);
                } else if constexpr (HasInsertValue<C, V>::value) {
                    if (p == QMetaContainerInterface::Unspecified)
                        container.insert(value);
                }
            };
        }
        if constexpr (HasPopFront<C>::value || HasPopBack<C>::value) {
            d.removeValueFn = [](void *c, QMetaContainerInterface::Position p) {
                C &container = *static_cast<C *>(c);
                if (container.empty())
                    return;
                if (p == QMetaContainerInterface::AtBegin) {
                    if constexpr (HasPopFront<C>::value)
                        container.pop_front();
                } else if constexpr (HasPopBack<C>::value) {
                    container.pop_back();
                } else if (p == QMetaContainerInterface::Unspecified) {
                    container.pop_front();
                }
            };
        }
        if constexpr (HasIterator<C>::value) {
            using It = typename C::iterator;
            constexpr bool randomAccess = capabilitiesOf<It>() & RandomAccessCapability;
            constexpr bool writable = std::is_assignable_v<decltype(*std::declval<It &>()), const V &>;
            // A container of contiguous values moves them by assignment to insert and erase.
            constexpr bool shifts = !randomAccess || std::is_move_assignable_v<V>;
            if constexpr (randomAccess && writable)
                d.setValueAtIndexFn = [](void *c, qsizetype i, const void *v) {
                    *(static_cast<C *>(c)->begin() + i) = *static_cast<const V *>(v);
                };
            if constexpr (assignable)
                d.valueAtIteratorFn = [](const void *i, void *r) {
                    *static_cast<V *>(r) = **static_cast<const It *>(i);
                };
            if constexpr (writable)
                d.setValueAtIteratorFn = [](const void *i, const void *v) {
                    **static_cast<const It *>(i) = *static_cast<const V *>(v);
                };
            if constexpr (HasInsertAt<C, It, V>::value && shifts)
                d.insertValueAtIteratorFn = [](void *c, const void *i, const void *v) {
                    static_cast<C *>(c)->insert(*static_cast<const It *>(i), *static_cast<const V *>(v));
                };
            if constexpr (HasEraseAt<C, It>::value && shifts)
                d.eraseValueAtIteratorFn = [](void *c, const void *i) {
                    static_cast<C *>(c)->erase(*static_cast<const It *>(i));
                };
            if constexpr (HasEraseRange<C, It>::value && shifts)
                d.eraseRangeAtIteratorFn = [](void *c, const void *i, const void *j) {
                    static_cast<C *>(c)->erase(*static_cast<const It *>(i), *static_cast<const It *>(j));
                };
        }
        if constexpr (HasConstIterator<C>::value && assignable) {
            using It = typename C::const_iterator;
            if constexpr (capabilitiesOf<It>() & RandomAccessCapability)
                d.valueAtIndexFn = [](const void *c, qsizetype i, void *r) {
                    *static_cast<V *>(r) = *(static_cast<const C *>(c)->begin() + i);
                };
            d.valueAtConstIteratorFn = [](const void *i, void *r) {
                *static_cast<V *>(r) = **static_cast<const It *>(i);
            };
        }
        return d;
    }

    static constexpr QMetaSequenceInterface value = make();
};

template <typename C> struct QMetaAssociationForContainer : QMetaContainerForContainer<C> {
    using K = typename C::key_type;
    using M = typename C::mapped_type;
    using Base = QMetaContainerForContainer<C>;

    static constexpr QMetaAssociationInterface make() {
        QMetaAssociationInterface d;
        Base::fill(d);
        d.keyMetaType = [] { return QMetaType::fromType<K>().iface(); };
        d.mappedMetaType = [] { return QMetaType::fromType<M>().iface(); };
        constexpr bool keyAssignable = std::is_copy_assignable_v<K>;
        constexpr bool mappedAssignable = std::is_copy_assignable_v<M>;
        constexpr bool subscript = HasSubscript<C, K>::value && std::is_default_constructible_v<M>;
        if constexpr (subscript)
            d.insertKeyFn = [](void *c, const void *k) { (void)(*static_cast<C *>(c))[*static_cast<const K *>(k)]; };
        if constexpr (HasRemoveKey<C, K>::value)
            d.removeKeyFn = [](void *c, const void *k) { static_cast<C *>(c)->remove(*static_cast<const K *>(k)); };
        else if constexpr (HasEraseKey<C, K>::value)
            d.removeKeyFn = [](void *c, const void *k) { static_cast<C *>(c)->erase(*static_cast<const K *>(k)); };
        if constexpr (HasFind<C, K>::value) {
            d.containsKeyFn = [](const void *c, const void *k) {
                const C &container = *static_cast<const C *>(c);
                return container.find(*static_cast<const K *>(k)) != container.end();
            };
            if constexpr (mappedAssignable && std::is_default_constructible_v<M>)
                d.mappedAtKeyFn = [](const void *c, const void *k, void *r) {
                    const C &container = *static_cast<const C *>(c);
                    const auto it = container.find(*static_cast<const K *>(k));
                    *static_cast<M *>(r) = it == container.end() ? M() : QtPrivate::assocMapped(it);
                };
            if constexpr (HasIterator<C>::value)
                d.createIteratorAtKeyFn = [](void *c, const void *k) -> void * {
                    return Base::template create<typename C::iterator>(
                        static_cast<C *>(c)->find(*static_cast<const K *>(k)));
                };
            if constexpr (HasConstIterator<C>::value)
                d.createConstIteratorAtKeyFn = [](const void *c, const void *k) -> void * {
                    return Base::template create<typename C::const_iterator>(
                        static_cast<const C *>(c)->find(*static_cast<const K *>(k)));
                };
        }
        if constexpr (subscript && mappedAssignable)
            d.setMappedAtKeyFn = [](void *c, const void *k, const void *m) {
                (*static_cast<C *>(c))[*static_cast<const K *>(k)] = *static_cast<const M *>(m);
            };
        if constexpr (HasIterator<C>::value) {
            using It = typename C::iterator;
            if constexpr (keyAssignable)
                d.keyAtIteratorFn = [](const void *i, void *r) {
                    *static_cast<K *>(r) = QtPrivate::assocKey(*static_cast<const It *>(i));
                };
            if constexpr (mappedAssignable) {
                d.mappedAtIteratorFn = [](const void *i, void *r) {
                    *static_cast<M *>(r) = QtPrivate::assocMapped(*static_cast<const It *>(i));
                };
                if constexpr (std::is_assignable_v<decltype(QtPrivate::assocMapped(std::declval<const It &>())),
                                                   const M &>)
                    d.setMappedAtIteratorFn = [](const void *i, const void *m) {
                        QtPrivate::assocMapped(*static_cast<const It *>(i)) = *static_cast<const M *>(m);
                    };
            }
        }
        if constexpr (HasConstIterator<C>::value) {
            using It = typename C::const_iterator;
            if constexpr (keyAssignable)
                d.keyAtConstIteratorFn = [](const void *i, void *r) {
                    *static_cast<K *>(r) = QtPrivate::assocKey(*static_cast<const It *>(i));
                };
            if constexpr (mappedAssignable)
                d.mappedAtConstIteratorFn = [](const void *i, void *r) {
                    *static_cast<M *>(r) = QtPrivate::assocMapped(*static_cast<const It *>(i));
                };
        }
        return d;
    }

    static constexpr QMetaAssociationInterface value = make();
};

} // namespace QtMetaContainerPrivate

class QMetaContainer {
public:
    QMetaContainer() = default;
    explicit QMetaContainer(const QtMetaContainerPrivate::QMetaContainerInterface *d) : d_ptr(d) {}

    bool hasInputIterator() const;
    bool hasForwardIterator() const;
    bool hasBidirectionalIterator() const;
    bool hasRandomAccessIterator() const;

    bool hasSize() const;
    qsizetype size(const void *container) const;
    bool canClear() const;
    void clear(void *container) const;

    bool hasIterator() const;
    void *begin(void *container) const;
    void *end(void *container) const;
    void destroyIterator(const void *iterator) const;
    bool compareIterator(const void *i, const void *j) const;
    void copyIterator(void *target, const void *source) const;
    void advanceIterator(void *iterator, qsizetype step) const;
    qsizetype diffIterator(const void *i, const void *j) const;

    bool hasConstIterator() const;
    void *constBegin(const void *container) const;
    void *constEnd(const void *container) const;
    void destroyConstIterator(const void *iterator) const;
    bool compareConstIterator(const void *i, const void *j) const;
    void copyConstIterator(void *target, const void *source) const;
    void advanceConstIterator(void *iterator, qsizetype step) const;
    qsizetype diffConstIterator(const void *i, const void *j) const;

protected:
    /*!
        \internal
        The functions of the container type, or null.
    */
    const QtMetaContainerPrivate::QMetaContainerInterface *d_ptr = nullptr;
};

class QMetaSequence : public QMetaContainer {
public:
    QMetaSequence() = default;
    explicit QMetaSequence(const QtMetaContainerPrivate::QMetaSequenceInterface *d) : QMetaContainer(d) {}

    template <typename T> static constexpr QMetaSequence fromContainer() {
        static_assert(QtMetaContainerPrivate::HasConstIterator<T>::value,
                      "QMetaSequence::fromContainer() needs a container with begin(), end() and value_type");
        return QMetaSequence(&QtMetaContainerPrivate::QMetaSequenceForContainer<T>::value);
    }

    QMetaType valueMetaType() const;
    bool isSortable() const;

    bool canAddValueAtBegin() const;
    void addValueAtBegin(void *container, const void *value) const;
    bool canAddValueAtEnd() const;
    void addValueAtEnd(void *container, const void *value) const;
    bool canRemoveValueAtBegin() const;
    void removeValueAtBegin(void *container) const;
    bool canRemoveValueAtEnd() const;
    void removeValueAtEnd(void *container) const;

    bool canGetValueAtIndex() const;
    void valueAtIndex(const void *container, qsizetype index, void *result) const;
    bool canSetValueAtIndex() const;
    void setValueAtIndex(void *container, qsizetype index, const void *value) const;

    bool canAddValue() const;
    void addValue(void *container, const void *value) const;
    bool canRemoveValue() const;
    void removeValue(void *container) const;

    bool canGetValueAtIterator() const;
    void valueAtIterator(const void *iterator, void *result) const;
    bool canSetValueAtIterator() const;
    void setValueAtIterator(const void *iterator, const void *value) const;
    bool canInsertValueAtIterator() const;
    void insertValueAtIterator(void *container, const void *iterator, const void *value) const;
    bool canEraseValueAtIterator() const;
    void eraseValueAtIterator(void *container, const void *iterator) const;
    bool canEraseRangeAtIterator() const;
    void eraseRangeAtIterator(void *container, const void *iterator1, const void *iterator2) const;

    bool canGetValueAtConstIterator() const;
    void valueAtConstIterator(const void *iterator, void *result) const;

    const QtMetaContainerPrivate::QMetaSequenceInterface *iface() const { return d(); }

    /*!
        Returns \c true if \a a and \a b stand for the same container type; otherwise returns \c false.
    */
    friend bool operator==(const QMetaSequence &a, const QMetaSequence &b) { return a.d_ptr == b.d_ptr; }
    /*!
        Returns \c true if \a a and \a b stand for different container types; otherwise returns \c false.
    */
    friend bool operator!=(const QMetaSequence &a, const QMetaSequence &b) { return a.d_ptr != b.d_ptr; }

private:
    const QtMetaContainerPrivate::QMetaSequenceInterface *d() const {
        return static_cast<const QtMetaContainerPrivate::QMetaSequenceInterface *>(d_ptr);
    }
};

class QMetaAssociation : public QMetaContainer {
public:
    QMetaAssociation() = default;
    explicit QMetaAssociation(const QtMetaContainerPrivate::QMetaAssociationInterface *d) : QMetaContainer(d) {}

    template <typename T> static constexpr QMetaAssociation fromContainer() {
        static_assert(QtMetaContainerPrivate::IsAssociative<T>::value,
                      "QMetaAssociation::fromContainer() needs a container with key_type and mapped_type");
        return QMetaAssociation(&QtMetaContainerPrivate::QMetaAssociationForContainer<T>::value);
    }

    QMetaType keyMetaType() const;
    QMetaType mappedMetaType() const;

    bool canInsertKey() const;
    void insertKey(void *container, const void *key) const;
    bool canRemoveKey() const;
    void removeKey(void *container, const void *key) const;
    bool canContainsKey() const;
    bool containsKey(const void *container, const void *key) const;

    bool canGetMappedAtKey() const;
    void mappedAtKey(const void *container, const void *key, void *mapped) const;
    bool canSetMappedAtKey() const;
    void setMappedAtKey(void *container, const void *key, const void *mapped) const;

    bool canGetKeyAtIterator() const;
    void keyAtIterator(const void *iterator, void *key) const;
    bool canGetKeyAtConstIterator() const;
    void keyAtConstIterator(const void *iterator, void *key) const;
    bool canGetMappedAtIterator() const;
    void mappedAtIterator(const void *iterator, void *mapped) const;
    bool canGetMappedAtConstIterator() const;
    void mappedAtConstIterator(const void *iterator, void *mapped) const;
    bool canSetMappedAtIterator() const;
    void setMappedAtIterator(const void *iterator, const void *mapped) const;

    bool canCreateIteratorAtKey() const;
    void *createIteratorAtKey(void *container, const void *key) const;
    bool canCreateConstIteratorAtKey() const;
    void *createConstIteratorAtKey(const void *container, const void *key) const;

    const QtMetaContainerPrivate::QMetaAssociationInterface *iface() const { return d(); }

    /*!
        Returns \c true if \a a and \a b stand for the same container type; otherwise returns \c false.
    */
    friend bool operator==(const QMetaAssociation &a, const QMetaAssociation &b) { return a.d_ptr == b.d_ptr; }
    /*!
        Returns \c true if \a a and \a b stand for different container types; otherwise returns \c false.
    */
    friend bool operator!=(const QMetaAssociation &a, const QMetaAssociation &b) { return a.d_ptr != b.d_ptr; }

private:
    const QtMetaContainerPrivate::QMetaAssociationInterface *d() const {
        return static_cast<const QtMetaContainerPrivate::QMetaAssociationInterface *>(d_ptr);
    }
};

template <typename C>
const QtMetaContainerPrivate::QMetaSequenceInterface *QtPrivate::SequenceInterfaceFor<C>::metaSequence() {
    return &QtMetaContainerPrivate::QMetaSequenceForContainer<C>::value;
}

template <typename C>
const QtMetaContainerPrivate::QMetaAssociationInterface *QtPrivate::AssociativeInterfaceFor<C>::metaAssociation() {
    return &QtMetaContainerPrivate::QMetaAssociationForContainer<C>::value;
}
