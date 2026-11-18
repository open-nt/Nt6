// MIT License - see LICENSE
//
// Overview page about Nt Positioning for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-positioning.html
    \title Using Nt Positioning
    \brief How to work with geographic coordinates and areas, and read positions from a GNSS receiver.

    Nt Positioning is the core of Qt Positioning. It depends only on Nt Core, so it also runs
    headless:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(tracker main.cpp)
    target_link_libraries(tracker PRIVATE Nt::Positioning)
    \endcode

    \section1 Coordinates and Areas

    QGeoCoordinate holds a WGS84 latitude and longitude and an optional altitude. It measures
    great-circle distances and bearings and moves along them:

    \code
    const QGeoCoordinate gate(51.1789, -1.8262);
    const QGeoCoordinate vehicle = gate.atDistanceAndAzimuth(250.0, 45.0);
    qDebug() << gate.distanceTo(vehicle) << gate.azimuthTo(vehicle);   // 250, 45
    \endcode

    QGeoRectangle and QGeoCircle describe areas, such as a site boundary or an exclusion zone, and
    test whether a coordinate lies inside. Rectangles may cross the 180th meridian. Both can be
    stored as a QGeoShape.

    \section1 Reading a GNSS Receiver

    QNmeaPositionInfoSource reads the NMEA 0183 sentences that almost every receiver sends, from
    any QIODevice: a QSerialPort for a receiver on a serial line or USB, a QTcpSocket or QUdpSocket
    for one on the network. The sentences of each fix are merged into one QGeoPositionInfo with the
    coordinate, the UTC time, the speed and course, and, when the receiver's user equivalent range
    error is set, the horizontal and vertical accuracy.

    \code
    auto *source = new QNmeaPositionInfoSource(QNmeaPositionInfoSource::RealTimeMode, this);
    source->setDevice(serialPort);
    connect(source, &QGeoPositionInfoSource::positionUpdated, this, [](const QGeoPositionInfo &fix) {
        qDebug() << fix.coordinate() << fix.attribute(QGeoPositionInfo::GroundSpeed) << "m/s";
    });
    connect(source, &QGeoPositionInfoSource::errorOccurred, this, [](QGeoPositionInfoSource::Error error) {
        qWarning() << "GNSS error" << error;
    });
    source->startUpdates();
    \endcode

    \section1 Satellites in View and in Use

    QNmeaSatelliteInfoSource reads the GSV and GSA sentences of the same receiver: which
    satellites of GPS, GLONASS, Galileo, BeiDou and QZSS it can see, at what elevation, azimuth
    and signal strength, and which it uses for its fix. Each source reads its device to the end,
    so give it its own copy of the data, for example a second socket or a QBuffer that a reader
    fills for both.

    \code
    auto *satellites = new QNmeaSatelliteInfoSource(QNmeaSatelliteInfoSource::RealTimeMode, this);
    satellites->setDevice(satelliteFeed);
    connect(satellites, &QGeoSatelliteInfoSource::satellitesInUseUpdated, this,
            [](const QList<QGeoSatelliteInfo> &used) { qDebug() << used.size() << "in use"; });
    satellites->startUpdates();
    \endcode

    \section1 Replaying a Recording

    In SimulationMode the source reads a recorded log, such as a QFile, and replays its fixes with
    the time gaps between them. Code written against QGeoPositionInfoSource then runs on a recorded
    drive exactly as it would on the road.

    \code
    QFile log(QStringLiteral("drive.nmea"));
    log.open(QIODevice::ReadOnly);
    QNmeaPositionInfoSource replay(QNmeaPositionInfoSource::SimulationMode);
    replay.setDevice(&log);
    replay.startUpdates();
    \endcode

    \section1 Named Sources

    QGeoPositionInfoSource::createDefaultSource() and createSource() create a source by name.
    Qt loads them from plugins; in Nt a library registers a factory with
    QGeoPositionInfoSource::registerSource(), so a statically linked application needs no plugin
    directory.

    \sa {Nt Positioning C++ Classes}, {Using Nt Twin}
*/
