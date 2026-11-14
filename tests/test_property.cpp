// MIT License - see LICENSE
// QProperty, QPropertyBinding, QBindable, QObjectBindableProperty, QObjectComputedProperty and update groups.
#include "property_classes.h"

#include <QCoreApplication>
#include <QProperty>
#include <QScopedPropertyUpdateGroup>
#include <QString>

#include <cstdio>
#include <memory>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                       \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testValuesAndBindings() {
    QProperty<int> width(10);
    QProperty<int> height(20);
    QProperty<int> area;
    CHECK(width.value() == 10 && *height == 20 && area == 0);
    CHECK(!area.hasBinding());

    area.setBinding([&] { return width * height; });
    CHECK(area.hasBinding() && area == 200);
    width = 30;
    CHECK(area == 600);
    height.setValue(2);
    CHECK(area.value() == 60);

    // Setting a value removes the binding; the dependencies no longer update it.
    area = 7;
    CHECK(!area.hasBinding() && area == 7);
    width = 1;
    CHECK(area == 7);

    // A binding can be taken, and set again on another property.
    QProperty<int> doubled([&] { return width * 2; });
    CHECK(doubled == 2);
    QPropertyBinding<int> binding = doubled.takeBinding();
    CHECK(!doubled.hasBinding() && !binding.isNull() && binding.valueMetaType() == QMetaType::fromType<int>());
    width = 5;
    CHECK(doubled == 2);
    QProperty<int> other;
    other.setBinding(binding);
    CHECK(other == 10);
    width = 6;
    CHECK(other == 12 && doubled == 2);
    CHECK(binding.location().line > 0);

    // Chains: a binding that depends on a bound property.
    QProperty<int> quadrupled([&] { return other * 2; });
    CHECK(quadrupled == 24);
    width = 1;
    CHECK(other == 2 && quadrupled == 4);

    // Equal values are not changes.
    int changes = 0;
    QProperty<QString> text(QStringLiteral("a"));
    auto handler = text.onValueChanged([&] { ++changes; });
    text = QStringLiteral("a");
    CHECK(changes == 0);
    text = QStringLiteral("b");
    CHECK(changes == 1);
    QProperty<QString> upper([&] { return text.value().toUpper(); });
    CHECK(upper.value() == QStringLiteral("B"));
    text = QStringLiteral("c");
    CHECK(upper.value() == QStringLiteral("C") && changes == 2);
    CHECK(upper->size() == 1);

    // A property with a constructed binding whose dependencies die is left with its last value.
    auto source = std::make_unique<QProperty<int>>(3);
    QProperty<int> reader([&] { return source->value() + 1; });
    CHECK(reader == 4);
    source.reset();
    CHECK(reader == 4 && reader.hasBinding());
}

static void testHandlers() {
    QProperty<int> p(1);
    int viaHandler = 0, viaSubscribe = 0, viaNotifier = 0;
    {
        auto handler = p.onValueChanged([&] { viaHandler = p.value(); });
        auto subscription = p.subscribe([&] { viaSubscribe = p.value(); });
        QPropertyNotifier notifier = p.addNotifier([&] { viaNotifier = p.value(); });
        CHECK(viaHandler == 0 && viaSubscribe == 1 && viaNotifier == 0);
        p = 2;
        CHECK(viaHandler == 2 && viaSubscribe == 2 && viaNotifier == 2);
        // A moved handler keeps working; the moved-from one is empty.
        auto moved = std::move(handler);
        p = 3;
        CHECK(viaHandler == 3);
    }
    p = 4;
    CHECK(viaHandler == 3 && viaSubscribe == 3 && viaNotifier == 3);

    // Handlers run after dependent bindings were evaluated.
    QProperty<int> dependent([&] { return p * 10; });
    int seen = -1;
    auto h = p.onValueChanged([&] { seen = dependent.value(); });
    p = 5;
    CHECK(seen == 50);
}

static void testBindingLoop() {
    QProperty<int> a(1);
    QProperty<int> b;
    b.setBinding([&] { return a + 1; });
    a.setBinding([&] { return b + 1; });
    // Each binding stays installed with a loop error on the one that was evaluated twice.
    CHECK(a.hasBinding() && b.hasBinding());
    const bool aLoop = a.binding().error().type() == QPropertyBindingError::BindingLoop;
    const bool bLoop = b.binding().error().type() == QPropertyBindingError::BindingLoop;
    CHECK(aLoop || bLoop);
    CHECK(!a.binding().error().description().isEmpty() || !b.binding().error().description().isEmpty());
    CHECK(QPropertyBindingError().type() == QPropertyBindingError::NoError && !QPropertyBindingError().hasError());

    // A binding that reads its own property does not depend on itself.
    QProperty<int> counter(0);
    QProperty<int> self(1);
    self.setBinding([&] { return self.valueBypassingBindings() + counter; });
    CHECK(self == 1 && !self.binding().error().hasError());
    counter = 2;
    CHECK(self == 3);
}

static void testUpdateGroups() {
    QProperty<int> x(1), y(2);
    int evaluations = 0, notifications = 0;
    QProperty<int> sum([&] {
        ++evaluations;
        return x + y;
    });
    auto handler = sum.onValueChanged([&] { ++notifications; });
    CHECK(evaluations == 1 && sum == 3);
    {
        QScopedPropertyUpdateGroup group;
        x = 10;
        y = 20;
        // Not evaluated until the group ends.
        CHECK(evaluations == 1 && sum.valueBypassingBindings() == 3 && notifications == 0);
    }
    CHECK(sum == 30 && evaluations == 2 && notifications == 1);

    Qt::beginPropertyUpdateGroup();
    Qt::beginPropertyUpdateGroup();
    x = 1;
    Qt::endPropertyUpdateGroup();
    CHECK(evaluations == 2);
    Qt::endPropertyUpdateGroup();
    CHECK(sum == 21 && evaluations == 3);
    // An unmatched end does nothing.
    Qt::endPropertyUpdateGroup();
    x = 2;
    CHECK(sum == 22);
}

static void testBindable() {
    QProperty<int> p(5);
    QBindable<int> bindable(&p);
    CHECK(bindable.isValid() && bindable.isBindable() && !bindable.isReadOnly());
    CHECK(bindable.value() == 5 && bindable.metaType() == QMetaType::fromType<int>());
    bindable.setValue(6);
    CHECK(p == 6);
    QProperty<int> source(2);
    bindable.setBinding([&] { return source * 3; });
    CHECK(p == 6 && bindable.hasBinding() && p.hasBinding());
    source = 3;
    CHECK(bindable.value() == 9);
    int seen = 0;
    auto h = bindable.onValueChanged([&] { seen = bindable.value(); });
    source = 4;
    CHECK(seen == 12);
    QPropertyBinding<int> taken = bindable.takeBinding();
    CHECK(!taken.isNull() && !p.hasBinding());

    // makeBinding() binds another property to this one.
    QProperty<int> mirror;
    mirror.setBinding(bindable.makeBinding());
    CHECK(mirror == 12);
    p = 1;
    CHECK(mirror == 1);

    // An untyped binding of the wrong type is refused.
    QProperty<QString> text;
    CHECK(!bindable.setBinding(QUntypedPropertyBinding(QPropertyBinding<QString>([&] { return text.value(); }))));
    CHECK(!p.setBinding(QUntypedPropertyBinding(QPropertyBinding<QString>([&] { return text.value(); }))));
    CHECK(p.setBinding(QUntypedPropertyBinding(QPropertyBinding<int>([] { return 42; }))));
    CHECK(p == 42);

    QUntypedBindable untyped;
    CHECK(!untyped.isValid() && !untyped.isBindable() && untyped.isReadOnly() && untyped.binding().isNull());
    QBindable<int> wrong{QUntypedBindable(QBindable<QString>(&text))};
    CHECK(!wrong.isValid());
}

static void testObjectProperties() {
    Thermostat thermostat;
    int emitted = 0, lastValue = -1;
    QObject::connect(&thermostat, &Thermostat::targetChanged, [&] { ++emitted; });
    QObject::connect(&thermostat, &Thermostat::currentChanged, [&](int v) { lastValue = v; });
    CHECK(thermostat.target() == 20);

    thermostat.setTarget(21);
    CHECK(thermostat.target() == 21 && emitted == 1);
    thermostat.setTarget(21);
    CHECK(emitted == 1);

    // A binding on the object's property emits the signal when a dependency changes.
    QProperty<int> outside(5);
    thermostat.bindableTarget().setBinding([&] { return outside + 10; });
    CHECK(thermostat.target() == 15 && emitted == 2);
    outside = 6;
    CHECK(thermostat.target() == 16 && emitted == 3);

    // The signal with an argument gets the new value.
    thermostat.m_current = 3;
    CHECK(lastValue == 3);
    thermostat.m_current.setBinding([&] { return thermostat.m_target.value() * 2; });
    CHECK(lastValue == 32);

    // Other properties can depend on the object's property.
    QProperty<QString> label([&] { return QStringLiteral("target %1").arg(thermostat.m_target.value()); });
    CHECK(label.value() == QStringLiteral("target 16"));
    outside = 7;
    CHECK(label.value() == QStringLiteral("target 17") && lastValue == 34);

    // A property without a signal.
    thermostat.m_silent = 9;
    CHECK(thermostat.m_silent == 9);
    QBindable<int> silent(&thermostat.m_silent);
    CHECK(silent.isValid() && silent.value() == 9);

    // A computed property.
    int ranges = 0;
    auto h = thermostat.m_range.onValueChanged([&] { ++ranges; });
    QBindable<int> range(&thermostat.m_range);
    CHECK(range.isReadOnly() && range.value() == thermostat.m_current.value() - thermostat.m_target.value());
    QProperty<int> rangeMirror([&] { return thermostat.m_range.value(); });
    CHECK(rangeMirror == 17);
    thermostat.m_silent = 1;
    thermostat.m_range.notify();
    CHECK(ranges == 1 && rangeMirror == 17);
    CHECK(!thermostat.m_range.hasBinding());

    // Through the meta-object: ntmoc routes QMetaProperty::bindable() to the BINDABLE function.
    QBindable<int> viaMeta(&thermostat, "target");
    CHECK(viaMeta.isValid() && viaMeta.isBindable() && !viaMeta.isReadOnly());
    CHECK(viaMeta.value() == thermostat.target());
    QProperty<int> remote(100);
    viaMeta.setBinding([&] { return remote + 1; });
    CHECK(thermostat.target() == 101 && thermostat.bindableTarget().hasBinding());
    remote = 200;
    CHECK(thermostat.target() == 201);
    QBindable<int> metaRange(&thermostat, "range");
    CHECK(metaRange.isValid() && metaRange.isReadOnly() && metaRange.value() == thermostat.range());
    CHECK(!QBindable<int>(&thermostat, "objectName").isValid());
    CHECK(!QBindable<QString>(&thermostat, "target").isValid());
    QUntypedBindable viaIndex = QtPrivate::metaPropertyBindable(
        &thermostat, thermostat.metaObject()->property(thermostat.metaObject()->indexOfProperty("current")));
    CHECK(viaIndex.isValid() && viaIndex.metaType() == QMetaType::fromType<int>());
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testValuesAndBindings();
    testHandlers();
    testBindingLoop();
    testUpdateGroups();
    testBindable();
    testObjectProperties();
    if (g_failures == 0)
        std::printf("All property tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
