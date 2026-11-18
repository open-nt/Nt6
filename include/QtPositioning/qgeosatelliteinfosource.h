// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtPositioning/qgeosatelliteinfo.h>

#include <functional>

class QGeoSatelliteInfoSource : public QObject {
    Q_OBJECT
    Q_PROPERTY(int updateInterval READ updateInterval WRITE setUpdateInterval)
    Q_PROPERTY(int minimumUpdateInterval READ minimumUpdateInterval)

public:
    enum Error { AccessError = 0, ClosedError = 1, NoError = 2, UnknownSourceError = -1, UpdateTimeoutError = 3 };
    Q_ENUM(Error)

    using Factory = std::function<QGeoSatelliteInfoSource *(const QVariantMap &parameters, QObject *parent)>;

    explicit QGeoSatelliteInfoSource(QObject *parent);
    ~QGeoSatelliteInfoSource() override;

    static QGeoSatelliteInfoSource *createDefaultSource(QObject *parent);
    static QGeoSatelliteInfoSource *createDefaultSource(const QVariantMap &parameters, QObject *parent);
    static QGeoSatelliteInfoSource *createSource(const QString &sourceName, QObject *parent);
    static QGeoSatelliteInfoSource *createSource(const QString &sourceName, const QVariantMap &parameters,
                                                 QObject *parent);
    static QStringList availableSources();
    static void registerSource(const QString &sourceName, const Factory &factory, int priority = 0);

    QString sourceName() const { return m_sourceName; }

    virtual void setUpdateInterval(int msec);
    int updateInterval() const { return m_updateInterval; }
    virtual int minimumUpdateInterval() const = 0;
    virtual Error error() const = 0;

    virtual bool setBackendProperty(const QString &name, const QVariant &value);
    virtual QVariant backendProperty(const QString &name) const;

public slots:
    virtual void startUpdates() = 0;
    virtual void stopUpdates() = 0;
    virtual void requestUpdate(int timeout = 0) = 0;

signals:
    void satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &satellites);
    void satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &satellites);
    void errorOccurred(QGeoSatelliteInfoSource::Error error);

private:
    int m_updateInterval = 0;
    QString m_sourceName;
};
