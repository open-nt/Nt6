// MIT License - see LICENSE
#include "qtwinreplicator.h"

#include <QtComms/qcommschat.h>

#include <QtCore/qcborarray.h>
#include <QtCore/qcborvalue.h>
#include <QtCore/qhash.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>

namespace QtTwin {

namespace {

constexpr int kTimerInterval = 100;
constexpr qint64 kQueryInterval = 2000;
constexpr qsizetype kEliminationHistory = 200;

enum class Message : int {
    SpawnRequest = 1,
    Spawn = 2,
    SpawnReject = 3,
    Despawn = 4,
    EntityQuery = 5,
    ModelNotice = 6,
    Eliminated = 7
};

const QString kWorld = QStringLiteral("world");
const QString kModels = QStringLiteral("models");
const QString kPlanned = QStringLiteral("planned");
const QString kDigest = QStringLiteral("modelsDigest");
const QString kStrict = QStringLiteral("strictModels");
const QString kUpdateUrl = QStringLiteral("updateUrl");

QCborMap encodeModel(const ModelRef &m) {
    QCborMap c;
    c.insert(1, m.id);
    c.insert(2, m.revision);
    c.insert(3, m.sha256);
    c.insert(4, static_cast<int>(m.kind));
    c.insert(5, m.disType.toString());
    return c;
}

ModelRef decodeModel(const QCborMap &c) {
    ModelRef m;
    m.id = c.value(1).toString();
    m.revision = static_cast<int>(c.value(2).toInteger());
    m.sha256 = c.value(3).toByteArray();
    const qint64 kind = c.value(4).toInteger();
    m.kind = kind >= 0 && kind <= static_cast<int>(EntityKind::Boat) ? static_cast<EntityKind>(kind)
                                                                     : EntityKind::GroundVehicle;
    m.disType = DisEntityType::fromString(c.value(5).toString());
    return m;
}

QCborArray encodeModels(const QList<ModelRef> &models) {
    QCborArray a;
    for (const ModelRef &m : models)
        a.append(encodeModel(m));
    return a;
}

QList<ModelRef> decodeModels(const QCborArray &a) {
    QList<ModelRef> models;
    for (const auto &v : a) {
        const ModelRef m = decodeModel(v.toMap());
        if (m.isValid())
            models.append(m);
    }
    return models;
}

ModelCatalogue catalogueFrom(const QCborArray &a) {
    ModelCatalogue c;
    for (const ModelRef &m : decodeModels(a))
        c.insert(m);
    return c;
}

QCborArray encodeMissing(const QList<MissingModel> &missing) {
    QCborArray a;
    for (const MissingModel &m : missing) {
        QCborMap e;
        e.insert(1, encodeModel(m.model));
        e.insert(2, m.installedRevision);
        QCborArray peers;
        for (const QString &p : m.peers)
            peers.append(p);
        e.insert(3, peers);
        a.append(e);
    }
    return a;
}

QList<MissingModel> decodeMissing(const QCborArray &a) {
    QList<MissingModel> missing;
    for (const auto &v : a) {
        const QCborMap e = v.toMap();
        MissingModel m;
        m.model = decodeModel(e.value(1).toMap());
        m.installedRevision = static_cast<int>(e.value(2).toInteger(-1));
        for (const auto &p : e.value(3).toArray())
            m.peers.append(p.toString());
        missing.append(m);
    }
    return missing;
}

QCborMap encodeElimination(const Elimination &e) {
    QCborMap m;
    m.insert(1, static_cast<qint64>(e.victim.key()));
    m.insert(2, e.victimMarking);
    m.insert(3, e.victimOwner);
    m.insert(4, e.victimOwnerName);
    m.insert(5, static_cast<qint64>(e.killer.key()));
    m.insert(6, e.killerMarking);
    m.insert(7, e.killerOwner);
    m.insert(8, e.killerOwnerName);
    m.insert(9, static_cast<qint64>(e.event));
    m.insert(10, e.time);
    return m;
}

Elimination decodeElimination(const QCborMap &m) {
    Elimination e;
    e.victim = DisEntityId::fromKey(static_cast<quint64>(m.value(1).toInteger()));
    e.victimMarking = m.value(2).toString();
    e.victimOwner = static_cast<quint16>(m.value(3).toInteger());
    e.victimOwnerName = m.value(4).toString();
    e.killer = DisEntityId::fromKey(static_cast<quint64>(m.value(5).toInteger()));
    e.killerMarking = m.value(6).toString();
    e.killerOwner = static_cast<quint16>(m.value(7).toInteger());
    e.killerOwnerName = m.value(8).toString();
    e.event = static_cast<quint32>(m.value(9).toInteger());
    e.time = m.value(10).toInteger();
    return e;
}

} // namespace

class ReplicatorPrivate {
public:
    struct SpawnRecord {
        DisEntityId id;
        ModelRef model;
        QString marking;
        DisForce force = DisForce::Neutral;
        quint16 owner = 0;
    };
    struct LocalEntity {
        ModelRef model;
        QString marking;
        DisForce force = DisForce::Neutral;
        EntityState current;
        EntityState lastSent;
        bool accepted = false;
        bool sent = false;
        int health = 1;
        bool destroyed = false;
    };
    struct Remote {
        RemoteEntity entity;
        EntityState previous;
        bool hasPrevious = false;
        qint64 blendStart = 0;
    };

    Replicator *q = nullptr;
    QtComms::Session *session = nullptr;
    ModelCatalogue catalogue;
    QString worldId;
    int worldRevision = 0;
    QByteArray worldHash;
    bool strict = false;
    QString updateUrl;
    QHash<int, DeadReckoningThresholds> thresholds;
    qint64 removeTimeout = 30000;
    qint64 blendTime = 150;
    QList<ModelRef> planned;
    quint16 nextHandle = 1;
    QMap<quint16, LocalEntity> local;
    QMap<quint64, SpawnRecord> spawns; // every entity in the session, kept by every peer for host migration
    QMap<quint64, Remote> remotes;
    QHash<quint64, qint64> queried;
    QTimer *timer = nullptr;
    QPointer<QtComms::Chat> chat;
    DamageModel damageModel;
    bool automaticDamage = true;
    quint16 nextEvent = 1;
    QList<Elimination> eliminations;

    DeadReckoningThresholds thresholdsFor(EntityKind kind) const {
        return thresholds.value(static_cast<int>(kind), DeadReckoningThresholds::forKind(kind));
    }
    qint64 now() const { return session->sessionTime(); }
    quint16 site() const { return session->info().site; }
    QCborMap worldMap() const {
        QCborMap w;
        w.insert(1, worldId);
        w.insert(2, worldRevision);
        w.insert(3, worldHash);
        return w;
    }
    QCborMap peerAttributes() const {
        QCborMap a;
        a.insert(kWorld, worldMap());
        a.insert(kModels, encodeModels(catalogue.models()));
        a.insert(kPlanned, encodeModels(planned));
        return a;
    }
    QCborMap sessionAttributes() const {
        QCborMap a;
        a.insert(kWorld, worldMap());
        a.insert(kDigest, catalogue.digest());
        a.insert(kStrict, strict);
        if (!updateUrl.isEmpty())
            a.insert(kUpdateUrl, updateUrl);
        return a;
    }
    bool sessionStrict() const {
        return session->isHost() ? strict : session->sessionAttributes().value(kStrict).toBool();
    }

    QList<ModelRef> currentModels() const;
    QtComms::Session::Admission admit(const QtComms::Peer &candidate);
    QList<MissingModel> peersLacking(const QList<ModelRef> &models, quint16 except);
    void notifyLacking(const QList<MissingModel> &missing, const QString &bringer);
    void validateSpawn(quint16 owner, quint16 handle, const ModelRef &model, const QString &marking, DisForce force);
    void applySpawn(const SpawnRecord &r);
    void applyDespawn(quint64 key);
    void sendSpawnRequest(quint16 handle);
    void sendPdu(quint16 handle, LocalEntity &e, const EntityState &state);
    void receiveMessage(quint16 peer, const QCborMap &m);
    void receiveDatagram(const QByteArray &datagram, quint16 peer);
    void receiveEntityState(DisEntityStatePdu &pdu, quint16 peer);
    void applyHit(const DisDetonationPdu &pdu);
    void sendNow(quint16 handle, LocalEntity &e);
    void eliminate(quint16 handle, LocalEntity &e, const DisEntityId &killer, quint32 event);
    void applyElimination(const Elimination &e);
    void removeOwner(quint16 owner);
    void tick();
    QCborMap encodeSpawn(const SpawnRecord &r) const;
    static SpawnRecord decodeSpawn(const QCborMap &m);
};

QCborMap ReplicatorPrivate::encodeSpawn(const SpawnRecord &r) const {
    QCborMap m;
    m.insert(0, static_cast<int>(Message::Spawn));
    m.insert(1, static_cast<qint64>(r.id.key()));
    m.insert(2, encodeModel(r.model));
    m.insert(3, r.marking);
    m.insert(4, static_cast<int>(r.force));
    m.insert(5, r.owner);
    return m;
}

ReplicatorPrivate::SpawnRecord ReplicatorPrivate::decodeSpawn(const QCborMap &m) {
    SpawnRecord r;
    r.id = DisEntityId::fromKey(static_cast<quint64>(m.value(1).toInteger()));
    r.model = decodeModel(m.value(2).toMap());
    r.marking = m.value(3).toString();
    r.force = static_cast<DisForce>(m.value(4).toInteger() & 0xff);
    r.owner = static_cast<quint16>(m.value(5).toInteger());
    return r;
}

QList<ModelRef> ReplicatorPrivate::currentModels() const {
    QList<ModelRef> models;
    for (const SpawnRecord &r : spawns) {
        const auto it =
            std::find_if(models.begin(), models.end(), [&](const ModelRef &m) { return m.id == r.model.id; });
        if (it == models.end())
            models.append(r.model);
        else if (r.model.revision > it->revision)
            *it = r.model;
    }
    return models;
}

QList<MissingModel> ReplicatorPrivate::peersLacking(const QList<ModelRef> &models, quint16 except) {
    QList<MissingModel> result;
    const bool strictModels = sessionStrict();
    for (const QtComms::Peer &p : session->peers()) {
        if (p.number == except)
            continue;
        const ModelCatalogue c =
            p.number == session->localPeer() ? catalogue : catalogueFrom(p.attributes.value(kModels).toArray());
        for (const ModelRef &m : c.missing(models, strictModels)) {
            auto it =
                std::find_if(result.begin(), result.end(), [&](const MissingModel &x) { return x.model.id == m.id; });
            if (it == result.end()) {
                MissingModel mm;
                mm.model = m;
                result.append(mm);
                it = result.end() - 1;
            }
            it->peers.append(p.name);
        }
    }
    return result;
}

void ReplicatorPrivate::notifyLacking(const QList<MissingModel> &missing, const QString &bringer) {
    for (const QtComms::Peer &p : session->peers()) {
        QList<MissingModel> theirs;
        for (const MissingModel &m : missing) {
            if (m.peers.contains(p.name))
                theirs.append(m);
        }
        if (theirs.isEmpty())
            continue;
        if (p.number == session->localPeer()) {
            emit q->modelUpdateNeeded(theirs, bringer);
            continue;
        }
        QCborMap notice;
        notice.insert(0, static_cast<int>(Message::ModelNotice));
        notice.insert(1, encodeMissing(theirs));
        notice.insert(2, bringer);
        session->sendMessage(p.number, Replicator::channel, notice);
    }
}

QtComms::Session::Admission ReplicatorPrivate::admit(const QtComms::Peer &candidate) {
    using Admission = QtComms::Session::Admission;
    using Reason = QtComms::Session::RejectReason;
    const QCborMap world = candidate.attributes.value(kWorld).toMap();
    const QString theirWorld = world.value(1).toString();
    const auto theirRevision = static_cast<int>(world.value(2).toInteger());
    const QByteArray theirHash = world.value(3).toByteArray();
    if (theirWorld != worldId || theirRevision != worldRevision ||
        (!theirHash.isEmpty() && !worldHash.isEmpty() && theirHash != worldHash)) {
        QCborMap details;
        details.insert(1, worldMap());
        return Admission::reject(Reason::WorldMismatch,
                                 QStringLiteral("This copy has the world \"%1\" r%2, but the session runs \"%3\" r%4.")
                                     .arg(theirWorld)
                                     .arg(theirRevision)
                                     .arg(worldId)
                                     .arg(worldRevision),
                                 details);
    }
    // Check 5: the candidate must render every model in the session.
    const ModelCatalogue theirs = catalogueFrom(candidate.attributes.value(kModels).toArray());
    const bool strictModels = sessionStrict();
    QList<MissingModel> lacking;
    for (const ModelRef &m : theirs.missing(currentModels(), strictModels)) {
        MissingModel mm;
        mm.model = m;
        const ModelRef installed = theirs.find(m.id);
        mm.installedRevision = installed.isValid() ? installed.revision : -1;
        mm.peers.append(candidate.name);
        lacking.append(mm);
    }
    if (!lacking.isEmpty()) {
        QCborMap details;
        details.insert(1, encodeMissing(lacking));
        if (!updateUrl.isEmpty())
            details.insert(2, updateUrl);
        return Admission::reject(
            Reason::MissingModels,
            QStringLiteral("This copy cannot show %1 model(s) used in the session.").arg(lacking.size()), details);
    }
    // Check 6: every peer must render every model the candidate brings.
    const QList<MissingModel> peersMissing =
        peersLacking(decodeModels(candidate.attributes.value(kPlanned).toArray()), 0);
    if (!peersMissing.isEmpty()) {
        notifyLacking(peersMissing, candidate.name);
        QCborMap details;
        details.insert(1, encodeMissing(peersMissing));
        return Admission::reject(
            Reason::PeersMissingModels,
            QStringLiteral("Peers in the session cannot show %1 model(s) this copy brings.").arg(peersMissing.size()),
            details);
    }
    QCborArray entities;
    for (const SpawnRecord &r : std::as_const(spawns))
        entities.append(encodeSpawn(r));
    QCborArray history;
    for (const Elimination &e : std::as_const(eliminations))
        history.append(encodeElimination(e));
    QCborMap accept;
    accept.insert(1, entities);
    accept.insert(2, history);
    return Admission::accept(accept);
}

void ReplicatorPrivate::validateSpawn(quint16 owner, quint16 handle, const ModelRef &model, const QString &marking,
                                      DisForce force) {
    const QtComms::Peer ownerPeer = session->peer(owner);
    QList<MissingModel> missing;
    if (!model.isValid() || ownerPeer.role == QtComms::Peer::Role::Observer) {
        MissingModel invalid;
        invalid.model = model;
        missing.append(invalid);
    } else {
        missing = peersLacking({model}, 0);
    }
    if (!missing.isEmpty()) {
        if (owner == session->localPeer()) {
            emit q->spawnRejected(handle, missing);
        } else {
            QCborMap reject;
            reject.insert(0, static_cast<int>(Message::SpawnReject));
            reject.insert(1, handle);
            reject.insert(2, encodeMissing(missing));
            session->sendMessage(owner, Replicator::channel, reject);
        }
        if (model.isValid())
            notifyLacking(missing, ownerPeer.name);
        return;
    }
    SpawnRecord r;
    r.id = DisEntityId(site(), owner, handle);
    r.model = model;
    r.marking = marking;
    r.force = force;
    r.owner = owner;
    session->broadcastMessage(Replicator::channel, encodeSpawn(r));
    applySpawn(r);
}

void ReplicatorPrivate::applySpawn(const SpawnRecord &r) {
    if (!r.id.isValid())
        return;
    spawns.insert(r.id.key(), r);
    queried.remove(r.id.key());
    if (r.owner == session->localPeer()) {
        auto it = local.find(r.id.entity);
        if (it != local.end() && !it->accepted) {
            it->accepted = true;
            emit q->spawnAccepted(r.id.entity);
            sendPdu(r.id.entity, *it, DeadReckoning::extrapolate(it->current, now()));
        }
        return;
    }
    Remote &remote = remotes[r.id.key()];
    remote.entity.id = r.id;
    remote.entity.model = r.model;
    remote.entity.marking = r.marking;
    remote.entity.force = r.force;
    remote.entity.owner = r.owner;
}

void ReplicatorPrivate::applyDespawn(quint64 key) {
    spawns.remove(key);
    const auto it = remotes.find(key);
    if (it == remotes.end())
        return;
    const bool wasVisible = it->entity.hasState;
    const DisEntityId id = it->entity.id;
    remotes.erase(it);
    if (wasVisible)
        emit q->entityRemoved(id);
}

void ReplicatorPrivate::removeOwner(quint16 owner) {
    QList<quint64> keys;
    for (auto it = spawns.cbegin(); it != spawns.cend(); ++it) {
        if (it->owner == owner)
            keys.append(it.key());
    }
    for (quint64 key : std::as_const(keys))
        applyDespawn(key);
}

void ReplicatorPrivate::sendSpawnRequest(quint16 handle) {
    const LocalEntity &e = local.value(handle);
    if (session->isHost()) {
        validateSpawn(session->localPeer(), handle, e.model, e.marking, e.force);
        return;
    }
    QCborMap request;
    request.insert(0, static_cast<int>(Message::SpawnRequest));
    request.insert(1, handle);
    request.insert(2, encodeModel(e.model));
    request.insert(3, e.marking);
    request.insert(4, static_cast<int>(e.force));
    session->sendMessage(session->hostPeer(), Replicator::channel, request);
}

void ReplicatorPrivate::sendPdu(quint16 handle, LocalEntity &e, const EntityState &state) {
    DisEntityStatePdu pdu;
    pdu.exercise = session->info().exercise;
    pdu.entityId = DisEntityId(site(), session->localPeer(), handle);
    pdu.force = e.force;
    pdu.entityType = e.model.disType;
    pdu.alternativeType = e.model.disType;
    pdu.state = state;
    pdu.timestamp = DisEntityStatePdu::timestampFromMsec(state.time);
    pdu.marking = e.marking.toLatin1();
    session->multicast(pdu.encode());
    e.lastSent = state;
    e.sent = true;
}

void ReplicatorPrivate::receiveMessage(quint16 peer, const QCborMap &m) {
    const auto type = static_cast<Message>(m.value(0).toInteger());
    const bool fromHost = peer == session->hostPeer();
    switch (type) {
    case Message::SpawnRequest:
        if (session->isHost()) {
            validateSpawn(peer, static_cast<quint16>(m.value(1).toInteger()), decodeModel(m.value(2).toMap()),
                          m.value(3).toString(), static_cast<DisForce>(m.value(4).toInteger() & 0xff));
        }
        break;
    case Message::Spawn:
        if (fromHost)
            applySpawn(decodeSpawn(m));
        break;
    case Message::SpawnReject:
        if (fromHost) {
            const auto handle = static_cast<quint16>(m.value(1).toInteger());
            if (local.remove(handle) > 0)
                emit q->spawnRejected(handle, decodeMissing(m.value(2).toArray()));
        }
        break;
    case Message::Despawn: {
        const auto key = static_cast<quint64>(m.value(1).toInteger());
        const DisEntityId id = DisEntityId::fromKey(key);
        if (session->isHost() && id.application == peer) {
            session->broadcastMessage(Replicator::channel, m);
            applyDespawn(key);
        } else if (fromHost) {
            applyDespawn(key);
        }
        break;
    }
    case Message::EntityQuery:
        if (session->isHost()) {
            const auto it = spawns.constFind(static_cast<quint64>(m.value(1).toInteger()));
            if (it != spawns.cend())
                session->sendMessage(peer, Replicator::channel, encodeSpawn(*it));
        }
        break;
    case Message::ModelNotice:
        if (fromHost)
            emit q->modelUpdateNeeded(decodeMissing(m.value(1).toArray()), m.value(2).toString());
        break;
    case Message::Eliminated: {
        const Elimination e = decodeElimination(m.value(1).toMap());
        if (session->isHost() && e.victim.application == peer) {
            session->broadcastMessage(Replicator::channel, m);
            applyElimination(e);
        } else if (fromHost && e.victim.application != session->localPeer()) {
            applyElimination(e);
        }
        break;
    }
    }
}

void ReplicatorPrivate::receiveDatagram(const QByteArray &datagram, quint16 peer) {
    if (peer == 0)
        return; // only peers of the session; a gateway for other DIS traffic comes later
    const quint8 exercise = session->info().exercise;
    for (qsizetype offset : DisPduHeader::split(datagram)) {
        DisPduHeader h;
        if (!DisPduHeader::peek(datagram, offset, &h) || h.exercise != exercise)
            continue;
        switch (h.type) {
        case DisPduType::EntityState: {
            DisEntityStatePdu pdu;
            if (pdu.decode(datagram, offset) && pdu.entityId.application == peer)
                receiveEntityState(pdu, peer);
            break;
        }
        case DisPduType::Fire: {
            DisFirePdu pdu;
            if (pdu.decode(datagram, offset) && pdu.firingEntity.application == peer)
                emit q->fireObserved(pdu);
            break;
        }
        case DisPduType::Detonation: {
            DisDetonationPdu pdu;
            if (pdu.decode(datagram, offset) && pdu.firingEntity.application == peer) {
                emit q->detonationObserved(pdu);
                if (pdu.hitsTarget() && pdu.targetEntity.application == session->localPeer())
                    applyHit(pdu);
            }
            break;
        }
        default:
            break;
        }
    }
}

void ReplicatorPrivate::receiveEntityState(DisEntityStatePdu &pdu, quint16 peer) {
    const quint64 key = pdu.entityId.key();
    auto it = remotes.find(key);
    if (it == remotes.end() || !spawns.contains(key)) {
        const qint64 t = now();
        if (!queried.contains(key) || t - queried.value(key) > kQueryInterval) {
            queried.insert(key, t);
            QCborMap query;
            query.insert(0, static_cast<int>(Message::EntityQuery));
            query.insert(1, static_cast<qint64>(key));
            session->sendMessage(session->hostPeer(), Replicator::channel, query);
        }
        return;
    }
    Q_UNUSED(peer);
    const qint64 t = now();
    pdu.state.time = DisEntityStatePdu::msecFromTimestamp(pdu.timestamp, t);
    Remote &r = *it;
    const bool first = !r.entity.hasState;
    const bool wasStale = r.entity.stale;
    if (!first && pdu.state.time < r.entity.lastState.time)
        return; // older than what we have
    r.previous = r.entity.lastState;
    r.hasPrevious = !first && !wasStale;
    r.blendStart = t;
    r.entity.lastState = pdu.state;
    r.entity.lastReceived = t;
    r.entity.hasState = true;
    r.entity.stale = false;
    const DamageState damage = DisAppearance(pdu.state.appearance).damage();
    const bool damageChanged = damage != r.entity.damage;
    r.entity.damage = damage;
    if (first)
        emit q->entityAppeared(r.entity.id);
    emit q->entityUpdated(r.entity.id);
    if (damageChanged)
        emit q->entityDamaged(r.entity.id, damage);
}

void ReplicatorPrivate::sendNow(quint16 handle, LocalEntity &e) {
    if (e.accepted)
        sendPdu(handle, e, DeadReckoning::extrapolate(e.current, std::max(now(), e.current.time)));
}

void ReplicatorPrivate::applyHit(const DisDetonationPdu &pdu) {
    const quint16 handle = pdu.targetEntity.entity;
    auto it = local.find(handle);
    if (it == local.end() || !it->accepted || it->destroyed)
        return;
    LocalEntity &e = *it;
    if (!automaticDamage) {
        emit q->entityHit(handle, pdu, DisAppearance(e.current.appearance).damage());
        return;
    }
    const DamageState state = damageModel.apply(e.model.kind, &e.health, damageModel.damageOf(pdu));
    if (state == DamageState::Destroyed) {
        eliminate(handle, e, pdu.firingEntity, pdu.event.number);
        emit q->entityHit(handle, pdu, state);
        return;
    }
    DisAppearance look(e.current.appearance);
    look = look.withDamage(state);
    if (state == DamageState::Moderate)
        look = look.withSmoke(DisAppearance::Smoke::Emitting);
    e.current.appearance = look.bits();
    sendNow(handle, e);
    emit q->entityHit(handle, pdu, state);
}

void ReplicatorPrivate::eliminate(quint16 handle, LocalEntity &e, const DisEntityId &killer, quint32 event) {
    if (e.destroyed)
        return;
    e.destroyed = true;
    e.health = 0;
    e.current.appearance = DisAppearance(e.current.appearance).destroyed().bits();
    sendNow(handle, e);
    Elimination elimination;
    elimination.victim = DisEntityId(site(), session->localPeer(), handle);
    elimination.victimMarking = e.marking;
    elimination.victimOwner = session->localPeer();
    elimination.victimOwnerName = session->peer(session->localPeer()).name;
    if (killer.isValid()) {
        elimination.killer = killer;
        elimination.killerOwner = killer.application;
        elimination.killerOwnerName = session->peer(killer.application).name;
        const auto spawn = spawns.constFind(killer.key());
        if (spawn != spawns.cend())
            elimination.killerMarking = spawn->marking;
    }
    elimination.event = event;
    elimination.time = now();
    QCborMap m;
    m.insert(0, static_cast<int>(Message::Eliminated));
    m.insert(1, encodeElimination(elimination));
    if (session->isHost())
        session->broadcastMessage(Replicator::channel, m);
    else
        session->sendMessage(session->hostPeer(), Replicator::channel, m);
    if (chat)
        chat->send(elimination.toString());
    applyElimination(elimination);
}

void ReplicatorPrivate::applyElimination(const Elimination &e) {
    if (!e.isValid())
        return;
    for (const Elimination &known : std::as_const(eliminations)) {
        if (known.victim == e.victim && known.time == e.time)
            return;
    }
    eliminations.append(e);
    if (eliminations.size() > kEliminationHistory)
        eliminations.removeFirst();
    const auto it = remotes.find(e.victim.key());
    if (it != remotes.end() && it->entity.damage != DamageState::Destroyed) {
        it->entity.damage = DamageState::Destroyed;
        if (it->entity.hasState) {
            it->entity.lastState.appearance = DisAppearance(it->entity.lastState.appearance).destroyed().bits();
            emit q->entityDamaged(e.victim, DamageState::Destroyed);
        }
    }
    emit q->entityEliminated(e);
}

void ReplicatorPrivate::tick() {
    const QtComms::Session::State state = session->state();
    if (state != QtComms::Session::State::Hosting && state != QtComms::Session::State::Joined)
        return;
    const qint64 t = now();
    // Heartbeats for entities whose application has stopped calling update().
    for (auto it = local.begin(); it != local.end(); ++it) {
        if (!it->accepted || !it->sent)
            continue;
        const EntityState s = DeadReckoning::extrapolate(it->current, std::max(t, it->current.time));
        if (DeadReckoning::updateReason(it->lastSent, s, thresholdsFor(it->model.kind)) ==
            DeadReckoning::UpdateReason::Heartbeat)
            sendPdu(it.key(), *it, s);
    }
    // Staleness and removal of remote entities.
    QList<quint64> gone;
    for (auto it = remotes.begin(); it != remotes.end(); ++it) {
        RemoteEntity &e = it->entity;
        if (!e.hasState)
            continue;
        const qint64 age = t - e.lastReceived;
        if (!e.stale && age > thresholdsFor(e.model.kind).staleTimeout()) {
            e.stale = true;
            emit q->entityStale(e.id);
        }
        if (age > removeTimeout)
            gone.append(it.key());
    }
    for (quint64 key : std::as_const(gone)) {
        const DisEntityId id = remotes.value(key).entity.id;
        Remote &r = remotes[key];
        r.entity.hasState = false;
        r.entity.stale = false;
        emit q->entityRemoved(id);
    }
}

/*!
    \module NtTwinNetwork
    \title Nt Twin Network C++ Classes
    \brief Nt Twin entities shared between the peers of a session as DIS 7.
    \cmaketarget Nt::TwinNetwork

    Nt Twin Network joins Nt Twin and Nt Comms: QtTwin::Replicator publishes the people, vehicles,
    aircraft, drones and boats a peer controls as DIS Entity State PDUs, shows everyone else's, and
    lets the host refuse peers that cannot render every model in the session. Keeping it apart lets
    Nt Twin itself run without a network. Qt has no such module.

    See \l{Multiplayer Sessions} for an introduction.
*/

/*!
    \class QtTwin::RemoteEntity
    \inmodule NtTwinNetwork
    \brief The RemoteEntity struct is an entity owned by another peer of the session.

    \differences
    Qt has no such type; this struct is part of Nt Twin Network.
    \enddifferences

    \sa QtTwin::Replicator
*/

/*!
    \variable QtTwin::RemoteEntity::id
    The DIS entity ID: the session's site, the owner's peer number and the owner's number for it.
*/

/*!
    \variable QtTwin::RemoteEntity::model
    The model to render it with.
*/

/*!
    \variable QtTwin::RemoteEntity::marking
    Its callsign.
*/

/*!
    \variable QtTwin::RemoteEntity::force
    Its side.
*/

/*!
    \variable QtTwin::RemoteEntity::owner
    The peer that owns it.
*/

/*!
    \variable QtTwin::RemoteEntity::lastState
    The last state received, with its time in session time.
*/

/*!
    \variable QtTwin::RemoteEntity::lastReceived
    The session time the last state arrived.
*/

/*!
    \variable QtTwin::RemoteEntity::hasState
    Whether a state has arrived; until then there is nothing to show.
*/

/*!
    \variable QtTwin::RemoteEntity::stale
    Whether no state has arrived for the stale timeout. Show it as stale, not as current.
*/

/*!
    \variable QtTwin::RemoteEntity::damage
    Its damage, from the appearance of its last state or an elimination report. Show a destroyed
    entity as a wreck.

    \sa QtTwin::Replicator::entityDamaged()
*/

/*!
    \class QtTwin::MissingModel
    \inmodule NtTwinNetwork
    \brief The MissingModel struct names a model that a peer cannot render.

    It is what a refused join or spawn carries, so that the application can tell its user exactly
    what to update.

    \differences
    Qt has no such type; this struct is part of Nt Twin Network.
    \enddifferences

    \sa QtTwin::Replicator::missingModels()
*/

/*!
    \variable QtTwin::MissingModel::model
    The model needed, at the revision needed.
*/

/*!
    \variable QtTwin::MissingModel::installedRevision
    The revision the peer has, or -1 if it has none.
*/

/*!
    \variable QtTwin::MissingModel::peers
    The names of the peers that lack the model.
*/

/*!
    \class QtTwin::Replicator
    \inmodule NtTwinNetwork
    \brief The Replicator class shares moving entities between the peers of a session, as DIS.

    A Replicator sits on a QtComms::Session. Each peer publishes the entities it controls, such as
    its own vehicle, and sees every other peer's on the map. Positions travel as DIS 7 Entity State
    PDUs on the session's multicast group; the owner of an entity sends only when the receivers'
    dead-reckoning prediction drifts past a threshold, and at least every heartbeat, so a parked
    vehicle costs one PDU every five seconds.

    \code
    QtComms::Session session;
    QtTwin::Replicator replicator(&session);
    replicator.setCatalogue(catalogue);                         // the models this copy can render
    replicator.setWorld(QStringLiteral("amesbury"), 3);
    replicator.join(found, QStringLiteral("bob"));
    connect(&session, &QtComms::Session::joined, [&] {
        handle = replicator.publish(catalogue.find(QStringLiteral("land/wheeled/lr110")),
                                    QStringLiteral("BRAVO-2"), myState);
    });
    // every frame:
    replicator.update(handle, myState);
    for (const QtTwin::RemoteEntity &e : replicator.remoteEntities())
        draw(e.model, replicator.remoteState(e.id, session.sessionTime()), e.stale);
    \endcode

    \section1 Models

    Every participant must be able to render every entity in the session. The host checks this when
    a peer joins: it refuses a peer that lacks a model already in use (QtComms::Session::RejectReason
    \c MissingModels), and a peer that brings a model some current peer lacks (\c PeersMissingModels,
    with modelUpdateNeeded() emitted on those peers). The same check runs for each new entity, and a
    refused one is reported with spawnRejected(). describeRejection() turns a refusal into text for
    the user, such as "you need an update: air/rotary/ec135 needs revision 4, you have 3".

    \section1 Receiving

    remoteState() predicts a remote entity to any session time and glides to each new update over
    150 milliseconds instead of jumping. People and ground vehicles should then be placed on the
    receiver's own terrain, and boats on its water. An entity that has sent nothing for its stale
    timeout (12 seconds by default) is frozen and reported with entityStale(); after 30 seconds it is
    removed. Entities of a peer that leaves are removed at once.

    \section1 Kinetic Effects

    An entity that shoots calls fire(), which sends a DIS Fire PDU that every peer gets as
    fireObserved(), so all can show the shot and fly the round with QtTwin::PhysicsEngine. The
    shooter flies it too, and when it hits or lands calls detonate(), which sends the Detonation PDU
    (detonationObserved() everywhere). As DIS requires, the owner of the entity that was hit decides
    the damage: with automatic damage, the default, the replicator applies its QtTwin::DamageModel,
    puts the result in the entity's appearance bits and sends a PDU at once, and emits entityHit().
    Other peers see the change as entityDamaged(). When the damage reaches \c Destroyed, the owner
    reports the elimination to every peer, which emit entityEliminated() with who was destroyed and
    by whom, and posts it in the session's chat if setChat() was called. An application with its own
    rules turns automatic damage off and calls setDamage() and eliminate() itself.

    \code
    const QtTwin::DisEventId shot = replicator.fire(turret, drone.id, munition, muzzle, velocity);
    // ... fly the round; on impact:
    replicator.detonate(turret, shot, drone.id, QtTwin::DisDetonationResult::EntityImpact, hit, speed);
    connect(&replicator, &QtTwin::Replicator::entityEliminated,
            [](const QtTwin::Elimination &e) { hud.announce(e.toString()); });  // "UAV-3 (bob) was eliminated by TURRET (alice)"
    \endcode

    A destroyed entity stays in the session as a wreck, with its destroyed appearance in every
    PDU, until its owner removes it.

    \differences
    Qt has no such class; this class is part of Nt Twin Network. Only Entity State, Fire and
    Detonation PDUs from peers of the session are used; DIS traffic from other applications is
    ignored.
    \enddifferences

    \sa QtComms::Session, QtTwin::DisEntityStatePdu, QtTwin::DeadReckoning, QtTwin::DamageModel
*/

/*!
    \variable QtTwin::Replicator::channel
    The session channel the replicator uses for spawns and despawns: 32.
*/

/*!
    Constructs a replicator on \a session, with the given \a parent. It installs the session's
    admission policy.
*/
Replicator::Replicator(QtComms::Session *session, QObject *parent)
    : QObject(parent), d(std::make_unique<ReplicatorPrivate>()) {
    d->q = this;
    d->session = session;
    session->setAdmissionPolicy([this](const QtComms::Peer &candidate) { return d->admit(candidate); });
    connect(session, &QtComms::Session::messageReceived, this, [this](quint16 peer, int ch, const QCborMap &m) {
        if (ch == channel)
            d->receiveMessage(peer, m);
    });
    connect(session, &QtComms::Session::datagramReceived, this,
            [this](const QByteArray &datagram, quint16 peer) { d->receiveDatagram(datagram, peer); });
    connect(session, &QtComms::Session::joined, this, [this](const QCborMap &accept) {
        for (const auto &v : accept.value(1).toArray())
            d->applySpawn(ReplicatorPrivate::decodeSpawn(v.toMap()));
        for (const auto &v : accept.value(2).toArray()) {
            const Elimination e = decodeElimination(v.toMap());
            if (e.isValid())
                d->eliminations.append(e);
        }
    });
    connect(session, &QtComms::Session::peerLeft, this, [this](quint16 peer) { d->removeOwner(peer); });
    connect(session, &QtComms::Session::hostChanged, this, [this](quint16) {
        if (d->session->isHost())
            d->session->setSessionAttributes(d->sessionAttributes());
        // Requests the old host never answered go to the new one.
        for (auto it = d->local.cbegin(); it != d->local.cend(); ++it) {
            if (!it->accepted)
                d->sendSpawnRequest(it.key());
        }
    });
    connect(session, &QtComms::Session::stateChanged, this, [this](QtComms::Session::State s) {
        if (s == QtComms::Session::State::Idle || s == QtComms::Session::State::Failed ||
            s == QtComms::Session::State::Rejected) {
            d->local.clear();
            d->spawns.clear();
            d->remotes.clear();
            d->eliminations.clear();
        }
    });
    d->timer = new QTimer(this);
    connect(d->timer, &QTimer::timeout, this, [this] { d->tick(); });
    d->timer->start(kTimerInterval);
}

/*!
    Destroys the replicator.
*/
Replicator::~Replicator() = default;

/*!
    Returns the session the replicator sits on.
*/
QtComms::Session *Replicator::session() const { return d->session; }

/*!
    Sets the models this copy can render to \a catalogue. Set it before host() or join(): it is sent
    to the host for the admission checks.
*/
void Replicator::setCatalogue(const ModelCatalogue &catalogue) { d->catalogue = catalogue; }

/*!
    Returns the models this copy can render.
*/
ModelCatalogue Replicator::catalogue() const { return d->catalogue; }

/*!
    Sets the world this copy runs to \a id at \a revision, with the optional \a hash of its
    manifest. A peer with another world, revision or hash is refused.
*/
void Replicator::setWorld(const QString &id, int revision, const QByteArray &hash) {
    d->worldId = id;
    d->worldRevision = revision;
    d->worldHash = hash;
}

/*!
    Sets whether models must match by SHA-256, not just by revision, to \a strict. It is the host's
    setting that counts.
*/
void Replicator::setStrictModels(bool strict) { d->strict = strict; }

/*!
    Returns whether models must match by SHA-256.
*/
bool Replicator::strictModels() const { return d->strict; }

/*!
    Sets the address where users can get missing models to \a url. A host announces it and sends it
    with refusals.
*/
void Replicator::setUpdateUrl(const QString &url) { d->updateUrl = url; }

/*!
    Sets the dead-reckoning \a thresholds for entities of \a kind, replacing
    QtTwin::DeadReckoningThresholds::forKind(). They decide when this peer sends, and when it marks
    remote entities stale.
*/
void Replicator::setThresholds(EntityKind kind, const DeadReckoningThresholds &thresholds) {
    d->thresholds.insert(static_cast<int>(kind), thresholds);
}

/*!
    Returns the dead-reckoning thresholds for entities of \a kind.
*/
DeadReckoningThresholds Replicator::thresholds(EntityKind kind) const { return d->thresholdsFor(kind); }

/*!
    Sets how long a remote entity may send nothing before it is removed to \a msec milliseconds,
    30000 by default.
*/
void Replicator::setRemoveTimeout(qint64 msec) { d->removeTimeout = msec; }

/*!
    Sets how long remoteState() takes to glide to a new update to \a msec milliseconds, 150 by
    default. 0 jumps.
*/
void Replicator::setBlendTime(qint64 msec) { d->blendTime = std::max<qint64>(0, msec); }

/*!
    Hosts a session called \a sessionName as the peer \a peerName, announcing the world, the digest of
    the catalogue and the update address. Returns \c false if the session cannot be opened.
*/
bool Replicator::host(const QString &sessionName, const QString &peerName) {
    d->planned.clear();
    return d->session->host(sessionName, peerName, d->peerAttributes(), d->sessionAttributes());
}

/*!
    Asks to join \a session as \a peerName in \a role, sending the world, the catalogue and the
    \a plannedModels this copy will publish, so the host can refuse at once if any peer lacks one.
    Returns \c false if the session cannot be opened.
*/
bool Replicator::join(const QtComms::SessionInfo &session, const QString &peerName, QtComms::Peer::Role role,
                      const QList<ModelRef> &plannedModels) {
    d->planned = role == QtComms::Peer::Role::Observer ? QList<ModelRef>() : plannedModels;
    return d->session->join(session, peerName, role, d->peerAttributes());
}

/*!
    Publishes an entity shown with \a model and called \a marking, starting at \a initial, on the side
    \a force, and returns its handle. The host checks that every peer can render \a model;
    spawnAccepted() or spawnRejected() follows. Returns 0 if this peer is not a participant in a
    session.
*/
quint16 Replicator::publish(const ModelRef &model, const QString &marking, const EntityState &initial, DisForce force) {
    const QtComms::Session::State s = d->session->state();
    if ((s != QtComms::Session::State::Hosting && s != QtComms::Session::State::Joined) ||
        d->session->peer(d->session->localPeer()).role == QtComms::Peer::Role::Observer || !model.isValid())
        return 0;
    const quint16 handle = d->nextHandle++;
    ReplicatorPrivate::LocalEntity e;
    e.model = model;
    e.marking = marking;
    e.force = force;
    e.current = initial;
    e.health = d->damageModel.hitPoints(model.kind);
    d->local.insert(handle, e);
    d->sendSpawnRequest(handle);
    return handle;
}

/*!
    Sets the state of the entity \a handle to \a state, whose time should be the current session
    time. Call it every frame: it sends a PDU only when the receivers' prediction is off by more
    than the thresholds, the appearance or a part changed, or the heartbeat is due.
*/
void Replicator::update(quint16 handle, const EntityState &state) {
    auto it = d->local.find(handle);
    if (it == d->local.end())
        return;
    it->current = state;
    if (it->destroyed)
        it->current.appearance = DisAppearance(state.appearance).destroyed().bits();
    if (!it->accepted)
        return;
    if (!it->sent) {
        d->sendPdu(handle, *it, it->current);
        return;
    }
    const DeadReckoning::UpdateReason reason =
        DeadReckoning::updateReason(it->lastSent, it->current, d->thresholdsFor(it->model.kind));
    if (reason == DeadReckoning::UpdateReason::Threshold || reason == DeadReckoning::UpdateReason::Changed ||
        reason == DeadReckoning::UpdateReason::Heartbeat)
        d->sendPdu(handle, *it, it->current);
}

/*!
    Removes the entity \a handle from the session.
*/
void Replicator::remove(quint16 handle) {
    const auto it = d->local.find(handle);
    if (it == d->local.end())
        return;
    const bool accepted = it->accepted;
    d->local.erase(it);
    if (!accepted)
        return;
    const quint64 key = DisEntityId(d->site(), d->session->localPeer(), handle).key();
    QCborMap despawn;
    despawn.insert(0, static_cast<int>(Message::Despawn));
    despawn.insert(1, static_cast<qint64>(key));
    if (d->session->isHost())
        d->session->broadcastMessage(channel, despawn);
    else
        d->session->sendMessage(d->session->hostPeer(), channel, despawn);
    d->spawns.remove(key);
}

/*!
    Returns \c true if the host has accepted the entity \a handle.
*/
bool Replicator::isAccepted(quint16 handle) const { return d->local.value(handle).accepted; }

/*!
    Returns the DIS entity ID of this peer's entity \a handle.
*/
DisEntityId Replicator::entityId(quint16 handle) const {
    return d->local.contains(handle) ? DisEntityId(d->site(), d->session->localPeer(), handle) : DisEntityId();
}

/*!
    Returns the models of all entities in the session, each once at its highest revision.
*/
QList<ModelRef> Replicator::sessionModels() const { return d->currentModels(); }

/*!
    Returns the entities of other peers that have sent a state, ordered by ID.
*/
QList<RemoteEntity> Replicator::remoteEntities() const {
    QList<RemoteEntity> result;
    for (const ReplicatorPrivate::Remote &r : std::as_const(d->remotes)) {
        if (r.entity.hasState)
            result.append(r.entity);
    }
    return result;
}

/*!
    Returns the remote entity \a id, or an entity without a state if there is none.
*/
RemoteEntity Replicator::remoteEntity(const DisEntityId &id) const { return d->remotes.value(id.key()).entity; }

/*!
    Returns the state of the remote entity \a id predicted to \a sessionTime. It glides from the
    previous prediction to the latest over the blend time, and stops extrapolating once the entity is
    stale. Returns a default state if the entity has no state.
*/
EntityState Replicator::remoteState(const DisEntityId &id, qint64 sessionTime) const {
    const auto it = d->remotes.constFind(id.key());
    if (it == d->remotes.cend() || !it->entity.hasState)
        return EntityState();
    const RemoteEntity &e = it->entity;
    const qint64 frozenAt = e.lastState.time + d->thresholdsFor(e.model.kind).staleTimeout();
    const qint64 t = std::min(sessionTime, frozenAt);
    EntityState latest = DeadReckoning::extrapolate(e.lastState, t);
    const qint64 sinceUpdate = sessionTime - it->blendStart;
    if (!it->hasPrevious || d->blendTime == 0 || sinceUpdate >= d->blendTime)
        return latest;
    const EntityState previous = DeadReckoning::extrapolate(it->previous, t);
    return DeadReckoning::blend(
        previous, latest, static_cast<double>(std::max<qint64>(0, sinceUpdate)) / static_cast<double>(d->blendTime));
}

/*!
    Sets the chat eliminations are announced in to \a chat, or none if null. The owner of a
    destroyed entity posts the report, such as "UAV-3 (bob) was eliminated by TURRET (alice)", to
    everyone.
*/
void Replicator::setChat(QtComms::Chat *chat) { d->chat = chat; }

/*!
    Returns the chat eliminations are announced in, or null.
*/
QtComms::Chat *Replicator::chat() const { return d->chat; }

/*!
    Sets the damage model applied to this peer's entities when they are hit to \a model. Entities
    published later start with its hit points.
*/
void Replicator::setDamageModel(const DamageModel &model) { d->damageModel = model; }

/*!
    Returns the damage model.
*/
DamageModel Replicator::damageModel() const { return d->damageModel; }

/*!
    Sets whether a detonation that hits one of this peer's entities is applied with the damage model
    to \a automatic, on by default. Off, entityHit() is still emitted and the application calls
    setDamage() or eliminate() itself.
*/
void Replicator::setAutomaticDamage(bool automatic) { d->automaticDamage = automatic; }

/*!
    Returns whether hits are applied with the damage model.
*/
bool Replicator::automaticDamage() const { return d->automaticDamage; }

/*!
    Sends a Fire PDU: this peer's entity \a shooter fired \a munition at \a target (which may be an
    invalid ID for no particular target) from \a location at \a velocity, both in Earth-centred,
    Earth-fixed coordinates, with the computed \a range in metres. Returns the event, to pass to
    detonate() when the round ends, or an invalid event if \a shooter is not an accepted, intact
    entity of this peer. fireObserved() is emitted here too, so the shooter's own view shows the shot.
*/
DisEventId Replicator::fire(quint16 shooter, const DisEntityId &target, const DisMunitionDescriptor &munition,
                            const Vector3d &location, const Vector3d &velocity, double range) {
    const auto it = d->local.constFind(shooter);
    if (it == d->local.cend() || !it->accepted || it->destroyed)
        return DisEventId();
    DisFirePdu pdu;
    pdu.exercise = d->session->info().exercise;
    pdu.timestamp = DisEntityStatePdu::timestampFromMsec(d->now());
    pdu.firingEntity = DisEntityId(d->site(), d->session->localPeer(), shooter);
    pdu.targetEntity = target;
    pdu.event = DisEventId(d->site(), d->session->localPeer(), d->nextEvent);
    d->nextEvent = d->nextEvent == 65535 ? 1 : static_cast<quint16>(d->nextEvent + 1);
    pdu.location = location;
    pdu.descriptor = munition;
    pdu.velocity = velocity;
    pdu.range = range;
    d->session->multicast(pdu.encode());
    emit fireObserved(pdu);
    return pdu.event;
}

/*!
    Sends a Detonation PDU: the round of \a event, fired by this peer's entity \a shooter, ended at
    \a location moving at \a velocity with the given \a result. If it struck \a target, pass the
    result \c EntityImpact and where on the target in its body frame as \a locationInTarget; the
    target's owner then applies the damage. \a munition says what detonated. detonationObserved() is
    emitted here too, and a hit on this peer's own entity is applied at once.
*/
void Replicator::detonate(quint16 shooter, const DisEventId &event, const DisEntityId &target,
                          DisDetonationResult result, const Vector3d &location, const Vector3d &velocity,
                          const DisMunitionDescriptor &munition, const Vector3d &locationInTarget) {
    const QtComms::Session::State s = d->session->state();
    if (s != QtComms::Session::State::Hosting && s != QtComms::Session::State::Joined)
        return;
    DisDetonationPdu pdu;
    pdu.exercise = d->session->info().exercise;
    pdu.timestamp = DisEntityStatePdu::timestampFromMsec(d->now());
    pdu.firingEntity = DisEntityId(d->site(), d->session->localPeer(), shooter);
    pdu.targetEntity = target;
    pdu.event = event;
    pdu.velocity = velocity;
    pdu.location = location;
    pdu.descriptor = munition;
    pdu.locationInEntity = locationInTarget;
    pdu.result = result;
    d->session->multicast(pdu.encode());
    emit detonationObserved(pdu);
    if (pdu.hitsTarget() && target.application == d->session->localPeer())
        d->applyHit(pdu);
}

/*!
    Returns the hit points this peer's entity \a handle has left, or 0 if there is no such entity.
*/
int Replicator::health(quint16 handle) const { return d->local.value(handle).health; }

/*!
    Returns the damage of this peer's entity \a handle, from its appearance.
*/
DamageState Replicator::damage(quint16 handle) const {
    return DisAppearance(d->local.value(handle).current.appearance).damage();
}

/*!
    Sets the damage of this peer's entity \a handle to \a state in its appearance and sends a PDU
    at once. \c Destroyed eliminates it, with no killer named.
*/
void Replicator::setDamage(quint16 handle, DamageState state) {
    auto it = d->local.find(handle);
    if (it == d->local.end() || it->destroyed)
        return;
    if (state == DamageState::Destroyed) {
        d->eliminate(handle, *it, DisEntityId(), 0);
        return;
    }
    it->current.appearance = DisAppearance(it->current.appearance).withDamage(state).bits();
    // Keep the hit points consistent with the state, so later hits continue from it.
    const int full = d->damageModel.hitPoints(it->model.kind);
    if (state == DamageState::None)
        it->health = full;
    else if (state == DamageState::Slight)
        it->health = std::max(1, full - 1);
    else
        it->health = std::max(1, full / 3);
    d->sendNow(handle, *it);
}

/*!
    Destroys this peer's entity \a handle: its appearance becomes a wreck, a PDU goes out at once,
    and every peer is told it was eliminated by the entity \a killer in the shot \a event, or just
    destroyed if \a killer is invalid. The entity stays in the session until remove() is called.
*/
void Replicator::eliminate(quint16 handle, const DisEntityId &killer, quint32 event) {
    auto it = d->local.find(handle);
    if (it == d->local.end() || !it->accepted)
        return;
    d->eliminate(handle, *it, killer, event);
}

/*!
    Returns \c true if this peer's entity \a handle has been destroyed.
*/
bool Replicator::isDestroyed(quint16 handle) const { return d->local.value(handle).destroyed; }

/*!
    Returns the eliminations reported in the session, oldest first, up to the last 200. A peer that
    joins late gets the history from the host.
*/
QList<Elimination> Replicator::eliminations() const { return d->eliminations; }

/*!
    Returns the models listed in the \a details of a refusal with the reason \c MissingModels or
    \c PeersMissingModels, or of spawnRejected().
*/
QList<MissingModel> Replicator::missingModels(const QCborMap &details) {
    return decodeMissing(details.value(1).toArray());
}

/*!
    Returns text for the user explaining a refusal: \a reason and \a text from
    QtComms::Session::rejected(), with the models from \a details listed one per line, and where to
    get them.
*/
QString Replicator::describeRejection(QtComms::Session::RejectReason reason, const QString &text,
                                      const QCborMap &details) {
    using Reason = QtComms::Session::RejectReason;
    const QList<MissingModel> missing = missingModels(details);
    QString out;
    if (reason == Reason::MissingModels) {
        out = QStringLiteral("You need an update: this copy cannot show %1 model(s) used in the session. "
                             "Update your model library:")
                  .arg(missing.size());
        for (const MissingModel &m : missing) {
            if (m.installedRevision >= 0)
                out += QStringLiteral("\n  - %1 needs revision %2, you have %3")
                           .arg(m.model.id)
                           .arg(m.model.revision)
                           .arg(m.installedRevision);
            else
                out += QStringLiteral("\n  - %1 revision %2, not installed").arg(m.model.id).arg(m.model.revision);
        }
        const QString url = details.value(2).toString();
        if (!url.isEmpty())
            out += QStringLiteral("\nGet the update from: %1").arg(url);
    } else if (reason == Reason::PeersMissingModels) {
        out = QStringLiteral("Peers in the session cannot show model(s) this copy brings; they need an update:");
        for (const MissingModel &m : missing)
            out += QStringLiteral("\n  - %1 r%2: %3")
                       .arg(m.model.id)
                       .arg(m.model.revision)
                       .arg(m.peers.join(QStringLiteral(", ")));
    } else if (reason == Reason::ProtocolTooOld || reason == Reason::ProtocolTooNew) {
        out = text + QStringLiteral(" Update the application, not the model library.");
    } else {
        out = text;
    }
    return out;
}

/*!
    \fn void QtTwin::Replicator::entityAppeared(const QtTwin::DisEntityId &id)
    This signal is emitted when the first state of the remote entity \a id arrives.
*/

/*!
    \fn void QtTwin::Replicator::entityUpdated(const QtTwin::DisEntityId &id)
    This signal is emitted when a state of the remote entity \a id arrives.
*/

/*!
    \fn void QtTwin::Replicator::entityStale(const QtTwin::DisEntityId &id)
    This signal is emitted when the remote entity \a id has sent nothing for its stale timeout.
*/

/*!
    \fn void QtTwin::Replicator::entityRemoved(const QtTwin::DisEntityId &id)
    This signal is emitted when the remote entity \a id is removed by its owner, its owner leaves, or
    it has sent nothing for the remove timeout.
*/

/*!
    \fn void QtTwin::Replicator::spawnAccepted(quint16 handle)
    This signal is emitted when the host accepts this peer's entity \a handle; its first PDU has
    been sent.
*/

/*!
    \fn void QtTwin::Replicator::spawnRejected(quint16 handle, const QList<QtTwin::MissingModel> &missing)
    This signal is emitted when the host refuses this peer's entity \a handle because peers lack its
    model; \a missing says which.
*/

/*!
    \fn void QtTwin::Replicator::modelUpdateNeeded(const QList<QtTwin::MissingModel> &missing, const QString &peerName)
    This signal is emitted on a peer that lacks the \a missing models that \a peerName tried to
    bring into the session. Tell the user to update the model library.
*/

/*!
    \fn void QtTwin::Replicator::fireObserved(const QtTwin::DisFirePdu &pdu)
    This signal is emitted when an entity in the session fires, including this peer's own; \a pdu
    says who, at what, from where and with which munition. Show the shot and fly the round.
*/

/*!
    \fn void QtTwin::Replicator::detonationObserved(const QtTwin::DisDetonationPdu &pdu)
    This signal is emitted when a round in the session detonates or lands, including this peer's
    own; \a pdu says where and with what result. Show the explosion there.
*/

/*!
    \fn void QtTwin::Replicator::entityHit(quint16 handle, const QtTwin::DisDetonationPdu &pdu, QtTwin::DamageState damage)
    This signal is emitted when a detonation \a pdu strikes this peer's entity \a handle. With
    automatic damage, \a damage is the new state after the damage model was applied and sent;
    without, it is the current state, and the application decides with setDamage() or eliminate().
*/

/*!
    \fn void QtTwin::Replicator::entityDamaged(const QtTwin::DisEntityId &id, QtTwin::DamageState damage)
    This signal is emitted when the remote entity \a id reports a new \a damage state in its
    appearance. At \c Destroyed, swap its model for the wreck and play the destruction effect.
*/

/*!
    \fn void QtTwin::Replicator::entityEliminated(const QtTwin::Elimination &elimination)
    This signal is emitted on every peer when an entity is destroyed; \a elimination says which,
    whose it was and who destroyed it. Announce it and keep score.
*/

} // namespace QtTwin

#include "moc_qtwinreplicator.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
