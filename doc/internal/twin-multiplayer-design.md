# Nt Twin multiplayer design

A design for shared sessions in Nt Twin. Several machines join one session on a LAN, each publishes the
entities it controls (people, ground vehicles, aircraft, drones and boats) and sees everyone else's on the
map, and the participants can chat by text. ntsim is the first client. No code has been written for it yet.

This file lives in `doc/internal/` and is Markdown, so ndoc does not publish it.

Related documents:

- `digital-twin-survey.md`: module boundaries (Nt Twin headless on Nt Core, Nt-only twin modules).
- `large-outdoor-environments.md`: replication of object state as small operations, and the plan to
  add HLA and DIS gateways later.

## Decisions (2026-11-22)

The first four items were confirmed by the user on 2026-11-22. The others are defaults this design
assumes; change them here if they are wrong.

- **Confirmed: entity updates use DIS** (IEEE 1278.1-2012, "DIS 7"): the Entity State PDU, its
  dead reckoning and its heartbeat rules. We do not invent an entity format. See §1.
- **Confirmed (2026-11-22): text chat uses Cursor on Target GeoChat** (CoT `b-t-f` events), the
  open chat format of ATAK and TAK. Messages go over the session's reliable channel. An optional
  bridge connects them to TAK devices on the LAN. See §9.
- **Confirmed: TAK protocol version 0 (XML) only.** TAK protocol version 1 (protobuf) is not
  planned.
- **Confirmed: no encryption.** Sessions run on trusted LANs. The optional session key
  authenticates datagrams but does not hide them.
- **Discovery, admission and model checks use a small Nt session protocol.** DIS has no discovery,
  no join handshake and no reliable delivery, and these are what the requirements add.
- **Two multicast ports.** Discovery is multicast on 47800, and updates (DIS state and heartbeats)
  on 47801. Session control and chat are unicast to a control port each peer announces (any free
  port by default, or fixed with `SessionConfiguration::controlPort`). A shared 47801 cannot carry
  unicast, because the kernel gives a unicast datagram to only one of the sockets bound to it, which
  would break several peers on one machine.
- **Admission rule:** a peer is admitted only if every participant can render every entity in the
  session. If it cannot, the join is refused and the user is told which models to update.
- **A new Nt-only module, Nt Comms** (`Nt::Comms`, `include/QtComms`, namespace `QtComms`), holds
  discovery, sessions, the reliable channel and chat. It depends only on Nt Core and Nt Network, so it
  also builds for Zephyr. It knows nothing about twins.
- **Nt Twin stays free of I/O.** It gains the entity kinds, model references, the model catalogue,
  the DIS record codec and dead reckoning: pure functions that tests can check without sockets.
- **A thin glue module, Nt Twin Network** (`Nt::TwinNetwork`), connects the two. To Nt Twin, the
  network is a source of entity state like MQTT, and the historian records it like any other.
- LAN first, IPv4 first, 64 peers and 2,000 entities per session as the design target.

## 1. Standard protocols considered

| Protocol | What it covers | Verdict |
| --- | --- | --- |
| **DIS 7** (IEEE 1278.1-2012) | Entity state over UDP broadcast or multicast, ECEF doubles, dead reckoning, 5 s heartbeat and 12 s timeout, entity types from SISO-REF-010 (lifeform, land, air, surface, subsurface), appearance bits, articulated parts | **Adopt for entity updates.** It matches the requirements almost field for field, and VBS, VR-Forces and other simulators speak it. Wireshark decodes it out of the box. |
| HLA (IEEE 1516, revised 2025) | Federation through a central RTI, publish and subscribe, time management | Too heavy as the native protocol: it needs an RTI, and its API is defined, not its wire format. Add it later as a gateway, as already planned. |
| DDS / RTPS (OMG) | Publish and subscribe with built-in multicast discovery (SPDP), quality of service | Strong discovery and reliability, but large to implement. It is already planned as Nt DDS for GVA. If Nt DDS lands first, the session layer could move onto it. |
| Cursor on Target / TAK | XML or protobuf position reports and GeoChat over UDP multicast; used by ATAK and WinTAK | **Adopt GeoChat for chat** (§9). Too coarse for entity state (1 Hz or slower, no dead reckoning), but a good bridge target later, so ATAK users can see the twin. |
| MAVLink, AIS, ADS-B | Live drone, ship and aircraft positions | Real-world sources, not a session protocol. Bind them to entities through the Nt Twin binding layer. |
| CIGI | Host to image generator | One host and one renderer; not multiplayer. |
| DNS-SD over mDNS (RFC 6762/6763) | Service discovery | The requirement is one port of our own, and mDNS needs port 5353, which avahi owns on Linux. We use our own beacon, but its fields could be published over DNS-SD later. |
| XMPP (MUC), IRC, Matrix | Chat | All need a server (or mDNS, for serverless XMPP). Too much for a LAN session, and no TAK interop. |

What DIS lacks, and the Nt session layer adds:

- discovery of sessions
- a join handshake with admission, including the model check
- reliable, ordered delivery for control messages and chat (DIS chat through Comment PDUs is
  unreliable)
- peer-level liveness, separate from entity heartbeats
- optional authentication of datagrams

The IEEE standard is not free, but the SISO enumerations (SISO-REF-010) are. Nt implements DIS
clean-room from the standard, and no code from Open-DIS or other implementations is used.

## 2. Architecture

```text
 ntsim / ntgltfview / headless tools
          │
 Nt Twin Network  ── Replicator: entities <-> DIS PDUs, admission policy for models
     │         │
  Nt Twin    Nt Comms ── SessionBrowser, Session, ReliableChannel, Chat
 (no I/O)      │
           Nt Network (QUdpSocket multicast) · Nt Core (QCborValue, QMessageAuthenticationCode)
```

- **Ownership.** Each entity has exactly one owner, the peer that created it. Only the owner sends
  its state. Other peers dead-reckon it between updates. This is the DIS model, and it needs no
  central server for state.
- **Host.** The peer that creates the session is its host. It sends the beacon, assigns peer
  numbers, keeps the roster and the session's set of models, and decides admission. It does not
  relay state: owners multicast directly.
- **Host migration.** The roster, with each peer's model catalogue, is replicated to every peer.
  If the host stops sending heartbeats for 5 s, the oldest remaining peer in the roster becomes host,
  increments the session epoch and starts sending the beacon. A peer ignores control messages from
  an older epoch.

## 3. Ports and addressing

| Use | Transport | Default |
| --- | --- | --- |
| Discovery beacons and queries | UDP multicast | group `239.255.78.84`, port **47800** |
| Entity state (DIS PDUs), peer heartbeats | UDP multicast | group chosen by the host and announced in the beacon (`239.255.78.128`–`.191`, from a hash of the session ID), port **47801** |
| Session control and chat (reliable) | UDP unicast | the peer's address and its own control port, announced in the beacon and the roster |

- Groups lie in `239.255.0.0/16`, the organisation-local scope. The default TTL is 1, so traffic
  stays on the local subnet unless the user raises it (`--multicast-ttl`).
- Sockets bind with `ShareAddress | ReuseAddressHint`, so several instances on one machine can join
  (for tests, and for running ntsim next to a headless observer). Multicast loopback is on.
- Each session has its own update group, so the kernel filters out other sessions' traffic. The
  session ID in every packet is still checked.
- The interface comes from `--interface`, or else the first interface that is up, can multicast and
  is not a loopback interface (`QNetworkInterface::CanMulticast`).
- The ports and groups are configurable. DIS tools that listen on the conventional port 3000 reach
  us through a gateway (§10), not by changing the default.

## 4. Wire format

Every datagram on either port starts with a 16-byte Nt header (little-endian), except raw DIS PDUs
on the update port, which follow IEEE 1278.1 exactly (big-endian) so that standard tools can read
them.

```text
offset size field
0      4    magic        "NTSS" (Nt session)
4      1    version      1
5      1    type         Beacon, Query, Control, Heartbeat (Nt header)
6      2    flags        bit 0 authenticated (16-byte trailer), bit 1 reliable, bit 2 fragment
8      4    sessionId    random, 0 in a Query
12     2    peer         the sender's peer number, 0 before it is admitted
14     2    epoch        the host epoch, see §2
```

A receiver tells the two apart by the first bytes. The Nt magic is `NTSS`. A DIS PDU starts with
protocol version 7 and an exercise ID. The DIS exercise ID is the session's exercise number from
the beacon (1–255).

- **Payloads.** Beacons and control messages are CBOR maps (QCborValue) with integer keys, so the
  format can be extended. Unknown keys are ignored. A chat control message carries a CoT GeoChat
  event as an XML string (§9).
- **Authentication (optional).** With a session key (`--session-key`), every datagram carries a
  trailer: the first 16 bytes of HMAC-SHA256 over the datagram, using `QMessageAuthenticationCode`.
  DIS PDUs carry the same trailer. A standard DIS tool ignores trailing bytes, so it can still read
  them. The key is derived from the passphrase and the session ID. The traffic is not encrypted,
  by decision (see Decisions).
- **Limits.** A datagram is at most 1,200 bytes, to stay below a typical MTU. The reliable channel
  fragments messages of up to 64 KiB. Anything larger, or malformed, is dropped and counted.

## 5. Discovery (port 47800)

- A host sends a **Beacon** every second, and immediately when it receives a Query.
- A browser (`QtComms::SessionBrowser`) sends one **Query** when it starts, so the list fills at once.
- A session is listed while its beacons arrive. It is dropped 5 s after the last beacon.

Beacon fields:

| Key | Field |
| --- | --- |
| 1 | session ID (u32), epoch |
| 2 | session name, host's display name |
| 3 | protocol version; minimum version accepted |
| 4 | application and version (`ntsim 6.1.0`) |
| 5 | world: ID, revision, SHA-256 of the world manifest |
| 6 | update group and port; DIS exercise number |
| 7 | peers in session / maximum |
| 8 | flags: key required, observers allowed, strict models |
| 9 | digest of the session's model set (SHA-256), so a browser can warn before joining |
| 10 | optional update URL, shown to users who need new models |

## 6. Joining and the model check

### 6.1 Models and catalogues

A model is named by a `QtTwin::ModelRef`: a stable ID (`air/rotary/ec135`, `land/wheeled/lr110`,
`sea/rib-7m`, `person/civilian-a`), an integer revision, the SHA-256 of its glTF file, and the DIS
entity type it represents.

The rule is: **a peer can render a model if it has the same ID at the same or a higher revision.**
A revision may improve geometry and textures, but it must keep node names and articulations. A
breaking change gets a new ID. In strict mode (beacon flag), the SHA-256 must match exactly, for
exercises that need identical assets on every machine.

Each application builds a `QtTwin::ModelCatalogue` from its model library: a `models.json` next to
the assets, listing ID, revision, kind, DIS entity type, file and hash. ntsim verifies the hashes
when it loads the catalogue, so a corrupted or edited file counts as missing.

The session's model set is the set of `ModelRef`s used by live entities. A model leaves the set when
its last entity is removed.

### 6.2 Handshake

All of these messages go over the reliable channel (§7) to the host's control port.

```text
Joiner                                 Host
  │ JoinRequest ─────────────────────▶ │  protocol, app version, display name, role
  │                                    │  (participant | observer), world ref,
  │                                    │  catalogue [ModelRef], entities it will bring
  │                                    │  [kind, ModelRef, marking]
  │                                    │  checks, in this order (§6.3)
  │ ◀──────────────────── JoinAccept   │  peer number, epoch, session time, roster,
  │                                    │  live entities (spawn records), chat backlog
  │   or                               │
  │ ◀──────────────────── JoinReject   │  reason, readable text, missing models
```

With a session key, the join request carries the HMAC trailer like every other datagram, and that is
the proof. A host that cannot verify it answers with an unsigned `Refused` packet, because the joiner
could not check a signed one. The chat backlog is the last 50 messages.

On JoinAccept, the joiner joins the update group, announces its entities with EntitySpawn messages
and starts sending state.

### 6.3 Admission checks

| # | Check | Reject reason |
| --- | --- | --- |
| 1 | Protocol version in the host's accepted range | `ProtocolTooOld` / `ProtocolTooNew` |
| 2 | Session key (the request's trailer) | `AuthenticationFailed` |
| 3 | Same world ID and hash; a lower revision is refused | `WorldMismatch` |
| 4 | Room in the session; display name unique | `SessionFull` / `NameInUse` |
| 5 | **The joiner can render every model in the session's set** | `MissingModels` |
| 6 | **Every current peer can render every model the joiner brings** | `PeersMissingModels` |

Check 6 compares against the catalogues of the existing peers, which the host already holds, so no
round trip is needed. When it fails, the host also sends a notice to each peer that lacks the model
("Bob tried to join with `air/rotary/ec135` r4; update your model library to see it"). Observers
bring no entities, but check 5 still applies to them.

The same checks run on an **EntitySpawn** in the middle of a session. If the new entity's model is
missing anywhere, the host answers with **SpawnReject**, listing the model and the peers that lack
it. The owner keeps its connection, the entity never appears, and the user gets the same message.

### 6.4 Telling the user

`JoinReject` carries structured data, so each application words the message for its own interface.
ntsim shows:

```text
Can't join "Amesbury exercise" (host: alice-ws)
This copy of ntsim can't show 2 models used in the session. Update your model library:
  • air/rotary/ec135   needs revision 4, you have 3
  • sea/rib-7m         not installed
Get the update from: https://… (from the session)
```

In headless use (`--observe` with `--frames`, CI), ntsim prints the same text to stderr and exits
with code 3. Version refusals tell the user to update ntsim or Nt, not the model library.

Models are not downloaded automatically in version 1. A later version may fetch missing models from
the beacon's update URL over HTTPS and accept them only if their hashes match the host's manifest.

## 7. Reliable channel (Nt Comms)

This is a small selective-repeat protocol over unicast UDP between each pair of peers, used for
control messages and chat:

- a 16-bit sequence number per message, and in every packet an acknowledgement of the highest
  sequence received plus a 32-bit bitfield of the ones before it
- retransmission after a timeout of the smoothed round-trip time × 2 (at least 50 ms), up to 10
  times; then the peer counts as lost
- in-order delivery to the application; fragments are reassembled before delivery
- acknowledgements ride on heartbeats when there is nothing else to send

Chat to everyone goes to each peer separately. With 64 peers this is still a few kilobytes, and
reliable multicast is not worth its complexity here.

## 8. Entity updates (DIS on port 47801)

### 8.1 Mapping to the Entity State PDU

| PDU field | Nt use |
| --- | --- |
| Entity ID (site, application, entity) | site = the exercise's site number (default 1), application = the owner's peer number, entity = the owner's local index |
| Force ID | from the entity, default Neutral |
| Entity type | from the `ModelRef` (SISO-REF-010): kind 3 lifeform (people), 1 platform with domain 1 land (vehicles), 2 air (aircraft and drones share it; the Nt kind comes from the spawn record), 3 surface (boats) |
| Location | ECEF in double precision. This is exactly `QtTwin::Vector3d` from `Ellipsoid::toEcef()`, so there is no precision loss. |
| Orientation | Euler angles ψ, θ, φ relative to ECEF, converted from the entity's quaternion in its local ENU frame |
| Linear velocity | ECEF, float32, m/s |
| Dead-reckoning parameters | algorithm, linear acceleration, angular velocity |
| Appearance | the DIS bits that apply: lights, engine running, damage, landing gear, and so on |
| Marking | the callsign, up to 11 ASCII characters |
| Variable parameters | articulated parts: rotor speed, wheel steer, rudder, turret, doors |

The PDU timestamp is session time. The host's `QtTwin::SimulationClock` defines it, and peers
estimate their offset from heartbeat pings, filtering for the smallest round-trip time over 8
samples. Session time runs at real time while multiplayer is on. (Pausing by the host is not
implemented yet.)

The spawn and despawn messages are Nt control messages that carry the `ModelRef`. A DIS PDU alone
names only an entity type, which is too coarse to pick one model at one revision. If state arrives
for an entity that has not been spawned, the receiver asks the owner for its spawn record.

### 8.2 When the owner sends

The owner runs the same dead-reckoning model as the receivers. It sends a PDU when any of these
holds:

- the dead-reckoned position or orientation differs from the true one by more than the kind's
  threshold
- the appearance or articulation changes
- the heartbeat interval has passed since the last PDU: **the keep-alive for an entity that is not
  moving or not changing**

Updates are also limited to the kind's maximum rate:

| Kind | DR algorithm | Position / angle threshold | Max rate | Heartbeat |
| --- | --- | --- | --- | --- |
| Person | DRM FPW (2), ground-clamped | 0.3 m / 5° | 10 Hz | 5 s |
| Ground vehicle | DRM RVW (4), ground-clamped | 0.5 m / 3° | 20 Hz | 5 s |
| Aircraft | DRM RVW (4) | 2 m / 3° | 30 Hz | 5 s |
| Drone | DRM RVW (4) | 0.2 m / 3° | 30 Hz | 5 s |
| Boat | DRM RVW (4), clamped to sea level | 0.5 m / 3° | 10 Hz | 5 s |

The 5 s heartbeat and 2.4 timeout multiplier are the DIS defaults. All values are per kind and
configurable.

At a typical 5 Hz average, 2,000 entities of 144 bytes each come to about 1.4 MB/s, comfortable on
wired gigabit, marginal on Wi-Fi. Several PDUs are bundled into each datagram, as DIS 7 allows.

### 8.3 What receivers do

- Dead-reckon each remote entity from its last PDU. When a new PDU arrives, blend to it over 150 ms
  instead of snapping.
- Clamp people and ground vehicles to the receiver's own terrain, and boats to its water surface,
  so small height differences never leave them floating or buried.
- Drop PDUs that are older than the last one applied, by timestamp.
- **Stale:** no PDU for 2.4 × the heartbeat (12 s). Stop extrapolating, freeze the entity and show
  it as stale: greyed out, with a marker. As the survey requires, data that has stopped updating
  must not look current.
- **Removed:** no PDU for 30 s, or a despawn, or the owner has left.

### 8.4 Peer liveness

Each peer multicasts a **Heartbeat** on the update group every second. It carries the session time
and a ping/pong pair for clock estimation, and acknowledgements for the reliable channel.

- No heartbeat for 5 s: the peer is unresponsive, and its entities are shown stale early.
- No heartbeat for 15 s: the host removes the peer and announces it in the roster and in chat.

Peer heartbeats and entity heartbeats are separate on purpose. A peer whose entities are all parked
still proves it is alive once a second, while 2,000 parked entities cost one PDU every 5 s each.

## 9. Text chat: Cursor on Target GeoChat

Chat messages are **Cursor on Target (CoT) GeoChat events**, the open XML format used for chat by
ATAK, WinTAK, iTAK and TAK Server. The CoT event schema was published by MITRE, and the GeoChat detail
elements are documented publicly and visible in any capture.

Why GeoChat:

- **Serverless.** It runs over UDP multicast on a LAN, as the rest of the design does. XMPP and
  IRC need a server, Matrix needs a homeserver over HTTP, and serverless XMPP (XEP-0174) needs mDNS.
- **It is geographic.** Each message carries the sender's position as a CoT `<point>`, so the twin
  can show chat on the map at the sender's entity.
- **It fits the domain.** DIS simulators and TAK devices sit side by side in exercises, so a TAK
  user on the LAN can chat with the simulator without a gateway.
- **It is small.** It is one XML element per message. `QXmlStreamReader` and `QXmlStreamWriter` are
  in Nt Core, so Nt Comms still needs only Nt Core and Nt Network, and it still builds for Zephyr.

### 9.1 Message format

A chat message is a CoT event of type `b-t-f`. Its `uid` is `GeoChat.<sender uid>.<room>.<message UUID>`.
It carries `time`, `start` and `stale`, and the detail elements below:

```xml
<event version="2.0" uid="GeoChat.NT-7f3a….All Chat Rooms.5c1e…" type="b-t-f" how="h-g-i-g-o"
       time="2026-11-22T03:14:07.000Z" start="2026-11-22T03:14:07.000Z" stale="2026-11-23T03:14:07.000Z">
  <point lat="51.1789" lon="-1.8262" hae="160.0" ce="9999999" le="9999999"/>
  <detail>
    <__chat chatroom="All Chat Rooms" id="All Chat Rooms" senderCallsign="ALPHA-1" groupOwner="false" parent="RootContactGroup">
      <chatgrp uid0="NT-7f3a…" uid1="All Chat Rooms" id="All Chat Rooms"/>
    </__chat>
    <link uid="NT-7f3a…" type="a-f-G-U-C" relation="p-p"/>
    <remarks source="Nt.NT-7f3a…" to="All Chat Rooms" time="2026-11-22T03:14:07.000Z">Crossing the bridge now</remarks>
  </detail>
</event>
```

- **Sender uid.** Each peer's uid is `NT-<session ID>-<peer number>`. In the session layer this maps
  to the peer, and in the scene to its first entity.
- **Rooms.** The room `All Chat Rooms` is everyone. A team uses the team's name as its room. A
  direct message names the recipient's uid in `chatroom`, `id` and `chatgrp uid1`, as GeoChat
  does.
- **Receipts.** Delivery and read receipts use GeoChat's `b-t-f-d` and `b-t-f-r` events. Nt Comms
  sends `b-t-f-d` when a message reaches the application, and the application calls `markRead()`
  to send `b-t-f-r`.
- **System messages.** Joins, departures, rejections and "X needs a model update" come from the
  host's uid with `senderCallsign="SESSION"`, so ATAK shows them as ordinary messages.
- **Clean-room.** The field details above must be checked against captures from ATAK-CIV and
  WinTAK before release. The implementation follows the schema and the captures, not any TAK code.

### 9.2 Transport

- **In a session**, each GeoChat event is the payload of a reliable control message (§7) on port
  47801. It therefore gets the session's authentication, ordering and retransmission, and the
  host's 50-message backlog for joiners. Messages are sent as XML only ("TAK protocol version 0"),
  so no protobuf is needed.
- **The TAK bridge** (`--tak-chat`, off by default) also sends each room message to the standard
  TAK chat multicast group, `224.10.10.1:17012`, and listens there. Events that arrive on the
  bridge are checked, given a peer-like entry in the roster (`TAK: <callsign>`) and passed to the
  session. Only TAK protocol version 0 (XML) is supported. A TAK device set to version 1
  (protobuf) must be switched back to version 0 for its mesh traffic. The bridge logs, once, that
  it is ignoring version 1 packets (those starting with `0xbf 0x01 0xbf`). The bridge uses a third port, but only when
  it is switched on, and the two-port rule holds for Nt peers.

### 9.3 Rules

- Text is at most 1,000 bytes of UTF-8 and is treated as data. Control characters are removed,
  XML is parsed with entity expansion off and a 16 KiB limit, and nothing in the text is
  interpreted.
- Each sender is limited to 5 messages a second. Events with a `stale` time in the past are
  dropped.
- Each peer keeps the history. Messages are shown in order of session time, then sender uid.
- Chat is not encrypted. Without a session key it is also unauthenticated. The TAK bridge is never
  authenticated, because TAK multicast has no authentication. The user documentation must say that
  sessions are meant for trusted networks.

## 10. API sketch

```cpp
// Nt Comms: generic sessions, no twin knowledge.
namespace QtComms {
struct SessionInfo { quint32 id; QString name, host; QHostAddress address; QVariantMap attributes; /*...*/ };

class SessionBrowser : public QObject {          // listens on the discovery port
    Q_OBJECT
public:
    void start(); QList<SessionInfo> sessions() const;
Q_SIGNALS:
    void sessionFound(const QtComms::SessionInfo &); void sessionLost(quint32 id);
};

class Session : public QObject {
    Q_OBJECT
public:
    enum class RejectReason { ProtocolTooOld, ProtocolTooNew, AuthenticationFailed, WorldMismatch,
                              SessionFull, NameInUse, MissingModels, PeersMissingModels, Custom };
    bool host(const QString &name, const QVariantMap &attributes);       // starts beaconing
    void join(const SessionInfo &info, const QCborMap &joinRequest);
    void setAdmissionPolicy(std::function<Admission(const QCborMap &request)>);   // the twin layer plugs in here
    void send(quint16 peer, const QCborMap &message);                     // reliable
    void multicast(const QByteArray &datagram);                           // unreliable, update group
Q_SIGNALS:
    void joined(); void rejected(RejectReason, const QString &text, const QCborMap &details);
    void peerJoined(quint16); void peerLeft(quint16); void peerUnresponsive(quint16);
    void messageReceived(quint16 peer, const QCborMap &); void datagramReceived(const QByteArray &);
};

class Chat : public QObject {
    Q_OBJECT
public:
    explicit Chat(Session *session); void send(const QString &text, quint16 to = 0);
Q_SIGNALS:
    void messageReceived(const QtComms::ChatMessage &);
};
}

// Nt Twin: data and pure functions.
namespace QtTwin {
enum class EntityKind { Person, GroundVehicle, Aircraft, Drone, Boat };
struct ModelRef { QString id; int revision; QByteArray sha256; DisEntityType disType; };
class ModelCatalogue { /* load(models.json), canRender(ModelRef, strict), missing(set) */ };
struct EntityState { Vector3d ecef; Quaterniond orientation; Vector3d velocity, acceleration, angularVelocity;
                     quint32 appearance; QList<Articulation> parts; qint64 time; };
namespace Dis { QByteArray encodeEntityState(...); std::optional<EntityStatePdu> decode(QByteArrayView); }
class DeadReckoner { /* extrapolate(state, algorithm, t), exceedsThreshold(...) */ };
}

// Nt Twin Network: the glue.
namespace QtTwin {
class Replicator : public QObject {
    Q_OBJECT
public:
    Replicator(QtComms::Session *session, const ModelCatalogue *catalogue);
    EntityHandle publish(EntityKind, const ModelRef &, const QString &marking);   // may be refused: spawnRejected
    void update(EntityHandle, const EntityState &);    // called every frame; decides whether to send
    EntityState remoteState(EntityId, qint64 sessionTime) const;   // dead-reckoned and blended
Q_SIGNALS:
    void entityAppeared(QtTwin::EntityId, const QtTwin::ModelRef &); void entityStale(QtTwin::EntityId);
    void entityRemoved(QtTwin::EntityId); void spawnRejected(EntityHandle, const QList<QtTwin::ModelRef> &missing);
};
}
```

Nt Comms takes a time source as a function, so tests can drive its timeouts with a
`QtTwin::SimulationClock` without Nt Comms depending on Nt Twin.

## 11. ntsim integration (ntmapgen repository)

- **Options:**
  - `--host NAME` creates a session.
  - `--join NAME|ID` joins one, and `--sessions` lists what is on the LAN.
  - `--observe` joins as an observer, with no entity of its own.
  - `--callsign`, `--session-key`, `--interface` and `--multicast-ttl` set the rest.
- **The local vehicle.** `Sim::Vehicle` is published as a GroundVehicle with its model's
  `ModelRef`. Its pose goes through `LocalFrame::toEcef()` every frame, and the Replicator decides
  whether to send.
- **Remote entities.** These are instanced from the model library through the catalogue. ntsim needs
  a per-entity model instance path. Today it renders one vehicle. Wheels, rotors and rudders are
  driven from the articulated parts.
- **HUD:**
  - a chat panel (`T` to type, `Enter` to send, the last 8 lines fade out)
  - a roster (`Tab`), with each peer's state and round-trip time
  - stale markers on frozen entities
  - the rejection screen from §6.4
- **Testing.** A headless `ntsim --observe --log entities.csv` records what it saw, for tests and
  for checking the historian.

## 12. Testing

- `tests/test_comms.cpp`: two or three sessions in one process on multicast loopback. It covers:
  - discovery, query and answer, and a session expiring
  - a join being accepted, and each rejection reason
  - reliable delivery under a lossy socket wrapper (20% loss, reordering)
  - host migration
  - chat ordering and the backlog
- `tests/test_twin.cpp` additions:
  - round-trips of the DIS codec against PDUs captured from a reference tool, kept as hex fixtures
  - dead-reckoning algorithms against values computed by hand
  - threshold and heartbeat send decisions, driven by `SimulationClock::advance()`
  - the catalogue's `canRender` rules
- An end-to-end test with `Replicator`: a parked entity sends only heartbeats, goes stale after 12 s
  of silence and disappears after 30 s; a spawn with an unknown model is refused.
- Manual checks: Wireshark decodes our DIS PDUs (they must show no malformed fields), and a
  ThreadSanitizer run covers the networking.

## 13. Phases

Status on 2026-11-22: phases 1 to 3 are implemented in Nt (`Nt::Comms`, the new Nt Twin classes,
`Nt::TwinNetwork`), with tests and ndoc. Phase 4 (ntsim) has not been started.

1. **Nt Comms:** the header, discovery, the reliable channel, sessions with an admission hook, peer
   heartbeats, and GeoChat chat with the optional TAK bridge, with tests and ndoc.
2. **Nt Twin:** `EntityKind`, `ModelRef`, `ModelCatalogue`, the DIS 7 Entity State codec and
   `DeadReckoner`. This is also the seed of the phase 1 entity store in the survey.
3. **Nt Twin Network:** `Replicator` with spawn and despawn, send decisions, staleness, and the
   model admission policy (checks 5 and 6, and SpawnReject).
4. **ntsim:** options, publishing the local vehicle, rendering remote entities, the chat and roster
   HUD, and the rejection screen.
5. **Later:**
   - a host relay for networks without multicast (VPNs, some Wi-Fi)
   - a raw DIS gateway on port 3000, to show third-party DIS entities whose type maps to a known
     model, and to show unknown ones as a placeholder with a notice
   - Cursor on Target position reports for ATAK (entities as CoT `a-…` events; chat is already
     CoT)
   - verified model downloads
   - IPv6 (`ff15::` scope)
   - an HLA gateway
