// MIT License - see LICENSE
#include "qdbusconnection_p.h"
#include "qdbusmarshaller_p.h"

#include <QtDBus/qdbusinterface.h>
#include <QtDBus/qdbusmetatype.h>

/*!
    \class QDBusInterface
    \brief The QDBusInterface class is a proxy for interfaces on remote objects.

    QDBusInterface calls methods of a remote object by name, without a class written for the
    interface:

    \code
    QDBusInterface clock("org.example.Clock", "/clock", "org.example.Clock");
    QDBusReply<QString> now = clock.call("Now");
    clock.setProperty("Volume", 7);
    \endcode

    When it is constructed, QDBusInterface asks the remote object for its introspection data.
    The arguments of call() and the values of setProperty() are then converted to the types the
    interface declares, so that \c{clock.call("SetAlarm", 7)} sends a \c uint if the method
    takes one.

    \differences
    QDBusInterface has no meta-object built from the introspection data, so its methods,
    signals and properties are not visible to QMetaObject and \c{QObject::connect()} with
    \c SIGNAL() cannot reach the remote signals; connect to them with QDBusConnection::connect().
    property() and setProperty() work as in QDBusAbstractInterface.
    \enddifferences

    \sa QDBusAbstractInterface, QDBusConnection::connect()
*/

namespace {

struct XmlTag {
    QString name;
    bool closing = false;
    bool selfClosing = false;
    QHash<QString, QString> attributes;
};

QString unescapeXml(QString s) {
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    s.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    s.replace(QStringLiteral("&apos;"), QStringLiteral("'"));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return s;
}

// The element tags of an introspection document; text, comments and declarations are skipped.
QList<XmlTag> tagsOf(const QString &xml) {
    QList<XmlTag> tags;
    qsizetype i = 0;
    while ((i = xml.indexOf(QLatin1Char('<'), i)) >= 0) {
        if (xml.mid(i, 4) == QLatin1String("<!--")) {
            const qsizetype end = xml.indexOf(QStringLiteral("-->"), i);
            if (end < 0)
                break;
            i = end + 3;
            continue;
        }
        const qsizetype end = xml.indexOf(QLatin1Char('>'), i);
        if (end < 0)
            break;
        QString body = xml.mid(i + 1, end - i - 1).trimmed();
        i = end + 1;
        if (body.startsWith(QLatin1Char('!')) || body.startsWith(QLatin1Char('?')))
            continue;
        XmlTag tag;
        if (body.startsWith(QLatin1Char('/'))) {
            tag.closing = true;
            body = body.mid(1);
        }
        if (body.endsWith(QLatin1Char('/'))) {
            tag.selfClosing = true;
            body.chop(1);
        }
        qsizetype p = 0;
        while (p < body.size() && !body.at(p).isSpace())
            ++p;
        tag.name = body.left(p);
        while (p < body.size()) {
            while (p < body.size() && body.at(p).isSpace())
                ++p;
            const qsizetype eq = body.indexOf(QLatin1Char('='), p);
            if (eq < 0)
                break;
            const QString key = body.mid(p, eq - p).trimmed();
            qsizetype q = eq + 1;
            while (q < body.size() && body.at(q).isSpace())
                ++q;
            if (q >= body.size())
                break;
            const QChar quote = body.at(q);
            const qsizetype close = body.indexOf(quote, q + 1);
            if (close < 0)
                break;
            tag.attributes.insert(key, unescapeXml(body.mid(q + 1, close - q - 1)));
            p = close + 1;
        }
        tags.append(tag);
    }
    return tags;
}

// Converts `value` to the D-Bus type `signature` where it is a different basic type.
QVariant fitTo(const QVariant &value, const QString &signature) {
    const QByteArray want = signature.toLatin1();
    if (want.isEmpty() || QDBusWriter::signatureOf(value) == want)
        return value;
    if (want == "v")
        return value.metaType() == QMetaType::fromType<QDBusVariant>() ? value
                                                                       : QVariant::fromValue(QDBusVariant(value));
    const QMetaType target = QDBusMetaType::signatureToMetaType(want.constData());
    if (!target.isValid() || target == QMetaType::fromType<QDBusArgument>())
        return value;
    bool ok = false;
    const QVariant converted = qDBusAdaptArgument(value, target, &ok);
    return ok ? converted : value;
}

} // namespace

class QDBusInterfacePrivate {
public:
    QMultiHash<QString, QStringList> methods; // the input signatures of each overload
    QHash<QString, QString> properties;
};

/*!
    Constructs an interface to \a interface of the object at \a path in \a service, reached
    through \a connection, with the given \a parent. If \a interface is empty, calls carry no
    interface and the members of all interfaces of the object are known.
*/
QDBusInterface::QDBusInterface(const QString &service, const QString &path, const QString &interface,
                               const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, interface, connection, parent, false), dd(new QDBusInterfacePrivate) {
    if (!connection.isConnected())
        return;
    QDBusMessage m = QDBusMessage::createMethodCall(
        service, path, QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
    const QDBusMessage reply = connection.call(m, QDBus::Block, timeout());
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;
    bool inInterface = false;
    QString method;
    QStringList args;
    for (const XmlTag &tag : tagsOf(reply.arguments().first().toString())) {
        if (tag.name == QLatin1String("interface")) {
            inInterface =
                !tag.closing && (interface.isEmpty() || tag.attributes.value(QStringLiteral("name")) == interface);
        } else if (!inInterface) {
            continue;
        } else if (tag.name == QLatin1String("method")) {
            if (tag.closing || tag.selfClosing) {
                const QString name = tag.closing ? method : tag.attributes.value(QStringLiteral("name"));
                if (!name.isEmpty())
                    dd->methods.insert(name, tag.closing ? args : QStringList());
                method.clear();
                args.clear();
            } else {
                method = tag.attributes.value(QStringLiteral("name"));
                args.clear();
            }
        } else if (tag.name == QLatin1String("arg") && !method.isEmpty()) {
            if (tag.attributes.value(QStringLiteral("direction"), QStringLiteral("in")) == QLatin1String("in"))
                args.append(tag.attributes.value(QStringLiteral("type")));
        } else if (tag.name == QLatin1String("property") && !tag.closing) {
            dd->properties.insert(tag.attributes.value(QStringLiteral("name")),
                                  tag.attributes.value(QStringLiteral("type")));
        }
    }
}

/*!
    Destroys the interface.
*/
QDBusInterface::~QDBusInterface() = default;

QList<QVariant> QDBusInterface::fitArguments(const QString &method, const QList<QVariant> &args) const {
    const QList<QStringList> overloads = dd->methods.values(method);
    for (const QStringList &types : overloads) {
        if (types.size() != args.size())
            continue;
        QList<QVariant> fitted;
        for (qsizetype i = 0; i < args.size(); ++i)
            fitted.append(fitTo(args.at(i), types.at(i)));
        return fitted;
    }
    return args;
}

QVariant QDBusInterface::fitProperty(const char *name, const QVariant &value) const {
    return fitTo(value, dd->properties.value(QString::fromLatin1(name)));
}

#include "moc_qdbusinterface.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
