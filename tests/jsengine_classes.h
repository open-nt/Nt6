// MIT License - see LICENSE
//
// A QObject for the JavaScript engine tests: properties of several types, an enum, overloads, signals and a
// method that throws into the script.
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtQml/qjsengine.h>

class Thermometer : public QObject {
    Q_OBJECT
    Q_PROPERTY(double celsius READ celsius WRITE setCelsius NOTIFY celsiusChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(int reading READ reading)
public:
    enum Mode { Idle, Heating, Cooling };
    Q_ENUM(Mode)

    explicit Thermometer(QObject *parent = nullptr) : QObject(parent) {}

    double celsius() const { return m_celsius; }
    void setCelsius(double c) {
        if (c == m_celsius)
            return;
        m_celsius = c;
        emit celsiusChanged(c);
    }
    Mode mode() const { return m_mode; }
    void setMode(Mode m) { m_mode = m; }
    int reading() const { return 7; }

    Q_INVOKABLE double fahrenheit() const { return m_celsius * 9.0 / 5.0 + 32.0; }
    Q_INVOKABLE QString describe(const QString &when) const {
        return objectName() + QLatin1Char(' ') + when + QStringLiteral(": ") + QString::number(m_celsius);
    }
    Q_INVOKABLE double scaled(double factor) const { return m_celsius * factor; }
    Q_INVOKABLE double scaled(double factor, double offset) const { return m_celsius * factor + offset; }
    Q_INVOKABLE QStringList tags() const { return {QStringLiteral("a"), QStringLiteral("b")}; }
    Q_INVOKABLE void fail() {
        if (engine)
            engine->throwError(QStringLiteral("failed on purpose"));
    }

    QJSEngine *engine = nullptr;

signals:
    void celsiusChanged(double celsius) { emitSignal(&Thermometer::celsiusChanged, celsius); }
    void alarm(const QString &reason) { emitSignal(&Thermometer::alarm, reason); }

private:
    double m_celsius = 0.0;
    Mode m_mode = Idle;
};
