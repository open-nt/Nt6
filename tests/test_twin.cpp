// MIT License - see LICENSE
//
// Nt Twin geodesy is checked against reference values computed with PROJ 9.7 (cs2cs, EPSG:4979 to
// EPSG:4978 for ECEF and EPSG:4326 to EPSG:326xx/327xx for UTM), and by round trips over a grid of
// positions that covers the whole globe and the edges of UTM zones.
#include <QtCore>
#include <QtPositioning>
#include <QtTwin>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace QtTwin;

namespace {

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

bool near(const Vector3d &a, const Vector3d &b, double tolerance) {
    return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) && near(a.z, b.z, tolerance);
}

// One millimetre of arc on the Earth is about 9e-9 degrees.
constexpr double kDegreeTolerance = 1e-9;

void testVector() {
    constexpr Vector3d a(1.0, 2.0, 3.0);
    constexpr Vector3d b(4.0, -5.0, 6.0);
    static_assert((a + b) == Vector3d(5.0, -3.0, 9.0));
    static_assert((b - a) == Vector3d(3.0, -7.0, 3.0));
    static_assert((a * 2.0) == Vector3d(2.0, 4.0, 6.0));
    static_assert(a.dotProduct(b) == 12.0);
    static_assert(a != b);
    CHECK(near(Vector3d(3.0, 4.0, 12.0).length(), 13.0, 1e-12));
    CHECK(Vector3d() == Vector3d(0.0, 0.0, 0.0));
}

void testEllipsoid() {
    constexpr Ellipsoid wgs84 = Ellipsoid::wgs84();
    static_assert(wgs84.semiMajorAxis() == 6378137.0);
    CHECK(near(wgs84.semiMinorAxis(), 6356752.314245, 1e-6));
    CHECK(near(wgs84.eccentricitySquared(), 0.00669437999014, 1e-14));

    struct Case {
        GeoPosition geo;
        Vector3d ecef;
    };
    const Case cases[] = {
        {GeoPosition(51.1789, -1.8262, 160.0), Vector3d(4004609.187618, -127682.961452, 4946170.236013)},
        {GeoPosition(-33.8568, 151.2153, 25.0), Vector3d(-4646986.832873, 2553086.916919, -3533281.055469)},
        {GeoPosition(35.0, 139.0, 400000.0), Vector3d(-4194742.003147, 3646433.591896, 3867297.483919)},
        {GeoPosition(89.9, 45.0, -50.0), Vector3d(7897.891239, 7897.891239, 6356692.567185)},
        {GeoPosition(0.0, 0.0, 0.0), Vector3d(6378137.0, 0.0, 0.0)},
    };
    for (const Case &c : cases) {
        CHECK(near(wgs84.toEcef(c.geo), c.ecef, 1e-5));
        const GeoPosition back = wgs84.fromEcef(c.ecef);
        CHECK(near(back.latitude, c.geo.latitude, kDegreeTolerance));
        CHECK(near(back.longitude, c.geo.longitude, kDegreeTolerance));
        CHECK(near(back.height, c.geo.height, 1e-5));
    }

    // The poles, where the distance from the axis is 0.
    const GeoPosition north = wgs84.fromEcef(Vector3d(0.0, 0.0, wgs84.semiMinorAxis() + 100.0));
    CHECK(near(north.latitude, 90.0, kDegreeTolerance));
    CHECK(near(north.height, 100.0, 1e-6));
    const GeoPosition south = wgs84.fromEcef(Vector3d(0.0, 0.0, -wgs84.semiMinorAxis() - 2000.0));
    CHECK(near(south.latitude, -90.0, kDegreeTolerance));
    CHECK(near(south.height, 2000.0, 1e-6));
    const GeoPosition centre = wgs84.fromEcef(Vector3d());
    CHECK(centre.latitude == 0.0 && centre.longitude == 0.0 && centre.height == -wgs84.semiMajorAxis());

    // Round trips from deep below the surface to geostationary height.
    for (double lat = -90.0; lat <= 90.0; lat += 7.5) {
        for (double lon = -180.0; lon < 180.0; lon += 22.5) {
            for (double h : {-11000.0, 0.0, 8848.0, 35786000.0}) {
                const GeoPosition geo(lat, lon, h);
                const GeoPosition back = wgs84.fromEcef(wgs84.toEcef(geo));
                CHECK(near(back.latitude, lat, kDegreeTolerance));
                if (std::fabs(lat) < 90.0)
                    CHECK(near(back.longitude, lon, kDegreeTolerance));
                CHECK(near(back.height, h, 1e-5));
            }
        }
    }

    const Vector3d normal = wgs84.surfaceNormal(45.0, 90.0);
    CHECK(near(normal, Vector3d(0.0, std::sqrt(0.5), std::sqrt(0.5)), 1e-15));
}

void testGeoPosition() {
    CHECK(GeoPosition().isValid());
    CHECK(GeoPosition(90.0, 180.0, -100.0).isValid());
    CHECK(GeoPosition(-90.0, -180.0).isValid());
    CHECK(!GeoPosition(90.5, 0.0).isValid());
    CHECK(!GeoPosition(0.0, 180.5).isValid());
    CHECK(!GeoPosition(std::nan(""), 0.0).isValid());
    CHECK(!GeoPosition(0.0, 0.0, HUGE_VAL).isValid());

    const GeoPosition stonehenge(51.1789, -1.8262, 160.0);
    const QGeoCoordinate coordinate = stonehenge.toGeoCoordinate();
    CHECK(coordinate == QGeoCoordinate(51.1789, -1.8262, 160.0));
    const GeoPosition back = GeoPosition::fromGeoCoordinate(coordinate);
    CHECK(back.latitude == stonehenge.latitude && back.longitude == stonehenge.longitude && back.height == 160.0);
    CHECK(GeoPosition::fromGeoCoordinate(QGeoCoordinate(1.0, 2.0)).height == 0.0);
    CHECK(!GeoPosition::fromGeoCoordinate(QGeoCoordinate()).isValid());
    CHECK(!GeoPosition(91.0, 0.0).toGeoCoordinate().isValid());
}

void testLocalFrame() {
    const Ellipsoid wgs84 = Ellipsoid::wgs84();
    const GeoPosition origin(51.1789, -1.8262, 100.0);
    const LocalFrame frame(origin);
    CHECK(near(frame.originEcef(), wgs84.toEcef(origin), 1e-9));
    CHECK(frame.origin().latitude == origin.latitude);
    CHECK(near(frame.toLocal(frame.originEcef()), Vector3d(), 1e-9));

    // The axes are orthonormal and right-handed.
    CHECK(near(frame.east().length(), 1.0, 1e-15));
    CHECK(near(frame.north().length(), 1.0, 1e-15));
    CHECK(near(frame.up().length(), 1.0, 1e-15));
    CHECK(near(frame.east().dotProduct(frame.north()), 0.0, 1e-15));
    CHECK(near(frame.east().dotProduct(frame.up()), 0.0, 1e-15));
    CHECK(near(frame.north().dotProduct(frame.up()), 0.0, 1e-15));
    const Vector3d e = frame.east();
    const Vector3d n = frame.north();
    const Vector3d cross(e.y * n.z - e.z * n.y, e.z * n.x - e.x * n.z, e.x * n.y - e.y * n.x);
    CHECK(near(cross, frame.up(), 1e-15));

    // A point straight above the origin is straight up; a point north of it is north and below the
    // tangent plane, by the curvature of the Earth (about d^2 / 2R).
    const Vector3d above = frame.toLocal(wgs84.toEcef(GeoPosition(origin.latitude, origin.longitude, 1100.0)));
    CHECK(near(above, Vector3d(0.0, 0.0, 1000.0), 1e-6));
    const Vector3d north = frame.toLocal(wgs84.toEcef(GeoPosition(origin.latitude + 0.27, origin.longitude, 100.0)));
    CHECK(near(north.x, 0.0, 1e-6));
    CHECK(north.y > 30000.0 && north.y < 30100.0);
    const double drop = north.y * north.y / (2.0 * 6371000.0);
    CHECK(near(north.z, -drop, 1.0));
    const Vector3d east = frame.toLocal(wgs84.toEcef(GeoPosition(origin.latitude, origin.longitude + 0.1, 100.0)));
    CHECK(east.x > 6900.0 && east.x < 7100.0);

    // Local to ECEF and back, and the matrix, which must match toEcef().
    const Vector3d local(-1234.5, 678.25, 42.0);
    CHECK(near(frame.toLocal(frame.toEcef(local)), local, 1e-8));
    const std::array<double, 16> m = frame.toEcefMatrix();
    const Vector3d viaMatrix(m[0] * local.x + m[4] * local.y + m[8] * local.z + m[12],
                             m[1] * local.x + m[5] * local.y + m[9] * local.z + m[13],
                             m[2] * local.x + m[6] * local.y + m[10] * local.z + m[14]);
    CHECK(near(viaMatrix, frame.toEcef(local), 1e-8));
    CHECK(m[3] == 0.0 && m[7] == 0.0 && m[11] == 0.0 && m[15] == 1.0);

    const LocalFrame null;
    CHECK(near(null.east(), Vector3d(0.0, 1.0, 0.0), 1e-15));
    CHECK(near(null.up(), Vector3d(1.0, 0.0, 0.0), 1e-15));
}

void testUtmZone() {
    CHECK(UtmZone::containing(51.1789, -1.8262).number == 30);
    CHECK(UtmZone::containing(51.1789, -1.8262).north);
    CHECK(UtmZone::containing(51.1789, -1.8262).toString() == QStringLiteral("30N"));
    CHECK(UtmZone::containing(-33.8568, 151.2153).toString() == QStringLiteral("56S"));
    CHECK(UtmZone::containing(-33.8568, 151.2153).epsgCode() == 32756);
    CHECK(UtmZone::containing(0.0, -180.0).number == 1);
    CHECK(UtmZone::containing(0.0, 180.0).number == 1);
    CHECK(UtmZone::containing(0.0, 179.999).number == 60);
    CHECK(UtmZone::containing(0.0, 0.0).number == 31);
    CHECK(UtmZone::containing(-0.001, 0.0).epsgCode() == 32731);
    // South-western Norway and Svalbard.
    CHECK(UtmZone::containing(60.0, 5.3).number == 32);
    CHECK(UtmZone::containing(55.9, 5.3).number == 31);
    CHECK(UtmZone::containing(78.2, 15.6).number == 33);
    CHECK(UtmZone::containing(78.2, 8.0).number == 31);
    CHECK(UtmZone::containing(78.2, 25.0).number == 35);
    CHECK(UtmZone::containing(78.2, 40.0).number == 37);
    // The polar caps have no zone.
    CHECK(!UtmZone::containing(84.5, 0.0).isValid());
    CHECK(!UtmZone::containing(-80.5, 0.0).isValid());
    CHECK(UtmZone::containing(-80.5, 0.0).toString().isEmpty());
    CHECK(UtmZone::containing(-80.5, 0.0).epsgCode() == 0);

    static_assert(UtmZone(30, true).centralMeridian() == -3.0);
    static_assert(UtmZone(1, false).centralMeridian() == -177.0);
    static_assert(UtmZone(60, true).epsgCode() == 32660);
    static_assert(!UtmZone().isValid());
    const TransverseMercator south = UtmZone(56, false).projection();
    CHECK(south.centralMeridian() == 153.0);
    CHECK(south.scaleFactor() == 0.9996);
    CHECK(south.falseEasting() == 500000.0);
    CHECK(south.falseNorthing() == 10000000.0);
}

void testTransverseMercator() {
    struct Case {
        UtmZone zone;
        double latitude;
        double longitude;
        GridPosition grid;
    };
    const Case cases[] = {
        {UtmZone(30, true), 51.1789, -1.8262, GridPosition(582046.706565, 5670374.377975)},
        {UtmZone(30, true), 45.0, -3.5, GridPosition(460592.350983, 4983071.987592)},
        // Beyond the edge of the zone, 2.9 degrees from its central meridian, near the equator.
        {UtmZone(30, true), 0.5, -5.9, GridPosition(177176.037447, 55336.377566)},
        {UtmZone(56, false), -33.8568, 151.2153, GridPosition(334900.569652, 6252288.752888)},
        {UtmZone(56, false), -79.5, 155.9, GridPosition(558974.642625, 1172753.137238)},
        {UtmZone(32, true), 60.0, 5.3, GridPosition(293694.312840, 6657183.077107)},
        {UtmZone(32, true), 70.0, 14.9, GridPosition(724905.401362, 7776766.425792)},
    };
    for (const Case &c : cases) {
        const TransverseMercator tm = c.zone.projection();
        const GridPosition grid = tm.forward(c.latitude, c.longitude);
        CHECK(near(grid.easting, c.grid.easting, 1e-5));
        CHECK(near(grid.northing, c.grid.northing, 1e-5));
        const GeoPosition back = tm.inverse(c.grid);
        CHECK(near(back.latitude, c.latitude, kDegreeTolerance));
        CHECK(near(back.longitude, c.longitude, kDegreeTolerance));
        CHECK(back.height == 0.0);
    }

    // The central meridian maps to the false easting, and the equator to the false northing.
    const TransverseMercator tm(15.0);
    CHECK(near(tm.forward(0.0, 15.0).easting, 500000.0, 1e-9));
    CHECK(near(tm.forward(0.0, 15.0).northing, 0.0, 1e-9));
    CHECK(near(tm.forward(48.0, 15.0).easting, 500000.0, 1e-9));
    CHECK(near(tm.forward(90.0, 40.0).easting, 500000.0, 1e-6));
    CHECK(near(tm.inverse(tm.forward(90.0, 0.0)).latitude, 90.0, 1e-9));

    // Round trips across and well beyond a zone, including across the antimeridian.
    for (double lat = -84.0; lat <= 84.0; lat += 6.0) {
        for (double dlon = -9.0; dlon <= 9.0; dlon += 1.5) {
            const GeoPosition back = tm.inverse(tm.forward(lat, 15.0 + dlon));
            CHECK(near(back.latitude, lat, kDegreeTolerance));
            CHECK(near(back.longitude, 15.0 + dlon, kDegreeTolerance));
        }
    }
    const TransverseMercator dateline(177.0);
    const GeoPosition across = dateline.inverse(dateline.forward(-17.0, -179.5));
    CHECK(near(across.latitude, -17.0, kDegreeTolerance));
    CHECK(near(across.longitude, -179.5, kDegreeTolerance));
    CHECK(dateline.forward(-17.0, -179.5).easting > 500000.0);
}

void testClock() {
    SimulationClock clock;
    CHECK(!clock.isRunning());
    const QDateTime start(QDate(2026, 11, 19), QTime(12, 0), Qt::UTC);
    int changes = 0;
    QObject::connect(&clock, &SimulationClock::timeChanged, [&changes] { ++changes; });
    clock.setTime(start);
    clock.advance(1500);
    CHECK(clock.time() == start.addMSecs(1500));
    clock.advance(0);
    CHECK(changes == 2);
    clock.setRate(-3.0);
    CHECK(clock.rate() == 0.0);
    clock.setRate(4.0);
    CHECK(clock.rate() == 4.0);
    clock.setTickInterval(0);
    CHECK(clock.tickInterval() == 1);

    CHECK(SimulationClock::leapSeconds(start) == 18);
    CHECK(SimulationClock::leapSeconds(QDateTime(QDate(2016, 12, 31), QTime(23, 59), Qt::UTC)) == 17);
    CHECK(SimulationClock::leapSeconds(QDateTime(QDate(1980, 1, 5), QTime(0, 0), Qt::UTC)) == 0);
    CHECK(SimulationClock::toGpsTime(start) == start.addSecs(18));

    // Running: four times real time for about 200 ms of wall-clock time.
    clock.setTickInterval(5);
    const QDateTime before = clock.time();
    clock.start();
    CHECK(clock.isRunning());
    QEventLoop loop;
    QTimer::singleShot(200, &loop, &QEventLoop::quit);
    loop.exec();
    clock.pause();
    const qint64 simulated = before.msecsTo(clock.time());
    CHECK(simulated >= 700 && simulated <= 1200);
}

// Due east at 10 m/s from Stonehenge, 100 m above the ellipsoid.
GeoPosition eastward(const QDateTime &start, const QDateTime &t) {
    const LocalFrame frame(GeoPosition(51.1789, -1.8262, 100.0));
    const double seconds = static_cast<double>(start.msecsTo(t)) / 1000.0;
    return Ellipsoid::wgs84().fromEcef(frame.toEcef(Vector3d(10.0 * seconds, 0.0, 0.0)));
}

void testReceiver() {
    const QDateTime start(QDate(2026, 11, 19), QTime(12, 0), Qt::UTC);
    SimulationClock clock;
    clock.setTime(start);
    GnssReceiver receiver;
    receiver.setClock(&clock);
    CHECK(receiver.clock() == &clock);
    receiver.setErrorModel(GnssErrorModel::perfect());
    receiver.setTrajectory([start](const QDateTime &t) { return eastward(start, t); });
    receiver.setUpdateInterval(200);
    receiver.setGeoidUndulation(47.9);
    QList<QGeoPositionInfo> fixes;
    QObject::connect(&receiver, &QGeoPositionInfoSource::positionUpdated,
                     [&fixes](const QGeoPositionInfo &info) { fixes.append(info); });
    receiver.startUpdates();
    clock.advance(1000);
    CHECK(fixes.size() == 6);
    if (fixes.size() == 6) {
        const QGeoPositionInfo &last = fixes.last();
        CHECK(last.timestamp() == start.addMSecs(1000));
        const GeoPosition truth = eastward(start, last.timestamp());
        CHECK(near(last.coordinate().latitude(), truth.latitude, 1e-9));
        CHECK(near(last.coordinate().longitude(), truth.longitude, 1e-9));
        CHECK(near(last.coordinate().altitude(), truth.height - 47.9, 1e-6));
        CHECK(near(last.attribute(QGeoPositionInfo::GroundSpeed), 10.0, 1e-3));
        CHECK(near(last.attribute(QGeoPositionInfo::Direction), 90.0, 0.01));
        CHECK(last.attribute(QGeoPositionInfo::HorizontalAccuracy) == 0.0);
    }
    CHECK(receiver.lastKnownPosition() == fixes.value(5));
    receiver.stopUpdates();
    clock.advance(1000);
    CHECK(fixes.size() == 6);

    // The same seed gives the same errors; another seed gives others. The errors have the size the
    // model gives them: with a correlation time much shorter than the spacing, every fix is
    // independent.
    GnssErrorModel model;
    model.correlationTime = 0.01;
    GnssReceiver a;
    GnssReceiver b;
    GnssReceiver c;
    for (GnssReceiver *r : {&a, &b, &c})
        r->setTrajectory([start](const QDateTime &t) { return eastward(start, t); });
    a.setErrorModel(model);
    b.setErrorModel(model);
    model.seed = 2;
    c.setErrorModel(model);
    double sumH = 0.0;
    double sumV = 0.0;
    const int n = 4000;
    bool same = true;
    bool different = false;
    const Ellipsoid wgs84 = Ellipsoid::wgs84();
    for (int i = 0; i < n; ++i) {
        const QDateTime t = start.addSecs(i);
        const QGeoPositionInfo fa = a.sample(t);
        const QGeoPositionInfo fb = b.sample(t);
        const QGeoPositionInfo fc = c.sample(t);
        same = same && fa == fb;
        different = different || fa.coordinate() != fc.coordinate();
        const GeoPosition truth = eastward(start, t);
        const Vector3d e = LocalFrame(truth).toLocal(wgs84.toEcef(GeoPosition::fromGeoCoordinate(fa.coordinate())));
        sumH += e.x * e.x + e.y * e.y;
        sumV += e.z * e.z;
    }
    CHECK(same);
    CHECK(different);
    const double rmsH = std::sqrt(sumH / n);
    const double rmsV = std::sqrt(sumV / n);
    const double expectedH = std::sqrt(2.0 * (1.5 * 1.5 + 0.5 * 0.5));
    const double expectedV = std::sqrt(3.0 * 3.0 + 1.0 * 1.0);
    std::printf("receiver errors: horizontal %.3f m RMS (model %.3f), vertical %.3f m RMS (model %.3f)\n", rmsH,
                expectedH, rmsV, expectedV);
    CHECK(std::fabs(rmsH - expectedH) < 0.1 * expectedH);
    CHECK(std::fabs(rmsV - expectedV) < 0.1 * expectedV);

    // A jammer 300 m east of the start: the fixes stop inside it and come back after it.
    SimulationClock jamClock;
    jamClock.setTime(start);
    GnssReceiver jammed;
    jammed.setClock(&jamClock);
    jammed.setErrorModel(GnssErrorModel::perfect());
    jammed.setTrajectory([start](const QDateTime &t) { return eastward(start, t); });
    const GeoPosition jammer = eastward(start, start.addSecs(30));
    jammed.setDeniedAreas({QGeoCircle(QGeoCoordinate(jammer.latitude, jammer.longitude), 100.0)});
    CHECK(jammed.deniedAreas().size() == 1);
    int fixCount = 0;
    int timeouts = 0;
    QObject::connect(&jammed, &QGeoPositionInfoSource::positionUpdated, [&fixCount] { ++fixCount; });
    QObject::connect(&jammed, &QGeoPositionInfoSource::errorOccurred, [&timeouts](QGeoPositionInfoSource::Error e) {
        if (e == QGeoPositionInfoSource::UpdateTimeoutError)
            ++timeouts;
    });
    jammed.startUpdates();
    jamClock.advance(60000);
    // 61 fixes at 1 Hz, less those within 100 m of the jammer: t = 20 to 40 s.
    CHECK(fixCount == 61 - 21);
    CHECK(timeouts == 1);
    CHECK(!jammed.sample(start.addSecs(30)).isValid());
}

void testNmeaRoundTrip() {
    const QDateTime start(QDate(2026, 11, 19), QTime(12, 0), Qt::UTC);
    SimulationClock clock;
    clock.setTime(start);
    GnssReceiver receiver;
    receiver.setClock(&clock);
    receiver.setGeoidUndulation(47.9);
    receiver.setTrajectory([start](const QDateTime &t) { return eastward(start, t); });
    QByteArray wire;
    QBuffer output(&wire);
    output.open(QIODevice::WriteOnly);
    receiver.setNmeaOutput(&output);
    CHECK(receiver.nmeaOutput() == &output);
    QList<QGeoPositionInfo> sent;
    QObject::connect(&receiver, &QGeoPositionInfoSource::positionUpdated,
                     [&sent](const QGeoPositionInfo &info) { sent.append(info); });
    receiver.startUpdates();
    clock.advance(4000);
    output.close();
    CHECK(sent.size() == 5);
    CHECK(wire.count("$GPGGA") == 5 && wire.count("$GPRMC") == 5 && wire.count("$GPGSA") == 5);

    QBuffer input(&wire);
    QNmeaPositionInfoSource reader(QNmeaPositionInfoSource::RealTimeMode);
    reader.setDevice(&input);
    QList<QGeoPositionInfo> read;
    QObject::connect(&reader, &QGeoPositionInfoSource::positionUpdated,
                     [&read](const QGeoPositionInfo &info) { read.append(info); });
    reader.startUpdates();
    CHECK(read.size() == sent.size());
    for (qsizetype i = 0; i < std::min(read.size(), sent.size()); ++i) {
        CHECK(read.at(i).timestamp() == sent.at(i).timestamp());
        CHECK(near(read.at(i).coordinate().latitude(), sent.at(i).coordinate().latitude(), 2e-7));
        CHECK(near(read.at(i).coordinate().longitude(), sent.at(i).coordinate().longitude(), 2e-7));
        CHECK(near(read.at(i).coordinate().altitude(), sent.at(i).coordinate().altitude(), 0.006));
        CHECK(near(read.at(i).attribute(QGeoPositionInfo::GroundSpeed), 10.0, 0.01));
    }
    CHECK(NmeaWriter::sentence("GPGLL,4916.45,N,12311.12,W,225444,A") ==
          QByteArray("$GPGLL,4916.45,N,12311.12,W,225444,A*31\r\n"));
}

void testRegisteredReceiver() {
    GnssReceiver::registerSource();
    CHECK(QGeoPositionInfoSource::availableSources().contains(QStringLiteral("nttwin")));
    QObject parent;
    QGeoPositionInfoSource *source = QGeoPositionInfoSource::createSource(
        QStringLiteral("nttwin"),
        {{QStringLiteral("latitude"), 51.1789}, {QStringLiteral("longitude"), -1.8262}, {QStringLiteral("seed"), 7}},
        &parent);
    CHECK(source && source->sourceName() == QStringLiteral("nttwin"));
    if (!source)
        return;
    CHECK(source->supportedPositioningMethods() == QGeoPositionInfoSource::SatellitePositioningMethods);
    QGeoPositionInfo fix;
    QObject::connect(source, &QGeoPositionInfoSource::positionUpdated,
                     [&fix](const QGeoPositionInfo &info) { fix = info; });
    source->requestUpdate();
    CHECK(fix.isValid());
    CHECK(fix.coordinate().distanceTo(QGeoCoordinate(51.1789, -1.8262)) < 25.0);
    QGeoPositionInfoSource::registerSource(QStringLiteral("nttwin"), {});

    GnssReceiver nothing;
    QGeoPositionInfoSource::Error error = QGeoPositionInfoSource::NoError;
    QObject::connect(&nothing, &QGeoPositionInfoSource::errorOccurred,
                     [&error](QGeoPositionInfoSource::Error e) { error = e; });
    nothing.startUpdates();
    CHECK(error == QGeoPositionInfoSource::AccessError);
}

constexpr double kPi = 3.14159265358979323846;

bool near(const EulerAngles &a, const EulerAngles &b, double tolerance) {
    return DeadReckoning::orientationDifference(a, b) <= tolerance;
}

void testEntityKinds() {
    for (EntityKind k :
         {EntityKind::Person, EntityKind::GroundVehicle, EntityKind::Aircraft, EntityKind::Drone, EntityKind::Boat}) {
        bool ok = false;
        CHECK(ModelRef::kindFromName(ModelRef::kindName(k), &ok) == k && ok);
        EntityKind back = EntityKind::Drone;
        CHECK(DisEntityType::forKind(k).toEntityKind(&back));
        CHECK(back == (k == EntityKind::Drone ? EntityKind::Aircraft : k));
    }
    bool ok = true;
    ModelRef::kindFromName(QStringLiteral("submarine"), &ok);
    CHECK(!ok);

    const DisEntityType t = DisEntityType::fromString(QStringLiteral("1.2.225.20.1.0.0"), &ok);
    CHECK(ok && t == DisEntityType(1, 2, 225, 20, 1));
    CHECK(t.toString() == QStringLiteral("1.2.225.20.1.0.0"));
    CHECK(DisEntityType::fromString(QStringLiteral("1.3"), &ok) == DisEntityType(1, 3, 0, 0) && ok);
    DisEntityType::fromString(QStringLiteral("1.300"), &ok);
    CHECK(!ok);
    DisEntityType::fromString(QStringLiteral("1.2.3.4.5.6.7.8"), &ok);
    CHECK(!ok);
    EntityKind k;
    CHECK(!DisEntityType(2, 1, 0, 0).toEntityKind(&k)); // munition
}

void testCatalogue() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    QDir(dir.path()).mkpath(QStringLiteral("vehicles"));
    const QByteArray glb("glTF-lr110");
    QFile f(dir.filePath(QStringLiteral("vehicles/lr110.glb")));
    CHECK(f.open(QIODevice::WriteOnly) && f.write(glb) == glb.size());
    f.close();
    const QByteArray hash = QCryptographicHash::hash(glb, QCryptographicHash::Sha256);
    const QByteArray json = QByteArray(R"({"models": [
        {"id": "land/wheeled/lr110", "revision": 3, "kind": "ground-vehicle", "dis": "1.1.225.6.1",
         "file": "vehicles/lr110.glb", "sha256": ")") +
                            hash.toHex() + QByteArray(R"("},
        {"id": "air/rotary/ec135", "revision": 4, "kind": "aircraft", "file": "air/ec135.glb"},
        {"id": "sea/rib", "revision": 1, "kind": "boat", "file": "vehicles/lr110.glb", "sha256": "00ff"},
        {"id": "", "revision": 1},
        {"id": "x/bad", "kind": "submarine"}
    ]})");
    QFile c(dir.filePath(QStringLiteral("models.json")));
    CHECK(c.open(QIODevice::WriteOnly) && c.write(json) == json.size());
    c.close();

    ModelCatalogue verified;
    CHECK(verified.load(dir.filePath(QStringLiteral("models.json"))));
    CHECK(verified.size() == 1); // the helicopter's file is missing, the boat's hash is wrong
    CHECK(verified.warnings().size() == 4);
    const ModelRef lr = verified.find(QStringLiteral("land/wheeled/lr110"));
    CHECK(lr.isValid() && lr.revision == 3 && lr.sha256 == hash && lr.disType == DisEntityType(1, 1, 225, 6, 1));
    CHECK(verified.filePath(lr.id) == QDir(dir.path()).absoluteFilePath(QStringLiteral("vehicles/lr110.glb")));
    CHECK(!verified.find(QStringLiteral("air/rotary/ec135")).isValid());

    ModelCatalogue unverified;
    CHECK(unverified.loadFromJson(json));
    CHECK(unverified.size() == 3);
    CHECK(unverified.find(QStringLiteral("air/rotary/ec135")).disType == DisEntityType::forKind(EntityKind::Aircraft));
    CHECK(!unverified.loadFromJson("[]") && unverified.isEmpty());
    CHECK(!ModelCatalogue().load(dir.filePath(QStringLiteral("nothing.json"))));

    ModelRef need = lr;
    CHECK(verified.canRender(need));
    need.revision = 2;
    CHECK(verified.canRender(need)); // an older revision is fine
    need.revision = 4;
    CHECK(!verified.canRender(need)); // a newer one is not
    need.revision = 3;
    CHECK(verified.canRender(need, true));
    need.sha256 = QByteArray(32, 'x');
    CHECK(!verified.canRender(need, true) && verified.canRender(need));

    ModelRef heli;
    heli.id = QStringLiteral("air/rotary/ec135");
    heli.revision = 4;
    const QList<ModelRef> lacking = verified.missing({lr, heli, heli});
    CHECK(lacking.size() == 1 && lacking.at(0).id == heli.id);
    CHECK(heli.toString() == QStringLiteral("air/rotary/ec135 r4"));

    ModelCatalogue copy;
    copy.insert(lr);
    CHECK(copy.digest() == verified.digest());
    copy.insert(heli);
    CHECK(copy.digest() != verified.digest() && copy.size() == 2);
    copy.insert(ModelRef());
    CHECK(copy.size() == 2);
}

void testOrientation() {
    // At latitude 0, longitude 0, facing east: forward is ECEF +y, right is south (-z), down is -x.
    EntityState s;
    s.setGeoPosition(GeoPosition(0.0, 0.0, 0.0));
    s.setHeadingPitchRoll(HeadingPitchRoll(90.0, 0.0, 0.0));
    CHECK(near(s.orientation, EulerAngles(kPi / 2, 0.0, -kPi / 2), 1e-9));

    const GeoPosition places[] = {GeoPosition(51.1789, -1.8262, 160.0), GeoPosition(-33.8568, 151.2153, 25.0),
                                  GeoPosition(78.2, 15.6, 10.0), GeoPosition(-0.5, -179.9, 3000.0)};
    const HeadingPitchRoll attitudes[] = {HeadingPitchRoll(0.0, 0.0, 0.0), HeadingPitchRoll(212.0, 4.5, -2.0),
                                          HeadingPitchRoll(359.0, -30.0, 60.0), HeadingPitchRoll(90.0, 80.0, 0.0)};
    for (const GeoPosition &p : places) {
        for (const HeadingPitchRoll &a : attitudes) {
            EntityState e;
            e.setGeoPosition(p);
            e.setHeadingPitchRoll(a);
            const HeadingPitchRoll back = e.headingPitchRoll();
            const double dh = std::fabs(std::remainder(back.heading - a.heading, 360.0));
            CHECK(dh < 1e-7 && near(back.pitch, a.pitch, 1e-7) && near(back.roll, a.roll, 1e-7));
            e.setLocalVelocity(Vector3d(-5.3, -8.5, 1.25));
            CHECK(near(e.localVelocity(), Vector3d(-5.3, -8.5, 1.25), 1e-9));
            CHECK(near(e.velocity.length(), Vector3d(-5.3, -8.5, 1.25).length(), 1e-9));
        }
    }

    // Up is away from the centre of the Earth.
    EntityState up;
    up.setGeoPosition(GeoPosition(51.0, -1.0, 100.0));
    up.setLocalVelocity(Vector3d(0.0, 0.0, 10.0));
    const Vector3d later = up.position + up.velocity;
    CHECK(near(Ellipsoid::wgs84().fromEcef(later).height, 110.0, 1e-3));
}

void testDeadReckoning() {
    EntityState s;
    s.time = 1000;
    s.setGeoPosition(GeoPosition(51.1789, -1.8262, 160.0));
    s.setHeadingPitchRoll(HeadingPitchRoll(0.0, 0.0, 0.0));
    s.setLocalVelocity(Vector3d(0.0, 10.0, 0.0));
    s.deadReckoning = DeadReckoningAlgorithm::Fpw;

    EntityState p = DeadReckoning::extrapolate(s, 3000);
    CHECK(p.time == 3000);
    CHECK(near(p.position, s.position + s.velocity * 2.0, 1e-9));
    CHECK(p.orientation == s.orientation);

    s.deadReckoning = DeadReckoningAlgorithm::Static;
    CHECK(DeadReckoning::extrapolate(s, 9000).position == s.position);

    s.deadReckoning = DeadReckoningAlgorithm::Fvw;
    s.acceleration = Vector3d(1.0, 0.0, -2.0);
    p = DeadReckoning::extrapolate(s, 3000);
    CHECK(near(p.position, s.position + s.velocity * 2.0 + s.acceleration * 2.0, 1e-9)); // 0.5 * 2^2 = 2
    CHECK(near(p.velocity, s.velocity + s.acceleration * 2.0, 1e-9));

    // Turning right at 0.1 rad/s for 10 s, from north: a heading of 57.2958 degrees.
    s.acceleration = Vector3d();
    s.deadReckoning = DeadReckoningAlgorithm::Rpw;
    s.angularVelocity = Vector3d(0.0, 0.0, 0.1);
    p = DeadReckoning::extrapolate(s, 11000);
    CHECK(near(p.headingPitchRoll().heading, 57.29578, 1e-3));
    CHECK(near(p.headingPitchRoll().pitch, 0.0, 1e-2)); // the horizon tilts by 0.001 degrees over 100 m

    // Rolling right at 0.2 rad/s for 1 s.
    s.angularVelocity = Vector3d(0.2, 0.0, 0.0);
    p = DeadReckoning::extrapolate(s, 2000);
    CHECK(near(p.headingPitchRoll().roll, 11.45916, 1e-3));

    // Whether the owner sends.
    EntityState sent = s;
    sent.deadReckoning = DeadReckoningAlgorithm::Fpw;
    sent.angularVelocity = Vector3d();
    const DeadReckoningThresholds t = DeadReckoningThresholds::forKind(EntityKind::GroundVehicle);
    CHECK(t.position == 0.5 && t.heartbeat == 5000 && t.staleTimeout() == 12000);
    EntityState now = DeadReckoning::extrapolate(sent, 2000);
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::None);
    now.position = now.position + Vector3d(0.0, 0.0, 0.6);
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::Threshold);
    now.time = sent.time + 10;
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::RateLimited);
    now = DeadReckoning::extrapolate(sent, 2000);
    now.setHeadingPitchRoll(HeadingPitchRoll(4.0, 0.0, 0.0));
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::Threshold);
    now = DeadReckoning::extrapolate(sent, 2000);
    now.appearance = 0x10;
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::Changed);
    now = DeadReckoning::extrapolate(sent, 2000);
    now.articulatedParts.append(ArticulatedPart{4107, 0.5f, 0, 1});
    CHECK(DeadReckoning::updateReason(sent, now, t) == DeadReckoning::UpdateReason::Changed);

    // A parked entity sends only heartbeats.
    EntityState parked = sent;
    parked.velocity = Vector3d();
    parked.deadReckoning = DeadReckoningAlgorithm::Static;
    EntityState still = parked;
    still.time = parked.time + 4999;
    CHECK(DeadReckoning::updateReason(parked, still, t) == DeadReckoning::UpdateReason::None);
    still.time = parked.time + 5000;
    CHECK(DeadReckoning::updateReason(parked, still, t) == DeadReckoning::UpdateReason::Heartbeat);

    // Blending.
    EntityState a = sent;
    EntityState b = sent;
    b.position = a.position + Vector3d(10.0, 0.0, 0.0);
    b.setHeadingPitchRoll(HeadingPitchRoll(90.0, 0.0, 0.0));
    const EntityState mid = DeadReckoning::blend(a, b, 0.5);
    CHECK(near(mid.position, a.position + Vector3d(5.0, 0.0, 0.0), 1e-9));
    CHECK(near(mid.headingPitchRoll().heading, 45.0, 1e-6));
    CHECK(near(DeadReckoning::blend(a, b, 2.0).position, b.position, 1e-9));
    CHECK(near(DeadReckoning::blend(a, b, 0.0).orientation, a.orientation, 1e-9));
}

quint16 be16(const QByteArray &d, int at) {
    return static_cast<quint16>((static_cast<uchar>(d[at]) << 8) | static_cast<uchar>(d[at + 1]));
}

double be64double(const QByteArray &d, int at) {
    quint64 bits = 0;
    for (int i = 0; i < 8; ++i)
        bits = (bits << 8) | static_cast<uchar>(d[at + i]);
    double v = 0.0;
    std::memcpy(&v, &bits, sizeof v);
    return v;
}

void testDisPdu() {
    DisEntityStatePdu pdu;
    pdu.exercise = 9;
    pdu.entityId = DisEntityId(1, 2, 3);
    pdu.force = DisForce::Friendly;
    pdu.entityType = DisEntityType(1, 2, 225, 20, 1);
    pdu.alternativeType = pdu.entityType;
    pdu.state.setGeoPosition(GeoPosition(51.1789, -1.8262, 160.0));
    pdu.state.setHeadingPitchRoll(HeadingPitchRoll(212.0, 3.0, -1.0));
    pdu.state.setLocalVelocity(Vector3d(-5.0, -8.0, 0.5));
    pdu.state.acceleration = Vector3d(0.25, -0.5, 1.0);
    pdu.state.angularVelocity = Vector3d(0.0, 0.0, 0.125);
    pdu.state.appearance = 0x00010020;
    pdu.state.deadReckoning = DeadReckoningAlgorithm::Rvw;
    pdu.state.articulatedParts = {ArticulatedPart{4107, 1.5f, 0, 2}, ArticulatedPart{4096 + 11, -0.25f, 1, 0}};
    pdu.marking = "ALPHA-1";
    pdu.capabilities = 7;
    pdu.timestamp = DisEntityStatePdu::timestampFromMsec(1234567);

    const QByteArray bytes = pdu.encode();
    CHECK(bytes.size() == 144 + 2 * 16);
    CHECK(bytes[0] == 7 && bytes[1] == 9 && bytes[2] == 1 && bytes[3] == 1);
    CHECK(be16(bytes, 8) == bytes.size());
    CHECK(be16(bytes, 12) == 1 && be16(bytes, 14) == 2 && be16(bytes, 16) == 3);
    CHECK(bytes[18] == 1 && bytes[19] == 2);
    CHECK(be16(bytes, 22) == 225);
    CHECK(be64double(bytes, 48) == pdu.state.position.x && be64double(bytes, 64) == pdu.state.position.z);
    CHECK(bytes[88] == 4);
    CHECK(bytes[128] == 1 && bytes.mid(129, 7) == "ALPHA-1" && bytes[136] == 0);

    DisEntityStatePdu back;
    qsizetype length = 0;
    CHECK(back.decode(bytes, 0, &length) && length == bytes.size());
    CHECK(back.exercise == 9 && back.entityId == pdu.entityId && back.force == DisForce::Friendly);
    CHECK(back.entityType == pdu.entityType && back.alternativeType == pdu.entityType);
    CHECK(back.state.position == pdu.state.position); // doubles on the wire
    CHECK(near(back.state.velocity, pdu.state.velocity, 1e-5));
    CHECK(near(back.state.orientation, pdu.state.orientation, 1e-4));
    CHECK(back.state.acceleration == pdu.state.acceleration && back.state.angularVelocity == pdu.state.angularVelocity);
    CHECK(back.state.appearance == pdu.state.appearance && back.state.deadReckoning == DeadReckoningAlgorithm::Rvw);
    CHECK(back.state.articulatedParts == pdu.state.articulatedParts);
    CHECK(back.marking == "ALPHA-1" && back.capabilities == 7 && back.timestamp == pdu.timestamp);

    // A long marking is cut to 11 characters.
    DisEntityStatePdu longMark = pdu;
    longMark.marking = "ABCDEFGHIJKLMNOP";
    CHECK(back.decode(longMark.encode()) && back.marking == "ABCDEFGHIJK");

    // Bundled PDUs, with a PDU of another type between them and garbage after them.
    DisEntityStatePdu second = pdu;
    second.entityId = DisEntityId(1, 2, 4);
    second.state.articulatedParts.clear();
    QByteArray other(24, '\0');
    other[0] = 7;
    other[2] = 22; // a Comment PDU
    other[9] = 24;
    const QByteArray bundle = bytes + other + second.encode() + QByteArray("junk");
    const QList<DisEntityStatePdu> all = DisEntityStatePdu::decodeAll(bundle);
    CHECK(all.size() == 2 && all.at(0).entityId.entity == 3 && all.at(1).entityId.entity == 4);

    // Malformed PDUs are refused.
    CHECK(!back.decode(bytes.left(143)));
    QByteArray wrong = bytes;
    wrong[0] = 4;
    CHECK(!back.decode(wrong));
    wrong = bytes;
    wrong[19] = 9; // more records than the length holds
    CHECK(!back.decode(wrong));
    wrong = bytes;
    wrong[9] = static_cast<char>(200);
    CHECK(!back.decode(wrong));
    CHECK(back.marking == "ABCDEFGHIJK"); // unchanged by the failures

    // Timestamps.
    for (qint64 t : {qint64(0), qint64(1234567), qint64(3599999), qint64(3600000 * 5 + 17)}) {
        const quint32 ts = DisEntityStatePdu::timestampFromMsec(t);
        CHECK((ts & 1u) == 0);
        CHECK(std::llabs(DisEntityStatePdu::msecFromTimestamp(ts, t + 250) - t) <= 1);
    }
    // Sent just before the hour, received just after.
    const quint32 late = DisEntityStatePdu::timestampFromMsec(3600000 - 100);
    CHECK(std::llabs(DisEntityStatePdu::msecFromTimestamp(late, 3600000 + 50) - (3600000 - 100)) <= 1);
    const quint32 early = DisEntityStatePdu::timestampFromMsec(7200000 + 100);
    CHECK(std::llabs(DisEntityStatePdu::msecFromTimestamp(early, 7200000 - 50) - (7200000 + 100)) <= 1);

    CHECK(DisEntityId(1, 2, 3).toString() == QStringLiteral("1:2:3"));
    CHECK(DisEntityId::fromKey(DisEntityId(7, 65535, 9).key()) == DisEntityId(7, 65535, 9));
    CHECK(!DisEntityId(0, 1, 1).isValid() && DisEntityId(1, 1, 1).isValid());
}

void testAtmosphere() {
    const Atmosphere isa;
    // ISO 2533 table values.
    AirState a = isa.at(0.0);
    CHECK(near(a.temperature, 288.15, 1e-9) && near(a.pressure, 101325.0, 1e-6) && near(a.density, 1.225, 1e-4));
    CHECK(near(a.speedOfSound, 340.29, 0.01));
    // The tables are indexed by geopotential height; convert them to the geometric height at() takes.
    const auto geometric = [](double geopotential) { return 6356766.0 * geopotential / (6356766.0 - geopotential); };
    a = isa.at(geometric(1000.0));
    CHECK(near(a.temperature, 281.65, 0.01) && near(a.pressure, 89874.6, 1.0) && near(a.density, 1.1117, 1e-3));
    a = isa.at(geometric(5000.0));
    CHECK(near(a.pressure, 54019.9, 1.0) && near(a.density, 0.7364, 1e-3));
    a = isa.at(geometric(11000.0));
    CHECK(near(a.temperature, 216.65, 0.01) && near(a.pressure, 22632.1, 1.0));
    a = isa.at(geometric(20000.0));
    CHECK(near(a.temperature, 216.65, 0.01) && near(a.pressure, 5474.9, 1.0)); // isothermal layer
    a = isa.at(geometric(32000.0));
    CHECK(near(a.temperature, 228.65, 0.01) && near(a.pressure, 868.02, 0.2));
    a = isa.at(geometric(47000.0));
    CHECK(near(a.temperature, 270.65, 0.01) && near(a.pressure, 110.91, 0.1));
    a = isa.at(geometric(71000.0));
    CHECK(near(a.temperature, 214.65, 0.01) && near(a.pressure, 3.9564, 0.01));
    CHECK(near(isa.at(5000.0).pressure, 54048.0, 1.0));           // geometric 5000 m is geopotential 4996 m
    CHECK(isa.at(100000.0).pressure == isa.at(86000.0).pressure); // clamped
    CHECK(near(Atmosphere::geopotentialHeight(11000.0), 10981.0, 1.0));
    // A warm, low-pressure day is thinner.
    const Atmosphere day(100000.0, 303.15);
    CHECK(day.density(0.0) < isa.density(0.0) && near(day.density(0.0), 100000.0 / (287.05287 * 303.15), 1e-9));
    CHECK(day.temperature(1000.0) < day.temperature(0.0));
    // A barometric altimeter reads back the height.
    for (double h : {0.0, 350.0, 2500.0, 9000.0})
        CHECK(near(isa.pressureAltitude(isa.pressure(h)), h, 1e-3));
    CHECK(isa.pressureAltitude(200000.0) == 0.0);
}

void testWind() {
    const WindModel calm;
    CHECK(calm.isCalm() && calm.at(100.0) == Vector3d() && calm.atEcef(Vector3d(4e6, 0.0, 4.9e6)) == Vector3d());
    // A westerly (from 270) blows east.
    const WindModel westerly(10.0, 270.0);
    CHECK(near(westerly.at(50.0), Vector3d(10.0, 0.0, 0.0), 1e-9));
    CHECK(near(WindModel::speed(westerly.at(0.0)), 10.0, 1e-9) &&
          near(WindModel::directionFrom(westerly.at(0.0)), 270.0, 1e-9));
    // A northerly blows south.
    CHECK(near(WindLayer::fromSpeedAndDirection(0.0, 5.0, 0.0).velocity, Vector3d(0.0, -5.0, 0.0), 1e-9));
    CHECK(near(WindModel::directionFrom(Vector3d(0.0, -5.0, 0.0)), 0.0, 1e-9));
    CHECK(near(WindModel::directionFrom(Vector3d(-3.0, -3.0, 0.0)), 45.0,
               1e-9)); // blowing south-west: from the north-east
    // Layers interpolate and clamp.
    WindModel profile;
    profile.addLayer(WindLayer(100.0, Vector3d(2.0, 0.0, 0.0)));
    profile.addLayer(WindLayer(0.0, Vector3d(0.0, 0.0, 0.0)));
    profile.addLayer(WindLayer(100.0, Vector3d(4.0, 0.0, 0.0))); // replaces
    CHECK(profile.layers().size() == 2 && profile.layers().first().height == 0.0);
    CHECK(near(profile.at(50.0), Vector3d(2.0, 0.0, 0.0), 1e-9));
    CHECK(near(profile.at(-10.0), Vector3d(), 1e-9) && near(profile.at(500.0), Vector3d(4.0, 0.0, 0.0), 1e-9));
    // In ECEF, an east wind at Amesbury has no component along the local up.
    const Vector3d at = Ellipsoid::wgs84().toEcef(GeoPosition(51.1789, -1.8262, 50.0));
    const Vector3d ecef = profile.atEcef(at);
    CHECK(near(ecef.length(), 2.0, 1e-9));
    CHECK(near(ecef.dotProduct(Ellipsoid::wgs84().surfaceNormal(51.1789, -1.8262)), 0.0, 1e-9));
    const LocalFrame frame(GeoPosition(51.1789, -1.8262, 50.0));
    CHECK(near(ecef.dotProduct(frame.east()), 2.0, 1e-9));
}

void testGravity() {
    // WGS 84 normal gravity.
    CHECK(near(Gravity::magnitude(0.0), 9.7803253359, 1e-9));
    CHECK(near(Gravity::magnitude(90.0), 9.8321849379, 1e-8));
    CHECK(near(Gravity::magnitude(45.0), 9.806199, 1e-5));
    CHECK(near(Gravity::magnitude(45.0, 1000.0), 9.806199 - 0.003086, 2e-5)); // about 3.086 mm/s^2 per km
    const GeoPosition amesbury(51.1789, -1.8262, 100.0);
    const Vector3d g = Gravity::at(amesbury);
    CHECK(near(g.length(), Gravity::magnitude(51.1789, 100.0), 1e-12));
    CHECK(near(g.normalized().dotProduct(Ellipsoid::wgs84().surfaceNormal(51.1789, -1.8262)), -1.0, 1e-12));
    CHECK(near(Gravity::at(Ellipsoid::wgs84().toEcef(amesbury)), g, 1e-9));
    CHECK(Vector3d(1.0, 0.0, 0.0).crossProduct(Vector3d(0.0, 1.0, 0.0)) == Vector3d(0.0, 0.0, 1.0));
    CHECK(Vector3d().normalized() == Vector3d());
}

void testPhysics() {
    const GeoPosition site(51.1789, -1.8262, 100.0);
    const LocalFrame frame(site);
    const Vector3d launcher = frame.originEcef();

    // A vacuum shot at 45 degrees lands at v^2/g, with the local g of the site.
    Environment vacuum;
    vacuum.drag = false;
    PhysicsEngine engine(vacuum);
    engine.setTerrain([](const GeoPosition &) { return 100.0; });
    PointMass shot;
    shot.position = launcher;
    shot.velocity = PhysicsEngine::launchVelocity(launcher, 100.0, 90.0, 45.0); // east
    CHECK(near(shot.velocity.length(), 100.0, 1e-9));
    CHECK(near(shot.velocity.dotProduct(frame.east()), 100.0 * std::cos(M_PI / 4.0), 1e-9));
    const Trajectory flight = engine.simulate(shot, 60.0, 0.1);
    const double g = Gravity::magnitude(site.latitude, site.height);
    CHECK(flight.end == Trajectory::End::Ground);
    CHECK(near(flight.timeOfFlight, 2.0 * 100.0 * std::sin(M_PI / 4.0) / g, 0.02));
    CHECK(near(flight.range, 100.0 * 100.0 / g, 2.0)); // the curved Earth and the ellipsoid add a little
    CHECK(near(flight.apogee - 100.0, 100.0 * 100.0 * 0.5 / (2.0 * g), 0.5));
    const Vector3d local = frame.toLocal(flight.impact);
    CHECK(local.x > 1000.0 && std::fabs(local.y) < 1.0 && std::fabs(local.z) < 0.5);
    CHECK(flight.samples.size() > 100 && near(flight.at(0.0).position, launcher, 1e-9));
    CHECK(near(flight.at(1.0).position, flight.samples.at(10).position, 1e-6));

    // Drag shortens the flight, and wind blows the round sideways.
    Environment air;
    PhysicsEngine airEngine(air);
    airEngine.setTerrain([](const GeoPosition &) { return 100.0; });
    PointMass ball = PointMass::sphere(2.0, 0.1, 0.47); // a heavy ball
    CHECK(near(ball.referenceArea, M_PI * 0.0025, 1e-12));
    ball.position = launcher;
    ball.velocity = shot.velocity;
    const Trajectory dragged = airEngine.simulate(ball, 60.0);
    CHECK(dragged.end == Trajectory::End::Ground && dragged.range < flight.range * 0.7 &&
          dragged.range > flight.range * 0.45);
    CHECK(near(frame.toLocal(dragged.impact).y, 0.0, 0.5));
    CHECK(airEngine.mach(ball) > 0.28 && airEngine.mach(ball) < 0.3);
    air.wind = WindModel(10.0, 180.0); // from the south: pushes north
    airEngine.setEnvironment(air);
    const Trajectory blown = airEngine.simulate(ball, 60.0);
    CHECK(frame.toLocal(blown.impact).y > 5.0);
    CHECK(near(airEngine.airVelocity(ball).dotProduct(frame.north()), -10.0, 1e-6));

    // A dropped drone falls; one that keeps flying does not.
    EntityState drone;
    drone.time = 5000;
    drone.setGeoPosition(GeoPosition(51.1789, -1.8262, 300.0));
    drone.setHeadingPitchRoll(HeadingPitchRoll(0.0, 0.0, 0.0));
    drone.setLocalVelocity(Vector3d(0.0, 10.0, 0.0));
    const EntityState fallen = airEngine.advance(drone, PointMass::sphere(2.0, 0.3, 0.5), 2.0);
    CHECK(fallen.time == 7000 && fallen.geoPosition().height < 300.0 - 15.0 &&
          fallen.geoPosition().height > 300.0 - 20.0);
    CHECK(fallen.localVelocity().z < -15.0 && near(fallen.localVelocity().y, 10.0, 0.5)); // it rides the 10 m/s wind
    CHECK(fallen.acceleration.length() > 5.0 && fallen.orientation == drone.orientation);
    CHECK(near(airEngine.heightAboveTerrain(drone.position), 200.0, 1e-6));

    // Hit tests. A truck heading north at the origin, 5 by 2.4 by 2 m.
    EntityState truck;
    truck.setGeoPosition(site);
    truck.setHeadingPitchRoll(HeadingPitchRoll(0.0, 0.0, 0.0));
    const BoundingBox box = BoundingBox::forKind(EntityKind::GroundVehicle);
    const Vector3d bodyPoint = PhysicsEngine::toBodyFrame(truck, frame.toEcef(Vector3d(1.0, 2.0, 0.5)));
    CHECK(near(bodyPoint, Vector3d(2.0, 1.0, -0.5), 1e-6)); // forward 2 (north), right 1 (east), down -0.5
    CHECK(near(PhysicsEngine::fromBodyFrame(truck, bodyPoint), frame.toEcef(Vector3d(1.0, 2.0, 0.5)), 1e-6));
    double fraction = 0.0;
    Vector3d hit;
    // From the west at 1 m up, through the cab.
    CHECK(PhysicsEngine::segmentHitsEntity(frame.toEcef(Vector3d(-10.0, 0.0, 1.0)),
                                           frame.toEcef(Vector3d(10.0, 0.0, 1.0)), truck, box, &fraction, &hit));
    CHECK(near(fraction, (10.0 - 1.2) / 20.0, 1e-6) && near(frame.toLocal(hit), Vector3d(-1.2, 0.0, 1.0), 1e-6));
    // Over the roof, and beside it.
    CHECK(!PhysicsEngine::segmentHitsEntity(frame.toEcef(Vector3d(-10.0, 0.0, 2.5)),
                                            frame.toEcef(Vector3d(10.0, 0.0, 2.5)), truck, box));
    CHECK(!PhysicsEngine::segmentHitsEntity(frame.toEcef(Vector3d(-10.0, 3.0, 1.0)),
                                            frame.toEcef(Vector3d(10.0, 3.0, 1.0)), truck, box));
    // Turned east, the long side faces north: a shot from the north at x=2 hits, where before it missed.
    truck.setHeadingPitchRoll(HeadingPitchRoll(90.0, 0.0, 0.0));
    CHECK(PhysicsEngine::segmentHitsEntity(frame.toEcef(Vector3d(2.0, 10.0, 1.0)),
                                           frame.toEcef(Vector3d(2.0, -10.0, 1.0)), truck, box));
    CHECK(!PhysicsEngine::segmentHitsEntity(frame.toEcef(Vector3d(2.0, 10.0, 1.0)),
                                            frame.toEcef(Vector3d(2.0, 1.5, 1.0)), truck, box));
    CHECK(PhysicsEngine::segmentHitsSphere(Vector3d(-5.0, 0.0, 0.0), Vector3d(5.0, 0.0, 0.0), Vector3d(), 1.0,
                                           &fraction));
    CHECK(near(fraction, 0.4, 1e-12));
    CHECK(!PhysicsEngine::segmentHitsSphere(Vector3d(-5.0, 2.0, 0.0), Vector3d(5.0, 2.0, 0.0), Vector3d(), 1.0));
    CHECK(PhysicsEngine::segmentHitsSphere(Vector3d(0.5, 0.0, 0.0), Vector3d(5.0, 0.0, 0.0), Vector3d(), 1.0,
                                           &fraction) &&
          fraction == 0.0);
    CHECK(near(BoundingBox::forKind(EntityKind::Drone).radius(), std::sqrt(0.25 + 0.25 + 0.04), 1e-12));

    // A firing solution against a drone crossing at 20 m/s, 600 m out and 150 m up, in wind.
    EntityState target;
    target.time = 0;
    target.position = frame.toEcef(Vector3d(0.0, 600.0, 150.0));
    target.setHeadingPitchRoll(HeadingPitchRoll(90.0, 0.0, 0.0));
    target.setLocalVelocity(Vector3d(20.0, 0.0, 0.0));
    target.deadReckoning = DeadReckoningAlgorithm::Fpw;
    PointMass round = PointMass::sphere(0.4, 0.04, 0.3);
    const FiringSolution solution = airEngine.aim(launcher, 400.0, round, target);
    CHECK(solution.valid && solution.miss <= 0.25 && solution.iterations < 24);
    CHECK(solution.timeOfFlight > 1.5 && solution.timeOfFlight < 2.5);
    CHECK(solution.azimuth > 1.0 && solution.azimuth < 6.0);       // lead to the east
    CHECK(solution.elevation > 14.0 && solution.elevation < 30.0); // above the line of sight, 14 degrees, for the drop
    CHECK(near(solution.direction.length(), 1.0, 1e-9));
    // Flying the solution meets the target where it says.
    round.position = launcher;
    round.velocity = solution.direction * 400.0;
    round.time = 0.0;
    airEngine.advance(round, solution.timeOfFlight);
    const Vector3d expected =
        DeadReckoning::extrapolate(target, static_cast<qint64>(solution.timeOfFlight * 1000.0)).position;
    CHECK(near(round.position, expected, 0.5) && near(solution.interceptPoint, expected, 0.5));
    // A target out of range gives no valid solution but the best attempt.
    EntityState far = target;
    far.position = frame.toEcef(Vector3d(0.0, 30000.0, 150.0));
    const FiringSolution none = airEngine.aim(launcher, 400.0, round, far, 60.0);
    CHECK(!none.valid && none.miss > 100.0 && none.iterations > 0);
    CHECK(!engine.aim(launcher, 0.0, round, target).valid);
}

void testKinetic() {
    // Appearance bits.
    DisAppearance look;
    CHECK(look.bits() == 0 && look.damage() == DamageState::None && !look.isDestroyed());
    look = look.withDamage(DamageState::Moderate).withSmoke(DisAppearance::Smoke::Emitting).withFlaming(true);
    CHECK(look.bits() == ((2u << 3) | (2u << 5) | (1u << 15)));
    CHECK(look.damage() == DamageState::Moderate && look.smoke() == DisAppearance::Smoke::Emitting && look.isFlaming());
    look = look.withFlaming(false).withDeactivated(true).withFrozen(true);
    CHECK(!look.isFlaming() && look.isDeactivated() && look.isFrozen() && (look.bits() & (1u << 23)));
    const DisAppearance wreck = DisAppearance(1u << 12).destroyed(); // keeps the headlights bit
    CHECK(wreck.isDestroyed() && wreck.isFlaming() && wreck.isMobilityKilled() && wreck.isFirepowerKilled());
    CHECK(wreck.smoke() == DisAppearance::Smoke::Both && (wreck.bits() & (1u << 12)));
    CHECK(wreck.withDamage(DamageState::None).damage() == DamageState::None && wreck != look);
    CHECK(DisAppearance::damageName(DamageState::Destroyed) == QStringLiteral("destroyed"));
    CHECK(DisAppearance::damageName(DamageState::Slight) == QStringLiteral("slight damage"));

    // The damage model.
    DamageModel model;
    CHECK(model.hitPoints(EntityKind::Drone) == 1 && model.hitPoints(EntityKind::GroundVehicle) == 3);
    CHECK(model.hitPoints(EntityKind::Boat) == 4 && model.hitPoints(EntityKind::Aircraft) == 2);
    int health = model.hitPoints(EntityKind::Boat);
    CHECK(model.apply(EntityKind::Boat, &health, 1) == DamageState::Slight && health == 3);
    CHECK(model.apply(EntityKind::Boat, &health, 1) == DamageState::Slight && health == 2);
    CHECK(model.apply(EntityKind::Boat, &health, 1) == DamageState::Moderate && health == 1);
    CHECK(model.apply(EntityKind::Boat, &health, 5) == DamageState::Destroyed && health == 0);
    CHECK(model.apply(EntityKind::Boat, &health, 1) == DamageState::Destroyed && health == 0);
    CHECK(model.stateFor(EntityKind::Drone, 1) == DamageState::None &&
          model.stateFor(EntityKind::Drone, 0) == DamageState::Destroyed);
    model.setHitPoints(EntityKind::Drone, 0);
    CHECK(model.hitPoints(EntityKind::Drone) == 1);
    DisDetonationPdu det;
    det.targetEntity = DisEntityId(1, 2, 3);
    det.result = DisDetonationResult::EntityImpact;
    CHECK(det.hitsTarget() && model.damageOf(det) == 1);
    det.descriptor.quantity = 3;
    CHECK(model.damageOf(det) == 3);
    det.result = DisDetonationResult::GroundImpact;
    CHECK(!det.hitsTarget() && model.damageOf(det) == 0);
    det.result = DisDetonationResult::AirHit;
    det.targetEntity = DisEntityId();
    CHECK(!det.hitsTarget());

    // Fire and Detonation PDUs on the wire.
    DisFirePdu fire;
    fire.exercise = 9;
    fire.timestamp = DisEntityStatePdu::timestampFromMsec(123456);
    fire.firingEntity = DisEntityId(1, 2, 3);
    fire.targetEntity = DisEntityId(1, 4, 5);
    fire.event = DisEventId(1, 2, 77);
    fire.fireMissionIndex = 6;
    fire.location = Vector3d(4004609.1876, -127682.9614, 4946170.2360);
    fire.descriptor.munition = DisEntityType(2, 1, 225, 2, 1);
    fire.descriptor.warhead = DisWarhead::HighExplosiveFragmentation;
    fire.descriptor.fuse = DisFuse::Proximity;
    fire.descriptor.quantity = 2;
    fire.descriptor.rate = 600;
    fire.velocity = Vector3d(100.0, -200.0, 300.0);
    fire.range = 1234.5;
    const QByteArray fireBytes = fire.encode();
    CHECK(fireBytes.size() == 96 && fireBytes[0] == 7 && fireBytes[1] == 9 && fireBytes[2] == 2 && fireBytes[3] == 2);
    CHECK(be16(fireBytes, 8) == 96 && be16(fireBytes, 12) == 1 && be16(fireBytes, 16) == 3);
    CHECK(be16(fireBytes, 18) == 1 && be16(fireBytes, 20) == 4 && be16(fireBytes, 22) == 5); // target
    CHECK(be16(fireBytes, 34) == 77);                                                        // event number
    CHECK(be64double(fireBytes, 40) == fire.location.x);
    CHECK(fireBytes[64] == 2 && fireBytes[65] == 1 && be16(fireBytes, 66) == 225);
    CHECK(be16(fireBytes, 72) == 1300 && be16(fireBytes, 74) == 300 && be16(fireBytes, 76) == 2 &&
          be16(fireBytes, 78) == 600);
    DisFirePdu fireBack;
    CHECK(fireBack.decode(fireBytes));
    CHECK(fireBack.exercise == 9 && fireBack.timestamp == fire.timestamp && fireBack.firingEntity == fire.firingEntity);
    CHECK(fireBack.targetEntity == fire.targetEntity && fireBack.event == fire.event && fireBack.fireMissionIndex == 6);
    CHECK(fireBack.location == fire.location && fireBack.descriptor == fire.descriptor);
    CHECK(near(fireBack.velocity, fire.velocity, 1e-4) && near(fireBack.range, 1234.5, 1e-3));
    CHECK(!fireBack.decode(fireBytes.left(95)));
    QByteArray wrong = fireBytes;
    wrong[2] = 3;
    CHECK(!fireBack.decode(wrong) && fireBack.event == fire.event);

    det.exercise = 9;
    det.timestamp = fire.timestamp;
    det.firingEntity = fire.firingEntity;
    det.targetEntity = fire.targetEntity;
    det.event = fire.event;
    det.velocity = Vector3d(90.0, -180.0, 250.0);
    det.location = fire.location + Vector3d(10.0, 20.0, 30.0);
    det.descriptor = fire.descriptor;
    det.locationInEntity = Vector3d(1.5, -0.25, -0.75);
    det.result = DisDetonationResult::EntityImpact;
    const QByteArray detBytes = det.encode();
    CHECK(detBytes.size() == 104 && detBytes[2] == 3 && detBytes[3] == 2 && be16(detBytes, 8) == 104);
    CHECK(be16(detBytes, 34) == 77 && be64double(detBytes, 48) == det.location.x);
    CHECK(detBytes[100] == 1 && detBytes[101] == 0);
    DisDetonationPdu detBack;
    CHECK(detBack.decode(detBytes));
    CHECK(detBack.event == det.event && detBack.targetEntity == det.targetEntity && detBack.location == det.location);
    CHECK(detBack.descriptor == det.descriptor && detBack.result == DisDetonationResult::EntityImpact);
    CHECK(near(detBack.locationInEntity, det.locationInEntity, 1e-6) && near(detBack.velocity, det.velocity, 1e-4));
    CHECK(detBack.hitsTarget());
    wrong = detBytes;
    wrong[101] = 5; // more records than the length holds
    CHECK(!detBack.decode(wrong));

    // Headers and bundles.
    DisPduHeader h;
    CHECK(DisPduHeader::peek(detBytes, 0, &h) && h.type == DisPduType::Detonation && h.family == 2 && h.length == 104);
    CHECK(h.exercise == 9 && h.timestamp == det.timestamp && h.version == 7);
    CHECK(!DisPduHeader::peek(detBytes, 100, &h));
    DisEntityStatePdu es;
    es.exercise = 9;
    const QByteArray bundle = fireBytes + es.encode() + detBytes + QByteArray("junk");
    const QList<qsizetype> offsets = DisPduHeader::split(bundle);
    CHECK(offsets == QList<qsizetype>({0, 96, 96 + 144}));
    CHECK(DisPduHeader::peek(bundle, 96, &h) && h.type == DisPduType::EntityState);
    CHECK(DisEntityStatePdu::decodeAll(bundle).size() == 1);
    CHECK(detBack.decode(bundle, 240) && detBack.event == det.event);
    QByteArray other = detBytes;
    other[2] = 55;
    CHECK(DisPduHeader::peek(other, 0, &h) && h.type == DisPduType::Other);
    CHECK(DisEventId(1, 2, 77).toString() == QStringLiteral("1:2:77") && !DisEventId().isValid());
    CHECK(DisEventId(1, 2, 77) != DisEventId(1, 2, 78) && DisEventId(1, 2, 77).key() == DisEntityId(1, 2, 77).key());

    // Destruction effects repeat from the seed.
    EntityState truck;
    truck.setGeoPosition(GeoPosition(51.1789, -1.8262, 100.0));
    truck.setHeadingPitchRoll(HeadingPitchRoll(45.0, 0.0, 0.0));
    const BoundingBox box = BoundingBox::forKind(EntityKind::GroundVehicle);
    DestructionEffect boom = DestructionEffect::forEntity(truck, EntityKind::GroundVehicle, box, 77);
    const DestructionEffect again = DestructionEffect::forEntity(truck, EntityKind::GroundVehicle, box, 77);
    const DestructionEffect other2 = DestructionEffect::forEntity(truck, EntityKind::GroundVehicle, box, 78);
    CHECK(boom.debris.size() == 24 && again.debris.size() == 24);
    CHECK(boom.debris.first().position == again.debris.first().position &&
          boom.debris.first().velocity == again.debris.first().velocity);
    CHECK(boom.debris.first().position != other2.debris.first().position);
    CHECK(near(boom.origin, PhysicsEngine::fromBodyFrame(truck, box.centre), 1e-9) && boom.fireballRadius > 3.0);
    CHECK(boom.smokeDuration > 60.0 &&
          DestructionEffect::forEntity(truck, EntityKind::Drone, box, 1).smokeDuration < 10.0);
    const Vector3d up = Ellipsoid::wgs84().surfaceNormal(51.1789, -1.8262);
    int upward = 0;
    for (const Debris &d : boom.debris) {
        CHECK(near(PhysicsEngine::toBodyFrame(truck, d.position).x, 0.0, 2.5 + 1e-6));
        CHECK(d.velocity.length() > 1.0 && d.size > 0.05 && d.lifetime >= 2.0);
        if (d.velocity.dotProduct(up) > 0.0)
            ++upward;
    }
    CHECK(upward == 24);
    PhysicsEngine physics;
    physics.setTerrain([](const GeoPosition &) { return 100.0; });
    const Vector3d before = boom.debris.first().position;
    boom.advance(physics, 0.1);
    CHECK(boom.debris.size() == 24 && boom.debris.first().position != before);
    for (int i = 0; i < 70; ++i)
        boom.advance(physics, 0.1);
    CHECK(boom.debris.isEmpty()); // every lifetime is under 6 s, and nothing ends up below the ground
    boom = DestructionEffect::forEntity(truck, EntityKind::GroundVehicle, box, 77);
    for (int i = 0; i < 30; ++i)
        boom.advance(physics, 0.1);
    for (const Debris &d : boom.debris)
        CHECK(physics.heightAboveTerrain(d.position) > -0.5);

    // Elimination reports.
    Elimination e;
    CHECK(!e.isValid());
    e.victim = DisEntityId(1, 2, 1);
    e.victimMarking = QStringLiteral("UAV-3");
    e.victimOwnerName = QStringLiteral("bob");
    CHECK(e.toString() == QStringLiteral("UAV-3 (bob) was destroyed") && !e.isSelfInflicted());
    e.killer = DisEntityId(1, 1, 1);
    e.killerMarking = QStringLiteral("TURRET");
    e.killerOwnerName = QStringLiteral("alice");
    CHECK(e.toString() == QStringLiteral("UAV-3 (bob) was eliminated by TURRET (alice)"));
    e.killer = DisEntityId(1, 2, 2);
    CHECK(e.isSelfInflicted());
    Elimination bare;
    bare.victim = DisEntityId(1, 2, 1);
    CHECK(bare.toString() == QStringLiteral("1:2:1 was destroyed"));

    // The catalogue knows the wreck.
    ModelCatalogue catalogue;
    CHECK(catalogue.loadFromJson(QByteArray(R"({"models":[{"id":"land/lr110","revision":1,"kind":"ground-vehicle",
        "file":"lr110.glb","wreck":"lr110-wreck.glb"},{"id":"air/uav","revision":1,"kind":"drone","file":"uav.glb"}]})"),
                                 QStringLiteral("/opt/models")));
    CHECK(catalogue.find(QStringLiteral("land/lr110")).wreck == QStringLiteral("lr110-wreck.glb"));
    CHECK(catalogue.wreckFilePath(QStringLiteral("land/lr110")) == QStringLiteral("/opt/models/lr110-wreck.glb"));
    CHECK(catalogue.wreckFilePath(QStringLiteral("air/uav")).isEmpty() &&
          catalogue.wreckFilePath(QStringLiteral("x")).isEmpty());
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testVector();
    testEllipsoid();
    testGeoPosition();
    testLocalFrame();
    testUtmZone();
    testTransverseMercator();
    testClock();
    testReceiver();
    testNmeaRoundTrip();
    testRegisteredReceiver();
    testEntityKinds();
    testCatalogue();
    testOrientation();
    testDeadReckoning();
    testDisPdu();
    testAtmosphere();
    testWind();
    testGravity();
    testPhysics();
    testKinetic();
    if (g_failures == 0)
        std::printf("All Nt Twin tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
