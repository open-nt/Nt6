// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qrect.h>
#include <QtGui/qicon.h>

#include <memory>

class QMenu;

class QSystemTrayIcon : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible)
public:
    enum ActivationReason { Unknown, Context, DoubleClick, Trigger, MiddleClick };
    Q_ENUM(ActivationReason)
    enum MessageIcon { NoIcon, Information, Warning, Critical };
    Q_ENUM(MessageIcon)

    explicit QSystemTrayIcon(QObject *parent = nullptr);
    explicit QSystemTrayIcon(const QIcon &icon, QObject *parent = nullptr);
    ~QSystemTrayIcon() override;

    void setContextMenu(QMenu *menu);
    QMenu *contextMenu() const;
    QIcon icon() const { return m_icon; }
    void setIcon(const QIcon &icon);
    QString toolTip() const { return m_toolTip; }
    void setToolTip(const QString &tip);
    QRect geometry() const;
    bool isVisible() const { return m_visible; }

    static bool isSystemTrayAvailable();
    static bool supportsMessages();

public slots:
    void setVisible(bool visible);
    void show() { setVisible(true); }
    void hide() { setVisible(false); }
    void showMessage(const QString &title, const QString &msg, QSystemTrayIcon::MessageIcon icon = Information,
                     int msecs = 10000);
    void showMessage(const QString &title, const QString &msg, const QIcon &icon, int msecs = 10000);

signals:
    /*!
        This signal is emitted when the user activates the icon, for the \a reason.
    */
    void activated(QSystemTrayIcon::ActivationReason reason) { emitSignal(&QSystemTrayIcon::activated, reason); }
    /*!
        This signal is emitted when the user clicks a message that showMessage() showed.
    */
    void messageClicked() { emitSignal(&QSystemTrayIcon::messageClicked); }

private slots:
    void notificationAction(uint id, const QString &action);

private:
    friend struct QSystemTrayIconPrivate;
    void notify(const QString &title, const QString &msg, const QString &iconName, int msecs);
    Q_DISABLE_COPY(QSystemTrayIcon)
    QIcon m_icon;
    QString m_toolTip;
    QPointer<QMenu> m_menu;
    bool m_visible = false;
    std::unique_ptr<struct QSystemTrayIconPrivate> d;
};
