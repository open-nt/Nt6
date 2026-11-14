// MIT License - see LICENSE
#include <QtCore/qproperty.h>

#include <QtCore/qmetaobject.h>
#include <QtCore/qobject.h>

#include <algorithm>
#include <vector>

namespace QtPrivate {

// A change handler of a property: alive until the QPropertyChangeHandler or QPropertyNotifier that owns it dies.
struct HandlerNode {
    std::function<void()> run;
};

// The binding data of one property: its binding, the bindings that read it, and its change handlers. A property
// allocates its cell when it first gets a binding or an observer, or is read while a binding is evaluated.
struct PropertyCell : std::enable_shared_from_this<PropertyCell> {
    QUntypedPropertyData *data = nullptr;
    std::shared_ptr<BindingNode> binding;
    std::vector<std::weak_ptr<BindingNode>> dependents;
    std::vector<std::weak_ptr<HandlerNode>> handlers;
    std::function<void()> ownerNotifier;
};

// A binding: the function that computes the value, the property it is installed on, and what it read last.
struct BindingNode : std::enable_shared_from_this<BindingNode> {
    BindingEvaluator evaluate;
    QMetaType type;
    QPropertyBindingSourceLocation location;
    QPropertyBindingError error;
    std::weak_ptr<PropertyCell> target;
    std::vector<std::weak_ptr<PropertyCell>> dependencies;
    bool evaluating = false;
};

namespace {

// The bindings being evaluated in this thread, innermost last; reads register with the innermost.
thread_local std::vector<BindingNode *> t_evaluating;
// Update groups: while the depth is above zero, changed cells queue here instead of notifying their observers.
thread_local int t_groupDepth = 0;
thread_local std::vector<std::weak_ptr<PropertyCell>> t_pending;

template <typename T> bool pointsTo(const std::weak_ptr<T> &weak, const T *object) {
    const std::shared_ptr<T> locked = weak.lock();
    return locked && locked.get() == object;
}

template <typename T> void dropExpired(std::vector<std::weak_ptr<T>> &list) {
    list.erase(std::remove_if(list.begin(), list.end(), [](const std::weak_ptr<T> &w) { return w.expired(); }),
               list.end());
}

// Removes `node` from the dependents of what it read.
void forgetDependencies(BindingNode *node) {
    for (const std::weak_ptr<PropertyCell> &weak : node->dependencies) {
        if (const std::shared_ptr<PropertyCell> cell = weak.lock()) {
            auto &deps = cell->dependents;
            deps.erase(std::remove_if(
                           deps.begin(), deps.end(),
                           [node](const std::weak_ptr<BindingNode> &w) { return w.expired() || pointsTo(w, node); }),
                       deps.end());
        }
    }
    node->dependencies.clear();
}

// Evaluates the bindings that read `cell`.
void evaluateDependents(const std::shared_ptr<PropertyCell> &cell) {
    // A copy: a binding may add or remove dependents while it runs.
    const std::vector<std::weak_ptr<BindingNode>> dependents = cell->dependents;
    for (const std::weak_ptr<BindingNode> &weak : dependents) {
        if (const std::shared_ptr<BindingNode> node = weak.lock())
            evaluateBinding(node);
    }
}

// Runs the change handlers of `cell` and tells its owner.
void runHandlers(const std::shared_ptr<PropertyCell> &cell) {
    const std::vector<std::weak_ptr<HandlerNode>> handlers = cell->handlers;
    for (const std::weak_ptr<HandlerNode> &weak : handlers) {
        if (const std::shared_ptr<HandlerNode> handler = weak.lock())
            handler->run();
    }
    dropExpired(cell->handlers);
    if (cell->ownerNotifier)
        cell->ownerNotifier();
}

// Tells the observers of `cell` about a change: dependent bindings first, then handlers and the owner.
void deliver(const std::shared_ptr<PropertyCell> &cell) {
    evaluateDependents(cell);
    runHandlers(cell);
}

} // namespace

// Evaluates `node` into its target, and notifies the target's observers if the value changed.
void evaluateBinding(const std::shared_ptr<BindingNode> &node) {
    const std::shared_ptr<PropertyCell> target = node->target.lock();
    if (!target || target->binding != node)
        return;
    if (node->evaluating) {
        node->error = QPropertyBindingError(QPropertyBindingError::BindingLoop,
                                            QStringLiteral("Binding depends on its own value"));
        return;
    }
    forgetDependencies(node.get());
    node->evaluating = true;
    node->error = QPropertyBindingError();
    t_evaluating.push_back(node.get());
    bool changed = false;
    try {
        changed = node->evaluate(target->data);
    } catch (...) {
        node->error =
            QPropertyBindingError(QPropertyBindingError::EvaluationError, QStringLiteral("Binding threw an exception"));
    }
    t_evaluating.pop_back();
    // The flag stays up while the observers are told, so that a chain of bindings that leads back here is seen as
    // a loop rather than evaluated again.
    if (changed)
        notifyChanged(target.get());
    node->evaluating = false;
}

std::shared_ptr<PropertyCell> makeCell(QUntypedPropertyData *data) {
    auto cell = std::make_shared<PropertyCell>();
    cell->data = data;
    return cell;
}

bool isEvaluatingBinding() { return !t_evaluating.empty(); }

void registerRead(PropertyCell *cell) {
    if (t_evaluating.empty())
        return;
    BindingNode *node = t_evaluating.back();
    // A binding that reads the property it is installed on does not depend on itself.
    if (pointsTo(node->target, cell))
        return;
    for (const std::weak_ptr<PropertyCell> &dep : node->dependencies) {
        if (pointsTo(dep, cell))
            return;
    }
    node->dependencies.push_back(cell->weak_from_this());
    cell->dependents.push_back(node->weak_from_this());
}

void notifyChanged(PropertyCell *cell) {
    if (t_groupDepth > 0) {
        for (const std::weak_ptr<PropertyCell> &pending : t_pending) {
            if (pointsTo(pending, cell))
                return;
        }
        t_pending.push_back(cell->weak_from_this());
        return;
    }
    deliver(cell->shared_from_this());
}

std::shared_ptr<BindingNode> setBinding(PropertyCell *cell, const std::shared_ptr<BindingNode> &binding) {
    std::shared_ptr<BindingNode> old = takeBinding(cell);
    if (!binding)
        return old;
    // A binding installed elsewhere moves here.
    if (const std::shared_ptr<PropertyCell> previous = binding->target.lock(); previous && previous.get() != cell)
        previous->binding.reset();
    forgetDependencies(binding.get());
    binding->target = cell->weak_from_this();
    cell->binding = binding;
    evaluateBinding(binding);
    return old;
}

std::shared_ptr<BindingNode> takeBinding(PropertyCell *cell) {
    std::shared_ptr<BindingNode> old = std::move(cell->binding);
    cell->binding.reset();
    if (old) {
        forgetDependencies(old.get());
        old->target.reset();
    }
    return old;
}

std::shared_ptr<BindingNode> currentBinding(const PropertyCell *cell) { return cell->binding; }

void setOwnerNotifier(PropertyCell *cell, std::function<void()> notifier) { cell->ownerNotifier = std::move(notifier); }

std::shared_ptr<HandlerNode> addHandler(PropertyCell *cell, std::function<void()> handler) {
    auto node = std::make_shared<HandlerNode>();
    node->run = std::move(handler);
    cell->handlers.push_back(node);
    return node;
}

std::shared_ptr<BindingNode> makeBinding(BindingEvaluator evaluate, QMetaType type,
                                         const QPropertyBindingSourceLocation &location) {
    auto node = std::make_shared<BindingNode>();
    node->evaluate = std::move(evaluate);
    node->type = type;
    node->location = location;
    return node;
}

QPropertyBindingError bindingError(const BindingNode *binding) {
    return binding ? binding->error : QPropertyBindingError();
}

QMetaType bindingType(const BindingNode *binding) { return binding ? binding->type : QMetaType(); }

QPropertyBindingSourceLocation bindingLocation(const BindingNode *binding) {
    return binding ? binding->location : QPropertyBindingSourceLocation();
}

QUntypedPropertyData *bindingTarget(const BindingNode *binding) {
    if (!binding)
        return nullptr;
    const std::shared_ptr<PropertyCell> target = binding->target.lock();
    return target ? target->data : nullptr;
}

QUntypedBindable metaPropertyBindable(QObject *object, const QMetaProperty &property) {
    const QMetaObject *mo = property.enclosingMetaObject();
    if (!object || !mo || !property.isValid() || !mo->d.static_metacall)
        return QUntypedBindable();
    QUntypedBindable bindable;
    void *args[] = {&bindable};
    mo->d.static_metacall(object, QMetaObject::BindableProperty, property.relativePropertyIndex(), args);
    return bindable;
}

QUntypedBindable metaPropertyBindable(QObject *object, const char *name) {
    if (!object || !name)
        return QUntypedBindable();
    const QMetaObject *mo = object->metaObject();
    const int index = mo->indexOfProperty(name);
    return index < 0 ? QUntypedBindable() : metaPropertyBindable(object, mo->property(index));
}

} // namespace QtPrivate

namespace Qt {

/*!
    \fn void Qt::beginPropertyUpdateGroup()
    \since 6.2
    Starts a group of property changes: until the matching endPropertyUpdateGroup(), the bindings and handlers that
    depend on changed properties are not run. Groups nest; the outermost end runs them, once per changed property.

    \sa endPropertyUpdateGroup(), QScopedPropertyUpdateGroup
*/
void beginPropertyUpdateGroup() { ++QtPrivate::t_groupDepth; }

/*!
    \fn void Qt::endPropertyUpdateGroup()
    \since 6.2
    Ends a group of property changes that beginPropertyUpdateGroup() started, and runs the bindings and handlers
    that depend on the properties changed in it.

    \sa beginPropertyUpdateGroup()
*/
void endPropertyUpdateGroup() {
    using namespace QtPrivate;
    if (t_groupDepth <= 0)
        return;
    if (--t_groupDepth > 0)
        return;
    // Changes made while delivering start new deliveries of their own.
    std::vector<std::weak_ptr<PropertyCell>> pending = std::move(t_pending);
    t_pending.clear();
    // A binding that read several of the changed properties is evaluated once: all dependents first, each once,
    // then the handlers of the changed properties.
    std::vector<std::shared_ptr<BindingNode>> dependents;
    for (const std::weak_ptr<PropertyCell> &weak : pending) {
        const std::shared_ptr<PropertyCell> cell = weak.lock();
        if (!cell)
            continue;
        for (const std::weak_ptr<BindingNode> &w : cell->dependents) {
            std::shared_ptr<BindingNode> node = w.lock();
            if (node && std::find(dependents.begin(), dependents.end(), node) == dependents.end())
                dependents.push_back(std::move(node));
        }
    }
    for (const std::shared_ptr<BindingNode> &node : dependents)
        evaluateBinding(node);
    for (const std::weak_ptr<PropertyCell> &weak : pending) {
        if (const std::shared_ptr<PropertyCell> cell = weak.lock())
            runHandlers(cell);
    }
}

} // namespace Qt
