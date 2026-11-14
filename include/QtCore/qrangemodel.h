// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class QRangeModel;

namespace QRangeModelPrivate {

// The part of the model that does not depend on the range: the role names, and access to the protected members of
// QAbstractItemModel that the implementation needs.
class ImplBase {
public:
    explicit ImplBase(QRangeModel *model) : m_model(model) {}
    virtual ~ImplBase();
    ImplBase(const ImplBase &) = delete;
    ImplBase &operator=(const ImplBase &) = delete;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent) const = 0;
    virtual QModelIndex parent(const QModelIndex &child) const = 0;
    virtual int rowCount(const QModelIndex &parent) const = 0;
    virtual int columnCount(const QModelIndex &parent) const = 0;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const = 0;
    virtual QVariant data(const QModelIndex &index, int role) const = 0;
    virtual QMap<int, QVariant> itemData(const QModelIndex &index) const = 0;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) = 0;
    virtual bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &values) = 0;
    virtual bool clearItemData(const QModelIndex &index) = 0;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const = 0;
    virtual bool insertRows(int row, int count, const QModelIndex &parent) = 0;
    virtual bool removeRows(int row, int count, const QModelIndex &parent) = 0;
    virtual bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                          const QModelIndex &destinationParent, int destinationChild) = 0;
    virtual bool insertColumns(int column, int count, const QModelIndex &parent) = 0;
    virtual bool removeColumns(int column, int count, const QModelIndex &parent) = 0;
    virtual bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count,
                             const QModelIndex &destinationParent, int destinationChild) = 0;
    // The meta-object whose properties are the roles of a multi-role item, or null.
    virtual const QMetaObject *itemMetaObject() const = 0;
    // Whether the items are multi-role: gadgets, objects or maps of roles.
    virtual bool multiRole() const = 0;

    QHash<int, QByteArray> roleNames() const;

protected:
    QModelIndex createIndex(int row, int column, const void *pointer) const;
    bool ownsIndex(const QModelIndex &index) const;
    void beginInsertRows(const QModelIndex &parent, int first, int last);
    void endInsertRows();
    void beginRemoveRows(const QModelIndex &parent, int first, int last);
    void endRemoveRows();
    bool beginMoveRows(const QModelIndex &sourceParent, int first, int last, const QModelIndex &destinationParent,
                       int destinationChild);
    void endMoveRows();
    void beginInsertColumns(const QModelIndex &parent, int first, int last);
    void endInsertColumns();
    void beginRemoveColumns(const QModelIndex &parent, int first, int last);
    void endRemoveColumns();
    bool beginMoveColumns(const QModelIndex &sourceParent, int first, int last, const QModelIndex &destinationParent,
                          int destinationChild);
    void endMoveColumns();
    void emitDataChanged(const QModelIndex &index, int role);
    // Points the persistent indexes whose internal pointer is in `from` at the matching entry of `to`.
    void repointPersistentIndexes(const std::vector<const void *> &from, const std::vector<const void *> &to);

    static QVariant readProperty(const QMetaObject *mo, const void *gadget, const QObject *object, int property);
    static bool writeProperty(const QMetaObject *mo, void *gadget, QObject *object, int property,
                              const QVariant &value);

private:
    QRangeModel *m_model;
};

// --- Type traits --------------------------------------------------------------------------------------------------

template <typename T, typename = void> struct is_range : std::false_type {};
template <typename T>
struct is_range<T, std::void_t<decltype(std::begin(std::declval<T &>())), decltype(std::end(std::declval<T &>()))>>
    : std::true_type {};

template <typename T>
inline constexpr bool is_string_v =
    std::is_same_v<T, QString> || std::is_same_v<T, QByteArray> || std::is_same_v<T, std::string> ||
    std::is_same_v<T, std::wstring> || std::is_same_v<T, std::u16string> || std::is_same_v<T, std::u32string>;

template <typename T, typename = void> struct is_tuple_like : std::false_type {};
template <typename T> struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>> : std::true_type {};

template <typename T, typename = void> struct has_meta_object : std::false_type {};
template <typename T> struct has_meta_object<T, std::void_t<decltype(&T::staticMetaObject)>> : std::true_type {};

template <typename T> inline constexpr bool is_qobject_v = std::is_base_of_v<QObject, T>;
template <typename T> inline constexpr bool is_gadget_v = has_meta_object<T>::value && !is_qobject_v<T>;

// A map from roles (an int, a Qt::ItemDataRole, or a role name) to values: one item with several roles.
template <typename T, typename = void> struct is_role_map : std::false_type {};
template <typename T>
struct is_role_map<T, std::void_t<typename T::key_type, typename T::mapped_type>>
    : std::bool_constant<
          std::is_same_v<typename T::mapped_type, QVariant> &&
          (std::is_same_v<typename T::key_type, int> || std::is_same_v<typename T::key_type, Qt::ItemDataRole> ||
           std::is_same_v<typename T::key_type, QString> || std::is_same_v<typename T::key_type, QByteArray>)> {};

// Pointers and smart pointers are looked through: a row or an item may be a T*, a std::shared_ptr<T>, a
// std::unique_ptr<T> or a std::reference_wrapper<T>.
template <typename T> struct pointer_traits {
    using type = T;
    static constexpr bool is_pointer = false;
    static T *get(T &t) { return &t; }
};
template <typename T> struct pointer_traits<T *> {
    using type = T;
    static constexpr bool is_pointer = true;
    static T *get(T *t) { return t; }
};
template <typename T> struct pointer_traits<std::shared_ptr<T>> {
    using type = T;
    static constexpr bool is_pointer = true;
    static T *get(const std::shared_ptr<T> &t) { return t.get(); }
};
template <typename T> struct pointer_traits<std::unique_ptr<T>> {
    using type = T;
    static constexpr bool is_pointer = true;
    static T *get(const std::unique_ptr<T> &t) { return t.get(); }
};
template <typename T> struct pointer_traits<std::reference_wrapper<T>> {
    using type = T;
    static constexpr bool is_pointer = false;
    static T *get(const std::reference_wrapper<T> &t) { return &t.get(); }
};
template <typename T> struct pointer_traits<const T> {
    using inner = pointer_traits<T>;
    using type = std::conditional_t<inner::is_pointer, typename inner::type, const typename inner::type>;
    static constexpr bool is_pointer = inner::is_pointer;
    static type *get(const T &t) { return inner::get(const_cast<T &>(t)); }
};

template <typename T> using unwrapped_t = typename pointer_traits<T>::type;
template <typename T> auto *deref(T &t) { return pointer_traits<T>::get(t); }

// A range of a model: given by value (the model keeps a copy), by pointer or as a std::reference_wrapper (the
// model works on the caller's range).
template <typename R> struct range_storage {
    using range_type = std::remove_reference_t<R>;
    static constexpr bool by_reference = false;
};
template <typename R> struct range_storage<R *> {
    using range_type = R;
    static constexpr bool by_reference = true;
};
template <typename R> struct range_storage<std::reference_wrapper<R>> {
    using range_type = R;
    static constexpr bool by_reference = true;
};

// Range is the type that the constructor of QRangeModel deduced: a reference for an lvalue, a value otherwise.
template <typename Range> class Storage {
    using traits = range_storage<std::decay_t<Range>>;

public:
    using range_type = typename traits::range_type;

    explicit Storage(Range &&range) {
        if constexpr (traits::by_reference)
            m_pointer = deref(range);
        else
            m_owned.emplace(std::forward<Range>(range));
    }
    range_type &get() const {
        if constexpr (traits::by_reference)
            return *m_pointer;
        else
            return const_cast<range_type &>(*m_owned);
    }

private:
    std::optional<std::remove_const_t<range_type>> m_owned;
    range_type *m_pointer = nullptr;
};

template <typename Range> using range_value_t = std::remove_reference_t<decltype(*std::begin(std::declval<Range &>()))>;

template <typename Range> int rangeSize(const Range &range) {
    return static_cast<int>(std::distance(std::begin(range), std::end(range)));
}

template <typename Range> auto rangeAt(Range &range, int row) { return std::next(std::begin(range), row); }

// Containers that rows can be inserted into and removed from.
template <typename Range, typename = void> struct is_resizable : std::false_type {};
template <typename Range>
struct is_resizable<Range, std::void_t<decltype(std::declval<Range &>().insert(std::begin(std::declval<Range &>()),
                                                                               std::declval<range_value_t<Range>>())),
                                       decltype(std::declval<Range &>().erase(std::begin(std::declval<Range &>()),
                                                                              std::end(std::declval<Range &>())))>>
    : std::bool_constant<!std::is_const_v<Range>> {};

// --- Items ----------------------------------------------------------------------------------------------------------

// How a value in a cell answers data() and setData().
template <typename E> struct Item {
    using Pointee = unwrapped_t<std::remove_const_t<E>>;
    using U = std::remove_const_t<Pointee>;
    static constexpr bool is_multi_role = is_gadget_v<U> || is_qobject_v<U> || is_role_map<U>::value;
    static constexpr bool read_only = std::is_const_v<E> || std::is_const_v<Pointee>;

    static const QMetaObject *metaObject() {
        if constexpr (is_gadget_v<U> || is_qobject_v<U>)
            return &U::staticMetaObject;
        else
            return nullptr;
    }

    static QVariant read(const ImplBase &impl, const E &e, int role) {
        const U *u = deref(const_cast<E &>(e));
        if (!u)
            return QVariant();
        if constexpr (is_gadget_v<U> || is_qobject_v<U>) {
            const int property = U::staticMetaObject.indexOfProperty(impl.roleNames().value(role).constData());
            if (property < 0)
                return QVariant();
            if constexpr (is_qobject_v<U>)
                return U::staticMetaObject.property(property).read(u);
            else
                return U::staticMetaObject.property(property).readOnGadget(u);
        } else if constexpr (is_role_map<U>::value) {
            using K = typename U::key_type;
            if constexpr (std::is_same_v<K, QString>)
                return mapValue(*u, QString::fromUtf8(impl.roleNames().value(role)));
            else if constexpr (std::is_same_v<K, QByteArray>)
                return mapValue(*u, impl.roleNames().value(role));
            else
                return mapValue(*u, static_cast<K>(role));
        } else if constexpr (std::is_same_v<U, QVariant>) {
            return role == Qt::DisplayRole || role == Qt::EditRole ? *u : QVariant();
        } else {
            return role == Qt::DisplayRole || role == Qt::EditRole ? QVariant::fromValue(*u) : QVariant();
        }
    }

    static bool write(const ImplBase &impl, E &e, const QVariant &value, int role) {
        if constexpr (read_only) {
            Q_UNUSED(impl)
            Q_UNUSED(e)
            Q_UNUSED(value)
            Q_UNUSED(role)
            return false;
        } else {
            U *u = deref(e);
            if (!u)
                return false;
            if constexpr (is_gadget_v<U> || is_qobject_v<U>) {
                const int property = U::staticMetaObject.indexOfProperty(impl.roleNames().value(role).constData());
                if (property < 0)
                    return false;
                if constexpr (is_qobject_v<U>)
                    return U::staticMetaObject.property(property).write(u, value);
                else
                    return U::staticMetaObject.property(property).writeOnGadget(u, value);
            } else if constexpr (is_role_map<U>::value) {
                using K = typename U::key_type;
                if constexpr (std::is_same_v<K, QString>)
                    (*u)[QString::fromUtf8(impl.roleNames().value(role))] = value;
                else if constexpr (std::is_same_v<K, QByteArray>)
                    (*u)[impl.roleNames().value(role)] = value;
                else
                    (*u)[static_cast<K>(role)] = value;
                return true;
            } else if constexpr (std::is_same_v<U, QVariant>) {
                if (role != Qt::DisplayRole && role != Qt::EditRole)
                    return false;
                *u = value;
                return true;
            } else if constexpr (std::is_copy_assignable_v<U> && std::is_default_constructible_v<U>) {
                if (role != Qt::DisplayRole && role != Qt::EditRole)
                    return false;
                QVariant converted = value;
                if (!converted.convert(QMetaType::fromType<U>()))
                    return false;
                *u = converted.template value<U>();
                return true;
            } else {
                Q_UNUSED(value)
                Q_UNUSED(role)
                return false;
            }
        }
    }

    // Clears every role of a multi-role map; other items cannot be cleared.
    static bool clear(E &e) {
        if constexpr (!read_only && is_role_map<U>::value) {
            if (U *u = deref(e)) {
                u->clear();
                return true;
            }
        } else {
            Q_UNUSED(e)
        }
        return false;
    }

private:
    template <typename Map, typename Key> static QVariant mapValue(const Map &map, const Key &key) {
        const auto it = map.find(key);
        if (it == map.end())
            return QVariant();
        if constexpr (std::is_same_v<std::decay_t<decltype(*it)>, QVariant>)
            return *it;
        else
            return it->second;
    }
};

enum class RowKind { Value, Tuple, Properties, List };

template <typename T, typename F, std::size_t... I>
void visitTuple(T &t, int column, F &&f, std::index_sequence<I...>) {
    using std::get;
    (void)((column == static_cast<int>(I) ? (f(get<I>(t)), true) : false) || ...);
}

} // namespace QRangeModelPrivate

class QRangeModel : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(
        QHash<int, QByteArray> roleNames READ roleNames WRITE setRoleNames RESET resetRoleNames NOTIFY roleNamesChanged)
public:
    /*!
        \enum QRangeModel::RowCategory
        \since 6.10
        This enum says how QRangeModel presents a row of a gadget or QObject type.

        \value Default Each property is a column.
        \value MultiRoleItem The row is one item, and each property is a role of it, by the name in roleNames().
    */
    enum class RowCategory { Default, MultiRoleItem };

    /*!
        \class QRangeModel::RowOptions
        \since 6.10
        \brief The RowOptions template lets a row type choose how QRangeModel presents it.

        Specialize it for a row type with a \c rowCategory member:

        \code
        template <>
        struct QRangeModel::RowOptions<Book> {
            static constexpr auto rowCategory = QRangeModel::RowCategory::MultiRoleItem;
        };
        \endcode

        \differences
        None.
        \enddifferences
    */
    template <typename T> struct RowOptions {};

    template <typename Range, typename = std::enable_if_t<!std::is_base_of_v<QObject, std::decay_t<Range>>>>
    explicit QRangeModel(Range &&range, QObject *parent = nullptr);
    template <typename Range, typename Protocol,
              typename = std::enable_if_t<!std::is_base_of_v<QObject, std::decay_t<Range>> &&
                                          !std::is_convertible_v<Protocol, QObject *>>>
    explicit QRangeModel(Range &&range, Protocol &&protocol, QObject *parent = nullptr);
    ~QRangeModel() override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const final;
    QModelIndex parent(const QModelIndex &child) const final;
    QModelIndex sibling(int row, int column, const QModelIndex &index) const final;
    int rowCount(const QModelIndex &parent = QModelIndex()) const final;
    int columnCount(const QModelIndex &parent = QModelIndex()) const final;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const final;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QMap<int, QVariant> itemData(const QModelIndex &index) const override;
    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &values) override;
    bool clearItemData(const QModelIndex &index) override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) final;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) final;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                  int destinationChild) final;
    bool insertColumns(int column, int count, const QModelIndex &parent = QModelIndex()) final;
    bool removeColumns(int column, int count, const QModelIndex &parent = QModelIndex()) final;
    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                     int destinationChild) final;
    QHash<int, QByteArray> roleNames() const override;
    void setRoleNames(const QHash<int, QByteArray> &names);
    void resetRoleNames();

signals:
    /*!
        This signal is emitted when the role names change.

        \sa setRoleNames(), resetRoleNames()
    */
    void roleNamesChanged() { emitSignal(&QRangeModel::roleNamesChanged); }

private:
    friend class QRangeModelPrivate::ImplBase;
    QHash<int, QByteArray> defaultRoleNames() const;

    std::unique_ptr<QRangeModelPrivate::ImplBase> m_impl;
    QHash<int, QByteArray> m_roleNames;
};

namespace QRangeModelPrivate {

template <typename T, typename = void> struct row_category {
    static constexpr QRangeModel::RowCategory value = QRangeModel::RowCategory::Default;
};
template <typename T> struct row_category<T, std::void_t<decltype(QRangeModel::RowOptions<T>::rowCategory)>> {
    static constexpr QRangeModel::RowCategory value = QRangeModel::RowOptions<T>::rowCategory;
};

template <typename U> constexpr RowKind rowKind() {
    constexpr bool object = is_gadget_v<U> || is_qobject_v<U>;
    if constexpr (object && row_category<U>::value == QRangeModel::RowCategory::Default)
        return RowKind::Properties;
    else if constexpr (!object && is_tuple_like<U>::value)
        return RowKind::Tuple;
    else if constexpr (!object && is_range<U>::value && !is_string_v<U> && !is_role_map<U>::value)
        return RowKind::List;
    else
        return RowKind::Value;
}

template <typename R> struct Row {
    using Pointee = unwrapped_t<std::remove_const_t<R>>;
    using U = std::remove_const_t<Pointee>;
    static constexpr bool read_only = std::is_const_v<R> || std::is_const_v<Pointee>;
    static constexpr RowKind kind = rowKind<U>();

    // The first property of a row object: the properties of QObject itself are left out.
    static int propertyOffset() {
        if constexpr (is_qobject_v<U>)
            return QObject::staticMetaObject.propertyCount();
        else
            return 0;
    }

    // The number of columns of a row; `row` matters only for a list.
    static int columns(const R *row) {
        if constexpr (kind == RowKind::Properties) {
            Q_UNUSED(row)
            return U::staticMetaObject.propertyCount() - propertyOffset();
        } else if constexpr (kind == RowKind::List) {
            const U *u = row ? deref(const_cast<R &>(*row)) : nullptr;
            return u ? rangeSize(*u) : 0;
        } else if constexpr (kind == RowKind::Tuple) {
            Q_UNUSED(row)
            return static_cast<int>(std::tuple_size<U>::value);
        } else {
            Q_UNUSED(row)
            return 1;
        }
    }

    // Calls f with the item at `column` of the row, for the kinds that have items.
    template <typename F> static void visit(R &row, int column, F &&f) {
        if constexpr (kind == RowKind::Value) {
            if (column == 0)
                f(row);
        } else {
            auto *u = deref(row);
            if (!u)
                return;
            if constexpr (kind == RowKind::Tuple) {
                visitTuple(*u, column, std::forward<F>(f), std::make_index_sequence<std::tuple_size<U>::value>());
            } else if constexpr (kind == RowKind::List) {
                if (column >= 0 && column < rangeSize(*u))
                    f(*rangeAt(*u, column));
            } else {
                Q_UNUSED(column)
                Q_UNUSED(f)
            }
        }
    }

    static const QMetaObject *itemMetaObject() {
        if constexpr (kind == RowKind::Value)
            return Item<R>::metaObject();
        else if constexpr (kind == RowKind::List)
            return Item<range_value_t<U>>::metaObject();
        else
            return nullptr;
    }
    static bool multiRole() {
        if constexpr (kind == RowKind::Value)
            return Item<R>::is_multi_role;
        else if constexpr (kind == RowKind::List)
            return Item<range_value_t<U>>::is_multi_role;
        else
            return false;
    }
};

// No tree: a list or a table.
struct NoTree {};

// The tree protocol that the row type implements with its parentRow(), childRows() and setParentRow() members.
struct ImplicitTree {
    template <typename U> static auto parentRow(const U &row) -> decltype(row.parentRow()) { return row.parentRow(); }
    template <typename U> static auto childRows(U &row) -> decltype(row.childRows()) { return row.childRows(); }
    template <typename U, typename P>
    static auto setParentRow(U &row, P *parent) -> decltype(row.setParentRow(parent)) {
        return row.setParentRow(parent);
    }
};

template <typename U, typename = void> struct has_implicit_tree : std::false_type {};
template <typename U>
struct has_implicit_tree<
    U, std::void_t<decltype(std::declval<const U &>().parentRow()), decltype(std::declval<U &>().childRows())>>
    : std::true_type {};

// A protocol given to the constructor, with parentRow(row), childRows(row) and setParentRow(row, parent).
template <typename P> struct ExplicitTree {
    P protocol;
    template <typename U> auto parentRow(const U &row) const -> decltype(protocol.parentRow(row)) {
        return protocol.parentRow(row);
    }
    template <typename U> auto childRows(U &row) const -> decltype(protocol.childRows(row)) {
        return protocol.childRows(row);
    }
    template <typename U, typename Parent>
    auto setParentRow(U &row, Parent *parent) const -> decltype(protocol.setParentRow(row, parent)) {
        return protocol.setParentRow(row, parent);
    }
};

template <typename Traversal, typename U, typename = void> struct can_set_parent : std::false_type {};
template <typename Traversal, typename U>
struct can_set_parent<
    Traversal, U,
    std::void_t<decltype(std::declval<const Traversal &>().setParentRow(std::declval<U &>(), std::declval<U *>()))>>
    : std::true_type {};

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

// The model itself, for one kind of range and tree protocol.
template <typename Range, typename Traversal> class Impl final : public ImplBase {
    using Store = Storage<Range>;
    using RangeType = typename Store::range_type;
    using R = range_value_t<RangeType>;
    using RowT = Row<R>;
    using U = typename RowT::U;
    static constexpr bool is_tree = !std::is_same_v<Traversal, NoTree>;
    static constexpr bool read_only = std::is_const_v<RangeType> || RowT::read_only;
    static constexpr bool resizable = is_resizable<RangeType>::value && !read_only;

public:
    Impl(QRangeModel *model, Range &&range, Traversal traversal)
        : ImplBase(model), m_range(std::forward<Range>(range)), m_traversal(std::move(traversal)) {
        if constexpr (is_tree)
            adoptChildren(m_range.get(), nullptr);
    }

    QModelIndex index(int row, int column, const QModelIndex &parent) const override {
        if (row < 0 || column < 0 || row >= rowCount(parent) || column >= columnCount(parent))
            return QModelIndex();
        if constexpr (is_tree)
            return createIndex(row, column, parent.isValid() ? static_cast<const void *>(rowAt(parent)) : nullptr);
        else
            return createIndex(row, column, nullptr);
    }

    QModelIndex parent(const QModelIndex &child) const override {
        if constexpr (is_tree) {
            const U *row = child.isValid() ? static_cast<const U *>(child.internalPointer()) : nullptr;
            if (!row)
                return QModelIndex();
            const U *grand = parentOf(*row);
            RangeType *siblings = grand ? childrenOf(const_cast<U &>(*grand)) : &m_range.get();
            const int position = siblings ? positionOf(*siblings, row) : -1;
            if (position < 0)
                return QModelIndex();
            return createIndex(position, 0, grand);
        } else {
            Q_UNUSED(child)
            return QModelIndex();
        }
    }

    int rowCount(const QModelIndex &parent) const override {
        const RangeType *range = childRange(parent);
        return range ? rangeSize(*range) : 0;
    }

    int columnCount(const QModelIndex &parent) const override {
        if constexpr (RowT::kind == RowKind::List) {
            // A table of lists has as many columns as its first row.
            Q_UNUSED(parent)
            RangeType &range = m_range.get();
            return rangeSize(range) == 0 ? 0 : RowT::columns(&*std::begin(range));
        } else {
            Q_UNUSED(parent)
            return RowT::columns(nullptr);
        }
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override {
        if (!mutableRowAt(index))
            return Qt::NoItemFlags;
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (editable(index))
            f |= Qt::ItemIsEditable;
        if (!is_tree)
            f |= Qt::ItemNeverHasChildren;
        return f;
    }

    QVariant data(const QModelIndex &index, int role) const override {
        R *row = mutableRowAt(index);
        if (!row)
            return QVariant();
        if constexpr (RowT::kind == RowKind::Properties) {
            if (role != Qt::DisplayRole && role != Qt::EditRole)
                return QVariant();
            const U *u = deref(*row);
            if (!u)
                return QVariant();
            return readProperty(&U::staticMetaObject, u, objectOf(u), RowT::propertyOffset() + index.column());
        } else {
            QVariant result;
            RowT::visit(*row, index.column(), [&](auto &item) {
                result = Item<std::remove_reference_t<decltype(item)>>::read(*this, item, role);
            });
            return result;
        }
    }

    QMap<int, QVariant> itemData(const QModelIndex &index) const override {
        QMap<int, QVariant> out;
        if (!mutableRowAt(index))
            return out;
        if (multiRole()) {
            const QHash<int, QByteArray> names = roleNames();
            for (auto it = names.cbegin(); it != names.cend(); ++it) {
                const QVariant v = data(index, it.key());
                if (v.isValid())
                    out.insert(it.key(), v);
            }
        } else {
            for (const int role : {int(Qt::DisplayRole), int(Qt::EditRole)}) {
                const QVariant v = data(index, role);
                if (v.isValid())
                    out.insert(role, v);
            }
        }
        return out;
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role) override {
        if constexpr (read_only) {
            Q_UNUSED(index)
            Q_UNUSED(value)
            Q_UNUSED(role)
            return false;
        } else {
            R *row = mutableRowAt(index);
            if (!row)
                return false;
            bool done = false;
            if constexpr (RowT::kind == RowKind::Properties) {
                if (role != Qt::DisplayRole && role != Qt::EditRole)
                    return false;
                U *u = deref(*row);
                done = u && writeProperty(&U::staticMetaObject, u, objectOf(u), RowT::propertyOffset() + index.column(),
                                          value);
            } else {
                RowT::visit(*row, index.column(), [&](auto &item) {
                    done = Item<std::remove_reference_t<decltype(item)>>::write(*this, item, value, role);
                });
            }
            if (done)
                emitDataChanged(index, role);
            return done;
        }
    }

    bool setItemData(const QModelIndex &index, const QMap<int, QVariant> &values) override {
        bool any = false;
        for (auto it = values.cbegin(); it != values.cend(); ++it)
            any = setData(index, it.value(), it.key()) || any;
        return any;
    }

    bool clearItemData(const QModelIndex &index) override {
        if constexpr (read_only || RowT::kind == RowKind::Properties) {
            Q_UNUSED(index)
            return false;
        } else {
            R *row = mutableRowAt(index);
            if (!row)
                return false;
            bool done = false;
            RowT::visit(*row, index.column(),
                        [&](auto &item) { done = Item<std::remove_reference_t<decltype(item)>>::clear(item); });
            if (done)
                emitDataChanged(index, -1);
            return done;
        }
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if constexpr (RowT::kind == RowKind::Properties) {
            if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 &&
                section < RowT::columns(nullptr))
                return QString::fromLatin1(U::staticMetaObject.property(RowT::propertyOffset() + section).name());
        }
        Q_UNUSED(section)
        Q_UNUSED(orientation)
        Q_UNUSED(role)
        return QVariant();
    }

    bool insertRows(int row, int count, const QModelIndex &parent) override {
        if constexpr (resizable && std::is_default_constructible_v<U>) {
            RangeType *range = childRange(parent, true);
            if (!range || row < 0 || count <= 0 || row > rangeSize(*range))
                return false;
            const int columns = columnCount(parent);
            const std::vector<const void *> before = addresses(*range);
            beginInsertRows(parent, row, row + count - 1);
            for (int i = 0; i < count; ++i) {
                auto it = range->insert(rangeAt(*range, row + i), makeRow());
                resizeRow(*it, columns);
            }
            afterChange(*range, parent, before, [row, count](int i) { return i < row ? i : i + count; });
            endInsertRows();
            return true;
        } else {
            Q_UNUSED(row)
            Q_UNUSED(count)
            Q_UNUSED(parent)
            return false;
        }
    }

    bool removeRows(int row, int count, const QModelIndex &parent) override {
        if constexpr (resizable) {
            RangeType *range = childRange(parent);
            if (!range || row < 0 || count <= 0 || row + count > rangeSize(*range))
                return false;
            const std::vector<const void *> before = addresses(*range);
            beginRemoveRows(parent, row, row + count - 1);
            if constexpr (std::is_pointer_v<R>) {
                // A raw pointer row belongs to the model, which made it or was given it with the range.
                for (int i = row; i < row + count; ++i)
                    delete *rangeAt(*range, i);
            }
            range->erase(rangeAt(*range, row), rangeAt(*range, row + count));
            afterChange(*range, parent, before,
                        [row, count](int i) { return i < row ? i : (i < row + count ? -1 : i - count); });
            endRemoveRows();
            return true;
        } else {
            Q_UNUSED(row)
            Q_UNUSED(count)
            Q_UNUSED(parent)
            return false;
        }
    }

    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent,
                  int destinationChild) override {
        if constexpr (!read_only && std::is_move_assignable_v<R> && std::is_move_constructible_v<R>) {
            if (sourceParent != destinationParent)
                return false;
            RangeType *range = childRange(sourceParent);
            const int size = range ? rangeSize(*range) : 0;
            const int last = sourceRow + count;
            if (!range || count <= 0 || sourceRow < 0 || last > size || destinationChild < 0 ||
                destinationChild > size || (destinationChild >= sourceRow && destinationChild <= last))
                return false;
            const std::vector<const void *> before = addresses(*range);
            if (!beginMoveRows(sourceParent, sourceRow, last - 1, destinationParent, destinationChild))
                return false;
            if (destinationChild < sourceRow)
                std::rotate(rangeAt(*range, destinationChild), rangeAt(*range, sourceRow), rangeAt(*range, last));
            else
                std::rotate(rangeAt(*range, sourceRow), rangeAt(*range, last), rangeAt(*range, destinationChild));
            afterChange(*range, sourceParent, before, [=](int i) {
                if (destinationChild < sourceRow) {
                    if (i >= sourceRow && i < last)
                        return destinationChild + (i - sourceRow);
                    if (i >= destinationChild && i < sourceRow)
                        return i + count;
                } else {
                    if (i >= sourceRow && i < last)
                        return destinationChild - count + (i - sourceRow);
                    if (i >= last && i < destinationChild)
                        return i - count;
                }
                return i;
            });
            endMoveRows();
            return true;
        } else {
            Q_UNUSED(sourceParent)
            Q_UNUSED(sourceRow)
            Q_UNUSED(count)
            Q_UNUSED(destinationParent)
            Q_UNUSED(destinationChild)
            return false;
        }
    }

    bool insertColumns(int column, int count, const QModelIndex &parent) override {
        if constexpr (canInsertColumns()) {
            if (parent.isValid() || column < 0 || count <= 0 || column > columnCount(parent))
                return false;
            beginInsertColumns(parent, column, column + count - 1);
            for (auto &row : m_range.get()) {
                if (U *u = deref(row)) {
                    const int at = std::min(column, rangeSize(*u));
                    for (int i = 0; i < count; ++i)
                        u->insert(rangeAt(*u, at + i), range_value_t<U>());
                }
            }
            endInsertColumns();
            return true;
        } else {
            Q_UNUSED(column)
            Q_UNUSED(count)
            Q_UNUSED(parent)
            return false;
        }
    }

    bool removeColumns(int column, int count, const QModelIndex &parent) override {
        if constexpr (canChangeColumns()) {
            if (parent.isValid() || column < 0 || count <= 0 || column + count > columnCount(parent))
                return false;
            beginRemoveColumns(parent, column, column + count - 1);
            for (auto &row : m_range.get()) {
                if (U *u = deref(row)) {
                    const int size = rangeSize(*u);
                    if (column < size)
                        u->erase(rangeAt(*u, column), rangeAt(*u, std::min(size, column + count)));
                }
            }
            endRemoveColumns();
            return true;
        } else {
            Q_UNUSED(column)
            Q_UNUSED(count)
            Q_UNUSED(parent)
            return false;
        }
    }

    bool moveColumns(const QModelIndex &sourceParent, int sourceColumn, int count, const QModelIndex &destinationParent,
                     int destinationChild) override {
        if constexpr (!read_only && RowT::kind == RowKind::List && !is_tree) {
            const int columns = columnCount(sourceParent);
            const int last = sourceColumn + count;
            if (sourceParent.isValid() || destinationParent.isValid() || count <= 0 || sourceColumn < 0 ||
                last > columns || destinationChild < 0 || destinationChild > columns ||
                (destinationChild >= sourceColumn && destinationChild <= last))
                return false;
            if (!beginMoveColumns(sourceParent, sourceColumn, last - 1, destinationParent, destinationChild))
                return false;
            for (auto &row : m_range.get()) {
                U *u = deref(row);
                if (!u || rangeSize(*u) < std::max(last, destinationChild))
                    continue;
                if (destinationChild < sourceColumn)
                    std::rotate(rangeAt(*u, destinationChild), rangeAt(*u, sourceColumn), rangeAt(*u, last));
                else
                    std::rotate(rangeAt(*u, sourceColumn), rangeAt(*u, last), rangeAt(*u, destinationChild));
            }
            endMoveColumns();
            return true;
        } else {
            Q_UNUSED(sourceParent)
            Q_UNUSED(sourceColumn)
            Q_UNUSED(count)
            Q_UNUSED(destinationParent)
            Q_UNUSED(destinationChild)
            return false;
        }
    }

    const QMetaObject *itemMetaObject() const override { return RowT::itemMetaObject(); }
    bool multiRole() const override { return RowT::multiRole(); }

private:
    static constexpr bool canChangeColumns() {
        if constexpr (!read_only && RowT::kind == RowKind::List && !is_tree)
            return is_resizable<U>::value;
        else
            return false;
    }
    // Whether a list row can be given more elements: the element type is only named for a list.
    static constexpr bool canGrowRow() {
        if constexpr (RowT::kind == RowKind::List && is_resizable<U>::value)
            return std::is_default_constructible_v<range_value_t<U>>;
        else
            return false;
    }
    static constexpr bool canInsertColumns() { return canChangeColumns() && canGrowRow(); }

    static const QObject *objectOf(const U *u) {
        if constexpr (is_qobject_v<U>) {
            return u;
        } else {
            Q_UNUSED(u)
            return nullptr;
        }
    }
    static QObject *objectOf(U *u) {
        if constexpr (is_qobject_v<U>) {
            return u;
        } else {
            Q_UNUSED(u)
            return nullptr;
        }
    }

    bool editable(const QModelIndex &index) const {
        if constexpr (read_only) {
            Q_UNUSED(index)
            return false;
        } else if constexpr (RowT::kind == RowKind::Properties) {
            return U::staticMetaObject.property(RowT::propertyOffset() + index.column()).isWritable();
        } else {
            bool writable = false;
            if (R *row = mutableRowAt(index)) {
                RowT::visit(*row, index.column(),
                            [&](auto &item) { writable = !Item<std::remove_reference_t<decltype(item)>>::read_only; });
            }
            return writable;
        }
    }

    static R makeRow() {
        if constexpr (std::is_pointer_v<R>)
            return new U();
        else if constexpr (pointer_traits<R>::is_pointer)
            return R(new U());
        else
            return R();
    }

    template <typename RowArg> static void resizeRow(RowArg &row, int columns) {
        if constexpr (canGrowRow()) {
            if (U *u = deref(row)) {
                while (rangeSize(*u) < columns)
                    u->insert(std::end(*u), range_value_t<U>());
            }
        } else {
            Q_UNUSED(row)
            Q_UNUSED(columns)
        }
    }

    // The range of the children of `parent`: the top-level range for an invalid index. With `create`, a row
    // without children gets an empty list of them, if its type allows.
    RangeType *childRange(const QModelIndex &parent, bool create = false) const {
        if (!parent.isValid())
            return &m_range.get();
        if constexpr (is_tree) {
            if (parent.column() != 0)
                return nullptr;
            const U *row = rowAt(parent);
            return row ? childrenOf(const_cast<U &>(*row), create) : nullptr;
        } else {
            Q_UNUSED(create)
            return nullptr;
        }
    }

    // The row that `index` is in, unwrapped.
    const U *rowAt(const QModelIndex &index) const {
        R *row = mutableRowAt(index);
        return row ? deref(*row) : nullptr;
    }

    R *mutableRowAt(const QModelIndex &index) const {
        if (!index.isValid() || !ownsIndex(index))
            return nullptr;
        RangeType *range = &m_range.get();
        if constexpr (is_tree) {
            if (const U *parentRow = static_cast<const U *>(index.internalPointer()))
                range = childrenOf(const_cast<U &>(*parentRow));
        }
        if (!range || index.row() < 0 || index.row() >= rangeSize(*range))
            return nullptr;
        return &*rangeAt(*range, index.row());
    }

    static int positionOf(RangeType &range, const U *row) {
        int i = 0;
        for (auto &r : range) {
            if (deref(r) == row)
                return i;
            ++i;
        }
        return -1;
    }

    static std::vector<const void *> addresses(RangeType &range) {
        std::vector<const void *> out;
        for (auto &r : range)
            out.push_back(deref(r));
        return out;
    }

    const U *parentOf(const U &row) const {
        if constexpr (is_tree) {
            const auto p = m_traversal.parentRow(row);
            if constexpr (std::is_pointer_v<std::decay_t<decltype(p)>>)
                return p;
            else
                return deref(p);
        } else {
            Q_UNUSED(row)
            return nullptr;
        }
    }

    RangeType *childrenOf(U &row, bool create = false) const {
        if constexpr (is_tree) {
            auto &&children = m_traversal.childRows(row);
            using Ref = decltype(children);
            using C = std::remove_cv_t<std::remove_reference_t<Ref>>;
            if constexpr (is_optional<C>::value) {
                if (!children.has_value()) {
                    if constexpr (!std::is_const_v<std::remove_reference_t<Ref>>) {
                        if (!create)
                            return nullptr;
                        children.emplace();
                    } else {
                        return nullptr;
                    }
                }
                return const_cast<RangeType *>(&*children);
            } else if constexpr (std::is_pointer_v<C>) {
                Q_UNUSED(create)
                return const_cast<RangeType *>(children);
            } else {
                Q_UNUSED(create)
                return const_cast<RangeType *>(&children);
            }
        } else {
            Q_UNUSED(row)
            Q_UNUSED(create)
            return nullptr;
        }
    }

    void setParent(U &row, U *parent) {
        if constexpr (!read_only && is_tree && can_set_parent<Traversal, U>::value) {
            m_traversal.setParentRow(row, parent);
        } else {
            Q_UNUSED(row)
            Q_UNUSED(parent)
        }
    }

    // Gives each row of `range`, and their children down the tree, their parent.
    void adoptChildren(RangeType &range, U *parent) {
        for (auto &r : range) {
            U *u = deref(r);
            if (!u)
                continue;
            setParent(*u, parent);
            if (RangeType *children = childrenOf(*u))
                adoptChildren(*children, u);
        }
    }

    // After rows of `range` were inserted, removed or moved, which can move rows in memory: the rows and their
    // children get their parents again, and the persistent indexes of those children follow their parents. `map`
    // gives the new position of the row that was at a position, or -1 if it was removed.
    template <typename Map>
    void afterChange(RangeType &range, const QModelIndex &parent, const std::vector<const void *> &before, Map map) {
        if constexpr (is_tree) {
            U *parentRow = parent.isValid() ? const_cast<U *>(rowAt(parent)) : nullptr;
            for (auto &r : range) {
                U *u = deref(r);
                if (!u)
                    continue;
                setParent(*u, parentRow);
                if (RangeType *children = childrenOf(*u)) {
                    for (auto &c : *children) {
                        if (U *cu = deref(c))
                            setParent(*cu, u);
                    }
                }
            }
            const std::vector<const void *> after = addresses(range);
            std::vector<const void *> from, to;
            for (std::size_t i = 0; i < before.size(); ++i) {
                const int j = map(static_cast<int>(i));
                if (j >= 0 && j < static_cast<int>(after.size()) && before[i] != after[static_cast<std::size_t>(j)]) {
                    from.push_back(before[i]);
                    to.push_back(after[static_cast<std::size_t>(j)]);
                }
            }
            if (!from.empty())
                repointPersistentIndexes(from, to);
        } else {
            Q_UNUSED(range)
            Q_UNUSED(parent)
            Q_UNUSED(before)
            Q_UNUSED(map)
        }
    }

    mutable Store m_range;
    Traversal m_traversal;
};

template <typename Range>
using range_row_t = std::remove_const_t<
    unwrapped_t<std::remove_const_t<range_value_t<typename range_storage<std::decay_t<Range>>::range_type>>>>;

} // namespace QRangeModelPrivate

template <typename Range, typename>
QRangeModel::QRangeModel(Range &&range, QObject *parent) : QAbstractItemModel(parent) {
    using namespace QRangeModelPrivate;
    if constexpr (has_implicit_tree<range_row_t<Range>>::value)
        m_impl = std::make_unique<Impl<Range, ImplicitTree>>(this, std::forward<Range>(range), ImplicitTree{});
    else
        m_impl = std::make_unique<Impl<Range, NoTree>>(this, std::forward<Range>(range), NoTree{});
    m_roleNames = defaultRoleNames();
}

template <typename Range, typename Protocol, typename>
QRangeModel::QRangeModel(Range &&range, Protocol &&protocol, QObject *parent) : QAbstractItemModel(parent) {
    using namespace QRangeModelPrivate;
    using Traversal = ExplicitTree<std::decay_t<Protocol>>;
    m_impl = std::make_unique<Impl<Range, Traversal>>(this, std::forward<Range>(range),
                                                      Traversal{std::forward<Protocol>(protocol)});
    m_roleNames = defaultRoleNames();
}
