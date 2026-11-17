// MIT License - see LICENSE
#include <QtWidgets/qsystemtrayicon.h>

#include <QtCore/qcoreapplication.h>
#include <QtDBus/qdbusargument.h>
#include <QtDBus/qdbusconnection.h>
#include <QtDBus/qdbusconnectioninterface.h>
#include <QtDBus/qdbusextratypes.h>
#include <QtDBus/qdbusmessage.h>
#include <QtDBus/qdbusmetatype.h>
#include <QtDBus/qdbuspendingcall.h>
#include <QtDBus/qdbusvirtualobject.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>
#include <QtWidgets/qmenu.h>

#include <atomic>

namespace QtPrivate {

// The icon of a StatusNotifierItem: (iiay), the pixels as ARGB32 in network byte order.
struct SniPixmap {
    int width = 0;
    int height = 0;
    QByteArray bytes;
};

// The tool tip: (sa(iiay)ss), an icon name, icon pixmaps, a title and a text.
struct SniToolTip {
    QString iconName;
    QList<SniPixmap> pixmaps;
    QString title;
    QString text;
};

inline QDBusArgument &operator<<(QDBusArgument &argument, const SniPixmap &pixmap) {
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.bytes;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SniPixmap &pixmap) {
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.bytes;
    argument.endStructure();
    return argument;
}

inline QDBusArgument &operator<<(QDBusArgument &argument, const SniToolTip &tip) {
    argument.beginStructure();
    argument << tip.iconName << tip.pixmaps << tip.title << tip.text;
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, SniToolTip &tip) {
    argument.beginStructure();
    argument >> tip.iconName >> tip.pixmaps >> tip.title >> tip.text;
    argument.endStructure();
    return argument;
}

} // namespace QtPrivate

namespace {

constexpr const char *kItemInterface = "org.kde.StatusNotifierItem";
constexpr const char *kItemPath = "/StatusNotifierItem";
constexpr const char *kWatcherService = "org.kde.StatusNotifierWatcher";

void registerTypes() {
    static const bool done = [] {
        qDBusRegisterMetaType<QtPrivate::SniPixmap>();
        qDBusRegisterMetaType<QList<QtPrivate::SniPixmap>>();
        qDBusRegisterMetaType<QtPrivate::SniToolTip>();
        return true;
    }();
    (void)done;
}

QList<QtPrivate::SniPixmap> pixmapsOf(const QIcon &icon) {
    QList<QtPrivate::SniPixmap> list;
    if (icon.isNull())
        return list;
    for (const int extent : {16, 22, 32, 48, 64}) {
        const QImage image =
            icon.pixmap(QSize(extent, extent)).toImage().convertToFormat(QImage::Format_ARGB32);
        if (image.isNull())
            continue;
        QtPrivate::SniPixmap pixmap;
        pixmap.width = image.width();
        pixmap.height = image.height();
        pixmap.bytes.reserve(static_cast<qsizetype>(image.width()) * image.height() * 4);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb pixel = image.pixel(x, y);
                pixmap.bytes.append(static_cast<char>(qAlpha(pixel)));
                pixmap.bytes.append(static_cast<char>(qRed(pixel)));
                pixmap.bytes.append(static_cast<char>(qGreen(pixel)));
                pixmap.bytes.append(static_cast<char>(qBlue(pixel)));
            }
        }
        list.append(pixmap);
    }
    return list;
}

} // namespace

// The StatusNotifierItem object of a tray icon on the session bus.
struct QSystemTrayIconPrivate : public QDBusVirtualObject {
    explicit QSystemTrayIconPrivate(QSystemTrayIcon *icon) : q(icon) {}

    QSystemTrayIcon *q;
    QString service; // the bus name of the item while it is shown
    uint notification = 0; // the id of the last notification, which a new one replaces
    bool listening = false;

    QVariant property(const QString &name) const {
        if (name == QLatin1String("Category"))
            return QStringLiteral("ApplicationStatus");
        if (name == QLatin1String("Id"))
            return QCoreApplication::applicationName();
        if (name == QLatin1String("Title"))
            return QGuiApplication::applicationDisplayName();
        if (name == QLatin1String("Status"))
            return QStringLiteral("Active");
        if (name == QLatin1String("WindowId"))
            return 0;
        if (name == QLatin1String("IconName"))
            return q->m_icon.name();
        if (name == QLatin1String("IconPixmap"))
            return QVariant::fromValue(pixmapsOf(q->m_icon));
        if (name == QLatin1String("OverlayIconName") || name == QLatin1String("AttentionIconName") ||
            name == QLatin1String("AttentionMovieName") || name == QLatin1String("IconThemePath"))
            return QString();
        if (name == QLatin1String("ToolTip")) {
            QtPrivate::SniToolTip tip;
            tip.iconName = q->m_icon.name();
            tip.title = q->m_toolTip;
            return QVariant::fromValue(tip);
        }
        if (name == QLatin1String("ItemIsMenu"))
            return false;
        if (name == QLatin1String("Menu"))
            return QVariant::fromValue(QDBusObjectPath(QStringLiteral("/NO_DBUSMENU")));
        return QVariant();
    }

    QString introspect(const QString &path) const override {
        (void)path;
        return QStringLiteral(
            "<interface name=\"org.kde.StatusNotifierItem\">"
            "<property name=\"Category\" type=\"s\" access=\"read\"/>"
            "<property name=\"Id\" type=\"s\" access=\"read\"/>"
            "<property name=\"Title\" type=\"s\" access=\"read\"/>"
            "<property name=\"Status\" type=\"s\" access=\"read\"/>"
            "<property name=\"WindowId\" type=\"i\" access=\"read\"/>"
            "<property name=\"IconName\" type=\"s\" access=\"read\"/>"
            "<property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>"
            "<property name=\"ToolTip\" type=\"(sa(iiay)ss)\" access=\"read\"/>"
            "<property name=\"ItemIsMenu\" type=\"b\" access=\"read\"/>"
            "<property name=\"Menu\" type=\"o\" access=\"read\"/>"
            "<method name=\"Activate\"><arg name=\"x\" type=\"i\" direction=\"in\"/>"
            "<arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
            "<method name=\"SecondaryActivate\"><arg name=\"x\" type=\"i\" direction=\"in\"/>"
            "<arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
            "<method name=\"ContextMenu\"><arg name=\"x\" type=\"i\" direction=\"in\"/>"
            "<arg name=\"y\" type=\"i\" direction=\"in\"/></method>"
            "<method name=\"Scroll\"><arg name=\"delta\" type=\"i\" direction=\"in\"/>"
            "<arg name=\"orientation\" type=\"s\" direction=\"in\"/></method>"
            "<signal name=\"NewIcon\"/><signal name=\"NewToolTip\"/>"
            "<signal name=\"NewStatus\"><arg name=\"status\" type=\"s\"/></signal>"
            "</interface>");
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override {
        const QString member = message.member();
        const QList<QVariant> args = message.arguments();
        if (message.interface() == QLatin1String("org.freedesktop.DBus.Properties")) {
            if (member == QLatin1String("Get") && args.size() == 2) {
                const QVariant value = property(args.at(1).toString());
                if (!value.isValid())
                    return connection.send(message.createErrorReply(
                        QStringLiteral("org.freedesktop.DBus.Error.UnknownProperty"), args.at(1).toString()));
                return connection.send(message.createReply(QVariant::fromValue(QDBusVariant(value))));
            }
            if (member == QLatin1String("GetAll")) {
                QVariantMap all;
                for (const char *name : {"Category", "Id", "Title", "Status", "WindowId", "IconName", "IconPixmap",
                                         "ToolTip", "ItemIsMenu", "Menu"})
                    all.insert(QString::fromLatin1(name), property(QString::fromLatin1(name)));
                return connection.send(message.createReply(QVariant::fromValue(all)));
            }
            return false;
        }
        if (member == QLatin1String("Activate")) {
            emit q->activated(QSystemTrayIcon::Trigger);
        } else if (member == QLatin1String("SecondaryActivate")) {
            emit q->activated(QSystemTrayIcon::MiddleClick);
        } else if (member == QLatin1String("ContextMenu")) {
            emit q->activated(QSystemTrayIcon::Context);
            if (q->m_menu && args.size() == 2)
                q->m_menu->popup(QPoint(args.at(0).toInt(), args.at(1).toInt()));
        } else if (member != QLatin1String("Scroll")) {
            return false;
        }
        return connection.send(message.createReply());
    }

    void emitItemSignal(const char *name, const QList<QVariant> &arguments = {}) {
        if (service.isEmpty())
            return;
        QDBusMessage signal = QDBusMessage::createSignal(QString::fromLatin1(kItemPath),
                                                         QString::fromLatin1(kItemInterface), QString::fromLatin1(name));
        signal.setArguments(arguments);
        QDBusConnection::sessionBus().send(signal);
    }

    void show() {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!service.isEmpty() || !bus.isConnected())
            return;
        registerTypes();
        static std::atomic<int> counter{0};
        service = QStringLiteral("org.kde.StatusNotifierItem-%1-%2")
                      .arg(QCoreApplication::applicationPid())
                      .arg(++counter);
        if (!bus.registerService(service) || !bus.registerVirtualObject(QString::fromLatin1(kItemPath), this)) {
            service.clear();
            return;
        }
        QDBusMessage call = QDBusMessage::createMethodCall(QString::fromLatin1(kWatcherService),
                                                           QStringLiteral("/StatusNotifierWatcher"),
                                                           QString::fromLatin1(kWatcherService),
                                                           QStringLiteral("RegisterStatusNotifierItem"));
        call.setArguments({service});
        bus.asyncCall(call);
    }

    void hide() {
        if (service.isEmpty())
            return;
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.unregisterObject(QString::fromLatin1(kItemPath));
        bus.unregisterService(service);
        service.clear();
    }
};

/*!
    \class QSystemTrayIcon
    \inmodule NtWidgets
    \since 6.0
    \brief The QSystemTrayIcon class puts an icon in the system tray of the desktop.

    The icon shows the application in the tray, or the panel, of the desktop. A click on it emits activated(), and
    the context menu opens on a right click; showMessage() shows a notification:

    \code
    auto *tray = new QSystemTrayIcon(QIcon::fromTheme("network-wireless"), this);
    tray->setToolTip("Radio link");
    tray->setContextMenu(menu);
    tray->show();
    tray->showMessage("Radio link", "Connected to the base station");
    \endcode

    On Linux the icon is a StatusNotifierItem on the D-Bus session bus, as KDE Plasma, GNOME with the
    AppIndicator extension, and most other desktops show; messages are desktop notifications.

    \differences
    There is no XEmbed tray: a desktop without a StatusNotifierItem host shows nothing, and isSystemTrayAvailable()
    says so. The context menu opens as a menu of the application where the desktop asks for it, not as a menu of
    the desktop exported with \c com.canonical.dbusmenu. geometry() is unknown, and there is no double click. Only
    Linux has a tray icon yet.
    \enddifferences
*/

/*!
    \enum QSystemTrayIcon::ActivationReason

    How the user activated the icon.

    \value Unknown For no known reason.
    \value Context The context menu was asked for.
    \value DoubleClick The icon was double-clicked.
    \value Trigger The icon was clicked.
    \value MiddleClick The icon was clicked with the middle button.
*/

/*!
    \enum QSystemTrayIcon::MessageIcon

    The icon of a message.

    \value NoIcon None.
    \value Information An information icon.
    \value Warning A warning icon.
    \value Critical An error icon.
*/

/*!
    Constructs a tray icon without an icon, with the given \a parent.
*/
QSystemTrayIcon::QSystemTrayIcon(QObject *parent)
    : QObject(parent), d(std::make_unique<QSystemTrayIconPrivate>(this)) {}

/*!
    Constructs a tray icon with \a icon and the given \a parent.
*/
QSystemTrayIcon::QSystemTrayIcon(const QIcon &icon, QObject *parent) : QSystemTrayIcon(parent) { m_icon = icon; }

/*!
    Destroys the tray icon, which leaves the tray.
*/
QSystemTrayIcon::~QSystemTrayIcon() { d->hide(); }

/*!
    Sets the menu that opens when the user asks for the context menu of the icon to \a menu; the tray icon does not
    take ownership of it.
*/
void QSystemTrayIcon::setContextMenu(QMenu *menu) { m_menu = menu; }

/*!
    Returns the context menu, or \c nullptr.
*/
QMenu *QSystemTrayIcon::contextMenu() const { return m_menu.data(); }

/*!
    \fn QIcon QSystemTrayIcon::icon() const

    Returns the icon.
*/

/*!
    Sets the icon to \a icon, which the desktop shows from its theme name when it has one and from its pixels
    otherwise.
*/
void QSystemTrayIcon::setIcon(const QIcon &icon) {
    m_icon = icon;
    d->emitItemSignal("NewIcon");
}

/*!
    \fn QString QSystemTrayIcon::toolTip() const

    Returns the tool tip of the icon.
*/

/*!
    Sets the tool tip of the icon to \a tip.
*/
void QSystemTrayIcon::setToolTip(const QString &tip) {
    m_toolTip = tip;
    d->emitItemSignal("NewToolTip");
}

/*!
    Returns where the icon is on the screen: an empty rectangle, as the desktop does not tell.
*/
QRect QSystemTrayIcon::geometry() const { return QRect(); }

/*!
    \fn bool QSystemTrayIcon::isVisible() const

    Returns \c true if the icon was shown.
*/

/*!
    Shows the icon in the tray if \a visible is \c true, or takes it out.
*/
void QSystemTrayIcon::setVisible(bool visible) {
    if (visible == m_visible)
        return;
    m_visible = visible;
    if (visible)
        d->show();
    else
        d->hide();
}

/*!
    \fn void QSystemTrayIcon::show()

    Shows the icon in the tray.
*/

/*!
    \fn void QSystemTrayIcon::hide()

    Takes the icon out of the tray.
*/

/*!
    Returns \c true if the desktop has a tray: a StatusNotifierItem host on the session bus.
*/
bool QSystemTrayIcon::isSystemTrayAvailable() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    return bus.interface()->isServiceRegistered(QString::fromLatin1(kWatcherService)).value();
}

/*!
    Returns \c true if the desktop shows notifications: a notification server on the session bus.
*/
bool QSystemTrayIcon::supportsMessages() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface())
        return false;
    return bus.interface()->isServiceRegistered(QStringLiteral("org.freedesktop.Notifications")).value();
}

/*!
    Shows the notification \a title with the text \a msg and \a icon for \a msecs milliseconds, as the notification
    server of the desktop shows it. A click on it emits messageClicked().
*/
void QSystemTrayIcon::showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon,
                                  int msecs) {
    static const char *const names[] = {"", "dialog-information", "dialog-warning", "dialog-error"};
    notify(title, msg, QString::fromLatin1(names[icon]), msecs);
}

/*!
    \overload

    Shows the notification \a title with the text \a msg for \a msecs milliseconds, with the theme icon of \a icon
    if it has one.
*/
void QSystemTrayIcon::showMessage(const QString &title, const QString &msg, const QIcon &icon, int msecs) {
    notify(title, msg, icon.name(), msecs);
}

void QSystemTrayIcon::notify(const QString &title, const QString &msg, const QString &iconName, int msecs) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    const QString service = QStringLiteral("org.freedesktop.Notifications");
    const QString path = QStringLiteral("/org/freedesktop/Notifications");
    // A click on the notification comes back as ActionInvoked of its id.
    if (!d->listening)
        d->listening = bus.connect(service, path, service, QStringLiteral("ActionInvoked"), this,
                                   SLOT(notificationAction(uint,QString)));
    QDBusMessage call = QDBusMessage::createMethodCall(service, path, service, QStringLiteral("Notify"));
    call.setArguments({QGuiApplication::applicationDisplayName(), d->notification, iconName, title, msg,
                       QStringList{QStringLiteral("default"), QStringLiteral("Open")}, QVariantMap(), msecs});
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        const QDBusMessage reply = w->reply();
        if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
            d->notification = reply.arguments().constFirst().toUInt();
        w->deleteLater();
    });
}

void QSystemTrayIcon::notificationAction(uint id, const QString &action) {
    if (id != 0 && id == d->notification && action == QLatin1String("default"))
        emit messageClicked();
}

#include "moc_qsystemtrayicon.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
