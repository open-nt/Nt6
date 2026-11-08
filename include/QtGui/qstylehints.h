// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

namespace QtPrivate {
struct GuiAccess;
}

class QStyleHints : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt::ColorScheme colorScheme READ colorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)
public:
    ~QStyleHints() override;

    Qt::ColorScheme colorScheme() const;
    void setColorScheme(Qt::ColorScheme scheme);
    void unsetColorScheme();

signals:
    /*!
        This signal is emitted when colorScheme() changes to \a colorScheme: the desktop
        switches between its light and dark appearance, or setColorScheme() is called.
    */
    void colorSchemeChanged(Qt::ColorScheme colorScheme) { emitSignal(&QStyleHints::colorSchemeChanged, colorScheme); }

private:
    friend struct QtPrivate::GuiAccess;
    friend class QGuiApplication;
    QStyleHints();

    void setPlatformColorScheme(Qt::ColorScheme scheme);

    Qt::ColorScheme m_platform = Qt::ColorScheme::Unknown;
    Qt::ColorScheme m_override = Qt::ColorScheme::Unknown;
};
