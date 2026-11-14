// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

class QObject;
class QMetaProperty;
class QUntypedBindable;
class QUntypedPropertyBinding;

namespace QtPrivate {
// Stands in for T && in the property classes when T is passed by value, so that the overloads taking an rvalue
// reference do not compete with the ones taking the value.
struct DisabledRvalueRef {};
template <typename T>
inline constexpr bool property_by_value_v = std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(void *) * 2;
} // namespace QtPrivate

/*!
    \class QUntypedPropertyData
    \since 6.0
    \brief The QUntypedPropertyData class is the base of all property data classes.

    It has no members; it lets the binding machinery refer to a property without knowing its type.

    \differences
    None.
    \enddifferences

    \sa QPropertyData
*/
class QUntypedPropertyData {
public:
    /*!
        Constructs the data.
    */
    QUntypedPropertyData() = default;
    QUntypedPropertyData(const QUntypedPropertyData &) = delete;
    QUntypedPropertyData &operator=(const QUntypedPropertyData &) = delete;
    /*!
        Destroys the data.
    */
    ~QUntypedPropertyData() = default;
};

/*!
    \class QPropertyData
    \since 6.0
    \brief The QPropertyData class holds the value of a property of type \c T.

    QProperty and QObjectBindableProperty derive from it and add the binding machinery. Its functions read and
    write the value without touching bindings or observers.

    \differences
    None.
    \enddifferences

    \sa QProperty
*/
template <typename T> class QPropertyData : public QUntypedPropertyData {
public:
    /*!
        \typedef QPropertyData::value_type
        The type of the value, \c T.
    */
    using value_type = T;
    /*!
        \typedef QPropertyData::parameter_type
        \c T by value if it is small and trivially copyable, otherwise \c{const T &}.
    */
    using parameter_type = std::conditional_t<QtPrivate::property_by_value_v<T>, T, const T &>;
    /*!
        \typedef QPropertyData::rvalue_ref
        \c{T &&}, or a private type that no argument matches when parameter_type is \c T by value.
    */
    using rvalue_ref = std::conditional_t<QtPrivate::property_by_value_v<T>, QtPrivate::DisabledRvalueRef, T &&>;
    /*!
        \typedef QPropertyData::arrow_operator_result
        What \c{operator->} of the property returns: a pointer for a class type.
    */
    using arrow_operator_result =
        std::conditional_t<std::is_pointer_v<T>, const T &, std::conditional_t<std::is_class_v<T>, const T *, void>>;

    /*!
        Constructs property data with a default-constructed value.
    */
    QPropertyData() = default;
    /*!
        Constructs property data holding \a t.
    */
    QPropertyData(parameter_type t) : val(t) {}
    /*!
        Constructs property data holding \a t, moved in.
    */
    QPropertyData(rvalue_ref t) : val(std::move(t)) {}
    /*!
        Destroys the data.
    */
    ~QPropertyData() = default;

    /*!
        Returns the value without registering a dependency on it.
    */
    parameter_type valueBypassingBindings() const { return val; }
    /*!
        Sets the value to \a v without removing a binding or notifying observers.
    */
    void setValueBypassingBindings(parameter_type v) { val = v; }
    /*!
        \overload
        Sets the value to \a v, moved in, without removing a binding or notifying observers.
    */
    void setValueBypassingBindings(rvalue_ref v) { val = std::move(v); }

protected:
    /*!
        \internal
    */
    T val = T(); // NOLINT(misc-non-private-member-variables-in-classes): the derived property templates use it
};

/*!
    \class QPropertyBindingSourceLocation
    \since 6.0
    \brief The QPropertyBindingSourceLocation class records where a binding was made.

    The constructor of a binding fills it in with the \c QT_PROPERTY_DEFAULT_BINDING_LOCATION macro, which uses
    the compiler's source location where it has one.

    \differences
    None.
    \enddifferences
*/
struct QPropertyBindingSourceLocation {
    /*!
        \variable QPropertyBindingSourceLocation::fileName
        The file that made the binding, or \c nullptr.
    */
    const char *fileName = nullptr;
    /*!
        \variable QPropertyBindingSourceLocation::functionName
        The function that made the binding, or \c nullptr.
    */
    const char *functionName = nullptr;
    /*!
        \variable QPropertyBindingSourceLocation::line
        The line that made the binding, or 0.
    */
    quint32 line = 0;
    /*!
        \variable QPropertyBindingSourceLocation::column
        The column that made the binding, or 0.
    */
    quint32 column = 0;

    /*!
        Constructs an empty location.
    */
    QPropertyBindingSourceLocation() = default;
    /*!
        Constructs a location for \a file, \a function, \a l and \a c.
    */
    QPropertyBindingSourceLocation(const char *file, const char *function, quint32 l, quint32 c)
        : fileName(file), functionName(function), line(l), column(c) {}
};

#if defined(__GNUC__) || defined(__clang__)
#define QT_PROPERTY_DEFAULT_BINDING_LOCATION                                                                           \
    QPropertyBindingSourceLocation(__builtin_FILE(), __builtin_FUNCTION(), __builtin_LINE(), 0)
#else
#define QT_PROPERTY_DEFAULT_BINDING_LOCATION QPropertyBindingSourceLocation(__FILE__, nullptr, __LINE__, 0)
#endif

/*!
    \class QPropertyBindingError
    \since 6.0
    \brief The QPropertyBindingError class describes what went wrong when a binding was evaluated.

    Get it from QUntypedPropertyBinding::error(). A binding loop, where a binding depends on its own value
    through other properties, is the usual error; the binding is then left with its last value.

    \differences
    None.
    \enddifferences
*/
class QPropertyBindingError {
public:
    /*!
        \enum QPropertyBindingError::Type
        The kinds of error.

        \value NoError The binding was evaluated.
        \value BindingLoop The binding depends on itself.
        \value EvaluationError The binding could not be evaluated.
        \value UnknownError Another error.
    */
    enum Type { NoError, BindingLoop, EvaluationError, UnknownError };

    /*!
        Constructs an error of the type \c NoError.
    */
    QPropertyBindingError() = default;
    /*!
        Constructs an error of the type \a type with the text \a description.
    */
    QPropertyBindingError(Type type, const QString &description = QString()) : m_type(type), m_text(description) {}

    /*!
        Returns \c true if the type is not \c NoError.
    */
    bool hasError() const { return m_type != NoError; }
    /*!
        Returns the type of the error.
    */
    Type type() const { return m_type; }
    /*!
        Returns a text that describes the error, which may be empty.
    */
    QString description() const { return m_text; }

private:
    Type m_type = NoError;
    QString m_text;
};

namespace QtPrivate {

struct BindingNode;
struct PropertyCell;
struct HandlerNode;

// The functions that evaluate bindings and deliver notifications; implemented in qproperty.cpp.
std::shared_ptr<PropertyCell> makeCell(QUntypedPropertyData *data);
// Registers `cell` as a dependency of the binding that is being evaluated, if there is one.
void registerRead(PropertyCell *cell);
// True while a binding is being evaluated in this thread, so that a property without a cell needs one.
bool isEvaluatingBinding();
// Tells the observers of `cell` that its value changed: dependent bindings are re-evaluated, then the change
// handlers and the owner's notifier run (or are deferred until the end of the update group).
void notifyChanged(PropertyCell *cell);
// Installs `binding` on `cell`, evaluating it; returns the binding that was installed before, or null.
std::shared_ptr<BindingNode> setBinding(PropertyCell *cell, const std::shared_ptr<BindingNode> &binding);
std::shared_ptr<BindingNode> takeBinding(PropertyCell *cell);
std::shared_ptr<BindingNode> currentBinding(const PropertyCell *cell);
void setOwnerNotifier(PropertyCell *cell, std::function<void()> notifier);
// Adds a change handler, which runs after each change until the returned node is destroyed.
std::shared_ptr<HandlerNode> addHandler(PropertyCell *cell, std::function<void()> handler);

// Evaluates the binding into the property it is installed on, notifying observers if the value changed.
void evaluateBinding(const std::shared_ptr<BindingNode> &binding);

// The binding's type-erased evaluation: writes the new value into the data and returns whether it changed.
using BindingEvaluator = std::function<bool(QUntypedPropertyData *)>;
std::shared_ptr<BindingNode> makeBinding(BindingEvaluator evaluate, QMetaType type,
                                         const QPropertyBindingSourceLocation &location);
QPropertyBindingError bindingError(const BindingNode *binding);
QMetaType bindingType(const BindingNode *binding);
QPropertyBindingSourceLocation bindingLocation(const BindingNode *binding);
// The property that the binding is installed on, or null.
QUntypedPropertyData *bindingTarget(const BindingNode *binding);

template <typename T, typename = void> struct has_equality : std::false_type {};
template <typename T>
struct has_equality<T, std::void_t<decltype(std::declval<const T &>() == std::declval<const T &>())>> : std::true_type {
};

// Stores the result of `f` into the typed data; reports a change unless the type is comparable and equal.
template <typename T, typename Functor> BindingEvaluator makeEvaluator(Functor f) {
    return [f = std::move(f)](QUntypedPropertyData *data) mutable -> bool {
        auto *typed = static_cast<QPropertyData<T> *>(data);
        T value = f();
        if constexpr (has_equality<T>::value) {
            if (typed->valueBypassingBindings() == value)
                return false;
        }
        typed->setValueBypassingBindings(std::move(value));
        return true;
    };
}

} // namespace QtPrivate

/*!
    \class QUntypedPropertyBinding
    \since 6.0
    \brief The QUntypedPropertyBinding class is a binding without its type: a function that computes the value of a
    property from other properties.

    QPropertyBinding derives from it. A binding is shared: copies refer to the same binding.

    \differences
    None.
    \enddifferences

    \sa QPropertyBinding, QProperty::setBinding()
*/
class QUntypedPropertyBinding {
public:
    /*!
        Constructs a null binding.
    */
    QUntypedPropertyBinding() = default;
    /*!
        \internal
    */
    explicit QUntypedPropertyBinding(std::shared_ptr<QtPrivate::BindingNode> node) : m_node(std::move(node)) {}
    /*!
        Constructs a copy of \a other, which refers to the same binding.
    */
    QUntypedPropertyBinding(const QUntypedPropertyBinding &other) = default;
    /*!
        Moves the binding out of \a other, which becomes null.
    */
    QUntypedPropertyBinding(QUntypedPropertyBinding &&other) noexcept = default;
    /*!
        Makes this a copy of \a other, which refers to the same binding.
    */
    QUntypedPropertyBinding &operator=(const QUntypedPropertyBinding &other) = default;
    /*!
        Moves the binding out of \a other, which becomes null.
    */
    QUntypedPropertyBinding &operator=(QUntypedPropertyBinding &&other) noexcept = default;
    /*!
        Destroys the handle; the binding lives on while a property or another handle refers to it.
    */
    ~QUntypedPropertyBinding() = default;

    /*!
        Returns \c true if there is no binding.
    */
    bool isNull() const { return !m_node; }
    /*!
        Returns the error of the last evaluation.
    */
    QPropertyBindingError error() const { return QtPrivate::bindingError(m_node.get()); }
    /*!
        Returns the type of the value that the binding computes.
    */
    QMetaType valueMetaType() const { return QtPrivate::bindingType(m_node.get()); }
    /*!
        \since 6.9
        Returns where the binding was made.
    */
    QPropertyBindingSourceLocation location() const { return QtPrivate::bindingLocation(m_node.get()); }

    /*!
        \internal
    */
    const std::shared_ptr<QtPrivate::BindingNode> &node() const { return m_node; }

private:
    std::shared_ptr<QtPrivate::BindingNode> m_node;
};

/*!
    \class QPropertyBinding
    \since 6.0
    \brief The QPropertyBinding class is a function that computes the value of a property of type \c T from other
    properties.

    Make one from a functor and install it with QProperty::setBinding(), or get the binding of a property with
    QProperty::binding(). Reading a property inside the functor makes the binding depend on it; the binding is
    evaluated again when a dependency changes.

    \differences
    None.
    \enddifferences

    \sa QProperty, QBindable
*/
template <typename T> class QPropertyBinding : public QUntypedPropertyBinding {
public:
    /*!
        Constructs a null binding.
    */
    QPropertyBinding() = default;
    /*!
        Constructs a binding that computes its value with \a f, made at \a location.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    QPropertyBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION)
        : QUntypedPropertyBinding(QtPrivate::makeBinding(QtPrivate::makeEvaluator<T>(std::forward<Functor>(f)),
                                                         QMetaType::fromType<T>(), location)) {}
    /*!
        Constructs a typed view of \a untyped, which must compute a \c T or be null.
    */
    explicit QPropertyBinding(const QUntypedPropertyBinding &untyped) : QUntypedPropertyBinding(untyped) {}
};

/*!
    \class QPropertyChangeHandler
    \since 6.0
    \brief The QPropertyChangeHandler class runs a functor whenever a property changes, for as long as it exists.

    Get one from QProperty::onValueChanged() or QProperty::subscribe() and keep it alive; destroying it stops the
    calls.

    \differences
    None.
    \enddifferences

    \sa QPropertyNotifier
*/
template <typename Functor> class QPropertyChangeHandler {
public:
    /*!
        Constructs a handler that is not attached to a property.
    */
    QPropertyChangeHandler() = default;
    /*!
        \internal
    */
    explicit QPropertyChangeHandler(std::shared_ptr<QtPrivate::HandlerNode> node) : m_node(std::move(node)) {}
    QPropertyChangeHandler(const QPropertyChangeHandler &) = delete;
    QPropertyChangeHandler &operator=(const QPropertyChangeHandler &) = delete;
    /*!
        Moves the handler out of \a other, which is detached.
    */
    QPropertyChangeHandler(QPropertyChangeHandler &&other) noexcept = default;
    /*!
        Moves the handler out of \a other.
    */
    QPropertyChangeHandler &operator=(QPropertyChangeHandler &&other) noexcept = default;
    /*!
        Destroys the handler: the functor is not called again.
    */
    ~QPropertyChangeHandler() = default;

private:
    std::shared_ptr<QtPrivate::HandlerNode> m_node;
};

/*!
    \class QPropertyNotifier
    \since 6.2
    \brief The QPropertyNotifier class runs a callback whenever a property changes, for as long as it exists.

    It is QPropertyChangeHandler without the functor in its type, so that it can be a member of a class. Get one
    from QProperty::addNotifier().

    \differences
    None.
    \enddifferences

    \sa QPropertyChangeHandler
*/
class QPropertyNotifier {
public:
    /*!
        Constructs a notifier that is not attached to a property.
    */
    QPropertyNotifier() = default;
    /*!
        \internal
    */
    explicit QPropertyNotifier(std::shared_ptr<QtPrivate::HandlerNode> node) : m_node(std::move(node)) {}
    QPropertyNotifier(const QPropertyNotifier &) = delete;
    QPropertyNotifier &operator=(const QPropertyNotifier &) = delete;
    /*!
        Moves the notifier out of \a other, which is detached.
    */
    QPropertyNotifier(QPropertyNotifier &&other) noexcept = default;
    /*!
        Moves the notifier out of \a other.
    */
    QPropertyNotifier &operator=(QPropertyNotifier &&other) noexcept = default;
    /*!
        Destroys the notifier: the callback is not called again.
    */
    ~QPropertyNotifier() = default;

private:
    std::shared_ptr<QtPrivate::HandlerNode> m_node;
};

namespace QtPrivate {

// The functions through which a QUntypedBindable works on a property of any kind.
struct QBindableInterface {
    void (*getter)(const QUntypedPropertyData *data, void *value);
    void (*setter)(QUntypedPropertyData *data, const void *value); // null for a read-only property
    QUntypedPropertyBinding (*getBinding)(const QUntypedPropertyData *data);
    QUntypedPropertyBinding (*setBinding)(QUntypedPropertyData *data, const QUntypedPropertyBinding &binding);
    QUntypedPropertyBinding (*makeBinding)(const QUntypedPropertyData *data,
                                           const QPropertyBindingSourceLocation &location);
    std::shared_ptr<HandlerNode> (*addHandler)(const QUntypedPropertyData *data, std::function<void()> handler);
    QMetaType (*metaType)();
};

// The common part of QProperty, QObjectBindableProperty and QObjectComputedProperty: the value, the cell that
// holds the bindings and observers, and the typed operations. Derived gives a static notifyOwner(cell) hook.
template <typename T, typename Derived> class QPropertyBase : public QPropertyData<T> {
    using Base = QPropertyData<T>;

public:
    using value_type = typename Base::value_type;
    using parameter_type = typename Base::parameter_type;
    using rvalue_ref = typename Base::rvalue_ref;
    using arrow_operator_result = typename Base::arrow_operator_result;

    QPropertyBase() = default;
    explicit QPropertyBase(parameter_type initialValue) : Base(initialValue) {}
    explicit QPropertyBase(rvalue_ref initialValue) : Base(std::move(initialValue)) {}

    parameter_type value() const {
        if (m_cell)
            registerRead(m_cell.get());
        else if (isEvaluatingBinding())
            registerRead(cell());
        return this->val;
    }
    arrow_operator_result operator->() const {
        if constexpr (std::is_pointer_v<T>)
            return value();
        else if constexpr (std::is_class_v<T>)
            return &this->val;
        else
            return;
    }
    parameter_type operator*() const { return value(); }
    operator parameter_type() const { return value(); }

    void setValue(parameter_type newValue) {
        if (m_cell)
            QtPrivate::takeBinding(m_cell.get());
        if constexpr (has_equality<T>::value) {
            if (this->val == newValue)
                return;
        }
        this->val = newValue;
        changed();
    }
    void setValue(rvalue_ref newValue) {
        if (m_cell)
            QtPrivate::takeBinding(m_cell.get());
        if constexpr (has_equality<T>::value) {
            if (this->val == newValue)
                return;
        }
        this->val = std::move(newValue);
        changed();
    }

    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &newBinding) {
        return QPropertyBinding<T>(QUntypedPropertyBinding(QtPrivate::setBinding(cell(), newBinding.node())));
    }
    bool setBinding(const QUntypedPropertyBinding &newBinding) {
        if (!newBinding.isNull() && newBinding.valueMetaType() != QMetaType::fromType<T>())
            return false;
        QtPrivate::setBinding(cell(), newBinding.node());
        return true;
    }
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    QPropertyBinding<T>
    setBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        return setBinding(QPropertyBinding<T>(std::forward<Functor>(f), location));
    }
    bool hasBinding() const { return m_cell && QtPrivate::currentBinding(m_cell.get()) != nullptr; }
    QPropertyBinding<T> binding() const {
        return QPropertyBinding<T>(QUntypedPropertyBinding(m_cell ? QtPrivate::currentBinding(m_cell.get()) : nullptr));
    }
    QPropertyBinding<T> takeBinding() {
        return QPropertyBinding<T>(QUntypedPropertyBinding(m_cell ? QtPrivate::takeBinding(m_cell.get()) : nullptr));
    }

    template <typename Functor> QPropertyChangeHandler<Functor> onValueChanged(Functor f) {
        return QPropertyChangeHandler<Functor>(addHandler(cell(), std::function<void()>(std::move(f))));
    }
    template <typename Functor> QPropertyChangeHandler<Functor> subscribe(Functor f) {
        f();
        return onValueChanged(std::move(f));
    }
    template <typename Functor> QPropertyNotifier addNotifier(Functor f) {
        return QPropertyNotifier(addHandler(cell(), std::function<void()>(std::move(f))));
    }

    // The interface for QUntypedBindable; the setter is omitted for a read-only property.
    static const QBindableInterface *bindableInterface(bool writable) {
        static const QBindableInterface writableInterface = {
            [](const QUntypedPropertyData *d, void *v) {
                *static_cast<T *>(v) = static_cast<const Derived *>(d)->value();
            },
            [](QUntypedPropertyData *d, const void *v) {
                static_cast<Derived *>(d)->setValue(*static_cast<const T *>(v));
            },
            [](const QUntypedPropertyData *d) {
                return QUntypedPropertyBinding(static_cast<const Derived *>(d)->binding());
            },
            [](QUntypedPropertyData *d, const QUntypedPropertyBinding &b) {
                return QUntypedPropertyBinding(static_cast<Derived *>(d)->setBinding(QPropertyBinding<T>(b)));
            },
            [](const QUntypedPropertyData *d, const QPropertyBindingSourceLocation &location) {
                const auto *p = static_cast<const Derived *>(d);
                return QUntypedPropertyBinding(QPropertyBinding<T>([p] { return p->value(); }, location));
            },
            [](const QUntypedPropertyData *d, std::function<void()> h) {
                return addHandler(const_cast<Derived *>(static_cast<const Derived *>(d))->cell(), std::move(h));
            },
            [] { return QMetaType::fromType<T>(); },
        };
        static const QBindableInterface readOnlyInterface = {
            writableInterface.getter,
            nullptr,
            nullptr,
            nullptr,
            writableInterface.makeBinding,
            writableInterface.addHandler,
            writableInterface.metaType,
        };
        return writable ? &writableInterface : &readOnlyInterface;
    }

protected:
    // The value changed: observers are told through the cell, or the owner directly when there is no cell yet.
    void changed() {
        if (m_cell)
            notifyChanged(m_cell.get());
        else
            Derived::changedWithoutCell(static_cast<Derived *>(this));
    }
    PropertyCell *cell() const {
        if (!m_cell) {
            m_cell = makeCell(const_cast<QPropertyBase *>(this));
            Derived::notifyOwner(const_cast<Derived *>(static_cast<const Derived *>(this)), m_cell.get());
        }
        return m_cell.get();
    }
    bool hasCell() const { return m_cell != nullptr; }

private:
    mutable std::shared_ptr<PropertyCell> m_cell;
};

} // namespace QtPrivate

/*!
    \class QUntypedBindable
    \since 6.0
    \brief The QUntypedBindable class is a handle onto a bindable property of any type.

    QBindable derives from it. Get one for a property of an object with QMetaProperty::bindable(); it then works
    on that property: binding(), setBinding(), takeBinding() and the change handlers.

    \differences
    A handle made from a QMetaProperty that is not bindable is invalid; it does not read and write the property
    through the meta-object as in Qt 6.5. There is no \c observe() with a QPropertyObserver.
    \enddifferences

    \sa QBindable, QMetaProperty::bindable()
*/
class QUntypedBindable {
public:
    /*!
        Constructs an invalid handle.
    */
    QUntypedBindable() = default;
    /*!
        \internal
    */
    QUntypedBindable(QUntypedPropertyData *d, const QtPrivate::QBindableInterface *i) : data(d), iface(i) {}

    /*!
        Returns \c true if the handle refers to a property.
    */
    bool isValid() const { return data != nullptr; }
    /*!
        Returns \c true if the property can have a binding.
    */
    bool isBindable() const { return iface && iface->getBinding; }
    /*!
        Returns \c true if the property cannot be set.
    */
    bool isReadOnly() const { return !(iface && iface->setter); }
    /*!
        Returns the binding of the property, or a null binding.
    */
    QUntypedPropertyBinding binding() const {
        return iface && iface->getBinding ? iface->getBinding(data) : QUntypedPropertyBinding();
    }
    /*!
        Installs \a binding on the property and returns \c true, or returns \c false if the property cannot have
        a binding or \a binding computes another type.
    */
    bool setBinding(const QUntypedPropertyBinding &binding) {
        if (!iface || !iface->setBinding)
            return false;
        if (!binding.isNull() && binding.valueMetaType() != iface->metaType())
            return false;
        iface->setBinding(data, binding);
        return true;
    }
    /*!
        Removes the binding of the property and returns it, or a null binding.
    */
    QUntypedPropertyBinding takeBinding() {
        return iface && iface->setBinding ? iface->setBinding(data, QUntypedPropertyBinding())
                                          : QUntypedPropertyBinding();
    }
    /*!
        Returns \c true if the property has a binding.
    */
    bool hasBinding() const { return !binding().isNull(); }
    /*!
        Returns a binding that evaluates to the value of the property, made at \a location, for use on another
        property of the same type.
    */
    QUntypedPropertyBinding
    makeBinding(const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) const {
        return iface && iface->makeBinding ? iface->makeBinding(data, location) : QUntypedPropertyBinding();
    }
    /*!
        Returns a handler that calls \a f whenever the property changes, for as long as the handler exists.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> onValueChanged(Functor f) const {
        if (!iface || !iface->addHandler)
            return QPropertyChangeHandler<Functor>();
        return QPropertyChangeHandler<Functor>(iface->addHandler(data, std::function<void()>(std::move(f))));
    }
    /*!
        Calls \a f once, then returns a handler that calls it whenever the property changes.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> subscribe(Functor f) const {
        f();
        return onValueChanged(std::move(f));
    }
    /*!
        Returns a notifier that calls \a f whenever the property changes, for as long as the notifier exists.
    */
    template <typename Functor> QPropertyNotifier addNotifier(Functor f) const {
        if (!iface || !iface->addHandler)
            return QPropertyNotifier();
        return QPropertyNotifier(iface->addHandler(data, std::function<void()>(std::move(f))));
    }
    /*!
        Returns the type of the property, or an invalid type for an invalid handle.
    */
    QMetaType metaType() const { return iface ? iface->metaType() : QMetaType(); }

protected:
    /*!
        \internal
    */
    QUntypedPropertyData *data = nullptr; // NOLINT(misc-non-private-member-variables-in-classes)
    /*!
        \internal
    */
    const QtPrivate::QBindableInterface *iface = nullptr; // NOLINT(misc-non-private-member-variables-in-classes)
};

namespace QtPrivate {
// The handle that the BINDABLE function of `property` returns for `object`, or an invalid one. Implemented with
// the meta-object in qproperty.cpp.
QUntypedBindable metaPropertyBindable(QObject *object, const QMetaProperty &property);
QUntypedBindable metaPropertyBindable(QObject *object, const char *name);
} // namespace QtPrivate

template <typename T> class QProperty;
template <typename Class, typename T, auto Offset, auto Signal> class QObjectBindableProperty;
template <typename Class, typename T, auto Offset, auto Getter> class QObjectComputedProperty;

/*!
    \class QBindable
    \since 6.0
    \brief The QBindable class is a handle onto a bindable property of type \c T.

    It gives one interface to QProperty, QObjectBindableProperty and QObjectComputedProperty, and to the bindable
    properties of a QObject found through the meta-object:

    \code
    QBindable<int> width(widget, "width");
    QProperty<int> height;
    height.setBinding([&] { return width.value() * 2; });
    \endcode

    \differences
    A handle made from a QMetaProperty that is not bindable is invalid; it does not read and write the property
    through the meta-object as in Qt 6.5.
    \enddifferences

    \sa QProperty, QMetaProperty::bindable()
*/
template <typename T> class QBindable : public QUntypedBindable {
public:
    using QUntypedBindable::QUntypedBindable;
    using QUntypedBindable::setBinding;

    /*!
        Constructs an invalid handle.
    */
    QBindable() = default;
    /*!
        Constructs a handle onto \a property.
    */
    QBindable(QProperty<T> *property) : QUntypedBindable(property, property->bindableInterface(true)) {}
    /*!
        Constructs a handle onto \a property.
    */
    template <typename Class, auto Offset, auto Signal>
    QBindable(QObjectBindableProperty<Class, T, Offset, Signal> *property)
        : QUntypedBindable(property, property->bindableInterface(true)) {}
    /*!
        Constructs a read-only handle onto \a property.
    */
    template <typename Class, auto Offset, auto Getter>
    QBindable(QObjectComputedProperty<Class, T, Offset, Getter> *property)
        : QUntypedBindable(property, property->bindableInterface(false)) {}
    /*!
        Constructs a handle onto the property \a property of \a obj, which must be bindable and of type \c T.
    */
    QBindable(QObject *obj, const QMetaProperty &property)
        : QBindable(QtPrivate::metaPropertyBindable(obj, property)) {}
    /*!
        Constructs a handle onto the property named \a property of \a obj, which must be bindable and of type \c T.
    */
    QBindable(QObject *obj, const char *property) : QBindable(QtPrivate::metaPropertyBindable(obj, property)) {}
    /*!
        Constructs a typed handle from \a untyped, which must be of type \c T or invalid.
    */
    explicit QBindable(const QUntypedBindable &untyped) : QUntypedBindable(untyped) {
        if (isValid() && metaType() != QMetaType::fromType<T>()) {
            data = nullptr;
            iface = nullptr;
        }
    }

    /*!
        Returns the binding of the property.
    */
    QPropertyBinding<T> binding() const { return QPropertyBinding<T>(QUntypedBindable::binding()); }
    /*!
        Installs \a binding on the property and returns the one that was installed before.
    */
    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &binding) {
        if (!iface || !iface->setBinding)
            return QPropertyBinding<T>();
        return QPropertyBinding<T>(iface->setBinding(data, binding));
    }
    /*!
        \overload
        Installs a binding that computes the value with \a f, made at \a location, and returns the binding that
        was installed before.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    QPropertyBinding<T>
    setBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        return setBinding(QPropertyBinding<T>(std::forward<Functor>(f), location));
    }
    /*!
        Removes the binding of the property and returns it.
    */
    QPropertyBinding<T> takeBinding() { return QPropertyBinding<T>(QUntypedBindable::takeBinding()); }
    /*!
        Returns a binding that evaluates to the value of the property, made at \a location.
    */
    QPropertyBinding<T>
    makeBinding(const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) const {
        return QPropertyBinding<T>(QUntypedBindable::makeBinding(location));
    }
    /*!
        Returns the value of the property, registering a dependency on it in a binding that is being evaluated.
    */
    T value() const {
        T v{};
        if (iface && iface->getter)
            iface->getter(data, &v);
        return v;
    }
    /*!
        Sets the value of the property to \a value, removing its binding. Does nothing on a read-only property.
    */
    void setValue(const T &value) {
        if (iface && iface->setter)
            iface->setter(data, &value);
    }
};

/*!
    \class QProperty
    \since 6.0
    \brief The QProperty class holds a value of type \c T that other properties can be bound to.

    A property has a value, and may have a binding: a function that computes the value from other properties.
    Reading a property inside a binding makes the binding depend on it, and a change to any dependency evaluates
    the binding again:

    \code
    QProperty<int> width(10), height(20);
    QProperty<int> area;
    area.setBinding([&] { return width * height; });
    width = 30;                       // area is now 600
    auto handler = area.onValueChanged([&] { qDebug() << "area is" << area.value(); });
    \endcode

    Setting a value with setValue() or \c operator= removes the binding. onValueChanged(), subscribe() and
    addNotifier() run a callback whenever the value changes. A binding that depends on its own value gets a
    QPropertyBindingError of the type \c BindingLoop and keeps its last value.

    Several changes can be grouped with Qt::beginPropertyUpdateGroup() and Qt::endPropertyUpdateGroup(), or
    QScopedPropertyUpdateGroup: bindings are evaluated and callbacks run once, at the end of the group.

    \differences
    Bindings are evaluated when a dependency changes, not lazily when the property is read. A property is not
    thread-safe: use it from one thread. There is no \c bindingData() and no QPropertyObserver.
    \enddifferences

    \sa QObjectBindableProperty, QBindable, QPropertyBinding
*/
template <typename T> class QProperty : public QtPrivate::QPropertyBase<T, QProperty<T>> {
    using Base = QtPrivate::QPropertyBase<T, QProperty<T>>;
    friend Base;

public:
    /*!
        \typedef QProperty::value_type
        The type of the value, \c T.
    */
    using value_type = typename Base::value_type;
    /*!
        \typedef QProperty::parameter_type
        \c T by value if it is small and trivially copyable, otherwise \c{const T &}.
    */
    using parameter_type = typename Base::parameter_type;
    /*!
        \typedef QProperty::rvalue_ref
        \c{T &&}, or a private type that no argument matches when parameter_type is \c T by value.
    */
    using rvalue_ref = typename Base::rvalue_ref;
    /*!
        \typedef QProperty::arrow_operator_result
        What \c{operator->} returns: a pointer to the value for a class type, the value for a pointer type.
    */
    using arrow_operator_result = typename Base::arrow_operator_result;

    /*!
        Constructs a property with a default-constructed value.
    */
    QProperty() = default;
    /*!
        Constructs a property with the value \a initialValue.
    */
    explicit QProperty(parameter_type initialValue) : Base(initialValue) {}
    /*!
        \overload
        Constructs a property with the value \a initialValue, moved in.
    */
    explicit QProperty(rvalue_ref initialValue) : Base(std::move(initialValue)) {}
    /*!
        Constructs a property with the binding \a binding.
    */
    explicit QProperty(const QPropertyBinding<T> &binding) { this->setBinding(binding); }
    /*!
        Constructs a property whose binding computes the value with \a f, made at \a location.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    explicit QProperty(Functor &&f,
                       const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        this->setBinding(std::forward<Functor>(f), location);
    }
    QProperty(const QProperty &) = delete;
    QProperty &operator=(const QProperty &) = delete;
    /*!
        Destroys the property. Bindings that depend on it are not evaluated again by it; handlers are detached.
    */
    ~QProperty() = default;

    /*!
        Returns the value, registering a dependency on it in a binding that is being evaluated.
    */
    parameter_type value() const { return Base::value(); }
    /*!
        Returns a pointer to the value, or the value if \c T is a pointer.
    */
    arrow_operator_result operator->() const { return Base::operator->(); }
    /*!
        Returns the value, as value() does.
    */
    parameter_type operator*() const { return Base::value(); }
    /*!
        Returns the value, as value() does.
    */
    operator parameter_type() const { return Base::value(); }
    /*!
        Sets the value to \a newValue, removes the binding, and notifies the observers if the value changed.
    */
    void setValue(parameter_type newValue) { Base::setValue(newValue); }
    /*!
        \overload
        Sets the value to \a newValue, moved in.
    */
    void setValue(rvalue_ref newValue) { Base::setValue(std::move(newValue)); }
    /*!
        Installs \a newBinding, evaluates it, and returns the binding that was installed before.
    */
    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &newBinding) { return Base::setBinding(newBinding); }
    /*!
        \overload
        Installs \a newBinding if it computes a \c T, and returns whether it did.
    */
    bool setBinding(const QUntypedPropertyBinding &newBinding) { return Base::setBinding(newBinding); }
    /*!
        \overload
        Installs a binding that computes the value with \a f, made at \a location, and returns the binding that
        was installed before.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    QPropertyBinding<T>
    setBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        return Base::setBinding(std::forward<Functor>(f), location);
    }
    /*!
        Returns \c true if the property has a binding.
    */
    bool hasBinding() const { return Base::hasBinding(); }
    /*!
        Returns the binding, or a null binding.
    */
    QPropertyBinding<T> binding() const { return Base::binding(); }
    /*!
        Removes the binding and returns it. The property keeps its value.
    */
    QPropertyBinding<T> takeBinding() { return Base::takeBinding(); }
    /*!
        Returns a handler that calls \a f whenever the value changes, for as long as the handler exists.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> onValueChanged(Functor f) {
        return Base::onValueChanged(std::move(f));
    }
    /*!
        Calls \a f once, then returns a handler that calls it whenever the value changes.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> subscribe(Functor f) {
        return Base::subscribe(std::move(f));
    }
    /*!
        Returns a notifier that calls \a f whenever the value changes, for as long as the notifier exists.
    */
    template <typename Functor> QPropertyNotifier addNotifier(Functor f) { return Base::addNotifier(std::move(f)); }

    /*!
        Sets the value to \a newValue, as setValue() does.
    */
    QProperty &operator=(parameter_type newValue) {
        this->setValue(newValue);
        return *this;
    }
    /*!
        \overload
        Sets the value to \a newValue, moved in.
    */
    QProperty &operator=(rvalue_ref newValue) {
        this->setValue(std::move(newValue));
        return *this;
    }
    /*!
        Installs \a newBinding, as setBinding() does.
    */
    QProperty &operator=(const QPropertyBinding<T> &newBinding) {
        this->setBinding(newBinding);
        return *this;
    }

private:
    static void notifyOwner(QProperty *, QtPrivate::PropertyCell *) {}
    static void changedWithoutCell(QProperty *) {}
};

/*!
    \class QObjectBindableProperty
    \since 6.0
    \brief The QObjectBindableProperty class is a bindable property that is a member of a QObject and emits a signal
    when it changes.

    Declare it with Q_OBJECT_BINDABLE_PROPERTY() in a class with Q_OBJECT, give the Q_PROPERTY a \c BINDABLE
    function that returns a QBindable of it, and ntmoc makes the property reachable through
    QMetaProperty::bindable():

    \code
    class Thermostat : public QObject {
        Q_OBJECT
        Q_PROPERTY(int target READ target WRITE setTarget NOTIFY targetChanged BINDABLE bindableTarget)
    public:
        int target() const { return m_target; }
        void setTarget(int t) { m_target = t; }
        QBindable<int> bindableTarget() { return &m_target; }
    signals:
        void targetChanged();
    private:
        Q_OBJECT_BINDABLE_PROPERTY(Thermostat, int, m_target, &Thermostat::targetChanged)
    };
    \endcode

    \c Signal may be \c nullptr, a member function of \c Class without arguments, or one that takes the new value.

    \differences
    The property holds its binding data itself, so an object with bindable properties is larger than in Qt, which
    keeps that data in the object's QBindingStorage. There is no \c bindingData().
    \enddifferences

    \sa QProperty, QBindable
*/
template <typename Class, typename T, auto Offset, auto Signal = nullptr>
class QObjectBindableProperty : public QtPrivate::QPropertyBase<T, QObjectBindableProperty<Class, T, Offset, Signal>> {
    using Base = QtPrivate::QPropertyBase<T, QObjectBindableProperty<Class, T, Offset, Signal>>;
    friend Base;

public:
    /*!
        \typedef QObjectBindableProperty::value_type
        The type of the value, \c T.
    */
    using value_type = typename Base::value_type;
    /*!
        \typedef QObjectBindableProperty::parameter_type
        \c T by value if it is small and trivially copyable, otherwise \c{const T &}.
    */
    using parameter_type = typename Base::parameter_type;
    /*!
        \typedef QObjectBindableProperty::rvalue_ref
        \c{T &&}, or a private type that no argument matches when parameter_type is \c T by value.
    */
    using rvalue_ref = typename Base::rvalue_ref;
    /*!
        \typedef QObjectBindableProperty::arrow_operator_result
        What \c{operator->} returns: a pointer to the value for a class type, the value for a pointer type.
    */
    using arrow_operator_result = typename Base::arrow_operator_result;

    /*!
        Constructs a property with a default-constructed value.
    */
    QObjectBindableProperty() = default;
    /*!
        Constructs a property with the value \a initialValue.
    */
    explicit QObjectBindableProperty(parameter_type initialValue) : Base(initialValue) {}
    /*!
        \overload
        Constructs a property with the value \a initialValue, moved in.
    */
    explicit QObjectBindableProperty(rvalue_ref initialValue) : Base(std::move(initialValue)) {}
    /*!
        Constructs a property with the binding \a binding.
    */
    explicit QObjectBindableProperty(const QPropertyBinding<T> &binding) { this->setBinding(binding); }
    /*!
        Constructs a property whose binding computes the value with \a f, made at \a location.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    explicit QObjectBindableProperty(
        Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        this->setBinding(std::forward<Functor>(f), location);
    }
    QObjectBindableProperty(const QObjectBindableProperty &) = delete;
    QObjectBindableProperty &operator=(const QObjectBindableProperty &) = delete;
    /*!
        Destroys the property.
    */
    ~QObjectBindableProperty() = default;

    /*!
        Returns the value, registering a dependency on it in a binding that is being evaluated.
    */
    parameter_type value() const { return Base::value(); }
    /*!
        Returns a pointer to the value, or the value if \c T is a pointer.
    */
    arrow_operator_result operator->() const { return Base::operator->(); }
    /*!
        Returns the value, as value() does.
    */
    parameter_type operator*() const { return Base::value(); }
    /*!
        Returns the value, as value() does.
    */
    operator parameter_type() const { return Base::value(); }
    /*!
        Sets the value to \a newValue, removes the binding, and emits the signal if the value changed.
    */
    void setValue(parameter_type newValue) { Base::setValue(newValue); }
    /*!
        \overload
        Sets the value to \a newValue, moved in.
    */
    void setValue(rvalue_ref newValue) { Base::setValue(std::move(newValue)); }
    /*!
        Installs \a newBinding, evaluates it, and returns the binding that was installed before.
    */
    QPropertyBinding<T> setBinding(const QPropertyBinding<T> &newBinding) { return Base::setBinding(newBinding); }
    /*!
        \overload
        Installs \a newBinding if it computes a \c T, and returns whether it did.
    */
    bool setBinding(const QUntypedPropertyBinding &newBinding) { return Base::setBinding(newBinding); }
    /*!
        \overload
        Installs a binding that computes the value with \a f, made at \a location, and returns the binding that
        was installed before.
    */
    template <typename Functor, std::enable_if_t<std::is_invocable_r_v<T, Functor &>, int> = 0>
    QPropertyBinding<T>
    setBinding(Functor &&f, const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) {
        return Base::setBinding(std::forward<Functor>(f), location);
    }
    /*!
        Returns \c true if the property has a binding.
    */
    bool hasBinding() const { return Base::hasBinding(); }
    /*!
        Returns the binding, or a null binding.
    */
    QPropertyBinding<T> binding() const { return Base::binding(); }
    /*!
        Removes the binding and returns it. The property keeps its value.
    */
    QPropertyBinding<T> takeBinding() { return Base::takeBinding(); }
    /*!
        Returns a handler that calls \a f whenever the value changes, for as long as the handler exists.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> onValueChanged(Functor f) {
        return Base::onValueChanged(std::move(f));
    }
    /*!
        Calls \a f once, then returns a handler that calls it whenever the value changes.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> subscribe(Functor f) {
        return Base::subscribe(std::move(f));
    }
    /*!
        Returns a notifier that calls \a f whenever the value changes, for as long as the notifier exists.
    */
    template <typename Functor> QPropertyNotifier addNotifier(Functor f) { return Base::addNotifier(std::move(f)); }

    /*!
        Sets the value to \a newValue, as setValue() does.
    */
    QObjectBindableProperty &operator=(parameter_type newValue) {
        this->setValue(newValue);
        return *this;
    }
    /*!
        \overload
        Sets the value to \a newValue, moved in.
    */
    QObjectBindableProperty &operator=(rvalue_ref newValue) {
        this->setValue(std::move(newValue));
        return *this;
    }
    /*!
        Installs \a newBinding, as setBinding() does.
    */
    QObjectBindableProperty &operator=(const QPropertyBinding<T> &newBinding) {
        this->setBinding(newBinding);
        return *this;
    }

    /*!
        Emits the signal, if there is one. Call it after changing the value with setValueBypassingBindings().
    */
    void notify() {
        if (this->hasCell())
            QtPrivate::notifyChanged(this->cell());
        else
            emitSignal(owner());
    }

private:
    Class *owner() { return reinterpret_cast<Class *>(reinterpret_cast<char *>(this) - Offset()); }
    static void emitSignal(Class *object) {
        if constexpr (!std::is_same_v<decltype(Signal), std::nullptr_t>) {
            if constexpr (std::is_invocable_v<decltype(Signal), Class *>)
                (object->*Signal)();
            else
                (object->*Signal)(
                    reinterpret_cast<QObjectBindableProperty *>(reinterpret_cast<char *>(object) + Offset())->value());
        } else {
            Q_UNUSED(object)
        }
    }
    static void changedWithoutCell(QObjectBindableProperty *self) { emitSignal(self->owner()); }
    static void notifyOwner(QObjectBindableProperty *self, QtPrivate::PropertyCell *cell) {
        if constexpr (!std::is_same_v<decltype(Signal), std::nullptr_t>) {
            QtPrivate::setOwnerNotifier(cell, [self] { emitSignal(self->owner()); });
        } else {
            Q_UNUSED(self)
            Q_UNUSED(cell)
        }
    }
};

/*!
    \class QObjectComputedProperty
    \since 6.2
    \brief The QObjectComputedProperty class is a read-only bindable property whose value a function of the
    object computes.

    Declare it with Q_OBJECT_COMPUTED_PROPERTY(). Bindings can depend on it; the object calls notify() when the
    computed value may have changed, which evaluates them again.

    \differences
    None.
    \enddifferences

    \sa QObjectBindableProperty
*/
template <typename Class, typename T, auto Offset, auto Getter>
class QObjectComputedProperty : public QUntypedPropertyData {
public:
    /*!
        \typedef QObjectComputedProperty::value_type
        The type of the value, \c T.
    */
    using value_type = T;
    /*!
        \typedef QObjectComputedProperty::parameter_type
        \c T by value.
    */
    using parameter_type = T;

    /*!
        Constructs the property.
    */
    QObjectComputedProperty() = default;
    QObjectComputedProperty(const QObjectComputedProperty &) = delete;
    QObjectComputedProperty &operator=(const QObjectComputedProperty &) = delete;
    /*!
        Destroys the property.
    */
    ~QObjectComputedProperty() = default;

    /*!
        Returns the computed value, registering a dependency on the property in a binding that is being evaluated.
    */
    parameter_type value() const {
        if (m_cell || QtPrivate::isEvaluatingBinding())
            QtPrivate::registerRead(cell());
        return (owner()->*Getter)();
    }
    /*!
        Returns the value, as value() does.
    */
    parameter_type operator*() const { return value(); }
    /*!
        Returns the value, as value() does.
    */
    operator parameter_type() const { return value(); }
    /*!
        Returns \c false: a computed property has no binding.
    */
    bool hasBinding() const { return false; }
    /*!
        Returns a binding that evaluates to the value, made at \a location, for use on another property.
    */
    QPropertyBinding<T>
    makeBinding(const QPropertyBindingSourceLocation &location = QT_PROPERTY_DEFAULT_BINDING_LOCATION) const {
        return QPropertyBinding<T>([this] { return value(); }, location);
    }
    /*!
        Returns a handler that calls \a f whenever notify() is called, for as long as the handler exists.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> onValueChanged(Functor f) {
        return QPropertyChangeHandler<Functor>(QtPrivate::addHandler(cell(), std::function<void()>(std::move(f))));
    }
    /*!
        Calls \a f once, then returns a handler that calls it whenever notify() is called.
    */
    template <typename Functor> QPropertyChangeHandler<Functor> subscribe(Functor f) {
        f();
        return onValueChanged(std::move(f));
    }
    /*!
        Returns a notifier that calls \a f whenever notify() is called, for as long as the notifier exists.
    */
    template <typename Functor> QPropertyNotifier addNotifier(Functor f) {
        return QPropertyNotifier(QtPrivate::addHandler(cell(), std::function<void()>(std::move(f))));
    }
    /*!
        Tells the bindings and handlers that depend on the property that its value may have changed.
    */
    void notify() {
        if (m_cell)
            QtPrivate::notifyChanged(m_cell.get());
    }

    /*!
        \internal
    */
    static const QtPrivate::QBindableInterface *bindableInterface(bool) {
        static const QtPrivate::QBindableInterface readOnlyInterface = {
            [](const QUntypedPropertyData *d, void *v) {
                *static_cast<T *>(v) = static_cast<const QObjectComputedProperty *>(d)->value();
            },
            nullptr,
            nullptr,
            nullptr,
            [](const QUntypedPropertyData *d, const QPropertyBindingSourceLocation &location) {
                return QUntypedPropertyBinding(static_cast<const QObjectComputedProperty *>(d)->makeBinding(location));
            },
            [](const QUntypedPropertyData *d, std::function<void()> h) {
                return QtPrivate::addHandler(static_cast<const QObjectComputedProperty *>(d)->cell(), std::move(h));
            },
            [] { return QMetaType::fromType<T>(); },
        };
        return &readOnlyInterface;
    }

private:
    const Class *owner() const {
        return reinterpret_cast<const Class *>(reinterpret_cast<const char *>(this) - Offset());
    }
    QtPrivate::PropertyCell *cell() const {
        if (!m_cell)
            m_cell = QtPrivate::makeCell(const_cast<QObjectComputedProperty *>(this));
        return m_cell.get();
    }

    mutable std::shared_ptr<QtPrivate::PropertyCell> m_cell;
};

// The offset function of a bindable member: offsetof() on a QObject class is conditionally supported, and GCC and
// Clang warn about it, so the warning is silenced around the function.
#if defined(__GNUC__) || defined(__clang__)
#define NT_PROPERTY_OFFSET_FUNCTION(Class, name)                                                                       \
    _Pragma("GCC diagnostic push")                                                                                     \
        _Pragma("GCC diagnostic ignored \"-Winvalid-offsetof\"") static constexpr std::size_t                          \
        _qt_property_##name##_offset() {                                                                               \
        return offsetof(Class, name);                                                                                  \
    }                                                                                                                  \
    _Pragma("GCC diagnostic pop")
#else
#define NT_PROPERTY_OFFSET_FUNCTION(Class, name)                                                                       \
    static constexpr std::size_t _qt_property_##name##_offset() { return offsetof(Class, name); }
#endif

// Declares a QObjectBindableProperty member `name` of `Class` with the value type `Type`, which emits `Signal` (a
// member function pointer, or nothing) when it changes. The offset function lets the property find its object.
#define Q_OBJECT_BINDABLE_PROPERTY_4(Class, Type, name, Signal)                                                        \
    NT_PROPERTY_OFFSET_FUNCTION(Class, name)                                                                           \
    QObjectBindableProperty<Class, Type, Class::_qt_property_##name##_offset, Signal> name;
#define Q_OBJECT_BINDABLE_PROPERTY_3(Class, Type, name) Q_OBJECT_BINDABLE_PROPERTY_4(Class, Type, name, nullptr)
#define NT_BINDABLE_PICK(_1, _2, _3, _4, NAME, ...) NAME
#define Q_OBJECT_BINDABLE_PROPERTY(...)                                                                                \
    NT_BINDABLE_PICK(__VA_ARGS__, Q_OBJECT_BINDABLE_PROPERTY_4, Q_OBJECT_BINDABLE_PROPERTY_3, unused)(__VA_ARGS__)

// The same, with `value` as the initial value.
#define Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS_5(Class, Type, name, value, Signal)                                       \
    NT_PROPERTY_OFFSET_FUNCTION(Class, name)                                                                           \
    QObjectBindableProperty<Class, Type, Class::_qt_property_##name##_offset, Signal> name =                           \
        QObjectBindableProperty<Class, Type, Class::_qt_property_##name##_offset, Signal>(value);
#define Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS_4(Class, Type, name, value)                                               \
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS_5(Class, Type, name, value, nullptr)
#define NT_BINDABLE_PICK5(_1, _2, _3, _4, _5, NAME, ...) NAME
#define Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(...)                                                                      \
    NT_BINDABLE_PICK5(__VA_ARGS__, Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS_5, Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS_4,     \
                      unused)                                                                                          \
    (__VA_ARGS__)

// Declares a QObjectComputedProperty member `name` of `Class` whose value `Getter` (a const member function)
// computes.
#define Q_OBJECT_COMPUTED_PROPERTY(Class, Type, name, Getter)                                                          \
    NT_PROPERTY_OFFSET_FUNCTION(Class, name)                                                                           \
    QObjectComputedProperty<Class, Type, Class::_qt_property_##name##_offset, Getter> name;

namespace Qt {
void beginPropertyUpdateGroup();
void endPropertyUpdateGroup();
} // namespace Qt

/*!
    \class QScopedPropertyUpdateGroup
    \since 6.6
    \brief The QScopedPropertyUpdateGroup class opens a property update group for the scope of a variable.

    Its constructor calls Qt::beginPropertyUpdateGroup() and its destructor Qt::endPropertyUpdateGroup(): the
    bindings and handlers that depend on properties changed within the scope run once, at its end.

    \differences
    None.
    \enddifferences

    \sa Qt::beginPropertyUpdateGroup()
*/
class QScopedPropertyUpdateGroup {
public:
    /*!
        Begins an update group.
    */
    QScopedPropertyUpdateGroup() { Qt::beginPropertyUpdateGroup(); }
    QScopedPropertyUpdateGroup(const QScopedPropertyUpdateGroup &) = delete;
    QScopedPropertyUpdateGroup &operator=(const QScopedPropertyUpdateGroup &) = delete;
    /*!
        Ends the update group.
    */
    ~QScopedPropertyUpdateGroup() { Qt::endPropertyUpdateGroup(); }
};
