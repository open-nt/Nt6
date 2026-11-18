// MIT License - see LICENSE
//
// QGeoSatelliteInfoSource and its registry of sources, the counterpart of QGeoPositionInfoSource's.
#include "qgeosatelliteinfosource.h"

#include <QtCore/qmap.h>
#include <QtCore/qmutex.h>

#include <algorithm>

namespace {

struct Registered {
    QGeoSatelliteInfoSource::Factory factory;
    int priority = 0;
};

QMutex &registryMutex() {
    static QMutex mutex;
    return mutex;
}

QMap<QString, Registered> &registry() {
    static QMap<QString, Registered> map;
    return map;
}

} // namespace

/*!
    \class QGeoSatelliteInfoSource
    \inmodule NtPositioning
    \brief The QGeoSatelliteInfoSource class is the abstract base of satellite information sources.

    A source reports the satellites a receiver can see, through satellitesInViewUpdated(), and
    those it uses for its fix, through satellitesInUseUpdated(): continuously between
    startUpdates() and stopUpdates(), or once after requestUpdate().

    \differences
    Sources are not found through plugins. A library registers a factory with registerSource(),
    which is Nt-only; Nt itself registers none.
    \enddifferences

    \sa QNmeaSatelliteInfoSource, QGeoSatelliteInfo, QGeoPositionInfoSource
*/

/*!
    \enum QGeoSatelliteInfoSource::Error

    \value AccessError The source could not be opened or read, or lacks permission.
    \value ClosedError The source was closed, for example because the device disconnected.
    \value NoError No error has occurred.
    \value UnknownSourceError An error that fits no other value.
    \value UpdateTimeoutError requestUpdate() got no satellite information within its timeout.
*/

/*!
    \typedef QGeoSatelliteInfoSource::Factory
    A function that creates a source from a map of parameters and a parent; see registerSource().
*/

/*!
    Constructs a source with the given \a parent.
*/
QGeoSatelliteInfoSource::QGeoSatelliteInfoSource(QObject *parent) : QObject(parent) {}

/*!
    Destroys the source.
*/
QGeoSatelliteInfoSource::~QGeoSatelliteInfoSource() = default;

/*!
    Creates the registered source with the highest priority, with the given \a parent, or returns
    \c nullptr if none is registered.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createDefaultSource(QObject *parent) {
    return createDefaultSource(QVariantMap(), parent);
}

/*!
    \overload
    Passes \a parameters to the source's factory, and \a parent as its parent.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createDefaultSource(const QVariantMap &parameters, QObject *parent) {
    for (const QString &name : availableSources()) {
        if (QGeoSatelliteInfoSource *source = createSource(name, parameters, parent))
            return source;
    }
    return nullptr;
}

/*!
    Creates the source registered as \a sourceName with the given \a parent, or returns \c nullptr
    if there is none.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createSource(const QString &sourceName, QObject *parent) {
    return createSource(sourceName, QVariantMap(), parent);
}

/*!
    \overload
    Passes \a parameters to the factory of \a sourceName, and \a parent as its parent.
*/
QGeoSatelliteInfoSource *QGeoSatelliteInfoSource::createSource(const QString &sourceName, const QVariantMap &parameters,
                                                               QObject *parent) {
    Factory factory;
    {
        const QMutexLocker locker(&registryMutex());
        const auto it = registry().constFind(sourceName);
        if (it == registry().constEnd())
            return nullptr;
        factory = it->factory;
    }
    QGeoSatelliteInfoSource *source = factory ? factory(parameters, parent) : nullptr;
    if (source)
        source->m_sourceName = sourceName;
    return source;
}

/*!
    Returns the names of the registered sources, highest priority first.
*/
QStringList QGeoSatelliteInfoSource::availableSources() {
    QList<std::pair<int, QString>> entries;
    {
        const QMutexLocker locker(&registryMutex());
        for (auto it = registry().constBegin(); it != registry().constEnd(); ++it)
            entries.append({it->priority, it.key()});
    }
    std::stable_sort(
        entries.begin(), entries.end(),
        [](const std::pair<int, QString> &a, const std::pair<int, QString> &b) { return a.first > b.first; });
    QStringList names;
    for (const auto &entry : entries)
        names.append(entry.second);
    return names;
}

/*!
    Registers \a factory as the source named \a sourceName, replacing any earlier registration of
    that name. createDefaultSource() tries sources in order of decreasing \a priority. A null
    factory removes the registration.

    \note This function is Nt-only; Qt finds sources through plugins.
*/
void QGeoSatelliteInfoSource::registerSource(const QString &sourceName, const Factory &factory, int priority) {
    const QMutexLocker locker(&registryMutex());
    if (!factory)
        registry().remove(sourceName);
    else
        registry().insert(sourceName, Registered{factory, priority});
}

/*!
    \fn QString QGeoSatelliteInfoSource::sourceName() const
    Returns the name under which the source was created by createSource() or createDefaultSource(),
    or an empty string for a source constructed directly.
*/

/*!
    Sets the interval between updates to \a msec milliseconds; 0, the default, lets the source
    deliver updates as they come.
*/
void QGeoSatelliteInfoSource::setUpdateInterval(int msec) { m_updateInterval = std::max(0, msec); }

/*!
    \fn int QGeoSatelliteInfoSource::updateInterval() const
    Returns the interval between updates in milliseconds.
*/

/*!
    \fn int QGeoSatelliteInfoSource::minimumUpdateInterval() const
    Returns the smallest update interval the source supports, in milliseconds.
*/

/*!
    \fn QGeoSatelliteInfoSource::Error QGeoSatelliteInfoSource::error() const
    Returns the last error, or NoError.
*/

/*!
    Sets the backend-specific property \a name to \a value, and returns \c true if the source
    supports it. The default implementation supports none.
*/
bool QGeoSatelliteInfoSource::setBackendProperty(const QString &name, const QVariant &value) {
    Q_UNUSED(name);
    Q_UNUSED(value);
    return false;
}

/*!
    Returns the backend-specific property \a name, or an invalid QVariant if the source has no
    such property.
*/
QVariant QGeoSatelliteInfoSource::backendProperty(const QString &name) const {
    Q_UNUSED(name);
    return QVariant();
}

/*!
    \fn void QGeoSatelliteInfoSource::startUpdates()
    Starts delivering updates until stopUpdates() is called.
*/

/*!
    \fn void QGeoSatelliteInfoSource::stopUpdates()
    Stops the updates started by startUpdates().
*/

/*!
    \fn void QGeoSatelliteInfoSource::requestUpdate(int timeout)
    Requests one update, to arrive within \a timeout milliseconds; 0 lets the source choose. If none
    arrives in time, errorOccurred() reports UpdateTimeoutError.
*/

/*!
    \fn void QGeoSatelliteInfoSource::satellitesInViewUpdated(const QList<QGeoSatelliteInfo> &satellites)
    This signal is emitted with the \a satellites the receiver can see.
*/

/*!
    \fn void QGeoSatelliteInfoSource::satellitesInUseUpdated(const QList<QGeoSatelliteInfo> &satellites)
    This signal is emitted with the \a satellites the receiver uses for its fix.
*/

/*!
    \fn void QGeoSatelliteInfoSource::errorOccurred(QGeoSatelliteInfoSource::Error error)
    This signal is emitted when \a error occurs.
*/

#include "moc_qgeosatelliteinfosource.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
