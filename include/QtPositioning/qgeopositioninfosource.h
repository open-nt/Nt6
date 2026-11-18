// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtPositioning/qgeopositioninfo.h>

#include <functional>

class QGeoPositionInfoSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval)
    Q_PROPERTY(int minimumUpdateInterval READ minimumUpdateInterval)
    Q_PROPERTY(QString sourceName READ sourceName)
    Q_PROPERTY(PositioningMethods preferredPositioningMethods READ preferredPositioningMethods WRITE
                   setPreferredPositioningMethods)

public:
    enum Error { AccessError = 0, ClosedError = 1, UnknownSourceError = 2, NoError = 3, UpdateTimeoutError = 4 };
    Q_ENUM(Error)

    enum PositioningMethod {
        NoPositioningMethods = 0x00000000,
        SatellitePositioningMethods = 0x000000ff,
        NonSatellitePositioningMethods = 0xffffff00,
        AllPositioningMethods = 0xffffffff
    };
    Q_DECLARE_FLAGS(PositioningMethods, PositioningMethod)
    Q_FLAG(PositioningMethods)

    using Factory = std::function<QGeoPositionInfoSource *(const QVariantMap &parameters, QObject *parent)>;

    explicit QGeoPositionInfoSource(QObject *parent);
    ~QGeoPositionInfoSource() override;

    virtual void setUpdateInterval(int msec);
    int updateInterval() const { return m_updateInterval; }
    virtual void setPreferredPositioningMethods(PositioningMethods methods);
    PositioningMethods preferredPositioningMethods() const { return m_preferredMethods; }

    virtual QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const = 0;
    virtual PositioningMethods supportedPositioningMethods() const = 0;
    virtual int minimumUpdateInterval() const = 0;
    QString sourceName() const { return m_sourceName; }

    virtual bool setBackendProperty(const QString &name, const QVariant &value);
    virtual QVariant backendProperty(const QString &name) const;

    static QGeoPositionInfoSource *createDefaultSource(QObject *parent);
    static QGeoPositionInfoSource *createDefaultSource(const QVariantMap &parameters, QObject *parent);
    static QGeoPositionInfoSource *createSource(const QString &sourceName, QObject *parent);
    static QGeoPositionInfoSource *createSource(const QString &sourceName, const QVariantMap &parameters,
                                                QObject *parent);
    static QStringList availableSources();
    static void registerSource(const QString &sourceName, const Factory &factory, int priority = 0);

    virtual Error error() const = 0;

public slots:
    virtual void startUpdates() = 0;
    virtual void stopUpdates() = 0;
    virtual void requestUpdate(int timeout = 0) = 0;

signals:
    void positionUpdated(const QGeoPositionInfo &update);
    void errorOccurred(QGeoPositionInfoSource::Error error);
    void supportedPositioningMethodsChanged();

private:
    int m_updateInterval = 0;
    PositioningMethods m_preferredMethods = AllPositioningMethods;
    QString m_sourceName;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGeoPositionInfoSource::PositioningMethods)
