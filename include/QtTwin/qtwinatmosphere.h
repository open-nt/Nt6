// MIT License - see LICENSE
#pragma once

#include "qtwingeodesy.h"

#include <QtCore/qlist.h>

namespace QtTwin {

struct AirState {
    double temperature = 288.15;
    double pressure = 101325.0;
    double density = 1.225;
    double speedOfSound = 340.294;
};

class Atmosphere {
public:
    static constexpr double standardPressure = 101325.0;
    static constexpr double standardTemperature = 288.15;
    static constexpr double standardDensity = 1.225;
    static constexpr double gasConstant = 287.05287;
    static constexpr double maximumHeight = 86000.0;

    constexpr Atmosphere() = default;
    constexpr Atmosphere(double seaLevelPressure, double seaLevelTemperature)
        : m_pressure(seaLevelPressure), m_temperature(seaLevelTemperature) {}

    constexpr double seaLevelPressure() const { return m_pressure; }
    constexpr double seaLevelTemperature() const { return m_temperature; }

    AirState at(double height) const;
    double temperature(double height) const { return at(height).temperature; }
    double pressure(double height) const { return at(height).pressure; }
    double density(double height) const { return at(height).density; }
    double speedOfSound(double height) const { return at(height).speedOfSound; }
    double pressureAltitude(double pressure) const;

    static double geopotentialHeight(double height);

private:
    double m_pressure = standardPressure;
    double m_temperature = standardTemperature;
};

struct WindLayer {
    double height = 0.0;
    Vector3d velocity;

    constexpr WindLayer() = default;
    constexpr WindLayer(double h, const Vector3d &eastNorthUp) : height(h), velocity(eastNorthUp) {}
    static WindLayer fromSpeedAndDirection(double height, double speed, double directionFrom);
};

class WindModel {
public:
    WindModel() = default;
    explicit WindModel(const Vector3d &eastNorthUp);
    WindModel(double speed, double directionFrom);

    bool isCalm() const { return m_layers.isEmpty(); }
    QList<WindLayer> layers() const { return m_layers; }
    void addLayer(const WindLayer &layer);
    void clear() { m_layers.clear(); }

    Vector3d at(double height) const;
    Vector3d atEcef(const Vector3d &ecef) const;
    static double speed(const Vector3d &eastNorthUp);
    static double directionFrom(const Vector3d &eastNorthUp);

private:
    QList<WindLayer> m_layers;
};

class Gravity {
public:
    static constexpr double standard = 9.80665;

    static double magnitude(double latitude, double height = 0.0);
    static Vector3d at(const Vector3d &ecef);
    static Vector3d at(const GeoPosition &position);
};

} // namespace QtTwin
