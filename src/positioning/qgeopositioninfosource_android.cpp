// MIT License - see LICENSE
//
// The position source of Android, registered as "android": android.location.LocationManager, through the
// startNtLocation(), stopNtLocation() and lastNtLocation() methods of org.nt.NtActivity, which pass each fix to
// nativeLocation() below. A fix comes on the UI thread of Android and is handed to the source's thread with a queued
// call; sources are found by a number, so that a fix that comes after a source was destroyed finds nothing. The
// application needs the location permission (see QLocationPermission) before it starts updates.

#include "qgeopositioninfosource_android_p.h"

#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qhash.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#include <QtCore/qmutex.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtCore/qtimezone.h>

#include <jni.h>

#include <cmath>

namespace {

constexpr const char *kActivityClass = "org/nt/NtActivity";
constexpr int kMinimumInterval = 100; // milliseconds

class AndroidPositionSource;

struct Sources {
    QMutex mutex;
    QHash<jlong, QPointer<AndroidPositionSource>> byId;
    jlong nextId = 1;
    bool nativesRegistered = false;
};

Sources &sources() {
    static Sources s;
    return s;
}

QGeoPositionInfo positionInfo(jdouble latitude, jdouble longitude, jdouble altitude, jfloat accuracy,
                              jfloat verticalAccuracy, jfloat bearing, jfloat speed, jlong time) {
    const QGeoCoordinate coordinate =
        std::isnan(altitude) ? QGeoCoordinate(latitude, longitude) : QGeoCoordinate(latitude, longitude, altitude);
    QGeoPositionInfo info(coordinate, QDateTime::fromMSecsSinceEpoch(time, QTimeZone::UTC));
    if (!std::isnan(accuracy))
        info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, qreal(accuracy));
    if (!std::isnan(verticalAccuracy))
        info.setAttribute(QGeoPositionInfo::VerticalAccuracy, qreal(verticalAccuracy));
    if (!std::isnan(bearing))
        info.setAttribute(QGeoPositionInfo::Direction, qreal(bearing));
    if (!std::isnan(speed))
        info.setAttribute(QGeoPositionInfo::GroundSpeed, qreal(speed));
    return info;
}

class AndroidPositionSource : public QGeoPositionInfoSource {
public:
    explicit AndroidPositionSource(QObject *parent) : QGeoPositionInfoSource(parent) {
        Sources &s = sources();
        const QMutexLocker locker(&s.mutex);
        m_id = s.nextId++;
        s.byId.insert(m_id, this);
        m_timeout.setSingleShot(true);
        QObject::connect(&m_timeout, &QTimer::timeout, this, [this] {
            if (!m_single)
                return;
            m_single = false;
            if (!m_running)
                stopListener();
            setError(UpdateTimeoutError);
        });
    }
    ~AndroidPositionSource() override {
        stopListener();
        Sources &s = sources();
        const QMutexLocker locker(&s.mutex);
        s.byId.remove(m_id);
    }
    AndroidPositionSource(const AndroidPositionSource &) = delete;
    AndroidPositionSource &operator=(const AndroidPositionSource &) = delete;

    void setUpdateInterval(int msec) override {
        QGeoPositionInfoSource::setUpdateInterval(msec <= 0 ? 0 : std::max(msec, kMinimumInterval));
        if (m_running) {
            stopListener();
            startListener();
        }
    }
    void setPreferredPositioningMethods(PositioningMethods methods) override {
        QGeoPositionInfoSource::setPreferredPositioningMethods(methods);
        if (m_running) {
            stopListener();
            startListener();
        }
    }

    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const override {
        if (fromSatellitePositioningMethodsOnly && !m_lastFromSatellite) {
            // Ask the providers for what they kept: lastNtLocation() calls nativeLocation(), which only queues.
            const QJniObject context(QNativeInterface::QAndroidApplication::context());
            if (context.isValid() && registerNatives())
                context.callMethod<jboolean>("lastNtLocation", "(JZ)Z", m_id, jboolean(true));
            return QGeoPositionInfo();
        }
        if (!m_last.isValid()) {
            const QJniObject context(QNativeInterface::QAndroidApplication::context());
            if (context.isValid() && registerNatives())
                context.callMethod<jboolean>("lastNtLocation", "(JZ)Z", m_id, jboolean(false));
        }
        return m_last;
    }
    PositioningMethods supportedPositioningMethods() const override { return AllPositioningMethods; }
    int minimumUpdateInterval() const override { return kMinimumInterval; }
    Error error() const override { return m_error; }

    void startUpdates() override {
        if (m_running)
            return;
        m_running = true;
        if (!m_listener.isValid() && !startListener()) {
            m_running = false;
            setError(AccessError);
        }
    }
    void stopUpdates() override {
        m_running = false;
        if (!m_single)
            stopListener();
    }
    void requestUpdate(int timeout) override {
        if (timeout < 0) {
            setError(UpdateTimeoutError);
            return;
        }
        m_single = true;
        m_timeout.start(timeout == 0 ? 30000 : std::max(timeout, kMinimumInterval));
        if (!m_listener.isValid() && !startListener()) {
            m_single = false;
            m_timeout.stop();
            setError(AccessError);
        }
    }

    // A fix from nativeLocation(), on this source's thread.
    void deliver(const QGeoPositionInfo &info, bool satellite) {
        m_last = info;
        m_lastFromSatellite = satellite;
        if (m_single) {
            m_single = false;
            m_timeout.stop();
            if (!m_running)
                stopListener();
        } else if (!m_running) {
            return; // what lastNtLocation() found: kept, not announced
        }
        emit positionUpdated(info);
    }

    static bool registerNatives();

private:
    void setError(Error e) {
        m_error = e;
        emit errorOccurred(e);
    }

    bool startListener() {
        const QJniObject context(QNativeInterface::QAndroidApplication::context());
        if (!context.isValid() || !registerNatives())
            return false;
        const PositioningMethods methods = preferredPositioningMethods();
        const bool satellite = methods & SatellitePositioningMethods;
        const bool network = methods & NonSatellitePositioningMethods;
        m_listener =
            context.callObjectMethod("startNtLocation", "(JJZZ)Ljava/lang/Object;", m_id, jlong(updateInterval()),
                                     jboolean(satellite || !network), jboolean(network || !satellite));
        QJniEnvironment env;
        env.checkAndClearExceptions();
        return m_listener.isValid();
    }

    void stopListener() {
        if (!m_listener.isValid())
            return;
        const QJniObject context(QNativeInterface::QAndroidApplication::context());
        if (context.isValid())
            context.callMethod<void>("stopNtLocation", "(Ljava/lang/Object;)V", m_listener.object());
        m_listener = QJniObject();
    }

    jlong m_id = 0;
    QJniObject m_listener;
    QGeoPositionInfo m_last;
    bool m_lastFromSatellite = false;
    bool m_running = false;
    bool m_single = false;
    QTimer m_timeout;
    Error m_error = NoError;
};

// Called by NtActivity on the UI thread of Android.
void nativeLocation(JNIEnv *, jclass, jlong id, jdouble latitude, jdouble longitude, jdouble altitude, jfloat accuracy,
                    jfloat verticalAccuracy, jfloat bearing, jfloat speed, jlong time, jboolean satellite) {
    QPointer<AndroidPositionSource> source;
    {
        Sources &s = sources();
        const QMutexLocker locker(&s.mutex);
        source = s.byId.value(id);
    }
    if (!source)
        return;
    const QGeoPositionInfo info =
        positionInfo(latitude, longitude, altitude, accuracy, verticalAccuracy, bearing, speed, time);
    const bool fromSatellite = satellite;
    QMetaObject::invokeMethod(
        source.data(),
        [source, info, fromSatellite] {
            if (source)
                source->deliver(info, fromSatellite);
        },
        Qt::QueuedConnection);
}

bool AndroidPositionSource::registerNatives() {
    Sources &s = sources();
    {
        const QMutexLocker locker(&s.mutex);
        if (s.nativesRegistered)
            return true;
    }
    QJniEnvironment env;
    const JNINativeMethod methods[] = {{"nativeLocation", "(JDDDFFFFJZ)V", reinterpret_cast<void *>(&nativeLocation)}};
    // A plain NativeActivity (packaged without a JDK) has no org.nt.NtActivity to ask.
    if (!env.findClass(kActivityClass) || !env.registerNativeMethods(kActivityClass, methods, 1))
        return false;
    const QMutexLocker locker(&s.mutex);
    s.nativesRegistered = true;
    return true;
}

} // namespace

QGeoPositionInfoSource *QtPositioningPrivate::createAndroidSource(const QVariantMap &, QObject *parent) {
    return new AndroidPositionSource(parent);
}
