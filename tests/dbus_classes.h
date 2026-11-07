// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qtimer.h>
#include <QtDBus/QtDBus>

#include <unistd.h>

struct Fix {
    double lat = 0;
    double lon = 0;
    QString name;
    bool operator==(const Fix &o) const { return lat == o.lat && lon == o.lon && name == o.name; }
};
Q_DECLARE_METATYPE(Fix)

inline QDBusArgument &operator<<(QDBusArgument &arg, const Fix &f) {
    arg.beginStructure();
    arg << f.lat << f.lon << f.name;
    arg.endStructure();
    return arg;
}

inline const QDBusArgument &operator>>(const QDBusArgument &arg, Fix &f) {
    arg.beginStructure();
    arg >> f.lat >> f.lon >> f.name;
    arg.endStructure();
    return arg;
}

class Clock : public QObject {
    Q_OBJECT
    Q_PROPERTY(int hidden READ hidden)
public:
    int hidden() const { return 1; }
public slots:
    Q_SCRIPTABLE int Scripted() { return 7; }
    int Plain() { return 8; }
signals:
    void Ticked(const QString &text);
    Q_SCRIPTABLE void Rang(int times);
};

class ClockAdaptor : public QDBusAbstractAdaptor, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.example.Clock")
    Q_PROPERTY(int volume READ volume WRITE setVolume)
    Q_PROPERTY(QString name READ name)
public:
    explicit ClockAdaptor(Clock *parent) : QDBusAbstractAdaptor(parent) { setAutoRelaySignals(true); }
    int volume() const { return m_volume; }
    void setVolume(int v) { m_volume = v; }
    QString name() const { return QStringLiteral("kitchen"); }

    int m_volume = 3;
    QString lastFired;

public slots:
    int Add(int a, int b) { return a + b; }
    QString Echo(const QString &s) { return s; }
    void Split(const QString &in, QString &first, int &count) {
        const QStringList parts = in.split(QLatin1Char(' '));
        first = parts.value(0);
        count = static_cast<int>(parts.size());
    }
    QString Sender(const QDBusMessage &message) { return message.service(); }
    Fix Move(const Fix &f, double dlat) { return Fix{f.lat + dlat, f.lon, f.name}; }
    QVariantMap Describe(const QVariantMap &m) {
        QVariantMap r = m;
        r.insert(QStringLiteral("count"), static_cast<int>(m.size()));
        return r;
    }
    QDBusVariant Wrap(const QDBusVariant &v) { return v; }
    QString TypeOf(const QVariant &v) { return QString::fromLatin1(v.typeName()); }
    QByteArray Bytes(const QByteArray &b) { return b + "!"; }
    QStringList Words(const QString &s) { return s.split(QLatin1Char(' ')); }
    QList<int> Squares(const QList<int> &in) {
        QList<int> r;
        for (int i : in)
            r.append(i * i);
        return r;
    }
    qlonglong Big(qulonglong v) { return -static_cast<qlonglong>(v); }
    Q_NOREPLY void Fire(const QString &text) { lastFired = text; }
    QString ReadFd(const QDBusUnixFileDescriptor &fd) {
        char buffer[64];
        const ssize_t n = ::read(fd.fileDescriptor(), buffer, sizeof buffer);
        return n > 0 ? QString::fromLatin1(buffer, static_cast<qsizetype>(n)) : QString();
    }
    QString Fail() {
        sendErrorReply(QStringLiteral("org.example.Error.Broken"), QStringLiteral("it broke"));
        return QString();
    }
    QString Later() {
        setDelayedReply(true);
        const QDBusMessage call = message();
        const QDBusConnection conn = connection();
        QTimer::singleShot(20, this, [call, conn] { conn.send(call.createReply(QStringLiteral("later"))); });
        return QString();
    }
    void Never() { setDelayedReply(true); }
    bool CalledFromDBus() { return calledFromDBus(); }

public:
    bool calledFromDBusPublic() const { return calledFromDBus(); }

signals:
    void Ticked(const QString &text);
};

class ClockProxy : public QDBusAbstractInterface {
    Q_OBJECT
public:
    ClockProxy(const QString &service, const QString &path, const QDBusConnection &connection,
               QObject *parent = nullptr)
        : QDBusAbstractInterface(service, path, "org.example.Clock", connection, parent) {}
    QDBusPendingReply<int> Add(int a, int b) { return asyncCall(QStringLiteral("Add"), a, b); }
signals:
    void Ticked(const QString &text);
};

class Receiver : public QObject {
    Q_OBJECT
public:
    QStringList ticks;
    QStringList senders;
    QList<int> replies;
    QStringList errors;
    QStringList owners;
public slots:
    void onTick(const QString &text) { ticks.append(text); }
    void onTickMessage(const QString &text, const QDBusMessage &m) {
        ticks.append(text);
        senders.append(m.service());
    }
    void onAny(const QDBusMessage &m) { senders.append(m.member()); }
    void onReply(int value) { replies.append(value); }
    void onError(const QDBusError &error) { errors.append(error.name()); }
    void onDisconnected() { ticks.append(QStringLiteral("disconnected")); }
};

class Files : public QDBusVirtualObject {
    Q_OBJECT
public:
    QString introspect(const QString &) const override {
        return QStringLiteral("  <interface name=\"org.example.Files\"><method name=\"Where\"><arg type=\"s\" "
                              "direction=\"out\"/></method></interface>\n");
    }
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
        if (message.member() != QLatin1String("Where"))
            return false;
        connection.send(message.createReply(message.path()));
        return true;
    }
};
