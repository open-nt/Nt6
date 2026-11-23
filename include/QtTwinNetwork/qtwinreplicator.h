// MIT License - see LICENSE
#pragma once

#include <QtComms/qcommssession.h>
#include <QtTwin/qtwindeadreckoning.h>
#include <QtTwin/qtwindis.h>
#include <QtTwin/qtwinentity.h>
#include <QtTwin/qtwinkinetic.h>

#include <QtCore/qobject.h>

#include <memory>

namespace QtComms {
class Chat;
}

namespace QtTwin {

struct RemoteEntity {
    DisEntityId id;
    ModelRef model;
    QString marking;
    DisForce force = DisForce::Neutral;
    quint16 owner = 0;
    EntityState lastState;
    qint64 lastReceived = 0;
    bool hasState = false;
    bool stale = false;
    DamageState damage = DamageState::None;
};

struct MissingModel {
    ModelRef model;
    int installedRevision = -1;
    QStringList peers;
};

class ReplicatorPrivate;

class Replicator : public QObject {
    Q_OBJECT

public:
    static constexpr int channel = 32;

    explicit Replicator(QtComms::Session *session, QObject *parent = nullptr);
    ~Replicator() override;

    QtComms::Session *session() const;
    void setCatalogue(const ModelCatalogue &catalogue);
    ModelCatalogue catalogue() const;
    void setWorld(const QString &id, int revision, const QByteArray &hash = QByteArray());
    void setStrictModels(bool strict);
    bool strictModels() const;
    void setUpdateUrl(const QString &url);
    void setThresholds(EntityKind kind, const DeadReckoningThresholds &thresholds);
    DeadReckoningThresholds thresholds(EntityKind kind) const;
    void setRemoveTimeout(qint64 msec);
    void setBlendTime(qint64 msec);

    bool host(const QString &sessionName, const QString &peerName);
    bool join(const QtComms::SessionInfo &session, const QString &peerName,
              QtComms::Peer::Role role = QtComms::Peer::Role::Participant,
              const QList<ModelRef> &plannedModels = QList<ModelRef>());

    quint16 publish(const ModelRef &model, const QString &marking, const EntityState &initial,
                    DisForce force = DisForce::Neutral);
    void update(quint16 handle, const EntityState &state);
    void remove(quint16 handle);
    bool isAccepted(quint16 handle) const;
    DisEntityId entityId(quint16 handle) const;
    QList<ModelRef> sessionModels() const;

    QList<RemoteEntity> remoteEntities() const;
    RemoteEntity remoteEntity(const DisEntityId &id) const;
    EntityState remoteState(const DisEntityId &id, qint64 sessionTime) const;

    void setChat(QtComms::Chat *chat);
    QtComms::Chat *chat() const;
    void setDamageModel(const DamageModel &model);
    DamageModel damageModel() const;
    void setAutomaticDamage(bool automatic);
    bool automaticDamage() const;

    DisEventId fire(quint16 shooter, const DisEntityId &target, const DisMunitionDescriptor &munition,
                    const Vector3d &location, const Vector3d &velocity, double range = 0.0);
    void detonate(quint16 shooter, const DisEventId &event, const DisEntityId &target, DisDetonationResult result,
                  const Vector3d &location, const Vector3d &velocity,
                  const DisMunitionDescriptor &munition = DisMunitionDescriptor(),
                  const Vector3d &locationInTarget = Vector3d());
    int health(quint16 handle) const;
    DamageState damage(quint16 handle) const;
    void setDamage(quint16 handle, DamageState state);
    void eliminate(quint16 handle, const DisEntityId &killer = DisEntityId(), quint32 event = 0);
    bool isDestroyed(quint16 handle) const;
    QList<Elimination> eliminations() const;

    static QList<MissingModel> missingModels(const QCborMap &details);
    static QString describeRejection(QtComms::Session::RejectReason reason, const QString &text,
                                     const QCborMap &details);

Q_SIGNALS:
    void entityAppeared(const QtTwin::DisEntityId &id);
    void entityUpdated(const QtTwin::DisEntityId &id);
    void entityStale(const QtTwin::DisEntityId &id);
    void entityRemoved(const QtTwin::DisEntityId &id);
    void spawnAccepted(quint16 handle);
    void spawnRejected(quint16 handle, const QList<QtTwin::MissingModel> &missing);
    void modelUpdateNeeded(const QList<QtTwin::MissingModel> &missing, const QString &peerName);
    void fireObserved(const QtTwin::DisFirePdu &pdu);
    void detonationObserved(const QtTwin::DisDetonationPdu &pdu);
    void entityHit(quint16 handle, const QtTwin::DisDetonationPdu &pdu, QtTwin::DamageState damage);
    void entityDamaged(const QtTwin::DisEntityId &id, QtTwin::DamageState damage);
    void entityEliminated(const QtTwin::Elimination &elimination);

private:
    std::unique_ptr<ReplicatorPrivate> d;
};

} // namespace QtTwin
