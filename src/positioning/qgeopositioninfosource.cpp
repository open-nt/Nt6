// MIT License - see LICENSE
//
// QGeoPositionInfoSource and its registry of sources. Qt finds sources through plugins; Nt keeps a
// table of factories that libraries fill with registerSource(), so that a statically linked
// application gets its sources without a plugin directory.
#include "qgeopositioninfosource.h"
#ifdef Q_OS_ANDROID
#include "qgeopositioninfosource_android_p.h"
#endif

#include <QtCore/qmap.h>
#include <QtCore/qmutex.h>

#include <algorithm>

namespace {

struct Registered {
    QGeoPositionInfoSource::Factory factory;
    int priority = 0;
};

QMutex &registryMutex() {
    static QMutex mutex;
    return mutex;
}

// The platform's own sources are there from the start, so that a statically linked application finds them.
QMap<QString, Registered> platformSources() {
    QMap<QString, Registered> map;
#ifdef Q_OS_ANDROID
    map.insert(QStringLiteral("android"), Registered{&QtPositioningPrivate::createAndroidSource, 0});
#endif
    return map;
}

QMap<QString, Registered> &registry() {
    static QMap<QString, Registered> map = platformSources();
    return map;
}

} // namespace

/*!
    \class QGeoPositionInfoSource
    \inmodule NtPositioning
    \brief The QGeoPositionInfoSource class is the abstract base of position sources.

    A source delivers QGeoPositionInfo updates through positionUpdated(), either continuously
    between startUpdates() and stopUpdates() or once after requestUpdate(). The interval between
    updates is a hint set with setUpdateInterval().

    \code
    auto *source = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, this);
    source->setDevice(serialPort);
    connect(source, &QGeoPositionInfoSource::positionUpdated, this, &Tracker::onPosition);
    source->startUpdates();
    \endcode

    createDefaultSource() and createSource() create sources by name from those that libraries
    have registered with registerSource().

    \differences
    Sources are not found through plugins. A library registers a factory with registerSource(),
    which is Nt-only. On Android Nt has the source \c android, which uses the GPS and network
    providers of the \c LocationManager and needs the location permission (see
    QLocationPermission) and an application packaged with \c org.nt.NtActivity; elsewhere Nt
    registers none, so createDefaultSource() returns \c nullptr unless a library such as Nt Twin
    has registered one. updateInterval is not a bindable property.
    \enddifferences

    \sa QNmeaPositionInfoSource, QGeoPositionInfo
*/

/*!
    \enum QGeoPositionInfoSource::Error

    \value AccessError The source could not be opened or read, or lacks permission.
    \value ClosedError The source was closed, for example because the device disconnected.
    \value UnknownSourceError An error that fits no other value.
    \value NoError No error has occurred.
    \value UpdateTimeoutError requestUpdate() got no fix within its timeout, or a running source
           has stopped delivering updates.
*/

/*!
    \enum QGeoPositionInfoSource::PositioningMethod

    \value NoPositioningMethods None.
    \value SatellitePositioningMethods Satellite-based methods such as GPS.
    \value NonSatellitePositioningMethods Other methods, such as Wi-Fi or cell towers.
    \value AllPositioningMethods Any method.
*/

/*!
    \typedef QGeoPositionInfoSource::PositioningMethods
    A QFlags of PositioningMethod values.
*/

/*!
    \typedef QGeoPositionInfoSource::Factory
    A function that creates a source from a map of parameters and a parent; see registerSource().
*/

/*!
    Constructs a source with the given \a parent.
*/
QGeoPositionInfoSource::QGeoPositionInfoSource(QObject *parent) : QObject(parent) {}

/*!
    Destroys the source.
*/
QGeoPositionInfoSource::~QGeoPositionInfoSource() = default;

/*!
    Sets the interval between updates to \a msec milliseconds. 0, the default, lets the source
    deliver updates as they come. Sources raise values below minimumUpdateInterval().
*/
void QGeoPositionInfoSource::setUpdateInterval(int msec) { m_updateInterval = std::max(0, msec); }

/*!
    \fn int QGeoPositionInfoSource::updateInterval() const
    Returns the interval between updates in milliseconds.
*/

/*!
    Sets the positioning methods the source should prefer to \a methods, limited to those it
    supports.
*/
void QGeoPositionInfoSource::setPreferredPositioningMethods(PositioningMethods methods) {
    m_preferredMethods = methods & supportedPositioningMethods();
    if (m_preferredMethods == NoPositioningMethods)
        m_preferredMethods = supportedPositioningMethods();
}

/*!
    \fn QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSource::preferredPositioningMethods() const
    Returns the preferred positioning methods.
*/

/*!
    \fn QGeoPositionInfo QGeoPositionInfoSource::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const
    Returns the most recent position, or an invalid one if there is none. If
    \a fromSatellitePositioningMethodsOnly is \c true, only a position from satellites is returned.
*/

/*!
    \fn QGeoPositionInfoSource::PositioningMethods QGeoPositionInfoSource::supportedPositioningMethods() const
    Returns the positioning methods the source can use.
*/

/*!
    \fn int QGeoPositionInfoSource::minimumUpdateInterval() const
    Returns the smallest update interval the source supports, in milliseconds.
*/

/*!
    \fn QString QGeoPositionInfoSource::sourceName() const
    Returns the name under which the source was created by createSource() or createDefaultSource(),
    or an empty string for a source constructed directly.
*/

/*!
    Sets the backend-specific property \a name to \a value, and returns \c true if the source
    supports it. The default implementation supports none.
*/
bool QGeoPositionInfoSource::setBackendProperty(const QString &name, const QVariant &value) {
    Q_UNUSED(name);
    Q_UNUSED(value);
    return false;
}

/*!
    Returns the backend-specific property \a name, or an invalid QVariant if the source has no
    such property.
*/
QVariant QGeoPositionInfoSource::backendProperty(const QString &name) const {
    Q_UNUSED(name);
    return QVariant();
}

/*!
    Creates the registered source with the highest priority, with the given \a parent, or returns
    \c nullptr if none is registered.
*/
QGeoPositionInfoSource *QGeoPositionInfoSource::createDefaultSource(QObject *parent) {
    return createDefaultSource(QVariantMap(), parent);
}

/*!
    \overload
    Passes \a parameters to the source's factory, and \a parent as its parent.
*/
QGeoPositionInfoSource *QGeoPositionInfoSource::createDefaultSource(const QVariantMap &parameters, QObject *parent) {
    const QStringList names = availableSources();
    for (const QString &name : names) {
        if (QGeoPositionInfoSource *source = createSource(name, parameters, parent))
            return source;
    }
    return nullptr;
}

/*!
    Creates the source registered as \a sourceName with the given \a parent, or returns \c nullptr
    if there is none.
*/
QGeoPositionInfoSource *QGeoPositionInfoSource::createSource(const QString &sourceName, QObject *parent) {
    return createSource(sourceName, QVariantMap(), parent);
}

/*!
    \overload
    Passes \a parameters to the factory of \a sourceName, and \a parent as its parent.
*/
QGeoPositionInfoSource *QGeoPositionInfoSource::createSource(const QString &sourceName, const QVariantMap &parameters,
                                                             QObject *parent) {
    Factory factory;
    {
        const QMutexLocker locker(&registryMutex());
        const auto it = registry().constFind(sourceName);
        if (it == registry().constEnd())
            return nullptr;
        factory = it->factory;
    }
    QGeoPositionInfoSource *source = factory ? factory(parameters, parent) : nullptr;
    if (source)
        source->m_sourceName = sourceName;
    return source;
}

/*!
    Returns the names of the registered sources, highest priority first.
*/
QStringList QGeoPositionInfoSource::availableSources() {
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
void QGeoPositionInfoSource::registerSource(const QString &sourceName, const Factory &factory, int priority) {
    const QMutexLocker locker(&registryMutex());
    if (!factory)
        registry().remove(sourceName);
    else
        registry().insert(sourceName, Registered{factory, priority});
}

/*!
    \fn QGeoPositionInfoSource::Error QGeoPositionInfoSource::error() const
    Returns the last error, or NoError.
*/

/*!
    \fn void QGeoPositionInfoSource::startUpdates()
    Starts delivering updates through positionUpdated() until stopUpdates() is called.
*/

/*!
    \fn void QGeoPositionInfoSource::stopUpdates()
    Stops the updates started by startUpdates().
*/

/*!
    \fn void QGeoPositionInfoSource::requestUpdate(int timeout)
    Requests one update, to arrive within \a timeout milliseconds; 0 lets the source choose. If none
    arrives in time, errorOccurred() reports UpdateTimeoutError.
*/

/*!
    \fn void QGeoPositionInfoSource::positionUpdated(const QGeoPositionInfo &update)
    This signal is emitted with each new position \a update.
*/

/*!
    \fn void QGeoPositionInfoSource::errorOccurred(QGeoPositionInfoSource::Error error)
    This signal is emitted when \a error occurs.
*/

/*!
    \fn void QGeoPositionInfoSource::supportedPositioningMethodsChanged()
    This signal is emitted when the positioning methods the source supports change.
*/

#include "moc_qgeopositioninfosource.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
