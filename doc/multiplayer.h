// MIT License - see LICENSE
//
// Overview page about multiplayer sessions (Nt Comms and Nt Twin Network) for the NDoc documentation. This file
// holds only a documentation comment and is not part of the library.

/*!
    \page nt-multiplayer.html
    \title Multiplayer Sessions
    \brief How copies of an application share a session on a LAN: discovery, admission, DIS entity updates and chat.

    Several machines can join one session and see each other's people, vehicles, aircraft, drones
    and boats move on the map, and chat. Two modules provide this:

    \list
    \li \l{Nt Comms C++ Classes}{Nt Comms} (\c Nt::Comms) finds sessions, admits peers, carries
        reliable messages and multicast datagrams, keeps peers alive with heartbeats, hands the
        session to another peer when the host goes away, and provides text chat. It depends only on
        Nt Core and Nt Network.
    \li \l{Nt Twin Network C++ Classes}{Nt Twin Network} (\c Nt::TwinNetwork) shares Nt Twin entities
        over a session as DIS 7 Entity State PDUs, and refuses peers that cannot render every model in
        use.
    \endlist

    \code
    find_package(Nt6 REQUIRED)
    target_link_libraries(sim PRIVATE Nt::TwinNetwork)
    \endcode

    \section1 Finding and Joining a Session

    A host announces its session with a multicast beacon every second on UDP port 47800, group
    \c 239.255.78.84. A QtComms::SessionBrowser lists what it hears:

    \code
    QtComms::Session session;
    QtTwin::Replicator replicator(&session);
    replicator.setCatalogue(catalogue);
    replicator.setWorld(QStringLiteral("amesbury"), 3);

    // Either host...
    replicator.host(QStringLiteral("Amesbury exercise"), QStringLiteral("alice"));

    // ...or join one that a browser found.
    QtComms::SessionBrowser browser;
    browser.start();
    connect(&browser, &QtComms::SessionBrowser::sessionFound, [&](const QtComms::SessionInfo &s) {
        if (s.name == QStringLiteral("Amesbury exercise"))
            replicator.join(s, QStringLiteral("bob"));
    });
    \endcode

    Updates travel on UDP port 47801, on a multicast group the host picks for the session. Each
    peer receives reliable messages on a port of its own, announced in the session, so several
    copies can run on one machine.

    \section1 Admission and Models

    The host decides who may join. QtComms::Session checks the protocol version, the session key,
    the room left and the name, and the replicator then checks the world and the models: a peer must
    be able to render every model already in the session, and every peer must be able to render the
    models the newcomer brings. A refused peer gets QtComms::Session::rejected() with the reason and
    the missing models, which QtTwin::Replicator::describeRejection() turns into text:

    \badcode
    You need an update: this copy cannot show 2 model(s) used in the session. Update your model library:
      - air/rotary/ec135 needs revision 4, you have 3
      - sea/rib-7m revision 1, not installed
    Get the update from: https://models.example/update
    \endcode

    A model is named by an ID and a revision (QtTwin::ModelRef); a newer revision of the same ID
    can always render an older one.

    \section1 Entity Updates

    Each entity has one owner, the peer that published it, and only the owner sends its state. The
    receivers predict it between updates with DIS dead reckoning, so the owner sends only when the
    prediction drifts past a threshold, and at least every five seconds as a heartbeat. A parked
    vehicle therefore costs one PDU every five seconds, and a receiver that hears nothing for 12
    seconds shows the entity as stale. Every peer also multicasts its own heartbeat each second;
    a silent peer is unresponsive after 5 seconds and removed after 15, and if it was the host, the
    oldest remaining peer takes over.

    The PDUs are standard DIS 7, so Wireshark and other DIS tools can read them.

    \section1 Chat

    QtComms::Chat sends text to everyone, to a team or to one peer, reliably. Messages are Cursor on
    Target GeoChat events, the open chat format of ATAK and TAK Server, and carry the sender's
    position. With QtComms::Chat::setTakBridgeEnabled(), TAK devices on the network join the room
    \c {All Chat Rooms}.

    \section1 Security

    Sessions are meant for trusted networks. A session key (QtComms::SessionConfiguration::key)
    signs every datagram, so peers without it cannot join or inject updates, but nothing is
    encrypted, and messages through the TAK bridge are not authenticated.

    \sa {Nt Comms C++ Classes}, {Nt Twin Network C++ Classes}, {Using Nt Twin}
*/
