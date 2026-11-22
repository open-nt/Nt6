// MIT License - see LICENSE
//
// Nt Twin Network: entities shared between sessions on the loopback interface. Publishing and receiving as DIS,
// the dead-reckoning send decisions and heartbeats, staleness and removal, every model admission check, spoofed
// PDUs, late joiners and host migration.
#include <QtComms>
#include <QtCore>
#include <QtNetwork>
#include <QtTwin>
#include <QtTwinNetwork>

#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace QtTwin;
using QtComms::Session;

namespace {

quint16 g_basePort = 0;

template <typename F> bool waitFor(F done, int msec = 4000) {
    QElapsedTimer t;
    t.start();
    while (!done()) {
        if (t.elapsed() > msec)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    return true;
}

void pump(int msec) {
    waitFor([] { return false; }, msec);
}

QtComms::SessionConfiguration testConfiguration(quint16 offset) {
    QtComms::SessionConfiguration c;
    c.discoveryPort = static_cast<quint16>(g_basePort + offset);
    c.updatePort = static_cast<quint16>(g_basePort + offset + 1);
    c.networkInterface = QNetworkInterface::interfaceFromName(QStringLiteral("lo"));
    c.beaconInterval = 100;
    c.sessionExpiry = 600;
    c.heartbeatInterval = 50;
    c.peerTimeout = 400;
    c.peerRemoveTimeout = 900;
    c.exercise = 7;
    return c;
}

QtComms::SessionInfo discover(const QtComms::SessionConfiguration &config, const QString &name) {
    QtComms::SessionBrowser browser;
    browser.setConfiguration(config);
    browser.start();
    QtComms::SessionInfo found;
    waitFor([&] {
        found = browser.find(name);
        return found.isValid();
    });
    return found;
}

ModelRef model(const QString &id, int revision, EntityKind kind) {
    ModelRef m;
    m.id = id;
    m.revision = revision;
    m.kind = kind;
    m.disType = DisEntityType::forKind(kind);
    m.sha256 = QCryptographicHash::hash(id.toUtf8() + QByteArray::number(revision), QCryptographicHash::Sha256);
    return m;
}

const ModelRef kLandRover = model(QStringLiteral("land/wheeled/lr110"), 3, EntityKind::GroundVehicle);
const ModelRef kHeli = model(QStringLiteral("air/rotary/ec135"), 4, EntityKind::Aircraft);
const ModelRef kRib = model(QStringLiteral("sea/rib-7m"), 1, EntityKind::Boat);
const ModelRef kWalker = model(QStringLiteral("person/civilian-a"), 2, EntityKind::Person);

ModelCatalogue catalogueOf(const QList<ModelRef> &models) {
    ModelCatalogue c;
    for (const ModelRef &m : models)
        c.insert(m);
    return c;
}

// A peer: a session and its replicator, with fast thresholds so the tests run quickly.
struct Peer {
    Session session;
    Replicator replicator{&session};
    QList<DisEntityId> appeared, updated, stale, removed;
    QList<quint16> accepted;
    QList<QPair<quint16, QList<MissingModel>>> rejectedSpawns;
    QList<QPair<QString, QList<MissingModel>>> notices;
    QtComms::Session::RejectReason refusal = QtComms::Session::RejectReason::None;
    QString refusalText;
    QCborMap refusalDetails;

    Peer(const QtComms::SessionConfiguration &config, const QList<ModelRef> &models) {
        session.setConfiguration(config);
        replicator.setCatalogue(catalogueOf(models));
        replicator.setWorld(QStringLiteral("amesbury"), 3, QByteArray("world-hash"));
        for (EntityKind k : {EntityKind::Person, EntityKind::GroundVehicle, EntityKind::Aircraft, EntityKind::Drone,
                             EntityKind::Boat}) {
            DeadReckoningThresholds t = DeadReckoningThresholds::forKind(k);
            t.heartbeat = 300;
            t.minimumInterval = 20;
            replicator.setThresholds(k, t);
        }
        replicator.setRemoveTimeout(1500);
        QObject::connect(&replicator, &Replicator::entityAppeared,
                         [this](const DisEntityId &id) { appeared.append(id); });
        QObject::connect(&replicator, &Replicator::entityUpdated,
                         [this](const DisEntityId &id) { updated.append(id); });
        QObject::connect(&replicator, &Replicator::entityStale, [this](const DisEntityId &id) { stale.append(id); });
        QObject::connect(&replicator, &Replicator::entityRemoved,
                         [this](const DisEntityId &id) { removed.append(id); });
        QObject::connect(&replicator, &Replicator::spawnAccepted, [this](quint16 h) { accepted.append(h); });
        QObject::connect(&replicator, &Replicator::spawnRejected,
                         [this](quint16 h, const QList<MissingModel> &m) { rejectedSpawns.append({h, m}); });
        QObject::connect(&replicator, &Replicator::modelUpdateNeeded,
                         [this](const QList<MissingModel> &m, const QString &who) { notices.append({who, m}); });
        QObject::connect(&session, &Session::rejected,
                         [this](QtComms::Session::RejectReason r, const QString &text, const QCborMap &details) {
                             refusal = r;
                             refusalText = text;
                             refusalDetails = details;
                         });
    }
    bool joinAndWait(const QtComms::SessionInfo &info, const QString &name, const QList<ModelRef> &planned = {},
                     QtComms::Peer::Role role = QtComms::Peer::Role::Participant) {
        replicator.join(info, name, role, planned);
        waitFor([&] { return session.state() != Session::State::Joining; });
        return session.state() == Session::State::Joined;
    }
};

EntityState stateAt(const Session &session, double north, double speedNorth) {
    static const LocalFrame site(GeoPosition(51.1789, -1.8262, 100.0));
    EntityState s;
    s.time = session.sessionTime();
    s.position = site.toEcef(Vector3d(0.0, north, 0.0));
    s.setHeadingPitchRoll(HeadingPitchRoll(0.0, 0.0, 0.0));
    s.setLocalVelocity(Vector3d(0.0, speedNorth, 0.0));
    s.deadReckoning = speedNorth == 0.0 ? DeadReckoningAlgorithm::Static : DeadReckoningAlgorithm::Fpw;
    return s;
}

double distance(const EntityState &a, const EntityState &b) { return (a.position - b.position).length(); }

void testReplication() {
    const QtComms::SessionConfiguration config = testConfiguration(0);
    Peer alice(config, {kLandRover, kHeli, kRib, kWalker});
    Peer bob(config, {kLandRover, kHeli, kRib, kWalker});
    CHECK(alice.replicator.host(QStringLiteral("Replication"), QStringLiteral("alice")));
    const QtComms::SessionInfo info = discover(config, QStringLiteral("Replication"));
    CHECK(info.isValid() && info.exercise == 7);
    CHECK(info.attributes.value(QStringLiteral("world")).toMap().value(1).toString() == QStringLiteral("amesbury"));
    CHECK(bob.joinAndWait(info, QStringLiteral("bob")));

    // Alice drives north at 10 m/s.
    const quint16 car = alice.replicator.publish(kLandRover, QStringLiteral("ALPHA-1"),
                                                 stateAt(alice.session, 0.0, 10.0), DisForce::Friendly);
    CHECK(car == 1);
    CHECK(waitFor([&] { return alice.accepted.contains(car) && alice.replicator.isAccepted(car); }));
    const DisEntityId id = alice.replicator.entityId(car);
    CHECK(id == DisEntityId(1, 1, 1));
    CHECK(waitFor([&] { return bob.appeared.contains(id); }));
    const RemoteEntity seen = bob.replicator.remoteEntity(id);
    CHECK(seen.model == kLandRover && seen.marking == QStringLiteral("ALPHA-1") && seen.owner == 1);
    CHECK(seen.force == DisForce::Friendly && !seen.stale);
    CHECK(alice.replicator.sessionModels().size() == 1 && bob.replicator.sessionModels().size() == 1);

    // Steady motion: bob's prediction follows without new PDUs.
    QElapsedTimer drive;
    drive.start();
    const qint64 start = alice.session.sessionTime();
    bob.updated.clear();
    while (drive.elapsed() < 250) {
        const double t = static_cast<double>(alice.session.sessionTime() - start) / 1000.0;
        alice.replicator.update(car, stateAt(alice.session, 10.0 * t, 10.0));
        pump(10);
    }
    CHECK(bob.updated.size() <= 1); // perhaps a heartbeat, no threshold updates
    const double t = static_cast<double>(alice.session.sessionTime() - start) / 1000.0;
    const EntityState truth = stateAt(alice.session, 10.0 * t, 10.0);
    CHECK(distance(bob.replicator.remoteState(id, truth.time), truth) < 0.5);

    // A sharp change is sent at once.
    bob.updated.clear();
    alice.replicator.update(car, stateAt(alice.session, 10.0 * t + 5.0, 0.0));
    CHECK(waitFor([&] { return !bob.updated.isEmpty(); }, 1000));
    pump(200); // past the blend time
    CHECK(distance(bob.replicator.remoteState(id, bob.session.sessionTime()),
                   stateAt(alice.session, 10.0 * t + 5.0, 0.0)) < 0.2);

    // Parked: only heartbeats, every 300 ms here.
    bob.updated.clear();
    for (int i = 0; i < 100; ++i) {
        alice.replicator.update(car, stateAt(alice.session, 10.0 * t + 5.0, 0.0));
        pump(10);
    }
    CHECK(bob.updated.size() >= 2 && bob.updated.size() <= 5);
    // The application stops calling update(); the replicator keeps the heartbeat going.
    bob.updated.clear();
    pump(1000);
    CHECK(bob.updated.size() >= 2 && bob.stale.isEmpty());

    // An appearance change is sent at once.
    EntityState lit = stateAt(alice.session, 10.0 * t + 5.0, 0.0);
    lit.appearance = 0x10000000;
    bob.updated.clear();
    alice.replicator.update(car, lit);
    CHECK(waitFor([&] { return !bob.updated.isEmpty(); }, 1000));
    CHECK(bob.replicator.remoteEntity(id).lastState.appearance == 0x10000000);

    // Bob's helicopter, seen by alice.
    const quint16 heli = bob.replicator.publish(kHeli, QStringLiteral("HELI-9"), stateAt(bob.session, 100.0, 0.0));
    CHECK(waitFor([&] { return bob.accepted.contains(heli) && alice.appeared.contains(DisEntityId(1, 2, 1)); }));
    CHECK(alice.replicator.remoteEntities().size() == 1 && bob.replicator.remoteEntities().size() == 1);

    // A late joiner learns the entities from the host and sees them.
    Peer carol(config, {kLandRover, kHeli, kRib, kWalker});
    CHECK(carol.joinAndWait(info, QStringLiteral("carol")));
    CHECK(waitFor([&] { return carol.appeared.contains(id) && carol.appeared.contains(DisEntityId(1, 2, 1)); }, 2000));

    // Removing an entity removes it everywhere.
    bob.replicator.remove(heli);
    CHECK(waitFor(
        [&] { return alice.removed.contains(DisEntityId(1, 2, 1)) && carol.removed.contains(DisEntityId(1, 2, 1)); }));
    CHECK(alice.replicator.remoteEntities().isEmpty());

    // A peer that speaks for someone else's entity is ignored.
    DisEntityStatePdu forged;
    forged.exercise = 7;
    forged.entityId = id; // alice's car
    forged.state = stateAt(bob.session, 9999.0, 0.0);
    forged.timestamp = DisEntityStatePdu::timestampFromMsec(bob.session.sessionTime());
    carol.updated.clear();
    bob.session.multicast(forged.encode());
    pump(150);
    CHECK(distance(carol.replicator.remoteState(id, carol.session.sessionTime()), forged.state) > 1000.0);

    // An observer cannot publish.
    Peer dave(config, {kLandRover, kHeli, kRib, kWalker});
    CHECK(dave.joinAndWait(info, QStringLiteral("dave"), {}, QtComms::Peer::Role::Observer));
    CHECK(dave.replicator.publish(kRib, QStringLiteral("RIB"), stateAt(dave.session, 0.0, 0.0)) == 0);
    CHECK(waitFor([&] { return dave.appeared.contains(id); }, 2000));

    // The owner leaves: its entities go with it, and bob takes over as host.
    carol.removed.clear();
    alice.session.leave();
    CHECK(waitFor([&] { return carol.removed.contains(id) && bob.removed.contains(id); }, 3000));
    CHECK(waitFor([&] { return bob.session.isHost(); }, 2000)); // bob took over
}

void testStale() {
    const QtComms::SessionConfiguration config = testConfiguration(2);
    Peer alice(config, {kLandRover});
    auto bob = std::make_unique<Peer>(config, QList<ModelRef>{kLandRover});
    CHECK(alice.replicator.host(QStringLiteral("Stale"), QStringLiteral("alice")));
    CHECK(bob->joinAndWait(discover(config, QStringLiteral("Stale")), QStringLiteral("bob")));
    const quint16 car = bob->replicator.publish(kLandRover, QStringLiteral("B-1"), stateAt(bob->session, 0.0, 5.0));
    const DisEntityId id(1, 2, car);
    CHECK(waitFor([&] { return alice.appeared.contains(id); }));

    // Bob's session keeps its heartbeats, but its replicator is gone: no more PDUs.
    Replicator *silenced = &bob->replicator;
    silenced->blockSignals(true);
    QObject::disconnect(&bob->session, nullptr, silenced, nullptr);
    for (QTimer *timer : silenced->findChildren<QTimer *>())
        timer->stop();
    const qint64 lastHeard = alice.replicator.remoteEntity(id).lastReceived;
    CHECK(waitFor([&] { return alice.stale.contains(id); }, 2000));
    const qint64 staleAfter = alice.session.sessionTime() - lastHeard;
    CHECK(staleAfter >= 700 && staleAfter < 1100); // 2.4 heartbeats of 300 ms
    CHECK(alice.replicator.remoteEntity(id).stale);
    // A stale entity is frozen, not extrapolated further.
    const EntityState a = alice.replicator.remoteState(id, alice.session.sessionTime());
    const EntityState b = alice.replicator.remoteState(id, alice.session.sessionTime() + 5000);
    CHECK(distance(a, b) < 1e-6);
    CHECK(waitFor([&] { return alice.removed.contains(id); }, 2000));
    CHECK(alice.replicator.remoteEntities().isEmpty());
    CHECK(alice.session.peers().size() == 2); // the peer itself is still there
}

void testModelAdmission() {
    const QtComms::SessionConfiguration config = testConfiguration(4);
    Peer alice(config, {kLandRover, kHeli, kWalker});
    alice.replicator.setUpdateUrl(QStringLiteral("https://models.example/update"));
    CHECK(alice.replicator.host(QStringLiteral("Models"), QStringLiteral("alice")));
    const quint16 heli = alice.replicator.publish(kHeli, QStringLiteral("H-1"), stateAt(alice.session, 0.0, 0.0));
    CHECK(waitFor([&] { return alice.accepted.contains(heli); }));
    const QtComms::SessionInfo info = discover(config, QStringLiteral("Models"));

    // Check 5: bob has an older helicopter and no RIB; the session uses the helicopter.
    ModelRef oldHeli = kHeli;
    oldHeli.revision = 3;
    Peer bob(config, {kLandRover, oldHeli});
    CHECK(!bob.joinAndWait(info, QStringLiteral("bob")));
    CHECK(bob.refusal == QtComms::Session::RejectReason::MissingModels);
    const QList<MissingModel> missing = Replicator::missingModels(bob.refusalDetails);
    CHECK(missing.size() == 1 && missing.value(0).model.id == kHeli.id && missing.value(0).model.revision == 4);
    CHECK(missing.value(0).installedRevision == 3);
    const QString text = Replicator::describeRejection(bob.refusal, bob.refusalText, bob.refusalDetails);
    CHECK(text.startsWith(QStringLiteral("You need an update")));
    CHECK(text.contains(QStringLiteral("air/rotary/ec135 needs revision 4, you have 3")));
    CHECK(text.contains(QStringLiteral("https://models.example/update")));

    Peer nothing(config, {kLandRover});
    CHECK(!nothing.joinAndWait(info, QStringLiteral("nothing")));
    CHECK(Replicator::describeRejection(nothing.refusal, nothing.refusalText, nothing.refusalDetails)
              .contains(QStringLiteral("air/rotary/ec135 revision 4, not installed")));

    // A newer revision is fine.
    ModelRef newHeli = kHeli;
    newHeli.revision = 5;
    Peer carol(config, {kLandRover, newHeli, kRib});
    // Check 6: carol brings a RIB, which alice cannot render.
    CHECK(!carol.joinAndWait(info, QStringLiteral("carol"), {kRib}));
    CHECK(carol.refusal == QtComms::Session::RejectReason::PeersMissingModels);
    const QList<MissingModel> lacking = Replicator::missingModels(carol.refusalDetails);
    CHECK(lacking.size() == 1 && lacking.value(0).model.id == kRib.id &&
          lacking.value(0).peers == QStringList{QStringLiteral("alice")});
    CHECK(Replicator::describeRejection(carol.refusal, carol.refusalText, carol.refusalDetails)
              .contains(QStringLiteral("sea/rib-7m r1: alice")));
    CHECK(alice.notices.size() == 1 && alice.notices.value(0).first == QStringLiteral("carol"));
    // Without the RIB, carol joins.
    Peer carol2(config, {kLandRover, newHeli, kRib});
    CHECK(carol2.joinAndWait(info, QStringLiteral("carol"), {kLandRover}));

    // Mid-session, carol's RIB is refused, and alice is told.
    alice.notices.clear();
    const quint16 rib = carol2.replicator.publish(kRib, QStringLiteral("RIB-1"), stateAt(carol2.session, 0.0, 0.0));
    CHECK(rib != 0);
    CHECK(waitFor([&] { return !carol2.rejectedSpawns.isEmpty(); }));
    CHECK(carol2.rejectedSpawns.value(0).first == rib);
    CHECK(carol2.rejectedSpawns.value(0).second.value(0).peers == QStringList{QStringLiteral("alice")});
    CHECK(!carol2.replicator.isAccepted(rib));
    CHECK(waitFor([&] { return !alice.notices.isEmpty(); }));
    // Her Land Rover is fine.
    const quint16 car = carol2.replicator.publish(kLandRover, QStringLiteral("LR"), stateAt(carol2.session, 0.0, 0.0));
    CHECK(waitFor([&] { return carol2.accepted.contains(car) && alice.appeared.contains(DisEntityId(1, 2, car)); }));

    // Strict mode compares hashes.
    const QtComms::SessionConfiguration strictConfig = testConfiguration(6);
    Peer host(strictConfig, {kLandRover});
    host.replicator.setStrictModels(true);
    CHECK(host.replicator.host(QStringLiteral("Strict"), QStringLiteral("host")));
    host.replicator.publish(kLandRover, QStringLiteral("LR"), stateAt(host.session, 0.0, 0.0));
    CHECK(waitFor([&] { return !host.accepted.isEmpty(); }));
    ModelRef rehashed = kLandRover;
    rehashed.revision = 9;
    rehashed.sha256 = QByteArray(32, 'z');
    Peer other(strictConfig, {rehashed});
    CHECK(!other.joinAndWait(discover(strictConfig, QStringLiteral("Strict")), QStringLiteral("other")));
    CHECK(other.refusal == QtComms::Session::RejectReason::MissingModels);

    // Another world is refused.
    Peer elsewhere(config, {kLandRover, kHeli});
    elsewhere.replicator.setWorld(QStringLiteral("studland"), 1);
    CHECK(!elsewhere.joinAndWait(info, QStringLiteral("elsewhere")));
    CHECK(elsewhere.refusal == QtComms::Session::RejectReason::WorldMismatch);
    CHECK(elsewhere.refusalText.contains(QStringLiteral("studland")));
}

void testMigration() {
    const QtComms::SessionConfiguration config = testConfiguration(8);
    Peer alice(config, {kLandRover, kRib});
    Peer bob(config, {kLandRover, kRib});
    Peer carol(config, {kLandRover, kRib});
    CHECK(alice.replicator.host(QStringLiteral("Migration"), QStringLiteral("alice")));
    const QtComms::SessionInfo info = discover(config, QStringLiteral("Migration"));
    CHECK(bob.joinAndWait(info, QStringLiteral("bob")));
    CHECK(carol.joinAndWait(info, QStringLiteral("carol")));
    const quint16 car = carol.replicator.publish(kLandRover, QStringLiteral("C-1"), stateAt(carol.session, 0.0, 0.0));
    CHECK(waitFor([&] { return bob.appeared.contains(DisEntityId(1, 3, car)); }));

    alice.session.leave();
    CHECK(waitFor([&] { return bob.session.isHost() && carol.session.hostPeer() == 2; }));
    // Bob, the new host, knows the entities and validates new spawns.
    CHECK(bob.replicator.sessionModels().size() == 1);
    const quint16 rib = carol.replicator.publish(kRib, QStringLiteral("R-1"), stateAt(carol.session, 50.0, 0.0));
    CHECK(waitFor([&] { return carol.accepted.contains(rib) && bob.appeared.contains(DisEntityId(1, 3, rib)); }));
    // And still admits on models: a peer without the RIB is refused.
    Peer dave(config, {kLandRover});
    CHECK(!dave.joinAndWait(discover(config, QStringLiteral("Migration")), QStringLiteral("dave")));
    CHECK(dave.refusal == QtComms::Session::RejectReason::MissingModels);
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (!QNetworkInterface::interfaceFromName(QStringLiteral("lo")).isValid()) {
        std::printf("SKIP: no loopback interface\n");
        return 0;
    }
    g_basePort = static_cast<quint16>(20000 + QRandomGenerator::global()->bounded(40000) / 20 * 20);
    testReplication();
    testStale();
    testModelAdmission();
    testMigration();
    if (g_failures == 0)
        std::printf("All Nt Twin Network tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
