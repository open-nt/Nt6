// MIT License - see LICENSE
//
// Overview page about Nt Twin for the NDoc documentation. This file holds only a documentation
// comment and is not part of the library.

/*!
    \page nt-twin.html
    \title Using Nt Twin
    \brief How to place things on the Earth in double precision: geodetic positions, ECEF, local frames and UTM.

    Nt Twin is the world model of a digital twin: a scene that mirrors the state of a real site,
    vehicle or area of terrain. Qt has no such module. Nt Twin depends only on Nt Core and
    Nt Positioning, so the same code runs in a viewer, on a headless simulation server and in tests:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(twin main.cpp)
    target_link_libraries(twin PRIVATE Nt::Twin)
    \endcode

    This version provides the geodesy that everything else in a twin rests on, the simulation
    clock and a simulated GNSS receiver. Entities, property bindings and the historian will
    follow.

    \section1 Positions on the Earth

    A GNSS receiver reports latitude, longitude and height above the WGS84 ellipsoid;
    QtTwin::GeoPosition holds those three values. For arithmetic, convert them to Earth-centred,
    Earth-fixed (ECEF) coordinates, a Cartesian frame in metres with its origin at the centre of
    the Earth:

    \code
    const QtTwin::Ellipsoid wgs84 = QtTwin::Ellipsoid::wgs84();
    const QtTwin::Vector3d a = wgs84.toEcef(QtTwin::GeoPosition(51.1789, -1.8262, 160.0));
    const QtTwin::Vector3d b = wgs84.toEcef(QtTwin::GeoPosition(51.1800, -1.8200, 150.0));
    const double distance = (b - a).length();    // straight-line distance in metres
    \endcode

    Keep such coordinates in QtTwin::Vector3d, which stores \c double. A \c float, as in
    QVector3D, resolves only about half a metre at the radius of the Earth.

    \section1 Local Frames

    Placing objects, running physics and rendering are easier in a flat frame near the point of
    interest. QtTwin::LocalFrame is the east-north-up frame tangent to the ellipsoid at an origin:
    x east, y north, z up, in metres.

    \code
    const QtTwin::LocalFrame site(QtTwin::GeoPosition(51.1789, -1.8262, 100.0));
    const QtTwin::Vector3d local = site.toLocal(b);      // metres east, north and up of the site
    const QtTwin::Vector3d ecef = site.toEcef(local);    // and back
    \endcode

    The Earth curves away below a tangent plane by about d²/2R: 8 cm at 1 km, 70 m at 30 km.
    A frame is therefore exact for one point and approximate for its neighbourhood, and a large
    world uses one frame per tile. toEcefMatrix() returns the frame as the 4x4 matrix of a
    3D Tiles \c transform, so a tile's content can be written in its own local frame.

    \section1 UTM Grids

    Maps, terrain generators and military users work on Universal Transverse Mercator grids.
    QtTwin::UtmZone finds the zone of a position, including the exceptions for Norway and
    Svalbard, and QtTwin::TransverseMercator projects to and from its grid:

    \code
    const QtTwin::UtmZone zone = QtTwin::UtmZone::containing(51.1789, -1.8262);   // 30N, EPSG:32630
    const QtTwin::TransverseMercator utm = zone.projection();
    const QtTwin::GridPosition grid = utm.forward(51.1789, -1.8262);   // easting 582046.707, northing 5670374.378
    const QtTwin::GeoPosition back = utm.inverse(grid);
    \endcode

    The projection uses Krüger's series to sixth order and agrees with PROJ to well below a
    millimetre, also outside the zone's 6 degrees.

    \section1 Heights

    QtTwin::GeoPosition::height is always ellipsoidal. Elevation models and maps give heights
    above mean sea level, which differ from ellipsoidal heights by the geoid undulation, up to
    about 100 metres. A twin that mixes the two must convert with a geoid model.

    \section1 Simulation Time

    QtTwin::SimulationClock is the time of the twin, separate from the system clock: it runs at
    any rate, pauses, and steps with advance() for a deterministic headless simulation. Anything
    in the twin that depends on time reads it.

    \section1 Simulated GNSS

    QtTwin::GnssReceiver is a simulated receiver for a twin entity. It follows a trajectory, adds
    the errors of a QtTwin::GnssErrorModel (a slowly drifting bias and per-fix noise, from a seeded
    generator so runs repeat), loses its fix in denied areas such as the range of a jammer, and is
    a QGeoPositionInfoSource, so navigation code written for Qt Positioning runs on it unchanged:

    \code
    QtTwin::SimulationClock clock;
    QtTwin::GnssReceiver gnss;
    gnss.setClock(&clock);
    gnss.setTrajectory([&](const QDateTime &t) { return vehicle.positionAt(t); });
    gnss.setNmeaOutput(&serialPort);                  // and as NMEA 0183, for real vehicle software
    connect(&gnss, &QGeoPositionInfoSource::positionUpdated, &navigator, &Navigator::onFix);
    gnss.startUpdates();
    for (int step = 0; step < 600; ++step)
        clock.advance(100);                           // one minute in 100 ms steps
    \endcode

    QtTwin::GnssReceiver::registerSource() makes it available to
    QGeoPositionInfoSource::createSource() as \c nttwin.

    \sa {Nt Twin C++ Classes}, {Using Nt Positioning}
*/
