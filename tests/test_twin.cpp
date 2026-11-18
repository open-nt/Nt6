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
    if (g_failures == 0)
        std::printf("All Nt Twin tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
