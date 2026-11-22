// MIT License - see LICENSE
#pragma once

#include <QtComms/qcommssession.h>
#include <QtTwin/qtwindeadreckoning.h>
#include <QtTwin/qtwindis.h>
#include <QtTwin/qtwinentity.h>

#include <QtCore/qobject.h>

#include <memory>

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

private:
    std::unique_ptr<ReplicatorPrivate> d;
};

} // namespace QtTwin
