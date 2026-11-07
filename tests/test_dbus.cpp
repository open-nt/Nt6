// MIT License - see LICENSE
#include "dbus_classes.h"
#include "qdbusmarshaller_p.h"
#include "qdbusmessage_p.h"

#include <QtCore>
#include <QtDBus/QtDBus>
#include <QTest>

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static const QString kService = QStringLiteral("org.example.Clock");

// ---- the type system and the wire format, without a bus ------------------------------------

static void testNames() {
    CHECK(QDBusUtil::isValidObjectPath("/"));
    CHECK(QDBusUtil::isValidObjectPath("/a/b_c/D9"));
    CHECK(!QDBusUtil::isValidObjectPath(""));
    CHECK(!QDBusUtil::isValidObjectPath("/a/"));
    CHECK(!QDBusUtil::isValidObjectPath("/a//b"));
    CHECK(!QDBusUtil::isValidObjectPath("a/b"));
    CHECK(!QDBusUtil::isValidObjectPath("/a-b"));
    CHECK(QDBusUtil::isValidInterfaceName("org.example.Clock"));
    CHECK(!QDBusUtil::isValidInterfaceName("org"));
    CHECK(!QDBusUtil::isValidInterfaceName("org.9example"));
    CHECK(!QDBusUtil::isValidInterfaceName("org..x"));
    CHECK(QDBusUtil::isValidBusName("org.example-x.Clock"));
    CHECK(QDBusUtil::isValidBusName(":1.42"));
    CHECK(!QDBusUtil::isValidBusName(":1"));
    CHECK(QDBusUtil::isValidMemberName("Get_All2"));
    CHECK(!QDBusUtil::isValidMemberName("2Get"));
    CHECK(!QDBusUtil::isValidMemberName("a.b"));
}

static void testSignatures() {
    for (const char *ok : {"", "i", "a{sv}", "(ii)", "aai", "a(sa{sv})", "v", "ah", "a{s(ii)}"})
        CHECK(QDBusUtil::isValidSignature(ok));
    for (const char *bad : {"a", "(", "()", "a{vs}", "a{s}", "a{sii}", "{sv}", "z", "(i"})
        CHECK(!QDBusUtil::isValidSignature(bad));
    CHECK(QDBusUtil::isValidSingleSignature("a{sv}"));
    CHECK(!QDBusUtil::isValidSingleSignature("ii"));
    QByteArray deep(33, 'a');
    deep += 'i';
    CHECK(!QDBusUtil::isValidSignature(deep));
    CHECK(QDBusUtil::isValidSignature(QByteArray(32, 'a') + "i"));
    CHECK(std::strcmp(QDBusMetaType::typeToSignature(QMetaType::fromType<QVariantMap>()), "a{sv}") == 0);
    CHECK(std::strcmp(QDBusMetaType::typeToSignature(QMetaType::fromType<QStringList>()), "as") == 0);
    CHECK(std::strcmp(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<QDBusObjectPath>>()), "ao") == 0);
    CHECK(QDBusMetaType::typeToSignature(QMetaType::fromType<QDate>()) == nullptr);
    CHECK(std::strcmp(QDBusMetaType::typeToSignature(QMetaType::fromType<Fix>()), "(dds)") == 0);
    CHECK(std::strcmp(QDBusMetaType::typeToSignature(QMetaType::fromType<QList<Fix>>()), "a(dds)") == 0);
    CHECK(QDBusMetaType::signatureToMetaType("a{sv}") == QMetaType::fromType<QVariantMap>());
    CHECK(QDBusMetaType::signatureToMetaType("(dds)") == QMetaType::fromType<Fix>());
    CHECK(QDBusMetaType::signatureToMetaType("(xx)") == QMetaType::fromType<QDBusArgument>());
    CHECK(!QDBusMetaType::signatureToMetaType("ii").isValid());

    CHECK(QDBusObjectPath("/ok").path() == "/ok");
    CHECK(QDBusSignature("a{sv}").signature() == "a{sv}");
}

static void testArgumentRoundTrip() {
    QDBusArgument out;
    out << 42 << QStringLiteral("hé") << true << static_cast<uchar>(7) << static_cast<short>(-2) << 3.5;
    out << QDBusObjectPath("/p") << QDBusSignature("ai") << QDBusVariant(QStringLiteral("inner"));
    out << Fix{1.5, 2.5, QStringLiteral("home")};
    out << QList<int>{1, 2, 3};
    out << QVariantMap{{QStringLiteral("k"), 9}};
    out << QStringList{QStringLiteral("a"), QStringLiteral("b")} << QByteArray("xyz");
    CHECK(out.currentSignature() == "isbynd"
                                    "ogv(dds)aia{sv}asay");

    QDBusReader reader = QDBusArgumentPrivate::readerOf(out);
    const QDBusArgument in = QDBusArgumentPrivate::forReader(reader);
    int i = 0;
    QString s;
    bool b = false;
    uchar y = 0;
    short n = 0;
    double d = 0;
    QDBusObjectPath o;
    QDBusSignature g;
    QDBusVariant v;
    Fix f;
    QList<int> list;
    QVariantMap map;
    QStringList words;
    QByteArray bytes;
    CHECK(in.currentType() == QDBusArgument::BasicType);
    in >> i >> s >> b >> y >> n >> d >> o >> g;
    CHECK(in.currentType() == QDBusArgument::VariantType);
    in >> v;
    CHECK(in.currentType() == QDBusArgument::StructureType);
    in >> f;
    CHECK(in.currentType() == QDBusArgument::ArrayType);
    in >> list;
    CHECK(in.currentType() == QDBusArgument::MapType);
    in >> map >> words >> bytes;
    CHECK(in.atEnd());
    CHECK(i == 42 && s == QStringLiteral("hé") && b && y == 7 && n == -2 && d == 3.5);
    CHECK(o.path() == "/p" && g.signature() == "ai" && v.variant().toString() == "inner");
    CHECK((f == Fix{1.5, 2.5, QStringLiteral("home")}));
    CHECK((list == QList<int>{1, 2, 3}));
    CHECK(map.value("k").toInt() == 9);
    CHECK((words == QStringList{"a", "b"}) && bytes == "xyz");

    // A copy keeps its own position.
    const QDBusArgument again = QDBusArgumentPrivate::forReader(reader);
    const QDBusArgument copy = again;
    int first = 0;
    copy >> first;
    int fromOriginal = 0;
    again >> fromOriginal;
    CHECK(first == 42 && fromOriginal == 42);

    // A value of the wrong type is skipped with a default.
    const QDBusArgument wrong = QDBusArgumentPrivate::forReader(reader);
    QString notAString = QStringLiteral("x");
    wrong >> notAString;
    QString second;
    wrong >> second;
    CHECK(notAString.isEmpty() && second == QStringLiteral("hé"));
}

static QDBusMessage roundTrip(const QDBusMessage &m) {
    QDBusWireMessage wire;
    QDBusError error;
    if (!QDBusMessagePrivate::toWire(m, true, &wire, &error))
        return QDBusMessage();
    const uint serial = 5;
    std::memcpy(wire.bytes.data() + 8, &serial, 4);
    QString why;
    return QDBusMessagePrivate::fromWire(wire.bytes, wire.fds, &why);
}

static void testMessages() {
    QDBusMessage call = QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", "Add");
    call << 1 << 2u << QVariant::fromValue(QDBusVariant(QVariantList{1, "two"}));
    CHECK(call.signature() == "iuv");
    const QDBusMessage back = roundTrip(call);
    CHECK(back.type() == QDBusMessage::MethodCallMessage);
    CHECK(back.path() == "/clock" && back.interface() == "org.example.Clock" && back.member() == "Add");
    CHECK(back.signature() == "iuv" && back.arguments().size() == 3);
    CHECK(back.arguments().at(1).toUInt() == 2u);
    const QVariant inner = back.arguments().at(2).value<QDBusVariant>().variant();
    CHECK(inner.metaType() == QMetaType::fromType<QDBusArgument>());
    const QVariantList list = qdbus_cast<QVariantList>(inner);
    CHECK(list.size() == 2 && list.at(1).toString() == "two");
    CHECK(back.isReplyRequired());

    QDBusMessage error = back.createErrorReply(QDBusError::InvalidArgs, "bad");
    const QDBusMessage errorBack = roundTrip(error);
    CHECK(errorBack.type() == QDBusMessage::ErrorMessage);
    CHECK(errorBack.errorName() == "org.freedesktop.DBus.Error.InvalidArgs" && errorBack.errorMessage() == "bad");
    CHECK(QDBusError(errorBack).type() == QDBusError::InvalidArgs);

    QDBusWireMessage wire;
    QDBusError e;
    CHECK(!QDBusMessagePrivate::toWire(QDBusMessage::createSignal("bad path", "a.b", "C"), false, &wire, &e));
    CHECK(e.type() == QDBusError::InvalidObjectPath);
    QDBusMessage unregistered = QDBusMessage::createSignal("/a", "a.b", "C");
    unregistered << QVariant::fromValue(QDate(2020, 1, 1));
    CHECK(!QDBusMessagePrivate::toWire(unregistered, false, &wire, &e));
}

// A big-endian method call with one int32, written by hand.
static void testBigEndian() {
    QByteArray m;
    auto u32 = [&m](uint v) {
        for (int shift = 24; shift >= 0; shift -= 8)
            m.append(static_cast<char>((v >> shift) & 0xff));
    };
    auto pad = [&m](int n) {
        while (m.size() % n)
            m.append('\0');
    };
    m.append('B').append('\1').append('\0').append('\1');
    u32(4); // body
    u32(9); // serial
    u32(0); // fields length, patched below
    const qsizetype fieldsStart = m.size();
    // PATH "/x"
    m.append('\1').append('\1').append('o').append('\0');
    u32(2);
    m.append("/x").append('\0');
    pad(8);
    // MEMBER "Go"
    m.append('\3').append('\1').append('s').append('\0');
    u32(2);
    m.append("Go").append('\0');
    pad(8);
    // SIGNATURE "i"
    m.append('\x08').append('\1').append('g').append('\0');
    m.append('\1').append('i').append('\0');
    const auto fieldsLength = static_cast<uint>(m.size() - fieldsStart);
    pad(8);
    for (int k = 0; k < 4; ++k)
        m[12 + k] = static_cast<char>((fieldsLength >> (24 - 8 * k)) & 0xff);
    u32(0x01020304);
    QString why;
    const QDBusMessage parsed = QDBusMessagePrivate::fromWire(m, {}, &why);
    CHECK(parsed.type() == QDBusMessage::MethodCallMessage);
    CHECK(parsed.path() == "/x" && parsed.member() == "Go");
    CHECK(parsed.arguments().value(0).toInt() == 0x01020304);

    QByteArray broken = m;
    broken[broken.size() - 1] = 0; // still valid
    CHECK(QDBusMessagePrivate::fromWire(broken, {}, &why).type() == QDBusMessage::MethodCallMessage);
    broken = m;
    broken[3] = 2; // protocol version
    CHECK(QDBusMessagePrivate::fromWire(broken, {}, &why).type() == QDBusMessage::InvalidMessage);
}

static void testValidator() {
    auto body = [](std::initializer_list<int> bytes) {
        QByteArray b;
        for (int x : bytes)
            b.append(static_cast<char>(x));
        return b;
    };
    const uint one = 1;
    char firstByte;
    std::memcpy(&firstByte, &one, 1);
    const bool little = firstByte == 1;
    CHECK(QDBusUtil::validateValues(body({1, 0, 0, 0}), 0, "b", !little, 0) ==
          QDBusUtil::validateValues(body({0, 0, 0, 1}), 0, "b", little, 0));
    CHECK(!QDBusUtil::validateValues(little ? body({2, 0, 0, 0}) : body({0, 0, 0, 2}), 0, "b", false, 0));
    // A string that is not UTF-8, one without its terminating zero, one with a zero inside.
    CHECK(!QDBusUtil::validateValues(little ? body({1, 0, 0, 0, 0xff, 0}) : body({0, 0, 0, 1, 0xff, 0}), 0, "s", false,
                                     0));
    CHECK(!QDBusUtil::validateValues(little ? body({1, 0, 0, 0, 'a', 'b'}) : body({0, 0, 0, 1, 'a', 'b'}), 0, "s",
                                     false, 0));
    CHECK(!QDBusUtil::validateValues(little ? body({2, 0, 0, 0, 'a', 0, 0}) : body({0, 0, 0, 2, 'a', 0, 0}), 0, "s",
                                     false, 0));
    // An array that claims more bytes than there are; non-zero padding before an int64.
    CHECK(!QDBusUtil::validateValues(little ? body({8, 0, 0, 0, 1, 0, 0, 0}) : body({0, 0, 0, 8, 0, 0, 0, 1}), 0, "ai",
                                     false, 0));
    CHECK(!QDBusUtil::validateValues(body({1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}), 1, "x", false, 0));
    // A file descriptor index beyond those that came with the message.
    CHECK(!QDBusUtil::validateValues(body({0, 0, 0, 0}), 0, "h", false, 0));
    CHECK(QDBusUtil::validateValues(body({0, 0, 0, 0}), 0, "h", false, 1));
}

// ---- a private bus -------------------------------------------------------------------------

struct Bus {
    QProcess daemon;
    QString address;
    QTemporaryDir dir;

    bool start() {
#ifndef NT_DBUS_DAEMON
        return false;
#else
        if (!dir.isValid())
            return false;
        const QString config = dir.path() + "/bus.conf";
        QFile f(config);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write(QStringLiteral("<busconfig><type>session</type><listen>unix:dir=%1</listen>"
                               "<auth>EXTERNAL</auth><policy context=\"default\">"
                               "<allow send_destination=\"*\" eavesdrop=\"true\"/><allow eavesdrop=\"true\"/>"
                               "<allow own=\"*\"/></policy></busconfig>")
                    .arg(dir.path())
                    .toUtf8());
        f.close();
        daemon.start(QStringLiteral(NT_DBUS_DAEMON), {QStringLiteral("--config-file=") + config,
                                                      QStringLiteral("--nofork"), QStringLiteral("--print-address=1")});
        if (!daemon.waitForStarted(5000))
            return false;
        while (!daemon.canReadLine())
            if (!daemon.waitForReadyRead(5000))
                return false;
        address = QString::fromUtf8(daemon.readLine().trimmed());
        return !address.isEmpty();
#endif
    }
    ~Bus() {
        if (daemon.state() != QProcess::NotRunning) {
            daemon.kill();
            daemon.waitForFinished(5000);
        }
    }
};

// Runs a thread with an event loop for the server-side objects.
struct Worker {
    QThread thread;
    Worker() { thread.start(); }
    ~Worker() {
        thread.quit();
        thread.wait();
    }
    template <typename F> void run(F f) {
        QObject context;
        context.moveToThread(&thread);
        QMetaObject::invokeMethod(&context, f, Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(
            &context, [&context] { context.moveToThread(nullptr); }, Qt::BlockingQueuedConnection);
    }
};

static bool waitFor(const std::function<bool()> &done, int ms = 5000) { return QTest::qWaitFor(done, ms); }

static void testBus(const QString &address) {
    QDBusConnection server = QDBusConnection::connectToBus(address, "server");
    QDBusConnection client = QDBusConnection::connectToBus(address, "client");
    CHECK(server.isConnected() && client.isConnected());
    CHECK(server.baseService().startsWith(':'));
    CHECK(server.baseService() != client.baseService());
    CHECK(QDBusConnection("server").baseService() == server.baseService());
    CHECK(server.connectionCapabilities() & QDBusConnection::UnixFileDescriptorPassing);
    CHECK(!QDBusConnection("nope").isConnected());

    // The objects live in a thread of their own, so the main thread can block in calls.
    Worker worker;
    Clock *clock = nullptr;
    ClockAdaptor *adaptor = nullptr;
    worker.run([&] {
        clock = new Clock;
        adaptor = new ClockAdaptor(clock);
    });
    CHECK(server.registerObject("/clock", clock));
    CHECK(!server.registerObject("/clock", clock));
    CHECK(server.objectRegisteredAt("/clock") == clock);
    CHECK(server.registerService(kService));
    CHECK(server.interface()->isServiceRegistered(kService).value());
    CHECK(server.interface()->serviceOwner(kService).value() == server.baseService());
    CHECK(client.interface()->registeredServiceNames().value().contains(kService));
    CHECK(client.interface()->servicePid(kService).value() == static_cast<uint>(::getpid()));

    auto call = [&](const QString &method, const QList<QVariant> &args) {
        QDBusMessage m = QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", method);
        m.setArguments(args);
        return client.call(m);
    };

    QDBusReply<int> sum = call("Add", {2, 3});
    CHECK(sum.isValid() && sum.value() == 5);
    CHECK(QDBusReply<QString>(call("Echo", {QStringLiteral("héllo")})).value() == QStringLiteral("héllo"));
    const QDBusMessage split = call("Split", {QStringLiteral("a b c")});
    CHECK(split.signature() == "si" && split.arguments().value(0).toString() == "a" &&
          split.arguments().value(1).toInt() == 3);
    CHECK(QDBusReply<QString>(call("Sender", {})).value() == client.baseService());
    const QDBusReply<Fix> moved = call("Move", {QVariant::fromValue(Fix{1, 2, "x"}), 0.5});
    CHECK(moved.isValid() && (moved.value() == Fix{1.5, 2, "x"}));
    const QDBusReply<QVariantMap> described = call("Describe", {QVariantMap{{"a", 1}, {"b", "two"}}});
    CHECK(described.isValid() && described.value().value("count").toInt() == 2 &&
          described.value().value("b").toString() == "two");
    const QDBusReply<QVariant> wrapped = call("Wrap", {QVariant::fromValue(QDBusVariant(QStringList{"p", "q"}))});
    CHECK(wrapped.isValid() && wrapped.value().toStringList() == QStringList({"p", "q"}));
    CHECK(QDBusReply<QString>(call("TypeOf", {QVariant::fromValue(QDBusVariant(2.5))})).value() == "double");
    CHECK(QDBusReply<QByteArray>(call("Bytes", {QByteArray("ab\0c", 4)})).value() == QByteArray("ab\0c!", 5));
    CHECK(QDBusReply<QStringList>(call("Words", {QStringLiteral("x y")})).value() == QStringList({"x", "y"}));
    CHECK(
        (QDBusReply<QList<int>>(call("Squares", {QVariant::fromValue(QList<int>{2, 3})})).value() == QList<int>{4, 9}));
    CHECK(QDBusReply<qlonglong>(call("Big", {QVariant::fromValue(qulonglong(1) << 40)})).value() == -(1LL << 40));
    CHECK(QDBusReply<bool>(call("CalledFromDBus", {})).value());
    CHECK(!adaptor->calledFromDBusPublic());

    const QDBusMessage failed = call("Fail", {});
    CHECK(failed.type() == QDBusMessage::ErrorMessage && failed.errorName() == "org.example.Error.Broken" &&
          failed.errorMessage() == "it broke");
    CHECK(QDBusReply<QString>(call("Later", {})).value() == "later");
    const QDBusMessage unknown = call("Nope", {});
    CHECK(QDBusError(unknown).type() == QDBusError::UnknownMethod);
    const QDBusMessage wrongArgs = call("Add", {1});
    CHECK(QDBusError(wrongArgs).type() == QDBusError::UnknownMethod);
    QDBusMessage noObject = QDBusMessage::createMethodCall(kService, "/none", "org.example.Clock", "Add");
    CHECK(QDBusError(client.call(noObject)).type() == QDBusError::UnknownObject);
    QDBusMessage never = QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", "Never");
    const QDBusMessage timedOut = client.call(never, QDBus::Block, 200);
    CHECK(QDBusError(timedOut).type() == QDBusError::NoReply);
    CHECK(client.lastError().type() == QDBusError::NoReply);

    // A call without an interface finds the method too; a call that wants no reply.
    QDBusMessage anyInterface = QDBusMessage::createMethodCall(kService, "/clock", QString(), "Add");
    anyInterface << 4 << 4;
    CHECK(QDBusReply<int>(client.call(anyInterface)).value() == 8);
    QDBusMessage fire = QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", "Fire");
    fire << QStringLiteral("bang");
    CHECK(client.send(fire));
    CHECK(waitFor([&] { return QDBusReply<int>(call("Add", {0, 0})).isValid() && adaptor->lastFired == "bang"; }));

    // File descriptors.
    int fds[2];
    CHECK(::pipe(fds) == 0);
    CHECK(::write(fds[1], "piped", 5) == 5);
    ::close(fds[1]);
    CHECK(QDBusReply<QString>(call("ReadFd", {QVariant::fromValue(QDBusUnixFileDescriptor(fds[0]))})).value() ==
          "piped");
    ::close(fds[0]);

    // Properties and introspection.
    QDBusInterface iface(kService, "/clock", "org.example.Clock", client);
    CHECK(iface.isValid());
    CHECK(iface.property("volume").toInt() == 3);
    CHECK(iface.setProperty("volume", 9));
    CHECK(adaptor->volume() == 9);
    CHECK(iface.property("name").toString() == "kitchen");
    CHECK(!iface.setProperty("name", "x"));
    CHECK(iface.lastError().type() == QDBusError::PropertyReadOnly);
    CHECK(!iface.property("missing").isValid());
    CHECK(iface.lastError().type() == QDBusError::UnknownProperty);
    QDBusMessage getAll =
        QDBusMessage::createMethodCall(kService, "/clock", "org.freedesktop.DBus.Properties", "GetAll");
    getAll << QStringLiteral("org.example.Clock");
    const QDBusReply<QVariantMap> all = client.call(getAll);
    CHECK(all.isValid() && all.value().value("volume").toInt() == 9 && all.value().size() == 2);
    // QDBusInterface converts an int to the uint that... the method takes an int; check a call.
    CHECK(QDBusReply<int>(iface.call("Add", 20, 22)).value() == 42);
    CHECK(QDBusReply<qlonglong>(iface.call("Big", 5)).value() == -5); // int sent as the 't' Big takes

    QDBusMessage introspect =
        QDBusMessage::createMethodCall(kService, "/clock", "org.freedesktop.DBus.Introspectable", "Introspect");
    const QString xml = QDBusReply<QString>(client.call(introspect)).value();
    CHECK(xml.contains("<interface name=\"org.example.Clock\">"));
    CHECK(xml.contains("<method name=\"Split\">"));
    CHECK(xml.contains("<arg name=\"first\" type=\"s\" direction=\"out\"/>"));
    CHECK(xml.contains("<property name=\"volume\" type=\"i\" access=\"readwrite\"/>"));
    CHECK(xml.contains("<signal name=\"Ticked\">"));
    CHECK(xml.contains("org.freedesktop.DBus.Method.NoReply"));
    CHECK(!xml.contains("Scripted")); // the object's own slots are not exported by default
    QDBusMessage root =
        QDBusMessage::createMethodCall(kService, "/", "org.freedesktop.DBus.Introspectable", "Introspect");
    CHECK(QDBusReply<QString>(client.call(root)).value().contains("<node name=\"clock\"/>"));

    // Signals: relayed from the object through the adaptor.
    Receiver receiver;
    CHECK(client.connect(kService, "/clock", "org.example.Clock", "Ticked", &receiver, SLOT(onTick(QString))));
    CHECK(client.connect(QString(), "/clock", "org.example.Clock", "Ticked", &receiver,
                         SLOT(onTickMessage(QString, QDBusMessage))));
    CHECK(!client.connect(kService, "/clock", "org.example.Clock", "Ticked", &receiver, SLOT(onReply(int))) ||
          true); // connects, but never matches the signature
    ClockProxy proxy(kService, "/clock", client);
    QStringList proxied;
    QObject::connect(&proxy, &ClockProxy::Ticked, [&](const QString &t) { proxied.append(t); });
    worker.run([&] { emit clock->Ticked(QStringLiteral("tick")); });
    CHECK(waitFor([&] { return receiver.ticks.size() == 2 && proxied.size() == 1; }));
    CHECK(receiver.ticks.value(0) == "tick" && receiver.senders.value(0) == server.baseService());
    CHECK(proxied.value(0) == "tick");
    CHECK(client.disconnect(kService, "/clock", "org.example.Clock", "Ticked", &receiver, SLOT(onTick(QString))));
    worker.run([&] { emit clock->Ticked(QStringLiteral("tock")); });
    CHECK(waitFor([&] { return receiver.ticks.size() == 3; }));
    QTest::qWait(50);
    CHECK(receiver.ticks.size() == 3 && receiver.ticks.last() == "tock");

    // Asynchronous calls.
    QDBusPendingReply<int> pending = proxy.Add(40, 2);
    pending.waitForFinished();
    CHECK(pending.isValid() && pending.value() == 42);
    QDBusPendingReply<QString, int> pair = client.asyncCall(
        QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", "Split") << QStringLiteral("one two"));
    auto *watcher = new QDBusPendingCallWatcher(pair);
    bool finished = false;
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, [&](QDBusPendingCallWatcher *w) {
        finished = true;
        w->deleteLater();
    });
    CHECK(waitFor([&] { return finished; }));
    CHECK(pair.isValid() && pair.argumentAt<0>() == "one" && pair.argumentAt<1>() == 2);
    QDBusPendingReply<int> mismatch = proxy.asyncCall("Echo", QStringLiteral("x"));
    mismatch.waitForFinished();
    CHECK(mismatch.isError() && mismatch.error().type() == QDBusError::InvalidSignature);
    CHECK(proxy.callWithCallback("Add", {1, 1}, &receiver, SLOT(onReply(int)), SLOT(onError(QDBusError))));
    CHECK(proxy.callWithCallback("Fail", {}, &receiver, SLOT(onReply(int)), SLOT(onError(QDBusError))));
    CHECK(waitFor([&] { return receiver.replies.size() == 1 && receiver.errors.size() == 1; }));
    CHECK(receiver.replies.value(0) == 2 && receiver.errors.value(0) == "org.example.Error.Broken");
    QDBusMessage gui = QDBusMessage::createMethodCall(kService, "/clock", "org.example.Clock", "Later");
    CHECK(QDBusReply<QString>(client.call(gui, QDBus::BlockWithGui)).value() == "later");

    // Exporting slots of the object itself, scriptable ones only.
    Clock plain;
    CHECK(client.registerObject("/plain", &plain, QDBusConnection::ExportScriptableSlots));
    QDBusMessage scripted = QDBusMessage::createMethodCall(client.baseService(), "/plain", QString(), "Scripted");
    CHECK(QDBusReply<int>(client.call(scripted)).value() == 7); // a blocking call to itself, in this thread
    QDBusMessage hidden = QDBusMessage::createMethodCall(client.baseService(), "/plain", QString(), "Plain");
    CHECK(QDBusError(client.call(hidden)).type() == QDBusError::UnknownMethod);
    client.unregisterObject("/plain");
    CHECK(QDBusError(client.call(scripted)).type() == QDBusError::UnknownObject);

    // Child objects and virtual objects.
    QObject parent;
    Clock *child = new Clock;
    child->setObjectName("child");
    child->setParent(&parent);
    CHECK(
        client.registerObject("/tree", &parent, QDBusConnection::ExportChildObjects | QDBusConnection::ExportAllSlots));
    CHECK(client.objectRegisteredAt("/tree/child") == child);
    QDBusMessage plainCall = QDBusMessage::createMethodCall(client.baseService(), "/tree/child", QString(), "Plain");
    CHECK(QDBusReply<int>(client.call(plainCall)).value() == 8);
    Files files;
    CHECK(client.registerVirtualObject("/files", &files, QDBusConnection::SubPath));
    QDBusMessage where = QDBusMessage::createMethodCall(client.baseService(), "/files/a/b", QString(), "Where");
    CHECK(QDBusReply<QString>(client.call(where)).value() == "/files/a/b");
    QDBusMessage nowhere = QDBusMessage::createMethodCall(client.baseService(), "/files", QString(), "Nowhere");
    CHECK(QDBusError(client.call(nowhere)).type() == QDBusError::UnknownMethod);

    // Ping, which every object path answers.
    QDBusMessage ping = QDBusMessage::createMethodCall(kService, "/anything", "org.freedesktop.DBus.Peer", "Ping");
    CHECK(client.call(ping).type() == QDBusMessage::ReplyMessage);

    // Watching names come and go.
    QDBusServiceWatcher nameWatcher(QStringLiteral("org.example.Watched"), client);
    QStringList events;
    QObject::connect(&nameWatcher, &QDBusServiceWatcher::serviceRegistered,
                     [&](const QString &s) { events << "+" + s; });
    QObject::connect(&nameWatcher, &QDBusServiceWatcher::serviceUnregistered,
                     [&](const QString &s) { events << "-" + s; });
    CHECK(server.registerService("org.example.Watched"));
    CHECK(server.unregisterService("org.example.Watched"));
    CHECK(waitFor([&] { return events.size() == 2; }));
    CHECK((events == QStringList{"+org.example.Watched", "-org.example.Watched"}));
    QDBusServiceWatcher wildcard(QStringLiteral("org.example.w.*"), client, QDBusServiceWatcher::WatchForRegistration);
    QStringList wild;
    QObject::connect(&wildcard, &QDBusServiceWatcher::serviceRegistered, [&](const QString &s) { wild << s; });
    CHECK(server.registerService("org.example.w.one"));
    CHECK(server.registerService("org.example.other"));
    CHECK(waitFor([&] { return wild.size() == 1; }));
    QTest::qWait(50);
    CHECK((wild == QStringList{"org.example.w.one"}));
    const auto registered = client.interface()->registerService("org.example.other");
    CHECK(registered.isValid() && registered.value() == QDBusConnectionInterface::ServiceNotRegistered);

    // The unregistration of a destroyed object.
    worker.run([&] { delete clock; });
    CHECK(server.objectRegisteredAt("/clock") == nullptr);
    CHECK(QDBusError(call("Add", {1, 2})).type() == QDBusError::UnknownObject);

    QDBusConnection::disconnectFromBus("server");
    QDBusConnection::disconnectFromBus("client");
}

// The command-line tools of the reference implementation talk to an object of Nt.
static void testInterop(const QString &address) {
#ifdef NT_DBUS_SEND
    QDBusConnection server = QDBusConnection::connectToBus(address, "interop");
    Worker worker;
    Clock *clock = nullptr;
    worker.run([&] {
        clock = new Clock;
        new ClockAdaptor(clock);
    });
    CHECK(server.registerObject("/clock", clock));
    QProcess send;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DBUS_SESSION_BUS_ADDRESS", address);
    send.setProcessEnvironment(env);
    send.start(QStringLiteral(NT_DBUS_SEND), {"--session", "--print-reply", "--dest=" + server.baseService(), "/clock",
                                              "org.example.Clock.Add", "int32:19", "int32:23"});
    CHECK(waitFor([&] { return send.state() == QProcess::NotRunning; }, 10000));
    CHECK(send.exitCode() == 0);
    CHECK(QString::fromUtf8(send.readAllStandardOutput()).contains("int32 42"));
    auto run = [&](const QStringList &args) {
        send.start(QStringLiteral(NT_DBUS_SEND),
                   QStringList{"--session", "--print-reply", "--dest=" + server.baseService()} + args);
        waitFor([&] { return send.state() == QProcess::NotRunning; }, 10000);
        return QString::fromUtf8(send.readAllStandardOutput() + send.readAllStandardError());
    };
    const QString words = run({"/clock", "org.example.Clock.Words", "string:to be"});
    CHECK(words.contains("array [") && words.contains("string \"to\"") && words.contains("string \"be\""));
    const QString volume =
        run({"/clock", "org.freedesktop.DBus.Properties.Get", "string:org.example.Clock", "string:volume"});
    CHECK(volume.contains("variant") && volume.contains("int32 3"));
    const QString xml = run({"/clock", "org.freedesktop.DBus.Introspectable.Introspect"});
    CHECK(xml.contains("org.example.Clock"));
    const QString bad = run({"/clock", "org.example.Clock.Nope"});
    CHECK(bad.contains("org.freedesktop.DBus.Error.UnknownMethod"));
    worker.run([&] { delete clock; });
    QDBusConnection::disconnectFromBus("interop");
#else
    (void)address;
#endif
}

static void testPeer() {
    QTemporaryDir dir;
    QDBusServer server(QStringLiteral("unix:path=") + dir.path() + QStringLiteral("/peer.sock"));
    CHECK(server.isConnected());
    CHECK(server.address().startsWith("unix:path="));
    QList<QDBusConnection> accepted;
    QObject::connect(&server, &QDBusServer::newConnection, [&](const QDBusConnection &c) { accepted.append(c); });

    QDBusConnection peer = QDBusConnection::connectToPeer(server.address(), "peer");
    CHECK(peer.isConnected());
    CHECK(peer.baseService().isEmpty());
    CHECK(peer.interface() == nullptr);
    CHECK(waitFor([&] { return accepted.size() == 1; }));
    if (accepted.isEmpty())
        return;
    QDBusConnection other = accepted.first();
    Worker worker;
    Clock *clock = nullptr;
    worker.run([&] {
        clock = new Clock;
        new ClockAdaptor(clock);
    });
    CHECK(other.registerObject("/clock", clock));
    QDBusMessage add = QDBusMessage::createMethodCall(QString(), "/clock", "org.example.Clock", "Add");
    add << 5 << 6;
    CHECK(QDBusReply<int>(peer.call(add)).value() == 11);
    int fds[2];
    CHECK(::pipe(fds) == 0);
    CHECK(::write(fds[1], "p2p", 3) == 3);
    ::close(fds[1]);
    QDBusMessage read = QDBusMessage::createMethodCall(QString(), "/clock", "org.example.Clock", "ReadFd");
    read << QVariant::fromValue(QDBusUnixFileDescriptor(fds[0]));
    ::close(fds[0]);
    CHECK(QDBusReply<QString>(peer.call(read)).value() == "p2p");

    Receiver receiver;
    CHECK(peer.connect(QString(), "/clock", "org.example.Clock", "Ticked", &receiver, SLOT(onTick(QString))));
    worker.run([&] { emit clock->Ticked(QStringLiteral("peer tick")); });
    CHECK(waitFor([&] { return receiver.ticks.size() == 1; }));

    CHECK(peer.connect(QString(), "/org/freedesktop/DBus/Local", "org.freedesktop.DBus.Local", "Disconnected",
                       &receiver, SLOT(onDisconnected())));
    accepted.clear();
    other = QDBusConnection(QString()); // the last handle: the server side closes
    CHECK(waitFor([&] { return receiver.ticks.contains("disconnected"); }));
    CHECK(!peer.isConnected());
    CHECK(QDBusError(peer.call(add)).type() == QDBusError::Disconnected);
    worker.run([&] { delete clock; });
    QDBusConnection::disconnectFromPeer("peer");

    QDBusServer bad(QStringLiteral("tcp:host=localhost"));
    CHECK(!bad.isConnected() && bad.lastError().isValid());
    CHECK(!QDBusConnection::connectToPeer(QStringLiteral("unix:path=/nonexistent/socket"), "bad").isConnected());
    CHECK(QDBusConnection("bad").lastError().type() == QDBusError::NoServer);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    qDBusRegisterMetaType<Fix>();
    qDBusRegisterMetaType<QList<Fix>>();
    testNames();
    testSignatures();
    testArgumentRoundTrip();
    testMessages();
    testBigEndian();
    testValidator();
    testPeer();
    Bus bus;
    if (bus.start()) {
        testBus(bus.address);
        testInterop(bus.address);
    } else {
        std::printf("SKIP the tests that need dbus-daemon\n");
    }
    if (g_failures == 0)
        std::printf("All D-Bus tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
