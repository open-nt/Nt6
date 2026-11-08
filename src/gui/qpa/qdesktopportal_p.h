// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

class QDBusVariant;

// Reads the appearance settings of the desktop from the XDG desktop portal
// (org.freedesktop.portal.Settings) and follows them as they change.
class QDesktopPortalSettings : public QObject {
    Q_OBJECT
public:
    explicit QDesktopPortalSettings(QObject *parent = nullptr);

    // Unknown if there is no session bus or no portal, or it has no color scheme.
    Qt::ColorScheme colorScheme() const { return m_scheme; }

    // Converts the value of org.freedesktop.appearance color-scheme: 1 is dark and 2 is light.
    static Qt::ColorScheme schemeFromValue(unsigned value);

signals:
    void colorSchemeChanged(Qt::ColorScheme scheme) { emitSignal(&QDesktopPortalSettings::colorSchemeChanged, scheme); }

private slots:
    void settingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value);

private:
    Qt::ColorScheme m_scheme = Qt::ColorScheme::Unknown;
};
