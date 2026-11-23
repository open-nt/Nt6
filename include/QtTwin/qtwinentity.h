// MIT License - see LICENSE
#pragma once

#include "qtwingeodesy.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

namespace QtTwin {

enum class EntityKind : quint8 { Person, GroundVehicle, Aircraft, Drone, Boat };

struct DisEntityType {
    quint8 kind = 0;
    quint8 domain = 0;
    quint16 country = 0;
    quint8 category = 0;
    quint8 subcategory = 0;
    quint8 specific = 0;
    quint8 extra = 0;

    constexpr DisEntityType() = default;
    constexpr DisEntityType(quint8 k, quint8 d, quint16 c, quint8 cat, quint8 sub = 0, quint8 spec = 0, quint8 ext = 0)
        : kind(k), domain(d), country(c), category(cat), subcategory(sub), specific(spec), extra(ext) {}

    constexpr bool operator==(const DisEntityType &o) const {
        return kind == o.kind && domain == o.domain && country == o.country && category == o.category &&
               subcategory == o.subcategory && specific == o.specific && extra == o.extra;
    }
    constexpr bool operator!=(const DisEntityType &o) const { return !(*this == o); }

    QString toString() const;
    static DisEntityType fromString(const QString &text, bool *ok = nullptr);
    static DisEntityType forKind(EntityKind kind);
    bool toEntityKind(EntityKind *result) const;
};

struct ModelRef {
    QString id;
    int revision = 0;
    QByteArray sha256;
    EntityKind kind = EntityKind::GroundVehicle;
    DisEntityType disType;
    QString file;
    QString wreck;

    bool isValid() const { return !id.isEmpty() && revision >= 0; }
    QString toString() const;
    static QString kindName(EntityKind kind);
    static EntityKind kindFromName(const QString &name, bool *ok = nullptr);
    bool operator==(const ModelRef &o) const {
        return id == o.id && revision == o.revision && sha256 == o.sha256 && kind == o.kind && disType == o.disType;
    }
    bool operator!=(const ModelRef &o) const { return !(*this == o); }
};

class ModelCatalogue {
public:
    ModelCatalogue() = default;

    bool load(const QString &path, bool verifyFiles = true);
    bool loadFromJson(const QByteArray &json, const QString &baseDirectory = QString(), bool verifyFiles = false);
    QStringList warnings() const { return m_warnings; }

    void insert(const ModelRef &model);
    void clear();
    bool isEmpty() const { return m_models.isEmpty(); }
    qsizetype size() const { return m_models.size(); }
    QList<ModelRef> models() const { return m_models.values(); }
    ModelRef find(const QString &id) const;
    QString filePath(const QString &id) const;
    QString wreckFilePath(const QString &id) const;

    bool canRender(const ModelRef &model, bool strict = false) const;
    QList<ModelRef> missing(const QList<ModelRef> &models, bool strict = false) const;
    QByteArray digest() const;

private:
    QMap<QString, ModelRef> m_models;
    QString m_baseDirectory;
    QStringList m_warnings;
};

struct EulerAngles {
    double psi = 0.0;
    double theta = 0.0;
    double phi = 0.0;

    constexpr EulerAngles() = default;
    constexpr EulerAngles(double yaw, double pitch, double roll) : psi(yaw), theta(pitch), phi(roll) {}
    constexpr bool operator==(const EulerAngles &o) const { return psi == o.psi && theta == o.theta && phi == o.phi; }
    constexpr bool operator!=(const EulerAngles &o) const { return !(*this == o); }
};

struct HeadingPitchRoll {
    double heading = 0.0;
    double pitch = 0.0;
    double roll = 0.0;

    constexpr HeadingPitchRoll() = default;
    constexpr HeadingPitchRoll(double h, double p, double r) : heading(h), pitch(p), roll(r) {}
};

enum class DeadReckoningAlgorithm : quint8 {
    Other = 0,
    Static = 1,
    Fpw = 2,
    Rpw = 3,
    Rvw = 4,
    Fvw = 5,
    Fpb = 6,
    Rpb = 7,
    Rvb = 8,
    Fvb = 9
};

struct ArticulatedPart {
    quint32 parameterType = 0;
    float value = 0.0f;
    quint16 attachedTo = 0;
    quint8 changeIndicator = 0;

    bool operator==(const ArticulatedPart &o) const {
        return parameterType == o.parameterType && value == o.value && attachedTo == o.attachedTo &&
               changeIndicator == o.changeIndicator;
    }
    bool operator!=(const ArticulatedPart &o) const { return !(*this == o); }
};

struct EntityState {
    qint64 time = 0;
    Vector3d position;
    EulerAngles orientation;
    Vector3d velocity;
    Vector3d acceleration;
    Vector3d angularVelocity;
    quint32 appearance = 0;
    DeadReckoningAlgorithm deadReckoning = DeadReckoningAlgorithm::Fpw;
    QList<ArticulatedPart> articulatedParts;

    GeoPosition geoPosition() const;
    void setGeoPosition(const GeoPosition &geo);
    HeadingPitchRoll headingPitchRoll() const;
    void setHeadingPitchRoll(const HeadingPitchRoll &angles);
    Vector3d localVelocity() const;
    void setLocalVelocity(const Vector3d &eastNorthUp);
};

} // namespace QtTwin
