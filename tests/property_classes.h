// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qproperty.h>

// A QObject with bindable properties: one that emits a signal without arguments, one whose signal carries the new
// value, one without a signal, and a computed one.
class Thermostat : public QObject {
    Q_OBJECT
    Q_PROPERTY(int target READ target WRITE setTarget NOTIFY targetChanged BINDABLE bindableTarget)
    Q_PROPERTY(int current READ current NOTIFY currentChanged BINDABLE bindableCurrent)
    Q_PROPERTY(int range READ range BINDABLE bindableRange)
public:
    explicit Thermostat(QObject *parent = nullptr) : QObject(parent) {}

    int target() const { return m_target; }
    void setTarget(int t) { m_target = t; }
    QBindable<int> bindableTarget() { return &m_target; }
    int current() const { return m_current; }
    QBindable<int> bindableCurrent() { return &m_current; }
    int range() const { return m_current.value() - m_target.value(); }
    QBindable<int> bindableRange() { return &m_range; }

signals:
    void targetChanged() { emitSignal(&Thermostat::targetChanged); }
    void currentChanged(int value) { emitSignal(&Thermostat::currentChanged, value); }

public:
    Q_OBJECT_BINDABLE_PROPERTY_WITH_ARGS(Thermostat, int, m_target, 20, &Thermostat::targetChanged)
    Q_OBJECT_BINDABLE_PROPERTY(Thermostat, int, m_current, &Thermostat::currentChanged)
    Q_OBJECT_BINDABLE_PROPERTY(Thermostat, int, m_silent)
    Q_OBJECT_COMPUTED_PROPERTY(Thermostat, int, m_range, &Thermostat::range)
};
