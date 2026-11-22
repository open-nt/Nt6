// MIT License - see LICENSE
#include "qtwinentity.h"

#include "qtwinrotation_p.h"

#include <QtCore/qcryptographichash.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>

#include <cmath>

namespace QtTwin {

namespace {

constexpr double kDegrees = 3.14159265358979323846 / 180.0;

struct KindName {
    EntityKind kind;
    const char *name;
};

constexpr KindName kKindNames[] = {
    {EntityKind::Person, "person"},     {EntityKind::GroundVehicle, "ground-vehicle"},
    {EntityKind::Aircraft, "aircraft"}, {EntityKind::Drone, "drone"},
    {EntityKind::Boat, "boat"},
};

QByteArray fileSha256(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return QByteArray();
    return hash.result();
}

} // namespace

/*!
    \enum QtTwin::EntityKind
    \inmodule NtTwin
    \brief The kinds of moving things a twin shares between participants.

    The kind chooses the dead-reckoning defaults (see QtTwin::DeadReckoningThresholds::forKind())
    and how a viewer places the entity: people and ground vehicles follow the terrain, boats the
    water surface.

    \value Person A person on foot.
    \value GroundVehicle A wheeled or tracked vehicle.
    \value Aircraft A crewed fixed-wing aircraft or helicopter.
    \value Drone An uncrewed aircraft.
    \value Boat A vessel on the water surface.
*/

/*!
    \class QtTwin::DisEntityType
    \inmodule NtTwin
    \brief The DisEntityType struct is the seven-part entity type of the Distributed Interactive
    Simulation protocol.

    DIS (IEEE 1278.1) names what an entity is with seven numbers from the SISO-REF-010
    enumerations: kind, domain, country, category, subcategory, specific and extra. For example
    \c 1.2.225.20.1.0.0 is a platform (1) in the air (2) from the United States (225).

    Nt Twin chooses a model by its QtTwin::ModelRef, not by this type, because one type can stand
    for many models. The type is still sent in every Entity State PDU so that other DIS tools can
    show the entity.

    \differences
    Qt has no DIS support; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DisEntityStatePdu
*/

/*!
    \variable QtTwin::DisEntityType::kind
    The entity kind: 1 platform, 3 life form, and so on.
*/

/*!
    \variable QtTwin::DisEntityType::domain
    The domain of a platform: 1 land, 2 air, 3 surface, 4 subsurface, 5 space.
*/

/*!
    \variable QtTwin::DisEntityType::country
    The country code from SISO-REF-010, or 0 for other.
*/

/*!
    \variable QtTwin::DisEntityType::category
    The category within the kind and domain.
*/

/*!
    \variable QtTwin::DisEntityType::subcategory
    The subcategory within the category.
*/

/*!
    \variable QtTwin::DisEntityType::specific
    The specific model within the subcategory.
*/

/*!
    \variable QtTwin::DisEntityType::extra
    Extra information about the model.
*/

/*!
    \fn constexpr QtTwin::DisEntityType::DisEntityType()
    Constructs the type 0.0.0.0.0.0.0 (other).
*/

/*!
    \fn constexpr QtTwin::DisEntityType::DisEntityType(quint8 k, quint8 d, quint16 c, quint8 cat, quint8 sub, quint8 spec, quint8 ext)
    Constructs the type with kind \a k, domain \a d, country \a c, category \a cat, subcategory
    \a sub, specific \a spec and extra \a ext.
*/

/*!
    \fn constexpr bool QtTwin::DisEntityType::operator==(const DisEntityType &o) const
    Returns \c true if all seven parts equal those of \a o.
*/

/*!
    \fn constexpr bool QtTwin::DisEntityType::operator!=(const DisEntityType &o) const
    Returns \c true if any part differs from \a o.
*/

/*!
    Returns the type as seven numbers separated by dots, such as \c 1.2.225.20.1.0.0.
*/
QString DisEntityType::toString() const {
    return QStringLiteral("%1.%2.%3.%4.%5.%6.%7")
        .arg(kind)
        .arg(domain)
        .arg(country)
        .arg(category)
        .arg(subcategory)
        .arg(specific)
        .arg(extra);
}

/*!
    Returns the type written as \a text in the form toString() writes. Missing trailing parts are
    0. Sets *\a ok to \c false, if \a ok is not null, when \a text is not such a type.
*/
DisEntityType DisEntityType::fromString(const QString &text, bool *ok) {
    const QStringList parts = text.split(QLatin1Char('.'));
    DisEntityType t;
    bool good = !text.isEmpty() && parts.size() <= 7;
    unsigned values[7] = {0, 0, 0, 0, 0, 0, 0};
    for (qsizetype i = 0; good && i < parts.size(); ++i) {
        bool partOk = false;
        values[i] = parts.at(i).toUInt(&partOk);
        const unsigned limit = i == 2 ? 0xffffu : 0xffu;
        good = partOk && values[i] <= limit;
    }
    if (good) {
        t = DisEntityType(static_cast<quint8>(values[0]), static_cast<quint8>(values[1]),
                          static_cast<quint16>(values[2]), static_cast<quint8>(values[3]),
                          static_cast<quint8>(values[4]), static_cast<quint8>(values[5]),
                          static_cast<quint8>(values[6]));
    }
    if (ok)
        *ok = good;
    return t;
}

/*!
    Returns a generic DIS type for \a kind, used when a model does not name its own: a life form on
    land for a person, and a land, air or surface platform for the others. Drones and crewed
    aircraft get the same type.
*/
DisEntityType DisEntityType::forKind(EntityKind kind) {
    switch (kind) {
    case EntityKind::Person:
        return DisEntityType(3, 1, 0, 0);
    case EntityKind::GroundVehicle:
        return DisEntityType(1, 1, 0, 0);
    case EntityKind::Aircraft:
    case EntityKind::Drone:
        return DisEntityType(1, 2, 0, 0);
    case EntityKind::Boat:
        return DisEntityType(1, 3, 0, 0);
    }
    return DisEntityType();
}

/*!
    Sets *\a result to the entity kind this DIS type describes and returns \c true: life forms are
    people, and land, air and surface platforms are ground vehicles, aircraft and boats. Returns
    \c false for any other type.
*/
bool DisEntityType::toEntityKind(EntityKind *result) const {
    EntityKind k = EntityKind::GroundVehicle;
    if (kind == 3) {
        k = EntityKind::Person;
    } else if (kind == 1 && domain == 1) {
        k = EntityKind::GroundVehicle;
    } else if (kind == 1 && domain == 2) {
        k = EntityKind::Aircraft;
    } else if (kind == 1 && domain == 3) {
        k = EntityKind::Boat;
    } else {
        return false;
    }
    if (result)
        *result = k;
    return true;
}

/*!
    \class QtTwin::ModelRef
    \inmodule NtTwin
    \brief The ModelRef struct names one revision of a 3D model that participants must be able to
    render.

    A model has a stable ID such as \c air/rotary/ec135 and an integer revision. A participant can
    render a model if it has the same ID at the same or a higher revision: a revision may improve
    the geometry and textures, but keeps the node names and articulated parts, so a breaking change
    gets a new ID. In strict mode, the SHA-256 of the file must match exactly.

    \differences
    Qt has no model references; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::ModelCatalogue
*/

/*!
    \variable QtTwin::ModelRef::id
    The stable ID of the model, such as \c land/wheeled/lr110.
*/

/*!
    \variable QtTwin::ModelRef::revision
    The revision of the model, from 0.
*/

/*!
    \variable QtTwin::ModelRef::sha256
    The SHA-256 of the model file (32 bytes), or empty if unknown.
*/

/*!
    \variable QtTwin::ModelRef::kind
    What kind of entity the model shows.
*/

/*!
    \variable QtTwin::ModelRef::disType
    The DIS entity type sent for entities that use the model.
*/

/*!
    \variable QtTwin::ModelRef::file
    The model file, relative to the catalogue. It is local to each participant and not compared.
*/

/*!
    \fn bool QtTwin::ModelRef::isValid() const
    Returns \c true if the reference has an ID and a revision of 0 or more.
*/

/*!
    \fn bool QtTwin::ModelRef::operator==(const ModelRef &o) const
    Returns \c true if the ID, revision, hash, kind and DIS type equal those of \a o. The file is
    not compared.
*/

/*!
    \fn bool QtTwin::ModelRef::operator!=(const ModelRef &o) const
    Returns \c true if this reference differs from \a o.
*/

/*!
    Returns the reference as text for messages, such as \c {air/rotary/ec135 r4}.
*/
QString ModelRef::toString() const { return QStringLiteral("%1 r%2").arg(id).arg(revision); }

/*!
    Returns the name of \a kind as used in \c models.json: \c person, \c ground-vehicle,
    \c aircraft, \c drone or \c boat.
*/
QString ModelRef::kindName(EntityKind kind) {
    for (const KindName &k : kKindNames) {
        if (k.kind == kind)
            return QString::fromLatin1(k.name);
    }
    return QString();
}

/*!
    Returns the kind called \a name, as kindName() writes it. If \a name is unknown, returns
    QtTwin::EntityKind::GroundVehicle and sets *\a ok to \c false, if \a ok is not null.
*/
EntityKind ModelRef::kindFromName(const QString &name, bool *ok) {
    for (const KindName &k : kKindNames) {
        if (name == QLatin1StringView(k.name)) {
            if (ok)
                *ok = true;
            return k.kind;
        }
    }
    if (ok)
        *ok = false;
    return EntityKind::GroundVehicle;
}

/*!
    \class QtTwin::ModelCatalogue
    \inmodule NtTwin
    \brief The ModelCatalogue class lists the models a participant can render.

    An application builds its catalogue from the \c models.json file of its model library:

    \code
    {
      "models": [
        { "id": "land/wheeled/lr110", "revision": 3, "kind": "ground-vehicle",
          "dis": "1.1.225.6.1.0.0", "file": "vehicles/lr110.glb", "sha256": "9f2c…" }
      ]
    }
    \endcode

    load() checks each file against its hash, so a missing, corrupted or edited file counts as not
    installed; warnings() says which. A session admits a participant only if every participant can
    render every model in use, which canRender() and missing() decide.

    \code
    QtTwin::ModelCatalogue catalogue;
    catalogue.load(QStringLiteral("/opt/models/models.json"));
    const QList<QtTwin::ModelRef> lacking = catalogue.missing(sessionModels);
    \endcode

    \differences
    Qt has no model catalogue; this class is part of Nt Twin.
    \enddifferences

    \sa QtTwin::ModelRef
*/

/*!
    \fn QtTwin::ModelCatalogue::ModelCatalogue()
    Constructs an empty catalogue.
*/

/*!
    Loads the catalogue from the \c models.json file at \a path, replacing what it held. Model
    files are relative to the directory of \a path. If \a verifyFiles is \c true, each model's file
    must exist and match its \c sha256, or the model is left out with a warning; a model without a
    \c sha256 gets the hash of its file. Returns \c false if the file cannot be read or is not a
    catalogue.

    \sa warnings()
*/
bool ModelCatalogue::load(const QString &path, bool verifyFiles) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        clear();
        m_warnings.append(QStringLiteral("cannot read %1").arg(path));
        return false;
    }
    return loadFromJson(file.readAll(), QFileInfo(path).absolutePath(), verifyFiles);
}

/*!
    Loads the catalogue from \a json in the format of \c models.json, replacing what it held.
    Model files are relative to \a baseDirectory. \a verifyFiles works as in load(). Returns
    \c false if \a json is not a catalogue.
*/
bool ModelCatalogue::loadFromJson(const QByteArray &json, const QString &baseDirectory, bool verifyFiles) {
    clear();
    m_baseDirectory = baseDirectory;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    if (!doc.isObject() || !doc.object().value(QStringLiteral("models")).isArray()) {
        m_warnings.append(QStringLiteral("not a model catalogue: %1").arg(error.errorString()));
        return false;
    }
    const QJsonArray models = doc.object().value(QStringLiteral("models")).toArray();
    for (const auto &v : models) {
        const QJsonObject o = v.toObject();
        ModelRef m;
        m.id = o.value(QStringLiteral("id")).toString();
        m.revision = o.value(QStringLiteral("revision")).toInt(0);
        if (m.id.isEmpty() || m.revision < 0) {
            m_warnings.append(QStringLiteral("model without a valid id or revision skipped"));
            continue;
        }
        bool ok = true;
        if (o.contains(QStringLiteral("kind")))
            m.kind = ModelRef::kindFromName(o.value(QStringLiteral("kind")).toString(), &ok);
        if (!ok) {
            m_warnings.append(QStringLiteral("%1: unknown kind").arg(m.id));
            continue;
        }
        m.disType = DisEntityType::forKind(m.kind);
        if (o.contains(QStringLiteral("dis")))
            m.disType = DisEntityType::fromString(o.value(QStringLiteral("dis")).toString(), &ok);
        if (!ok) {
            m_warnings.append(QStringLiteral("%1: invalid DIS entity type").arg(m.id));
            continue;
        }
        m.file = o.value(QStringLiteral("file")).toString();
        m.sha256 = QByteArray::fromHex(o.value(QStringLiteral("sha256")).toString().toLatin1());
        if (verifyFiles) {
            const QString path = m.file.isEmpty() ? QString() : QDir(m_baseDirectory).filePath(m.file);
            if (m.file.isEmpty() || !QFileInfo::exists(path)) {
                m_warnings.append(QStringLiteral("%1: file %2 is missing").arg(m.toString(), m.file));
                continue;
            }
            const QByteArray actual = fileSha256(path);
            if (!m.sha256.isEmpty() && actual != m.sha256) {
                m_warnings.append(QStringLiteral("%1: file %2 does not match its hash").arg(m.toString(), m.file));
                continue;
            }
            m.sha256 = actual;
        }
        insert(m);
    }
    return true;
}

/*!
    \fn QStringList QtTwin::ModelCatalogue::warnings() const
    Returns what the last load() or loadFromJson() left out and why.
*/

/*!
    Adds \a model to the catalogue, replacing any model with the same ID.
*/
void ModelCatalogue::insert(const ModelRef &model) {
    if (model.isValid())
        m_models.insert(model.id, model);
}

/*!
    Removes all models and warnings.
*/
void ModelCatalogue::clear() {
    m_models.clear();
    m_warnings.clear();
}

/*!
    \fn bool QtTwin::ModelCatalogue::isEmpty() const
    Returns \c true if the catalogue holds no models.
*/

/*!
    \fn qsizetype QtTwin::ModelCatalogue::size() const
    Returns the number of models in the catalogue.
*/

/*!
    \fn QList<QtTwin::ModelRef> QtTwin::ModelCatalogue::models() const
    Returns the models, ordered by ID.
*/

/*!
    Returns the model with the ID \a id, or an invalid reference if the catalogue has none.
*/
ModelRef ModelCatalogue::find(const QString &id) const { return m_models.value(id); }

/*!
    Returns the absolute path of the file of the model \a id, or an empty string if the catalogue
    has no such model or it names no file.
*/
QString ModelCatalogue::filePath(const QString &id) const {
    const auto it = m_models.constFind(id);
    if (it == m_models.cend() || it->file.isEmpty())
        return QString();
    return QDir(m_baseDirectory).absoluteFilePath(it->file);
}

/*!
    Returns \c true if the catalogue can render \a model: it holds the same ID at the same or a
    higher revision. If \a strict is \c true, the SHA-256 must also be known and equal.
*/
bool ModelCatalogue::canRender(const ModelRef &model, bool strict) const {
    const auto it = m_models.constFind(model.id);
    if (it == m_models.cend())
        return false;
    if (strict)
        return !model.sha256.isEmpty() && it->sha256 == model.sha256;
    return it->revision >= model.revision;
}

/*!
    Returns the models in \a models that the catalogue cannot render, each once, in the order
    given. \a strict works as in canRender().
*/
QList<ModelRef> ModelCatalogue::missing(const QList<ModelRef> &models, bool strict) const {
    QList<ModelRef> result;
    for (const ModelRef &m : models) {
        if (canRender(m, strict))
            continue;
        bool listed = false;
        for (const ModelRef &r : result)
            listed = listed || r.id == m.id;
        if (!listed)
            result.append(m);
    }
    return result;
}

/*!
    Returns a SHA-256 over the IDs, revisions and hashes of all models. Two catalogues with the same
    digest hold the same models.
*/
QByteArray ModelCatalogue::digest() const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const ModelRef &m : m_models) {
        hash.addData(m.id.toUtf8());
        hash.addData(QByteArray("\t") + QByteArray::number(m.revision) + '\t' + m.sha256.toHex() + '\n');
    }
    return hash.result();
}

/*!
    \class QtTwin::EulerAngles
    \inmodule NtTwin
    \brief The EulerAngles struct is an orientation relative to the Earth-centred, Earth-fixed
    frame, as DIS sends it.

    The angles, in radians, turn the ECEF axes into the entity's body axes (x forward, y right,
    z down): \l psi about z, then \l theta about the new y, then \l phi about the new x. They are
    awkward to read; QtTwin::EntityState::headingPitchRoll() gives the same orientation relative
    to the local horizon.

    \differences
    Qt has no Earth-fixed orientation type; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::EulerAngles::psi
    The first rotation, about the ECEF z axis, in radians.
*/

/*!
    \variable QtTwin::EulerAngles::theta
    The second rotation, about the new y axis, in radians.
*/

/*!
    \variable QtTwin::EulerAngles::phi
    The third rotation, about the new x axis, in radians.
*/

/*!
    \fn constexpr QtTwin::EulerAngles::EulerAngles()
    Constructs the angles 0, 0, 0.
*/

/*!
    \fn constexpr QtTwin::EulerAngles::EulerAngles(double yaw, double pitch, double roll)
    Constructs the angles \l psi = \a yaw, \l theta = \a pitch and \l phi = \a roll, in radians.
*/

/*!
    \fn constexpr bool QtTwin::EulerAngles::operator==(const EulerAngles &o) const
    Returns \c true if all three angles equal those of \a o.
*/

/*!
    \fn constexpr bool QtTwin::EulerAngles::operator!=(const EulerAngles &o) const
    Returns \c true if any angle differs from that of \a o.
*/

/*!
    \class QtTwin::HeadingPitchRoll
    \inmodule NtTwin
    \brief The HeadingPitchRoll struct is an orientation relative to the local horizon, in degrees.

    \l heading is clockwise from true north, \l pitch is positive nose up and \l roll is positive
    right wing down.

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::EntityState::headingPitchRoll()
*/

/*!
    \variable QtTwin::HeadingPitchRoll::heading
    The heading in degrees clockwise from true north, 0 to 360.
*/

/*!
    \variable QtTwin::HeadingPitchRoll::pitch
    The pitch in degrees, positive nose up.
*/

/*!
    \variable QtTwin::HeadingPitchRoll::roll
    The roll in degrees, positive right side down.
*/

/*!
    \fn constexpr QtTwin::HeadingPitchRoll::HeadingPitchRoll()
    Constructs a level orientation facing north.
*/

/*!
    \fn constexpr QtTwin::HeadingPitchRoll::HeadingPitchRoll(double h, double p, double r)
    Constructs the orientation with heading \a h, pitch \a p and roll \a r, in degrees.
*/

/*!
    \enum QtTwin::DeadReckoningAlgorithm
    \inmodule NtTwin
    \brief The dead-reckoning algorithms of DIS.

    A receiver predicts where an entity is between updates with the algorithm the owner chose. The
    names are those of IEEE 1278.1: F or R for fixed or rotating orientation, P or V for constant
    velocity or constant acceleration, and W or B for world or body coordinates.

    \value Other Not dead-reckoned; treated as Static.
    \value Static The entity does not move.
    \value Fpw Constant velocity, fixed orientation.
    \value Rpw Constant velocity, rotating at the angular velocity.
    \value Rvw Constant acceleration, rotating.
    \value Fvw Constant acceleration, fixed orientation.
    \value Fpb As Fpw, in body coordinates; Nt Twin extrapolates it as Fpw.
    \value Rpb As Rpw, in body coordinates; extrapolated as Rpw.
    \value Rvb As Rvw, in body coordinates; extrapolated as Rvw.
    \value Fvb As Fvw, in body coordinates; extrapolated as Fvw.
*/

/*!
    \class QtTwin::ArticulatedPart
    \inmodule NtTwin
    \brief The ArticulatedPart struct is the state of one moving part of an entity, such as a
    rotor, a turret or the steering.

    It is the articulated part record of DIS. parameterType combines the part's type class from
    SISO-REF-010 (multiples of 32, such as 4096 for the primary turret) with the metric in its low
    five bits (such as 11 for azimuth).

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences
*/

/*!
    \variable QtTwin::ArticulatedPart::parameterType
    The type class plus the metric of the part.
*/

/*!
    \variable QtTwin::ArticulatedPart::value
    The value of the metric, such as an angle in radians.
*/

/*!
    \variable QtTwin::ArticulatedPart::attachedTo
    The part this part is attached to, or 0 for the body.
*/

/*!
    \variable QtTwin::ArticulatedPart::changeIndicator
    A counter that the owner increments when the part changes.
*/

/*!
    \fn bool QtTwin::ArticulatedPart::operator==(const ArticulatedPart &o) const
    Returns \c true if all fields equal those of \a o.
*/

/*!
    \fn bool QtTwin::ArticulatedPart::operator!=(const ArticulatedPart &o) const
    Returns \c true if any field differs from \a o.
*/

/*!
    \class QtTwin::EntityState
    \inmodule NtTwin
    \brief The EntityState struct holds where an entity is and how it moves at one instant.

    Positions, velocities and accelerations are Earth-centred, Earth-fixed, in double precision,
    exactly as DIS sends them, so nothing is lost between the twin and the network. The helpers
    convert to the local frame an application thinks in:

    \code
    QtTwin::EntityState s;
    s.time = session->sessionTime();
    s.setGeoPosition(QtTwin::GeoPosition(51.1789, -1.8262, 160.0));
    s.setHeadingPitchRoll(QtTwin::HeadingPitchRoll(212.0, 0.0, 0.0));   // set the position first
    s.setLocalVelocity(QtTwin::Vector3d(-5.3, -8.5, 0.0));            // east, north, up in m/s
    \endcode

    \differences
    Qt has no such type; this struct is part of Nt Twin.
    \enddifferences

    \sa QtTwin::DeadReckoning, QtTwin::DisEntityStatePdu
*/

/*!
    \variable QtTwin::EntityState::time
    The session time of this state, in milliseconds.
*/

/*!
    \variable QtTwin::EntityState::position
    The position in ECEF, in metres.
*/

/*!
    \variable QtTwin::EntityState::orientation
    The orientation relative to ECEF.
*/

/*!
    \variable QtTwin::EntityState::velocity
    The linear velocity in ECEF, in metres per second.
*/

/*!
    \variable QtTwin::EntityState::acceleration
    The linear acceleration in ECEF, in metres per second squared.
*/

/*!
    \variable QtTwin::EntityState::angularVelocity
    The angular velocity in body coordinates (x forward, y right, z down), in radians per second.
    A positive z turns the entity to the right.
*/

/*!
    \variable QtTwin::EntityState::appearance
    The DIS appearance bits, such as lights or damage.
*/

/*!
    \variable QtTwin::EntityState::deadReckoning
    How receivers predict the entity between updates.
*/

/*!
    \variable QtTwin::EntityState::articulatedParts
    The states of the entity's moving parts.
*/

/*!
    Returns the position as latitude, longitude and ellipsoidal height on WGS84.
*/
GeoPosition EntityState::geoPosition() const { return Ellipsoid::wgs84().fromEcef(position); }

/*!
    Sets the position to \a geo on WGS84. The orientation is kept relative to ECEF, so set the
    position before setHeadingPitchRoll() and setLocalVelocity().
*/
void EntityState::setGeoPosition(const GeoPosition &geo) { position = Ellipsoid::wgs84().toEcef(geo); }

/*!
    Returns the orientation relative to the local horizon at the entity's position.
*/
HeadingPitchRoll EntityState::headingPitchRoll() const {
    const GeoPosition geo = geoPosition();
    const Rotation::Matrix local = Rotation::multiply(
        Rotation::bodyAxes(orientation), Rotation::transposed(Rotation::nedAxes(geo.latitude, geo.longitude)));
    double h = 0.0, p = 0.0, r = 0.0;
    Rotation::toEuler(local, &h, &p, &r);
    double heading = std::fmod(h / kDegrees, 360.0);
    if (heading < 0.0)
        heading += 360.0;
    return HeadingPitchRoll(heading, p / kDegrees, r / kDegrees);
}

/*!
    Sets the orientation to \a angles relative to the local horizon at the entity's position.
*/
void EntityState::setHeadingPitchRoll(const HeadingPitchRoll &angles) {
    const GeoPosition geo = geoPosition();
    const Rotation::Matrix body = Rotation::multiply(
        Rotation::fromEuler(angles.heading * kDegrees, angles.pitch * kDegrees, angles.roll * kDegrees),
        Rotation::nedAxes(geo.latitude, geo.longitude));
    orientation = Rotation::eulerFromAxes(body);
}

/*!
    Returns the velocity as east, north and up components at the entity's position, in metres per
    second.
*/
Vector3d EntityState::localVelocity() const {
    const GeoPosition geo = geoPosition();
    const Rotation::Matrix ned = Rotation::nedAxes(geo.latitude, geo.longitude);
    const Vector3d north = Rotation::row(ned, 0), east = Rotation::row(ned, 1), down = Rotation::row(ned, 2);
    return Vector3d(velocity.dotProduct(east), velocity.dotProduct(north), -velocity.dotProduct(down));
}

/*!
    Sets the velocity from \a eastNorthUp, in metres per second east, north and up of the entity's
    position.
*/
void EntityState::setLocalVelocity(const Vector3d &eastNorthUp) {
    const GeoPosition geo = geoPosition();
    const Rotation::Matrix ned = Rotation::nedAxes(geo.latitude, geo.longitude);
    const Vector3d north = Rotation::row(ned, 0), east = Rotation::row(ned, 1), down = Rotation::row(ned, 2);
    velocity = east * eastNorthUp.x + north * eastNorthUp.y + down * -eastNorthUp.z;
}

} // namespace QtTwin
