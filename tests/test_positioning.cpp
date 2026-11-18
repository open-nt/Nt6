// MIT License - see LICENSE
//
// Nt Positioning: coordinates, distances and formats against known values, rectangles and circles
// (including across the 180th meridian), and QNmeaPositionInfoSource reading NMEA 0183 from a
// QBuffer in both update modes, and QNmeaSatelliteInfoSource reading GSV and GSA.
#include <QtCore>
#include <QtPositioning>

#include <cmath>
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

QByteArray nmea(const QByteArray &body) {
    quint8 sum = 0;
    for (char c : body)
        sum ^= static_cast<quint8>(c);
    return '$' + body + '*' + QByteArray::number(sum, 16).toUpper().rightJustified(2, '0') + "\r\n";
}

void testCoordinate() {
    const QGeoCoordinate invalid;
    CHECK(!invalid.isValid());
    CHECK(invalid.type() == QGeoCoordinate::InvalidCoordinate);
    CHECK(std::isnan(invalid.latitude()));
    CHECK(invalid.toString().isEmpty());
    CHECK(QGeoCoordinate(51.0, -1.0).type() == QGeoCoordinate::Coordinate2D);
    CHECK(QGeoCoordinate(51.0, -1.0, 100.0).type() == QGeoCoordinate::Coordinate3D);
    CHECK(!QGeoCoordinate(90.1, 0.0).isValid());
    CHECK(!QGeoCoordinate(0.0, -180.1).isValid());

    // London to Paris: 343.5 km on a sphere of the mean radius, initial bearing 148.1 degrees.
    const QGeoCoordinate london(51.5074, -0.1278);
    const QGeoCoordinate paris(48.8566, 2.3522);
    CHECK(near(london.distanceTo(paris), 343556.0, 200.0));
    CHECK(near(london.azimuthTo(paris), 148.1, 0.1));
    CHECK(london.distanceTo(invalid) == 0.0);
    CHECK(near(QGeoCoordinate(0.0, 0.0).azimuthTo(QGeoCoordinate(1.0, 0.0)), 0.0, 1e-9));
    CHECK(near(QGeoCoordinate(0.0, 0.0).azimuthTo(QGeoCoordinate(0.0, -1.0)), 270.0, 1e-9));

    // Travelling back the same distance on the reverse bearing returns to the start.
    const QGeoCoordinate start(51.1789, -1.8262, 100.0);
    const QGeoCoordinate there = start.atDistanceAndAzimuth(5000.0, 60.0, 12.5);
    CHECK(near(start.distanceTo(there), 5000.0, 1e-6));
    CHECK(near(start.azimuthTo(there), 60.0, 1e-6));
    CHECK(near(there.altitude(), 112.5, 1e-9));
    const QGeoCoordinate back = there.atDistanceAndAzimuth(5000.0, there.azimuthTo(start));
    CHECK(near(back.latitude(), start.latitude(), 1e-9));
    CHECK(near(back.longitude(), start.longitude(), 1e-9));
    CHECK(QGeoCoordinate(0.0, 179.99).atDistanceAndAzimuth(10000.0, 90.0).longitude() < -179.9);
    CHECK(!invalid.atDistanceAndAzimuth(10.0, 0.0).isValid());

    const QGeoCoordinate brisbane(-27.46758, 153.027892);
    CHECK(brisbane.toString(QGeoCoordinate::Degrees) == QStringLiteral("-27.46758°, 153.02789°"));
    CHECK(brisbane.toString(QGeoCoordinate::DegreesWithHemisphere) == QStringLiteral("27.46758° S, 153.02789° E"));
    CHECK(brisbane.toString(QGeoCoordinate::DegreesMinutes) == QStringLiteral("-27° 28.055', 153° 1.674'"));
    CHECK(brisbane.toString(QGeoCoordinate::DegreesMinutesSecondsWithHemisphere) ==
          QStringLiteral("27° 28' 3.3\" S, 153° 1' 40.4\" E"));
    CHECK(QGeoCoordinate(-27.77, 153.03, 28.1).toString() ==
          QStringLiteral("27° 46' 12.0\" S, 153° 1' 48.0\" E, 28.1m"));

    CHECK(QGeoCoordinate(1.0, 2.0) == QGeoCoordinate(1.0, 2.0));
    CHECK(QGeoCoordinate(1.0, 2.0) != QGeoCoordinate(1.0, 2.0, 3.0));
    CHECK(QGeoCoordinate(10.0, 180.0) == QGeoCoordinate(10.0, -180.0));
    CHECK(invalid == QGeoCoordinate());
    QGeoCoordinate set;
    set.setLatitude(5.0);
    set.setLongitude(6.0);
    set.setAltitude(7.0);
    CHECK(set == QGeoCoordinate(5.0, 6.0, 7.0));
    CHECK(QVariant::fromValue(set).value<QGeoCoordinate>() == set);
}

void testRectangle() {
    QGeoRectangle r(QGeoCoordinate(52.0, -2.0), QGeoCoordinate(51.0, -1.0));
    CHECK(r.type() == QGeoShape::RectangleType);
    CHECK(r.isValid() && !r.isEmpty());
    CHECK(near(r.width(), 1.0, 1e-12) && near(r.height(), 1.0, 1e-12));
    CHECK(r.center() == QGeoCoordinate(51.5, -1.5));
    CHECK(r.topRight() == QGeoCoordinate(52.0, -1.0));
    CHECK(r.bottomLeft() == QGeoCoordinate(51.0, -2.0));
    CHECK(r.contains(QGeoCoordinate(51.5, -1.2)));
    CHECK(!r.contains(QGeoCoordinate(51.5, -0.9)));
    CHECK(r.toString() == QStringLiteral("QGeoRectangle({52, -2}, {51, -1})"));
    CHECK(!QGeoRectangle().isValid());
    CHECK(!QGeoRectangle(QGeoCoordinate(51.0, 0.0), QGeoCoordinate(52.0, 1.0)).isValid());

    r.setCenter(QGeoCoordinate(10.0, 20.0));
    CHECK(r.topLeft() == QGeoCoordinate(10.5, 19.5) && r.bottomRight() == QGeoCoordinate(9.5, 20.5));
    r.setWidth(4.0);
    r.setHeight(2.0);
    CHECK(r == QGeoRectangle(QGeoCoordinate(10.0, 20.0), 4.0, 2.0));

    // Across the 180th meridian.
    const QGeoRectangle pacific(QGeoCoordinate(10.0, 170.0), QGeoCoordinate(-10.0, -170.0));
    CHECK(near(pacific.width(), 20.0, 1e-12));
    CHECK(pacific.center() == QGeoCoordinate(0.0, 180.0));
    CHECK(pacific.contains(QGeoCoordinate(0.0, 179.0)) && pacific.contains(QGeoCoordinate(0.0, -175.0)));
    CHECK(!pacific.contains(QGeoCoordinate(0.0, 0.0)));
    CHECK(pacific.contains(QGeoRectangle(QGeoCoordinate(5.0, 175.0), QGeoCoordinate(-5.0, -175.0))));
    CHECK(pacific.intersects(QGeoRectangle(QGeoCoordinate(5.0, -172.0), QGeoCoordinate(-5.0, -160.0))));
    CHECK(!pacific.intersects(QGeoRectangle(QGeoCoordinate(5.0, -160.0), QGeoCoordinate(-5.0, -150.0))));
    const QGeoRectangle moved = pacific.translated(85.0, 20.0);
    CHECK(moved.topLeft() == QGeoCoordinate(90.0, -170.0) && moved.bottomRight() == QGeoCoordinate(70.0, -150.0));

    // Growing takes the shorter way round.
    QGeoRectangle grow(QGeoCoordinate(1.0, 170.0), QGeoCoordinate(0.0, 175.0));
    grow.extendRectangle(QGeoCoordinate(2.0, -178.0));
    CHECK(grow.topLeft() == QGeoCoordinate(2.0, 170.0) && grow.bottomRight() == QGeoCoordinate(0.0, -178.0));
    const QGeoRectangle list({QGeoCoordinate(1.0, 1.0), QGeoCoordinate(-2.0, 3.0), QGeoCoordinate(4.0, -1.0)});
    CHECK(list.topLeft() == QGeoCoordinate(4.0, -1.0) && list.bottomRight() == QGeoCoordinate(-2.0, 3.0));
    const QGeoRectangle a(QGeoCoordinate(1.0, 0.0), QGeoCoordinate(0.0, 1.0));
    const QGeoRectangle b(QGeoCoordinate(3.0, 5.0), QGeoCoordinate(2.0, 6.0));
    CHECK((a | b) == QGeoRectangle(QGeoCoordinate(3.0, 0.0), QGeoCoordinate(0.0, 6.0)));
    QGeoRectangle c = a;
    c |= QGeoRectangle();
    CHECK(c == a);

    const QGeoShape shape = pacific;
    CHECK(shape.type() == QGeoShape::RectangleType);
    CHECK(QGeoRectangle(shape) == pacific);
    CHECK(!QGeoCircle(shape).isValid());
    CHECK(shape.boundingGeoRectangle() == pacific);
    CHECK(!QGeoShape().isValid() && QGeoShape().isEmpty());
    CHECK(QGeoShape().toString() == QStringLiteral("QGeoShape(Unknown)"));
}

void testCircle() {
    QGeoCircle circle(QGeoCoordinate(51.0, 0.0), 1000.0);
    CHECK(circle.type() == QGeoShape::CircleType && circle.isValid() && !circle.isEmpty());
    CHECK(circle.contains(circle.center().atDistanceAndAzimuth(999.0, 33.0)));
    CHECK(!circle.contains(circle.center().atDistanceAndAzimuth(1001.0, 33.0)));
    CHECK(!QGeoCircle(QGeoCoordinate(51.0, 0.0)).isValid());
    CHECK(QGeoCircle(QGeoCoordinate(51.0, 0.0), 0.0).isEmpty());
    const QGeoRectangle box = circle.boundingGeoRectangle();
    for (double az = 0.0; az < 360.0; az += 15.0)
        CHECK(box.contains(circle.center().atDistanceAndAzimuth(999.0, az)));
    CHECK(near(box.height(), 2.0 * 1000.0 / 6371007.2 * 180.0 / M_PI, 1e-9));
    CHECK(QGeoCircle(QGeoCoordinate(89.99, 0.0), 5000.0).boundingGeoRectangle().width() == 360.0);
    circle.extendCircle(QGeoCoordinate(51.1, 0.0));
    CHECK(near(circle.radius(), QGeoCoordinate(51.0, 0.0).distanceTo(QGeoCoordinate(51.1, 0.0)), 1e-6));
    CHECK(circle.translated(1.0, 2.0).center() == QGeoCoordinate(52.0, 2.0));
    CHECK(QGeoCircle(QGeoCoordinate(1.0, 2.0), 3.0).toString() == QStringLiteral("QGeoCircle({1, 2}, 3)"));
    const QGeoShape shape = circle;
    CHECK(QGeoCircle(shape) == circle && shape.center() == circle.center());
    CHECK(shape.contains(QGeoCoordinate(51.05, 0.0)));
}

void testInfo() {
    QGeoPositionInfo info;
    CHECK(!info.isValid());
    info = QGeoPositionInfo(QGeoCoordinate(1.0, 2.0), QDateTime(QDate(2026, 11, 19), QTime(10, 0), Qt::UTC));
    CHECK(info.isValid());
    CHECK(!info.hasAttribute(QGeoPositionInfo::GroundSpeed));
    CHECK(std::isnan(info.attribute(QGeoPositionInfo::GroundSpeed)));
    info.setAttribute(QGeoPositionInfo::GroundSpeed, 12.5);
    CHECK(info.attribute(QGeoPositionInfo::GroundSpeed) == 12.5);
    QGeoPositionInfo copy = info;
    CHECK(copy == info);
    copy.removeAttribute(QGeoPositionInfo::GroundSpeed);
    CHECK(copy != info);

    QGeoSatelliteInfo sat;
    CHECK(sat.satelliteIdentifier() == -1 && sat.signalStrength() == -1);
    sat.setSatelliteSystem(QGeoSatelliteInfo::GALILEO);
    sat.setSatelliteIdentifier(11);
    sat.setSignalStrength(42);
    sat.setAttribute(QGeoSatelliteInfo::Elevation, 35.0);
    CHECK(sat.satelliteSystem() == QGeoSatelliteInfo::GALILEO && sat.attribute(QGeoSatelliteInfo::Elevation) == 35.0);
    CHECK(std::isnan(sat.attribute(QGeoSatelliteInfo::Azimuth)));
    QGeoSatelliteInfo other = sat;
    CHECK(other == sat);
    other.setSignalStrength(10);
    CHECK(other != sat);
}

// One fix of a u-blox receiver: GGA, GSA, RMC and VTG for the same second.
QByteArray epoch(int second, double latMinutes, bool fix = true) {
    const QByteArray t = "1230" + QByteArray::number(second).rightJustified(2, '0') + ".00";
    const QByteArray lat = "5110." + QByteArray::number(latMinutes, 'f', 4).split('.').at(1);
    QByteArray out;
    out += nmea("GNGGA," + t + "," + lat + ",N,00149.5720,W," + (fix ? "1" : "0") + ",08,1.2,102.5,M,47.9,M,,");
    out += nmea("GNGSA,A,3,01,02,03,04,05,06,07,08,,,,,1.9,1.2,1.5");
    out += nmea("GNRMC," + t + "," + (fix ? "A" : "V") + "," + lat + ",N,00149.5720,W,10.0,045.0,191126,1.5,W,A");
    out += nmea("GNVTG,045.0,T,,M,10.0,N,18.52,K,A");
    return out;
}

void testNmeaRealTime() {
    QByteArray data = epoch(0, 0.7340) + "$GNGGA,garbage*00\r\n" + epoch(1, 0.7350) + epoch(2, 0.7360, false);
    QBuffer buffer(&data);
    QNmeaPositionInfoSource source(QNmeaPositionInfoSource::RealTimeMode);
    CHECK(source.updateMode() == QNmeaPositionInfoSource::RealTimeMode);
    CHECK(source.supportedPositioningMethods() == QGeoPositionInfoSource::SatellitePositioningMethods);
    source.setUserEquivalentRangeError(5.0);
    source.setDevice(&buffer);
    CHECK(source.device() == &buffer);
    QList<QGeoPositionInfo> updates;
    QObject::connect(&source, &QGeoPositionInfoSource::positionUpdated,
                     [&updates](const QGeoPositionInfo &info) { updates.append(info); });
    source.startUpdates();
    CHECK(source.error() == QGeoPositionInfoSource::NoError);
    // The third epoch has no fix and is not delivered.
    CHECK(updates.size() == 2);
    if (updates.size() == 2) {
        const QGeoPositionInfo &u = updates.first();
        CHECK(u.timestamp() == QDateTime(QDate(2026, 11, 19), QTime(12, 30, 0), Qt::UTC));
        CHECK(near(u.coordinate().latitude(), 51.0 + 10.7340 / 60.0, 1e-9));
        CHECK(near(u.coordinate().longitude(), -(1.0 + 49.5720 / 60.0), 1e-9));
        CHECK(near(u.coordinate().altitude(), 102.5, 1e-9));
        CHECK(near(u.attribute(QGeoPositionInfo::GroundSpeed), 18.52 / 3.6, 1e-9));
        CHECK(near(u.attribute(QGeoPositionInfo::Direction), 45.0, 1e-9));
        CHECK(near(u.attribute(QGeoPositionInfo::MagneticVariation), -1.5, 1e-9));
        CHECK(near(u.attribute(QGeoPositionInfo::HorizontalAccuracy), 6.0, 1e-9));
        CHECK(near(u.attribute(QGeoPositionInfo::VerticalAccuracy), 7.5, 1e-9));
        CHECK(updates.at(1).timestamp().time() == QTime(12, 30, 1));
    }
    CHECK(source.lastKnownPosition() == updates.value(1));

    source.stopUpdates();
    CHECK(std::isnan(QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode).userEquivalentRangeError()));

    QNmeaPositionInfoSource noDevice(QNmeaPositionInfoSource::RealTimeMode);
    QList<QGeoPositionInfoSource::Error> errors;
    QObject::connect(&noDevice, &QGeoPositionInfoSource::errorOccurred,
                     [&errors](QGeoPositionInfoSource::Error e) { errors.append(e); });
    noDevice.startUpdates();
    CHECK(errors == QList<QGeoPositionInfoSource::Error>{QGeoPositionInfoSource::AccessError});
    CHECK(noDevice.error() == QGeoPositionInfoSource::AccessError);
}

void testNmeaRequestAndTimeout() {
    QByteArray data = epoch(5, 0.1);
    QBuffer buffer(&data);
    QNmeaPositionInfoSource source(QNmeaPositionInfoSource::RealTimeMode);
    source.setDevice(&buffer);
    int updates = 0;
    QObject::connect(&source, &QGeoPositionInfoSource::positionUpdated, [&updates] { ++updates; });
    source.requestUpdate(1000);
    CHECK(updates == 1);

    QByteArray empty;
    QBuffer silent(&empty);
    QNmeaPositionInfoSource waiting(QNmeaPositionInfoSource::RealTimeMode);
    waiting.setDevice(&silent);
    QEventLoop loop;
    QGeoPositionInfoSource::Error error = QGeoPositionInfoSource::NoError;
    QObject::connect(&waiting, &QGeoPositionInfoSource::errorOccurred, [&](QGeoPositionInfoSource::Error e) {
        error = e;
        loop.quit();
    });
    waiting.requestUpdate(50);
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(error == QGeoPositionInfoSource::UpdateTimeoutError);
}

void testNmeaSimulation() {
    QByteArray data = epoch(0, 0.1) + epoch(1, 0.2) + epoch(2, 0.3);
    QBuffer buffer(&data);
    QNmeaPositionInfoSource source(QNmeaPositionInfoSource::SimulationMode);
    source.setDevice(&buffer);
    QList<qint64> arrivals;
    QElapsedTimer clock;
    QEventLoop loop;
    QObject::connect(&source, &QGeoPositionInfoSource::positionUpdated, [&](const QGeoPositionInfo &) {
        arrivals.append(clock.elapsed());
        if (arrivals.size() == 3)
            loop.quit();
    });
    clock.start();
    source.startUpdates();
    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(arrivals.size() == 3);
    // The log has one fix per second, so the replay takes about two seconds.
    if (arrivals.size() == 3)
        CHECK(arrivals.at(2) - arrivals.at(0) >= 1900 && arrivals.at(2) - arrivals.at(0) < 3000);
}

// One fix: a GGA for the time, two GPS GSV messages (5 satellites), one GLONASS GSV message and
// combined GSA sentences that name their system (NMEA 4.10).
QByteArray satelliteEpoch(int second, int gpsSnr) {
    const QByteArray t = "1230" + QByteArray::number(second).rightJustified(2, '0') + ".00";
    QByteArray out;
    out += nmea("GNGGA," + t + ",5110.7340,N,00149.5720,W,1,07,1.2,102.5,M,47.9,M,,");
    out += nmea("GPGSV,2,1,05,02,45,120," + QByteArray::number(gpsSnr) + ",05,30,045,38,12,10,300,25,15,80,200,45");
    out += nmea("GPGSV,2,2,05,29,05,010,");
    out += nmea("GLGSV,1,1,02,65,50,090,40,72,20,180,33");
    out += nmea("GNGSA,A,3,02,05,15,,,,,,,,,,1.9,1.2,1.5,1");
    out += nmea("GNGSA,A,3,65,72,,,,,,,,,,,1.9,1.2,1.5,2");
    return out;
}

void testSatellites() {
    QByteArray data = satelliteEpoch(0, 41) + satelliteEpoch(1, 44);
    QBuffer buffer(&data);
    QNmeaSatelliteInfoSource source(QNmeaSatelliteInfoSource::RealTimeMode);
    source.setDevice(&buffer);
    CHECK(source.device() == &buffer && source.updateMode() == QNmeaSatelliteInfoSource::RealTimeMode);
    QList<QList<QGeoSatelliteInfo>> inView;
    QList<QList<QGeoSatelliteInfo>> inUse;
    QObject::connect(&source, &QGeoSatelliteInfoSource::satellitesInViewUpdated,
                     [&](const QList<QGeoSatelliteInfo> &list) { inView.append(list); });
    QObject::connect(&source, &QGeoSatelliteInfoSource::satellitesInUseUpdated,
                     [&](const QList<QGeoSatelliteInfo> &list) { inUse.append(list); });
    source.startUpdates();
    CHECK(source.error() == QGeoSatelliteInfoSource::NoError);
    CHECK(inView.size() == 2 && inUse.size() == 2);
    if (inView.size() == 2 && inUse.size() == 2) {
        const QList<QGeoSatelliteInfo> &view = inView.first();
        CHECK(view.size() == 7);
        int gps = 0;
        for (const QGeoSatelliteInfo &sat : view) {
            if (sat.satelliteSystem() == QGeoSatelliteInfo::GPS)
                ++gps;
            if (sat.satelliteIdentifier() == 2) {
                CHECK(sat.signalStrength() == 41);
                CHECK(sat.attribute(QGeoSatelliteInfo::Elevation) == 45.0);
                CHECK(sat.attribute(QGeoSatelliteInfo::Azimuth) == 120.0);
            }
            if (sat.satelliteIdentifier() == 29)
                CHECK(sat.signalStrength() == -1);
        }
        CHECK(gps == 5);
        const QList<QGeoSatelliteInfo> &used = inUse.first();
        CHECK(used.size() == 5);
        bool glonass = false;
        for (const QGeoSatelliteInfo &sat : used) {
            CHECK(sat.hasAttribute(QGeoSatelliteInfo::Elevation));
            if (sat.satelliteIdentifier() == 72)
                glonass = sat.satelliteSystem() == QGeoSatelliteInfo::GLONASS && sat.signalStrength() == 33;
        }
        CHECK(glonass);
        bool updated = false;
        for (const QGeoSatelliteInfo &sat : inView.at(1))
            updated = updated || (sat.satelliteIdentifier() == 2 && sat.signalStrength() == 44);
        CHECK(updated);
    }
    source.stopUpdates();

    // A combined GSA without a system ID: the satellite numbers tell GPS from GLONASS.
    QByteArray old = nmea("GNGSA,A,3,03,07,,,,,,,,,,,2.0,1.0,1.7") + nmea("GNGSA,A,3,70,80,,,,,,,,,,,2.0,1.0,1.7");
    QBuffer oldBuffer(&old);
    QNmeaSatelliteInfoSource legacy(QNmeaSatelliteInfoSource::RealTimeMode);
    legacy.setDevice(&oldBuffer);
    QList<QGeoSatelliteInfo> legacyUsed;
    QObject::connect(&legacy, &QGeoSatelliteInfoSource::satellitesInUseUpdated,
                     [&](const QList<QGeoSatelliteInfo> &list) { legacyUsed = list; });
    legacy.requestUpdate(1000);
    CHECK(legacyUsed.size() == 4);
    if (legacyUsed.size() == 4) {
        CHECK(legacyUsed.at(0).satelliteSystem() == QGeoSatelliteInfo::GPS);
        CHECK(legacyUsed.at(3).satelliteSystem() == QGeoSatelliteInfo::GLONASS);
    }

    // Simulation: one fix per SimulationUpdateInterval.
    QByteArray recording = satelliteEpoch(0, 40) + satelliteEpoch(1, 41) + satelliteEpoch(2, 42);
    QBuffer recordingBuffer(&recording);
    QNmeaSatelliteInfoSource replay(QNmeaSatelliteInfoSource::SimulationMode);
    CHECK(replay.backendProperty(QNmeaSatelliteInfoSource::SimulationUpdateInterval).toInt() == 100);
    CHECK(!replay.setBackendProperty(QNmeaSatelliteInfoSource::SimulationUpdateInterval, 0));
    CHECK(replay.setBackendProperty(QNmeaSatelliteInfoSource::SimulationUpdateInterval, 200));
    CHECK(!replay.setBackendProperty(QStringLiteral("other"), 1));
    replay.setDevice(&recordingBuffer);
    QElapsedTimer clock;
    QList<qint64> arrivals;
    QEventLoop loop;
    QObject::connect(&replay, &QGeoSatelliteInfoSource::satellitesInViewUpdated, [&](const QList<QGeoSatelliteInfo> &) {
        arrivals.append(clock.elapsed());
        if (arrivals.size() == 3)
            loop.quit();
    });
    clock.start();
    replay.startUpdates();
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(arrivals.size() == 3);
    if (arrivals.size() == 3)
        CHECK(arrivals.at(2) - arrivals.at(0) >= 380 && arrivals.at(2) - arrivals.at(0) < 1500);

    QNmeaSatelliteInfoSource noDevice(QNmeaSatelliteInfoSource::RealTimeMode);
    noDevice.startUpdates();
    CHECK(noDevice.error() == QGeoSatelliteInfoSource::AccessError);

    CHECK(QGeoSatelliteInfoSource::createDefaultSource(nullptr) == nullptr);
    QGeoSatelliteInfoSource::registerSource(QStringLiteral("nmea"), [](const QVariantMap &, QObject *parent) {
        return new QNmeaSatelliteInfoSource(QNmeaSatelliteInfoSource::RealTimeMode, parent);
    });
    CHECK(QGeoSatelliteInfoSource::availableSources() == QStringList{QStringLiteral("nmea")});
    QObject parent;
    QGeoSatelliteInfoSource *created = QGeoSatelliteInfoSource::createSource(QStringLiteral("nmea"), &parent);
    CHECK(created && created->sourceName() == QStringLiteral("nmea"));
    QGeoSatelliteInfoSource::registerSource(QStringLiteral("nmea"), {});
    CHECK(QGeoSatelliteInfoSource::availableSources().isEmpty());
}

void testRegistry() {
    CHECK(QGeoPositionInfoSource::createSource(QStringLiteral("none"), nullptr) == nullptr);
    CHECK(QGeoPositionInfoSource::createDefaultSource(nullptr) == nullptr);
    QGeoPositionInfoSource::registerSource(
        QStringLiteral("low"),
        [](const QVariantMap &, QObject *parent) {
            return new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, parent);
        },
        1);
    QGeoPositionInfoSource::registerSource(
        QStringLiteral("high"),
        [](const QVariantMap &parameters, QObject *parent) -> QGeoPositionInfoSource * {
            if (parameters.value(QStringLiteral("refuse")).toBool())
                return nullptr;
            return new QNmeaPositionInfoSource(QNmeaPositionInfoSource::SimulationMode, parent);
        },
        5);
    CHECK(QGeoPositionInfoSource::availableSources() == QStringList({QStringLiteral("high"), QStringLiteral("low")}));
    QObject parent;
    QGeoPositionInfoSource *source = QGeoPositionInfoSource::createDefaultSource(&parent);
    CHECK(source && source->sourceName() == QStringLiteral("high") && source->parent() == &parent);
    source = QGeoPositionInfoSource::createDefaultSource({{QStringLiteral("refuse"), true}}, &parent);
    CHECK(source && source->sourceName() == QStringLiteral("low"));
    CHECK(source && !source->setBackendProperty(QStringLiteral("x"), 1) &&
          !source->backendProperty(QStringLiteral("x")).isValid());
    source->setUpdateInterval(1);
    CHECK(source->updateInterval() == source->minimumUpdateInterval());
    source->setPreferredPositioningMethods(QGeoPositionInfoSource::NonSatellitePositioningMethods);
    CHECK(source->preferredPositioningMethods() == QGeoPositionInfoSource::SatellitePositioningMethods);
    QGeoPositionInfoSource::registerSource(QStringLiteral("high"), {});
    QGeoPositionInfoSource::registerSource(QStringLiteral("low"), {});
    CHECK(QGeoPositionInfoSource::availableSources().isEmpty());
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testCoordinate();
    testRectangle();
    testCircle();
    testInfo();
    testNmeaRealTime();
    testNmeaRequestAndTimeout();
    testNmeaSimulation();
    testSatellites();
    testRegistry();
    if (g_failures == 0)
        std::printf("All Nt Positioning tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
