// MIT License - see LICENSE
//
// C++ types for the QML tests: a gauge with properties, a signal and a default list property, and a
// singleton.
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmllist.h>

class Gauge : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QString label READ label WRITE setLabel NOTIFY labelChanged)
    Q_PROPERTY(Gauge *next READ next WRITE setNext NOTIFY nextChanged)
    Q_PROPERTY(QQmlListProperty<QObject> parts READ parts)
    Q_PROPERTY(Unit unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_CLASSINFO("DefaultProperty", "parts")
public:
    enum Unit { Celsius, Fahrenheit };
    Q_ENUM(Unit)

    explicit Gauge(QObject *parent = nullptr) : QObject(parent) {}

    double value() const { return m_value; }
    void setValue(double v) {
        if (v == m_value)
            return;
        m_value = v;
        emit valueChanged();
    }
    QString label() const { return m_label; }
    void setLabel(const QString &l) {
        if (l == m_label)
            return;
        m_label = l;
        emit labelChanged();
    }
    Gauge *next() const { return m_next; }
    void setNext(Gauge *n) {
        if (n == m_next)
            return;
        m_next = n;
        emit nextChanged();
    }
    QQmlListProperty<QObject> parts() { return QQmlListProperty<QObject>(this, &m_parts); }
    Unit unit() const { return m_unit; }
    void setUnit(Unit u) {
        if (u == m_unit)
            return;
        m_unit = u;
        emit unitChanged();
    }

    Q_INVOKABLE double doubled() const { return m_value * 2; }

    void classBegin() override { ++begun; }
    void componentComplete() override { completedWithValue = m_value; }

    QList<QObject *> m_parts;
    int begun = 0;
    double completedWithValue = -1;

signals:
    void valueChanged() { emitSignal(&Gauge::valueChanged); }
    void labelChanged() { emitSignal(&Gauge::labelChanged); }
    void nextChanged() { emitSignal(&Gauge::nextChanged); }
    void unitChanged() { emitSignal(&Gauge::unitChanged); }
    void alarm(int level, const QString &reason) { emitSignal(&Gauge::alarm, level, reason); }

private:
    double m_value = 0;
    QString m_label;
    Gauge *m_next = nullptr;
    Unit m_unit = Celsius;
};

class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
public:
    QString theme() const { return m_theme; }
    void setTheme(const QString &t) {
        if (t == m_theme)
            return;
        m_theme = t;
        emit themeChanged();
    }

signals:
    void themeChanged() { emitSignal(&Settings::themeChanged); }

private:
    QString m_theme = QStringLiteral("light");
};
