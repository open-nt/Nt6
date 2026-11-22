// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtNetwork/qhostaddress.h>
#include <QtNetwork/qnetworkinterface.h>

#include <functional>
#include <memory>

namespace QtComms {

struct SessionConfiguration {
    QHostAddress discoveryGroup = QHostAddress(QStringLiteral("239.255.78.84"));
    quint16 discoveryPort = 47800;
    quint16 updatePort = 47801;
    quint16 controlPort = 0;
    QNetworkInterface networkInterface;
    int multicastTtl = 1;
    QByteArray key;
    quint8 exercise = 1;
    quint16 site = 1;
    int maximumPeers = 64;
    bool observersAllowed = true;
    QString application;
    int beaconInterval = 1000;
    int sessionExpiry = 5000;
    int heartbeatInterval = 1000;
    int peerTimeout = 5000;
    int peerRemoveTimeout = 15000;
};

struct SessionInfo {
    quint32 id = 0;
    quint16 epoch = 0;
    QString name;
    QString hostName;
    QHostAddress hostAddress;
    quint16 controlPort = 0;
    QHostAddress updateGroup;
    quint16 updatePort = 0;
    quint8 exercise = 1;
    quint16 site = 1;
    int protocolVersion = 0;
    int minimumProtocolVersion = 0;
    QString application;
    int peerCount = 0;
    int maximumPeers = 0;
    bool keyRequired = false;
    bool observersAllowed = true;
    QCborMap attributes;

    bool isValid() const { return id != 0; }
};

struct Peer {
    enum class Role : quint8 { Participant, Observer };
    enum class State : quint8 { Active, Unresponsive };

    quint16 number = 0;
    QString name;
    Role role = Role::Participant;
    QHostAddress address;
    quint16 controlPort = 0;
    QCborMap attributes;
    State state = State::Active;
    int roundTripTime = -1;

    bool isValid() const { return number != 0; }
};

class SessionPrivate;

class Session : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Hosting, Joining, Joined, Rejected, Failed };
    Q_ENUM(State)
    enum class RejectReason {
        None,
        ProtocolTooOld,
        ProtocolTooNew,
        AuthenticationFailed,
        WorldMismatch,
        SessionFull,
        NameInUse,
        ObserversNotAllowed,
        MissingModels,
        PeersMissingModels,
        HostUnreachable,
        Other
    };
    Q_ENUM(RejectReason)

    struct Admission {
        bool accepted = true;
        RejectReason reason = RejectReason::None;
        QString text;
        QCborMap details;
        QCborMap acceptData;

        static Admission accept(const QCborMap &acceptData = QCborMap());
        static Admission reject(RejectReason reason, const QString &text, const QCborMap &details = QCborMap());
    };
    using AdmissionPolicy = std::function<Admission(const Peer &candidate)>;

    static constexpr int protocolVersion = 1;
    static constexpr int minimumProtocolVersion = 1;
    static constexpr int firstApplicationChannel = 16;

    explicit Session(QObject *parent = nullptr);
    ~Session() override;

    void setConfiguration(const SessionConfiguration &configuration);
    SessionConfiguration configuration() const;

    bool host(const QString &sessionName, const QString &peerName, const QCborMap &peerAttributes = QCborMap(),
              const QCborMap &sessionAttributes = QCborMap());
    bool join(const SessionInfo &session, const QString &peerName, Peer::Role role = Peer::Role::Participant,
              const QCborMap &peerAttributes = QCborMap());
    void leave();

    State state() const;
    bool isHost() const;
    quint16 localPeer() const;
    quint16 hostPeer() const;
    SessionInfo info() const;
    QList<Peer> peers() const;
    Peer peer(quint16 number) const;
    QString errorString() const;

    void setSessionAttributes(const QCborMap &attributes);
    QCborMap sessionAttributes() const;
    void setAdmissionPolicy(const AdmissionPolicy &policy);

    qint64 sessionTime() const;

    bool sendMessage(quint16 peer, int channel, const QCborMap &message);
    void broadcastMessage(int channel, const QCborMap &message);
    bool multicast(const QByteArray &datagram);

Q_SIGNALS:
    void stateChanged(QtComms::Session::State state);
    void joined(const QCborMap &acceptData);
    void rejected(QtComms::Session::RejectReason reason, const QString &text, const QCborMap &details);
    void joinRefused(const QtComms::Peer &candidate, QtComms::Session::RejectReason reason, const QString &text);
    void peerJoined(const QtComms::Peer &peer);
    void peerLeft(quint16 peer);
    void peerUnresponsive(quint16 peer);
    void peerRecovered(quint16 peer);
    void hostChanged(quint16 peer);
    void sessionAttributesChanged(const QCborMap &attributes);
    void messageReceived(quint16 peer, int channel, const QCborMap &message);
    void datagramReceived(const QByteArray &datagram, quint16 peer);

private:
    friend class SessionPrivate;
    std::unique_ptr<SessionPrivate> d;
};

} // namespace QtComms
