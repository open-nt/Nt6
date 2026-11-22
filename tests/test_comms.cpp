// MIT License - see LICENSE
//
// Nt Comms: sessions on the loopback interface. Discovery, joining and every refusal, the reliable channel under
// loss and reordering, heartbeats, host migration after a crash (a child process hosts and is killed), the
// session key, and GeoChat chat with receipts, backlog, system messages and the TAK bridge.
#include <QtComms>
#include <QtCore>
#include <QtNetwork>

#include "qcommsprotocol_p.h"
#include "qcommsreliable_p.h"

#include <algorithm>
#include <cstdio>
#include <random>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace QtComms;

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

// Short intervals on the loopback interface, on ports of this test run.
SessionConfiguration testConfiguration(quint16 offset = 0) {
    SessionConfiguration c;
    c.discoveryPort = static_cast<quint16>(g_basePort + offset);
    c.updatePort = static_cast<quint16>(g_basePort + offset + 1);
    c.networkInterface = QNetworkInterface::interfaceFromName(QStringLiteral("lo"));
    c.beaconInterval = 100;
    c.sessionExpiry = 600;
    c.heartbeatInterval = 50;
    c.peerTimeout = 400;
    c.peerRemoveTimeout = 900;
    c.application = QStringLiteral("nt_comms_tests");
    return c;
}

SessionInfo discover(const SessionConfiguration &config, const QString &name) {
    SessionBrowser browser;
    browser.setConfiguration(config);
    if (!browser.start())
        return SessionInfo();
    SessionInfo found;
    waitFor([&] {
        found = browser.find(name);
        return found.isValid();
    });
    return found;
}

QCborMap attributes(const QString &world) {
    QCborMap m;
    m.insert(1, world);
    return m;
}

void testProtocol() {
    Protocol::Header h;
    h.type = Protocol::Type::Heartbeat;
    h.flags = Protocol::AckValid;
    h.session = 0xdeadbeef;
    h.peer = 7;
    h.epoch = 3;
    const QByteArray bytes = Protocol::encodeHeader(h) + QByteArray("payload");
    CHECK(bytes.size() == Protocol::kHeaderSize + 7 && bytes.startsWith("NTSS"));
    Protocol::Header back;
    CHECK(Protocol::decodeHeader(bytes, &back));
    CHECK(back.type == h.type && back.flags == h.flags && back.session == h.session && back.peer == 7 &&
          back.epoch == 3);
    CHECK(!Protocol::decodeHeader(QByteArray("NTSS"), &back));
    QByteArray badVersion = bytes;
    badVersion[4] = 9;
    CHECK(!Protocol::decodeHeader(badVersion, &back));

    const QByteArray key = Protocol::deriveKey("secret", 0xdeadbeef);
    CHECK(key.size() == 32 && key != Protocol::deriveKey("secret", 1) && Protocol::deriveKey("", 1).isEmpty());
    QByteArray signedBytes = Protocol::sign(key, bytes);
    CHECK(signedBytes.size() == bytes.size() + 16);
    QByteArray tampered = signedBytes;
    tampered[20] = 'X';
    CHECK(!Protocol::verify(key, &tampered));
    CHECK(Protocol::verify(key, &signedBytes) && signedBytes == bytes);
    QByteArray unsignedBytes = bytes;
    CHECK(Protocol::verify(QByteArray(), &unsignedBytes) && unsignedBytes == bytes);
    QByteArray tiny("abc");
    CHECK(!Protocol::verify(key, &tiny));

    SessionInfo info;
    info.id = 42;
    info.epoch = 2;
    info.name = QStringLiteral("Amesbury exercise");
    info.hostName = QStringLiteral("alice");
    info.updateGroup = QHostAddress(QStringLiteral("239.255.78.170"));
    info.updatePort = 47801;
    info.controlPort = 50000;
    info.exercise = 9;
    info.keyRequired = true;
    info.observersAllowed = false;
    info.attributes = attributes(QStringLiteral("amesbury"));
    const SessionInfo decoded = Protocol::decodeBeacon(Protocol::encodeBeacon(info));
    CHECK(decoded.id == 42 && decoded.epoch == 2 && decoded.name == info.name && decoded.hostName == info.hostName);
    CHECK(decoded.updateGroup == info.updateGroup && decoded.updatePort == 47801 && decoded.controlPort == 50000);
    CHECK(decoded.exercise == 9 && decoded.keyRequired && !decoded.observersAllowed);
    CHECK(decoded.attributes.value(1).toString() == QStringLiteral("amesbury"));
    info.updateGroup = QHostAddress(QStringLiteral("10.0.0.1"));
    CHECK(!Protocol::decodeBeacon(Protocol::encodeBeacon(info)).isValid()); // not a multicast group

    QCborMap map;
    CHECK(Protocol::decodeCbor(Protocol::encodeCbor(attributes(QStringLiteral("x"))), &map));
    CHECK(!Protocol::decodeCbor(QByteArray("\xff\x00", 2), &map));
}

void testReliableChannel() {
    // Two channels joined by a network that drops 20% of packets, duplicates some and reorders them all.
    ReliableChannel a, b;
    std::mt19937 random(1234);
    struct Packet {
        bool toB;
        bool isAck;
        ReliableChannel::Fragment fragment;
        quint16 ack;
        quint32 bits;
    };
    QList<Packet> wire;
    QList<QByteArray> received;
    qint64 now = 0;
    const auto senderTo = [&](bool toB) {
        return [&wire, toB](const ReliableChannel::Fragment &f) { wire.append(Packet{toB, false, f, 0, 0}); };
    };
    QList<QByteArray> sent;
    for (int i = 0; i < 200; ++i) {
        QByteArray m = QByteArray::number(i) + ':';
        if (i % 37 == 0)
            m += QByteArray(10000 + i, static_cast<char>('a' + i % 26)); // needs ten fragments
        sent.append(m);
        CHECK(a.send(m, now, senderTo(true)));
    }
    CHECK(!a.send(QByteArray(65 * 1024, 'x'), now, senderTo(true)));
    for (int step = 0; step < 4000 && received.size() < sent.size(); ++step) {
        now += 5;
        std::shuffle(wire.begin(), wire.end(), random);
        QList<Packet> deliver;
        deliver.swap(wire);
        for (const Packet &p : std::as_const(deliver)) {
            if (random() % 5 == 0)
                continue; // lost
            if (random() % 20 == 0)
                wire.append(p); // duplicated, arrives later
            if (p.toB) {
                received.append(b.receive(p.fragment));
                wire.append(Packet{false, true, {}, b.ack(), b.ackBits()});
            } else if (p.isAck) {
                a.acknowledge(p.ack, p.bits, now);
            }
        }
        a.resend(now, senderTo(true));
    }
    CHECK(received == sent);
    CHECK(!a.failed());
    for (int i = 0; i < 100 && !a.idle(); ++i) {
        now += 50;
        a.acknowledge(b.ack(), b.ackBits(), now);
        a.resend(now, senderTo(true));
    }
    CHECK(a.idle());
    CHECK(a.roundTripTime() >= 0);

    // A peer that never answers makes the channel fail after ten sends.
    ReliableChannel lonely;
    int sends = 0;
    const auto count = [&sends](const ReliableChannel::Fragment &) { ++sends; };
    lonely.send("hello", 0, count);
    for (qint64 t = 0; t < 60000 && !lonely.failed(); t += 10)
        lonely.resend(t, count);
    CHECK(lonely.failed() && sends == ReliableChannel::kMaxSends);
    CHECK(!lonely.send("again", 0, count));

    // Sequence numbers wrap.
    CHECK(ReliableChannel::difference(2, 65534) == 4 && ReliableChannel::difference(65534, 2) == -4);
}

void testDiscovery() {
    const SessionConfiguration config = testConfiguration();
    SessionBrowser browser;
    browser.setConfiguration(config);
    CHECK(browser.start() && browser.isActive());
    QList<quint32> lost;
    QObject::connect(&browser, &SessionBrowser::sessionLost, [&lost](quint32 id) { lost.append(id); });

    Session host;
    host.setConfiguration(config);
    QCborMap sessionAttributes;
    sessionAttributes.insert(QStringLiteral("world"), QStringLiteral("amesbury"));
    CHECK(host.host(QStringLiteral("Discovery test"), QStringLiteral("alice"), QCborMap(), sessionAttributes));
    CHECK(host.state() == Session::State::Hosting && host.isHost() && host.localPeer() == 1);
    CHECK(waitFor([&] { return browser.sessions().size() == 1; }));
    const SessionInfo found = browser.find(QStringLiteral("Discovery test"));
    CHECK(found.isValid() && found.id == host.info().id);
    CHECK(browser.find(QString::number(found.id)).id == found.id);
    CHECK(browser.find(QStringLiteral("0x") + QString::number(found.id, 16)).id == found.id);
    CHECK(found.hostName == QStringLiteral("alice") && found.peerCount == 1 && found.maximumPeers == 64);
    CHECK(found.application == QStringLiteral("nt_comms_tests") && found.protocolVersion == Session::protocolVersion);
    CHECK(found.updateGroup.isMulticast() && found.updatePort == config.updatePort && !found.keyRequired);
    CHECK(found.attributes.value(QStringLiteral("world")).toString() == QStringLiteral("amesbury"));

    // A browser started later finds the session at once, by asking.
    SessionBrowser late;
    late.setConfiguration(config);
    CHECK(late.start());
    CHECK(waitFor([&] { return !late.sessions().isEmpty(); }, 1000));

    host.leave();
    CHECK(host.state() == Session::State::Idle);
    CHECK(waitFor([&] { return lost.contains(found.id); }, 3000));
    CHECK(browser.sessions().isEmpty());
}

void testJoin() {
    const SessionConfiguration config = testConfiguration(2);
    Session host, bob, carol;
    for (Session *s : {&host, &bob, &carol})
        s->setConfiguration(config);
    CHECK(host.host(QStringLiteral("Join test"), QStringLiteral("alice"), attributes(QStringLiteral("alice-attrs"))));
    const SessionInfo info = discover(config, QStringLiteral("Join test"));
    CHECK(info.isValid());

    QCborMap bobAccept;
    QObject::connect(&bob, &Session::joined, [&bobAccept](const QCborMap &m) { bobAccept = m; });
    host.setAdmissionPolicy([](const Peer &candidate) {
        QCborMap data;
        data.insert(1, QStringLiteral("welcome ") + candidate.name);
        return Session::Admission::accept(data);
    });
    QList<Peer> hostSaw;
    QObject::connect(&host, &Session::peerJoined, [&hostSaw](const Peer &p) { hostSaw.append(p); });
    CHECK(bob.join(info, QStringLiteral("bob"), Peer::Role::Participant, attributes(QStringLiteral("bob-attrs"))));
    CHECK(bob.state() == Session::State::Joining);
    CHECK(waitFor([&] { return bob.state() == Session::State::Joined; }));
    CHECK(bobAccept.value(1).toString() == QStringLiteral("welcome bob"));
    CHECK(bob.localPeer() == 2 && bob.hostPeer() == 1 && !bob.isHost());
    CHECK(hostSaw.size() == 1 && hostSaw.at(0).name == QStringLiteral("bob"));
    CHECK(hostSaw.at(0).attributes.value(1).toString() == QStringLiteral("bob-attrs"));

    QList<quint16> bobSawJoin;
    QObject::connect(&bob, &Session::peerJoined, [&bobSawJoin](const Peer &p) { bobSawJoin.append(p.number); });
    CHECK(carol.join(info, QStringLiteral("carol"), Peer::Role::Observer));
    CHECK(waitFor([&] { return carol.state() == Session::State::Joined && bobSawJoin.contains(3); }));
    CHECK(carol.localPeer() == 3);
    for (Session *s : {&host, &bob, &carol}) {
        const QList<Peer> peers = s->peers();
        CHECK(peers.size() == 3);
        if (peers.size() == 3) {
            CHECK(peers.at(0).name == QStringLiteral("alice") && peers.at(1).name == QStringLiteral("bob"));
            CHECK(peers.at(2).name == QStringLiteral("carol") && peers.at(2).role == Peer::Role::Observer);
            CHECK(peers.at(0).attributes.value(1).toString() == QStringLiteral("alice-attrs"));
        }
    }
    CHECK(host.info().peerCount == 3);

    // The session clock agrees.
    pump(300);
    CHECK(std::llabs(bob.sessionTime() - host.sessionTime()) < 30);
    CHECK(std::llabs(carol.sessionTime() - host.sessionTime()) < 30);

    // Reliable messages, directly between joiners too.
    QList<QPair<quint16, QString>> carolGot;
    QObject::connect(&carol, &Session::messageReceived, [&carolGot](quint16 peer, int channel, const QCborMap &m) {
        if (channel == Session::firstApplicationChannel + 1)
            carolGot.append({peer, m.value(1).toString()});
    });
    QCborMap hello;
    hello.insert(1, QStringLiteral("hello carol"));
    CHECK(bob.sendMessage(3, Session::firstApplicationChannel + 1, hello));
    CHECK(!bob.sendMessage(99, Session::firstApplicationChannel + 1, hello));
    QCborMap big;
    big.insert(1, QString(20000, QLatin1Char('z')));
    host.broadcastMessage(Session::firstApplicationChannel + 1, big);
    CHECK(waitFor([&] { return carolGot.size() == 2; }));
    if (carolGot.size() == 2) {
        CHECK(carolGot.at(0) == qMakePair(quint16(2), QStringLiteral("hello carol")));
        CHECK(carolGot.at(1).first == 1 && carolGot.at(1).second.size() == 20000);
    }

    // Multicast datagrams, with the sender's peer number; the sender does not get its own.
    QList<QPair<quint16, QByteArray>> hostDatagrams, bobDatagrams;
    QObject::connect(&host, &Session::datagramReceived,
                     [&hostDatagrams](const QByteArray &d, quint16 p) { hostDatagrams.append({p, d}); });
    QObject::connect(&bob, &Session::datagramReceived,
                     [&bobDatagrams](const QByteArray &d, quint16 p) { bobDatagrams.append({p, d}); });
    CHECK(bob.multicast(QByteArray("\x07\x01\x01pdu-from-bob")));
    CHECK(!bob.multicast(QByteArray("NTSS-not-allowed")));
    CHECK(waitFor([&] { return !hostDatagrams.isEmpty(); }));
    pump(100);
    CHECK(hostDatagrams.size() == 1 && hostDatagrams.at(0).first == 2);
    CHECK(bobDatagrams.isEmpty());
    // A datagram from outside the session arrives with peer 0.
    QUdpSocket outsider;
    outsider.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    outsider.setMulticastInterface(QNetworkInterface::interfaceFromName(QStringLiteral("lo")));
    outsider.writeDatagram(QByteArray("\x07\x01\x01outside"), info.updateGroup, info.updatePort);
    CHECK(waitFor([&] { return hostDatagrams.size() == 2; }));
    CHECK(hostDatagrams.size() == 2 && hostDatagrams.at(1).first == 0);

    // Session attributes follow the host.
    QCborMap newAttributes = attributes(QStringLiteral("changed"));
    QCborMap carolAttributes;
    QObject::connect(&carol, &Session::sessionAttributesChanged,
                     [&carolAttributes](const QCborMap &m) { carolAttributes = m; });
    host.setSessionAttributes(newAttributes);
    CHECK(waitFor([&] { return !carolAttributes.isEmpty(); }));
    CHECK(carol.sessionAttributes().value(1).toString() == QStringLiteral("changed"));
    bob.setSessionAttributes(attributes(QStringLiteral("ignored"))); // only the host may
    CHECK(bob.sessionAttributes().value(1).toString() == QStringLiteral("changed"));

    // A peer that leaves is gone at once for everyone.
    QList<quint16> hostLeft;
    QObject::connect(&host, &Session::peerLeft, [&hostLeft](quint16 p) { hostLeft.append(p); });
    carol.leave();
    CHECK(waitFor([&] { return hostLeft.contains(3) && bob.peers().size() == 2; }, 2000));

    // The host leaves: bob takes over at once.
    QList<quint16> bobHost;
    QObject::connect(&bob, &Session::hostChanged, [&bobHost](quint16 p) { bobHost.append(p); });
    host.leave();
    CHECK(waitFor([&] { return bob.isHost(); }, 2000));
    CHECK(bob.state() == Session::State::Hosting && bob.hostPeer() == 2 && bobHost == QList<quint16>{2});
    CHECK(bob.peers().size() == 1 && bob.info().epoch == 1);
    // The session is still found, now hosted by bob.
    const SessionInfo again = discover(config, QStringLiteral("Join test"));
    CHECK(again.id == info.id && again.hostName == QStringLiteral("bob") && again.epoch == 1);
}

struct Refusal {
    Session::RejectReason reason = Session::RejectReason::None;
    QString text;
    QCborMap details;
};

Refusal tryJoin(const SessionInfo &info, const SessionConfiguration &config, const QString &name,
                Peer::Role role = Peer::Role::Participant, const QCborMap &attrs = QCborMap()) {
    Session s;
    s.setConfiguration(config);
    Refusal r;
    QObject::connect(
        &s, &Session::rejected,
        [&r](Session::RejectReason reason, const QString &text, const QCborMap &d) { r = Refusal{reason, text, d}; });
    s.join(info, name, role, attrs);
    waitFor([&] { return s.state() != Session::State::Joining; });
    if (s.state() == Session::State::Joined)
        r.reason = Session::RejectReason::None;
    else
        CHECK(s.state() == Session::State::Rejected || s.state() == Session::State::Failed);
    return r;
}

void testRefusals() {
    SessionConfiguration config = testConfiguration(4);
    config.maximumPeers = 3;
    config.observersAllowed = false;
    Session host, bob;
    host.setConfiguration(config);
    bob.setConfiguration(config);
    host.setAdmissionPolicy([](const Peer &candidate) {
        if (candidate.attributes.value(1).toString() != QStringLiteral("amesbury")) {
            QCborMap details;
            details.insert(1, QStringLiteral("amesbury"));
            return Session::Admission::reject(Session::RejectReason::WorldMismatch, QStringLiteral("wrong world"),
                                              details);
        }
        return Session::Admission::accept();
    });
    QList<Session::RejectReason> refusedOnHost;
    QObject::connect(
        &host, &Session::joinRefused,
        [&refusedOnHost](const Peer &, Session::RejectReason r, const QString &) { refusedOnHost.append(r); });
    CHECK(host.host(QStringLiteral("Refusal test"), QStringLiteral("alice")));
    const SessionInfo info = discover(config, QStringLiteral("Refusal test"));
    CHECK(info.isValid() && !info.observersAllowed);

    Refusal r =
        tryJoin(info, config, QStringLiteral("dave"), Peer::Role::Participant, attributes(QStringLiteral("studland")));
    CHECK(r.reason == Session::RejectReason::WorldMismatch && r.text == QStringLiteral("wrong world"));
    CHECK(r.details.value(1).toString() == QStringLiteral("amesbury"));
    r = tryJoin(info, config, QStringLiteral("ALICE"), Peer::Role::Participant, attributes(QStringLiteral("amesbury")));
    CHECK(r.reason == Session::RejectReason::NameInUse);
    r = tryJoin(info, config, QStringLiteral("eve"), Peer::Role::Observer, attributes(QStringLiteral("amesbury")));
    CHECK(r.reason == Session::RejectReason::ObserversNotAllowed);
    CHECK(bob.join(info, QStringLiteral("bob"), Peer::Role::Participant, attributes(QStringLiteral("amesbury"))));
    CHECK(waitFor([&] { return bob.state() == Session::State::Joined; }));
    r = tryJoin(info, config, QStringLiteral("frank"), Peer::Role::Participant, attributes(QStringLiteral("amesbury")));
    CHECK(r.reason == Session::RejectReason::None); // the third peer fits
    pump(50);
    CHECK(host.peers().size() == 2 || host.peers().size() == 3);
    CHECK(waitFor([&] { return host.peers().size() == 2; }, 2000)); // frank left when tryJoin returned
    CHECK(refusedOnHost.size() == 3);

    // A host that does not answer.
    SessionInfo nowhere = info;
    nowhere.controlPort = static_cast<quint16>(g_basePort + 40);
    r = tryJoin(nowhere, config, QStringLiteral("gina"));
    CHECK(r.reason == Session::RejectReason::HostUnreachable);
    CHECK(!Session().join(SessionInfo(), QStringLiteral("x")));
}

void testKey() {
    SessionConfiguration config = testConfiguration(6);
    config.key = "correct horse";
    Session host;
    host.setConfiguration(config);
    CHECK(host.host(QStringLiteral("Key test"), QStringLiteral("alice")));
    const SessionInfo info = discover(config, QStringLiteral("Key test"));
    CHECK(info.isValid() && info.keyRequired);

    SessionConfiguration wrong = config;
    wrong.key = "battery staple";
    Refusal r = tryJoin(info, wrong, QStringLiteral("mallory"));
    CHECK(r.reason == Session::RejectReason::AuthenticationFailed);
    SessionConfiguration none = config;
    none.key.clear();
    r = tryJoin(info, none, QStringLiteral("mallory"));
    CHECK(r.reason == Session::RejectReason::AuthenticationFailed);

    Session bob;
    bob.setConfiguration(config);
    QList<QByteArray> got;
    QObject::connect(&host, &Session::datagramReceived, [&got](const QByteArray &d, quint16) { got.append(d); });
    CHECK(bob.join(info, QStringLiteral("bob")));
    CHECK(waitFor([&] { return bob.state() == Session::State::Joined; }));
    CHECK(bob.multicast("signed pdu"));
    CHECK(waitFor([&] { return got.size() == 1; }));
    CHECK(got.value(0) == "signed pdu"); // the trailer is stripped
    // An unsigned datagram is dropped.
    QUdpSocket outsider;
    outsider.bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    outsider.setMulticastInterface(QNetworkInterface::interfaceFromName(QStringLiteral("lo")));
    outsider.writeDatagram(QByteArray("forged pdu with a trailer-sized tail"), info.updateGroup, info.updatePort);
    pump(200);
    CHECK(got.size() == 1);
}

// The child process: hosts a session, or joins one, and runs until it is killed.
int runChild(const QString &mode, const QString &name, quint16 offset) {
    const SessionConfiguration config = testConfiguration(offset);
    Session session;
    session.setConfiguration(config);
    if (mode == QStringLiteral("--child-host")) {
        if (!session.host(name, QStringLiteral("doomed")))
            return 2;
    } else {
        const SessionInfo info = discover(config, name);
        if (!info.isValid() || !session.join(info, QStringLiteral("silent")))
            return 3;
    }
    QTimer::singleShot(30000, qApp, &QCoreApplication::quit);
    return QCoreApplication::exec();
}

QStringList childArguments(const QString &mode, const QString &name, quint16 offset) {
    return {mode, name, QString::number(offset), QString::number(g_basePort)};
}

void testCrash() {
    const quint16 offset = 8;
    const SessionConfiguration config = testConfiguration(offset);
    QProcess child;
    child.start(QCoreApplication::applicationFilePath(),
                childArguments(QStringLiteral("--child-host"), QStringLiteral("Crash test"), offset));
    CHECK(child.waitForStarted(5000));
    const SessionInfo info = discover(config, QStringLiteral("Crash test"));
    CHECK(info.isValid());
    Session bob, carol;
    bob.setConfiguration(config);
    carol.setConfiguration(config);
    CHECK(bob.join(info, QStringLiteral("bob")));
    CHECK(waitFor([&] { return bob.state() == Session::State::Joined; }));
    CHECK(carol.join(info, QStringLiteral("carol")));
    CHECK(waitFor([&] { return carol.state() == Session::State::Joined && bob.peers().size() == 3; }));
    QList<quint16> carolLost, carolHost;
    QObject::connect(&carol, &Session::peerLeft, [&carolLost](quint16 p) { carolLost.append(p); });
    QObject::connect(&carol, &Session::hostChanged, [&carolHost](quint16 p) { carolHost.append(p); });

    child.kill(); // no Leave: the heartbeats just stop
    child.waitForFinished(5000);
    CHECK(waitFor([&] { return bob.isHost() && carol.hostPeer() == 2; }, 4000));
    CHECK(carolLost.contains(1) && carolHost == QList<quint16>{2});
    CHECK(bob.peers().size() == 2 && carol.peers().size() == 2);
    // The session goes on under bob: a new peer can join, and carol's clock still agrees.
    const SessionInfo again = discover(config, QStringLiteral("Crash test"));
    CHECK(again.id == info.id && again.hostName == QStringLiteral("bob"));
    Session dave;
    dave.setConfiguration(config);
    CHECK(dave.join(again, QStringLiteral("dave")));
    CHECK(waitFor([&] { return dave.state() == Session::State::Joined; }));
    CHECK(dave.localPeer() == 4);
    pump(300);
    CHECK(std::llabs(carol.sessionTime() - bob.sessionTime()) < 30);

    // A joined peer that stops without leaving is unresponsive, then removed.
    QList<quint16> bobLeft, bobUnresponsive;
    QObject::connect(&bob, &Session::peerLeft, [&bobLeft](quint16 p) { bobLeft.append(p); });
    QObject::connect(&bob, &Session::peerUnresponsive, [&bobUnresponsive](quint16 p) { bobUnresponsive.append(p); });
    QProcess silent;
    silent.start(QCoreApplication::applicationFilePath(),
                 childArguments(QStringLiteral("--child-join"), QStringLiteral("Crash test"), offset));
    CHECK(silent.waitForStarted(5000));
    CHECK(waitFor([&] { return bob.peers().size() == 4; }, 5000));
    silent.kill();
    silent.waitForFinished(5000);
    CHECK(waitFor([&] { return bobLeft.contains(5); }, 4000));
    CHECK(bobUnresponsive.contains(5));
    CHECK(waitFor([&] { return carol.peers().size() == 3 && dave.peers().size() == 3; }, 2000));
}

void testChat() {
    const SessionConfiguration config = testConfiguration(10);
    Session host, bob;
    host.setConfiguration(config);
    bob.setConfiguration(config);
    Chat hostChat(&host), bobChat(&bob);
    hostChat.setCallsign(QStringLiteral("ALPHA-1"));
    hostChat.setPosition(51.1789, -1.8262, 160.0);
    QList<ChatMessage> hostGot, bobGot;
    QObject::connect(&hostChat, &Chat::messageReceived, [&hostGot](const ChatMessage &m) { hostGot.append(m); });
    QObject::connect(&bobChat, &Chat::messageReceived, [&bobGot](const ChatMessage &m) { bobGot.append(m); });
    QList<QString> delivered, read;
    QObject::connect(&hostChat, &Chat::messageDelivered,
                     [&delivered](const QString &uid, quint16) { delivered.append(uid); });
    QObject::connect(&hostChat, &Chat::messageRead, [&read](const QString &uid, quint16) { read.append(uid); });

    CHECK(hostChat.send(QStringLiteral("not yet")).isEmpty()); // not in a session
    CHECK(host.host(QStringLiteral("Chat test"), QStringLiteral("alice")));
    const QString early = hostChat.send(QStringLiteral("Before bob"));
    CHECK(!early.isEmpty() && hostGot.size() == 1 && hostGot.at(0).peer == 1);
    CHECK(hostChat.uid() == QStringLiteral("NT-%1-1").arg(host.info().id, 0, 16));

    const SessionInfo info = discover(config, QStringLiteral("Chat test"));
    CHECK(bob.join(info, QStringLiteral("bob")));
    // The backlog, then the system message about bob.
    CHECK(waitFor([&] { return bobGot.size() >= 2; }));
    if (bobGot.size() >= 2) {
        CHECK(bobGot.at(0).text == QStringLiteral("Before bob") &&
              bobGot.at(0).senderCallsign == QStringLiteral("ALPHA-1"));
        CHECK(bobGot.at(0).hasPosition && std::fabs(bobGot.at(0).latitude - 51.1789) < 1e-6);
        CHECK(bobGot.at(1).system && bobGot.at(1).text == QStringLiteral("bob joined"));
    }
    CHECK(delivered.isEmpty()); // no receipts for the backlog

    const QString uid = hostChat.send(QStringLiteral("Crossing\nthe bridge\x01now"));
    CHECK(waitFor([&] { return bobGot.size() == 3 && delivered.contains(uid); }));
    const ChatMessage m = bobGot.value(2);
    CHECK(m.uid == uid && m.text == QStringLiteral("Crossing the bridge now") && m.peer == 1 && !m.system);
    CHECK(m.room == ChatMessage::allChatRooms() && m.senderUid == hostChat.uid());
    bobChat.markRead(m);
    CHECK(waitFor([&] { return read.contains(uid); }));

    // Direct messages, and team rooms only for members.
    const QString direct = bobChat.sendDirect(1, QStringLiteral("just for alice"));
    CHECK(!direct.isEmpty() && bobChat.sendDirect(2, QStringLiteral("myself")).isEmpty());
    CHECK(waitFor([&] { return hostGot.size() >= 4; }));
    CHECK(std::any_of(hostGot.cbegin(), hostGot.cend(), [&](const ChatMessage &x) {
        return x.uid == direct && x.roomId == hostChat.uid() && x.room == QStringLiteral("alice");
    }));
    const qsizetype before = bobGot.size();
    hostChat.send(QStringLiteral("blue team only"), QStringLiteral("Blue"));
    pump(200);
    CHECK(bobGot.size() == before + 0); // bob is not in Blue
    bobChat.setRooms({QStringLiteral("Blue")});
    CHECK(bobChat.rooms() == QStringList{QStringLiteral("Blue")});
    hostChat.send(QStringLiteral("blue again"), QStringLiteral("Blue"));
    CHECK(waitFor([&] { return bobGot.size() == before + 1; }));

    // A flood is cut at five messages a second per sender.
    pump(1100); // let the earlier messages leave the one-second window
    const qsizetype beforeFlood = bobGot.size();
    for (int i = 0; i < 8; ++i)
        hostChat.send(QStringLiteral("flood %1").arg(i));
    pump(300);
    CHECK(bobGot.size() - beforeFlood == 5);

    // Long text is cut to 1000 bytes.
    hostChat.send(QString(1500, QChar(0x00e9))); // two bytes each in UTF-8
    pump(1100);                                  // keeps the heartbeats going, unlike sleeping
    hostChat.send(QString(1500, QChar(0x00e9)));
    CHECK(waitFor([&] { return !bobGot.isEmpty() && bobGot.last().text.size() == 500; }));
    CHECK(hostChat.history().size() >= 10);

    // Refusals and departures are announced.
    host.setAdmissionPolicy([](const Peer &) {
        return Session::Admission::reject(Session::RejectReason::MissingModels, QStringLiteral("models missing"));
    });
    tryJoin(info, config, QStringLiteral("zed"));
    CHECK(waitFor([&] {
        return !bobGot.isEmpty() && bobGot.last().text == QStringLiteral("zed could not join: models missing");
    }));
    bob.leave();
    CHECK(waitFor([&] { return !hostGot.isEmpty() && hostGot.last().text == QStringLiteral("bob left"); }));
}

void testGeoChat() {
    ChatMessage m;
    m.uid = QStringLiteral("GeoChat.NT-1a-2.All Chat Rooms.5c1e");
    m.senderUid = QStringLiteral("NT-1a-2");
    m.senderCallsign = QStringLiteral("ALPHA-1");
    m.room = m.roomId = ChatMessage::allChatRooms();
    m.text = QStringLiteral("a < b & \"c\"");
    m.time = QDateTime::currentDateTimeUtc();
    m.latitude = -33.8568;
    m.longitude = 151.2153;
    m.height = 25.0;
    m.hasPosition = true;
    const QByteArray xml = m.toCot();
    CHECK(xml.contains("type=\"b-t-f\"") && xml.contains("<__chat") && xml.contains("senderCallsign=\"ALPHA-1\""));
    CHECK(xml.contains("<chatgrp uid0=\"NT-1a-2\"") && xml.contains("relation=\"p-p\""));
    bool ok = false;
    const ChatMessage back = ChatMessage::fromCot(xml, &ok);
    CHECK(ok && back.kind == ChatMessage::Kind::Message && back.uid == m.uid && back.text == m.text);
    CHECK(back.senderUid == m.senderUid && back.senderCallsign == m.senderCallsign && back.roomId == m.roomId);
    CHECK(back.hasPosition && std::fabs(back.longitude - 151.2153) < 1e-6 && std::fabs(back.height - 25.0) < 0.1);
    CHECK(std::llabs(back.time.msecsTo(m.time)) <= 1);

    // An event as ATAK writes it.
    const QByteArray atak = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<event version="2.0" uid="GeoChat.ANDROID-abc.All Chat Rooms.9f8e" type="b-t-f" how="h-g-i-g-o"
 time="2030-01-01T00:00:00.000Z" start="2030-01-01T00:00:00.000Z" stale="2099-01-02T00:00:00.000Z">
<point lat="0.0" lon="0.0" hae="9999999.0" ce="9999999.0" le="9999999.0"/>
<detail><__chat parent="RootContactGroup" groupOwner="false" messageId="9f8e" chatroom="All Chat Rooms"
 id="All Chat Rooms" senderCallsign="WOLF"><chatgrp uid0="ANDROID-abc" uid1="All Chat Rooms" id="All Chat Rooms"/>
</__chat><link uid="ANDROID-abc" type="a-f-G-U-C" relation="p-p"/><__serverdestination destinations="x"/>
<remarks source="BAO.F.ATAK.ANDROID-abc" to="All Chat Rooms" time="2030-01-01T00:00:00.000Z">Roger</remarks>
</detail></event>)";
    const ChatMessage fromAtak = ChatMessage::fromCot(atak, &ok);
    CHECK(ok && fromAtak.text == QStringLiteral("Roger") && fromAtak.senderCallsign == QStringLiteral("WOLF"));
    CHECK(fromAtak.senderUid == QStringLiteral("ANDROID-abc") && !fromAtak.hasPosition && !fromAtak.system);

    ChatMessage receipt = m;
    receipt.kind = ChatMessage::Kind::Read;
    const ChatMessage readBack = ChatMessage::fromCot(receipt.toCot(), &ok);
    CHECK(ok && readBack.kind == ChatMessage::Kind::Read && readBack.uid == m.uid);

    QByteArray stale = atak;
    stale.replace("2099-01-02", "2001-01-02");
    ChatMessage::fromCot(stale, &ok);
    CHECK(!ok);
    ChatMessage::fromCot(QByteArray("<!DOCTYPE x [<!ENTITY a \"b\">]>") + atak, &ok);
    CHECK(!ok);
    ChatMessage::fromCot(QByteArray(20000, ' ') + atak, &ok);
    CHECK(!ok);
    QByteArray position = atak;
    position.replace("type=\"b-t-f\"", "type=\"a-f-G-U-C\"");
    ChatMessage::fromCot(position, &ok);
    CHECK(!ok); // a position report, not chat
    ChatMessage::fromCot("<event", &ok);
    CHECK(!ok);
    QByteArray noRoom = atak;
    noRoom.replace("id=\"All Chat Rooms\"", "")
        .replace("to=\"All Chat Rooms\"", "")
        .replace("uid1=\"All Chat Rooms\"", "");
    ChatMessage::fromCot(noRoom, &ok);
    CHECK(!ok);
}

void testTakBridge() {
    const SessionConfiguration config = testConfiguration(12);
    const QHostAddress takGroup(QStringLiteral("239.255.78.99"));
    const auto takPort = static_cast<quint16>(g_basePort + 14);
    Session host, bob;
    host.setConfiguration(config);
    bob.setConfiguration(config);
    Chat hostChat(&host), bobChat(&bob);
    hostChat.setTakEndpoint(takGroup, takPort);
    CHECK(hostChat.setTakBridgeEnabled(true) && hostChat.isTakBridgeEnabled());
    QList<ChatMessage> hostGot, bobGot;
    QObject::connect(&hostChat, &Chat::messageReceived, [&hostGot](const ChatMessage &m) { hostGot.append(m); });
    QObject::connect(&bobChat, &Chat::messageReceived, [&bobGot](const ChatMessage &m) { bobGot.append(m); });
    CHECK(host.host(QStringLiteral("TAK test"), QStringLiteral("alice")));
    CHECK(bob.join(discover(config, QStringLiteral("TAK test")), QStringLiteral("bob")));
    CHECK(waitFor([&] { return bob.state() == Session::State::Joined; }));

    // A TAK device on the network.
    const QNetworkInterface lo = QNetworkInterface::interfaceFromName(QStringLiteral("lo"));
    QUdpSocket device;
    CHECK(device.bind(QHostAddress(QHostAddress::AnyIPv4), takPort,
                      QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint));
    CHECK(device.joinMulticastGroup(takGroup, lo));
    device.setMulticastInterface(lo);
    QList<QByteArray> deviceGot;
    QObject::connect(&device, &QUdpSocket::readyRead, [&] {
        while (device.hasPendingDatagrams())
            deviceGot.append(device.receiveDatagram().data());
    });

    // Bob's message reaches the TAK device through the host.
    bobChat.send(QStringLiteral("hello TAK"));
    CHECK(waitFor([&] {
        return std::any_of(deviceGot.cbegin(), deviceGot.cend(),
                           [](const QByteArray &d) { return d.contains("hello TAK"); });
    }));

    // The TAK device's message reaches the host and, through it, bob. Protobuf traffic is ignored.
    device.writeDatagram(QByteArray("\xbf\x01\xbf\x0a\x00", 5), takGroup, takPort);
    ChatMessage fromDevice;
    fromDevice.uid = QStringLiteral("GeoChat.ANDROID-1.All Chat Rooms.1");
    fromDevice.senderUid = QStringLiteral("ANDROID-1");
    fromDevice.senderCallsign = QStringLiteral("WOLF");
    fromDevice.room = fromDevice.roomId = ChatMessage::allChatRooms();
    fromDevice.text = QStringLiteral("Roger from TAK");
    fromDevice.time = QDateTime::currentDateTimeUtc();
    device.writeDatagram(fromDevice.toCot(), takGroup, takPort);
    device.writeDatagram(fromDevice.toCot(), takGroup, takPort); // a repeat is shown once
    CHECK(waitFor([&] { return !bobGot.isEmpty() && bobGot.last().text == QStringLiteral("Roger from TAK"); }));
    pump(200);
    const auto count = std::count_if(hostGot.cbegin(), hostGot.cend(),
                                     [](const ChatMessage &x) { return x.text == QStringLiteral("Roger from TAK"); });
    CHECK(count == 1);
    CHECK(bobGot.last().fromTak && bobGot.last().peer == 0 && bobGot.last().senderCallsign == QStringLiteral("WOLF"));
    CHECK(hostChat.setTakBridgeEnabled(false) && !hostChat.isTakBridgeEnabled());
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() == 5 && args.at(1).startsWith(QStringLiteral("--child-"))) {
        g_basePort = static_cast<quint16>(args.at(4).toUInt());
        return runChild(args.at(1), args.at(2), static_cast<quint16>(args.at(3).toUInt()));
    }
    if (!QNetworkInterface::interfaceFromName(QStringLiteral("lo")).isValid()) {
        std::printf("SKIP: no loopback interface\n");
        return 0;
    }
    g_basePort = static_cast<quint16>(20000 + QRandomGenerator::global()->bounded(40000) / 20 * 20);
    testProtocol();
    testReliableChannel();
    testDiscovery();
    testJoin();
    testRefusals();
    testKey();
    testCrash();
    testChat();
    testGeoChat();
    testTakBridge();
    if (g_failures == 0)
        std::printf("All Nt Comms tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
