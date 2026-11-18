// MIT License - see LICENSE
//
// The simulated GNSS receiver of Nt Twin. The true position comes from a trajectory; the error
// added to it is a first-order Gauss-Markov bias per east, north and up axis (the slowly drifting
// error of satellite geometry, orbits and atmosphere) plus white noise per fix, drawn from a seeded
// generator so that a simulation repeats exactly. Inside a denied area (jamming) there is no fix.
// Fixes are delivered as QGeoPositionInfo and, optionally, written as NMEA 0183.
#include "qtwingnss.h"

#include <QtCore/qiodevice.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qtimer.h>

#include <cmath>
#include <cstdio>

namespace QtTwin {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kKnot = 1852.0 / 3600.0; // metres per second
constexpr int kDefaultRequestTimeout = 10000;

QByteArray hhmmss(const QDateTime &time) {
    const QTime t = time.toUTC().time();
    char text[16];
    std::snprintf(text, sizeof text, "%02d%02d%02d.%02d", t.hour(), t.minute(), t.second(), t.msec() / 10);
    return QByteArray(text);
}

// "ddmm.mmmmm,N" or "dddmm.mmmmm,E".
QByteArray angle(double value, bool latitude) {
    const double magnitude = std::fabs(value);
    int degrees = static_cast<int>(magnitude);
    double minutes = (magnitude - degrees) * 60.0;
    if (minutes >= 59.999995) {
        ++degrees;
        minutes = 0.0;
    }
    char text[32];
    std::snprintf(text, sizeof text, latitude ? "%02d%08.5f,%c" : "%03d%08.5f,%c", degrees, minutes,
                  latitude ? (value < 0.0 ? 'S' : 'N') : (value < 0.0 ? 'W' : 'E'));
    return QByteArray(text);
}

QByteArray number(double value, int decimals) {
    return std::isnan(value) ? QByteArray() : QByteArray::number(value, 'f', decimals);
}

} // namespace

/*!
    \class QtTwin::GnssErrorModel
    \inmodule NtTwin
    \brief The GnssErrorModel struct describes the errors of a simulated GNSS receiver.

    Each fix is the true position plus a bias and noise, in metres along the east, north and up
    axes. The bias drifts slowly as a first-order Gauss-Markov process with the given standard
    deviations and correlation time; the noise is independent for each fix. The defaults are those
    of a good single-frequency receiver under open sky: a horizontal error of about 2.2 m RMS and a
    vertical error of about 3.2 m RMS.

    \differences
    Qt has no GNSS simulation; this struct is part of Nt Twin. Satellite geometry is not modelled:
    the dilutions of precision are fixed values that are only reported.
    \enddifferences

    \sa QtTwin::GnssReceiver
*/

/*!
    \variable QtTwin::GnssErrorModel::horizontalBias
    The standard deviation of the bias along the east and the north axis, in metres.
*/

/*!
    \variable QtTwin::GnssErrorModel::verticalBias
    The standard deviation of the bias along the up axis, in metres.
*/

/*!
    \variable QtTwin::GnssErrorModel::correlationTime
    The correlation time of the bias in seconds: how long it takes to change appreciably.
*/

/*!
    \variable QtTwin::GnssErrorModel::horizontalNoise
    The standard deviation of the noise of each fix along the east and the north axis, in metres.
*/

/*!
    \variable QtTwin::GnssErrorModel::verticalNoise
    The standard deviation of the noise of each fix along the up axis, in metres.
*/

/*!
    \variable QtTwin::GnssErrorModel::hdop
    The horizontal dilution of precision that the receiver reports.
*/

/*!
    \variable QtTwin::GnssErrorModel::vdop
    The vertical dilution of precision that the receiver reports.
*/

/*!
    \variable QtTwin::GnssErrorModel::satellites
    The number of satellites in use that the receiver reports.
*/

/*!
    \variable QtTwin::GnssErrorModel::seed
    The seed of the random numbers; the same seed gives the same errors.
*/

/*!
    Returns a model without errors, for tests and for driving a system with the true position.
*/
GnssErrorModel GnssErrorModel::perfect() {
    GnssErrorModel model;
    model.horizontalBias = 0.0;
    model.verticalBias = 0.0;
    model.horizontalNoise = 0.0;
    model.verticalNoise = 0.0;
    return model;
}

/*!
    \class QtTwin::NmeaWriter
    \inmodule NtTwin
    \brief The NmeaWriter class formats position fixes as NMEA 0183 sentences.

    A simulated receiver writes these to a serial port or socket so that real vehicle software,
    which expects a GNSS receiver, can run against the twin. The sentences use the GP talker and end
    with a checksum and CR LF; positions have five decimals of minutes (about 2 cm).

    \code
    port.write(QtTwin::NmeaWriter::gga(fix, 9, 0.9, 47.9));
    port.write(QtTwin::NmeaWriter::rmc(fix));
    \endcode

    \differences
    Qt has no NMEA writer; this class is part of Nt Twin.
    \enddifferences

    \sa QNmeaPositionInfoSource
*/

/*!
    Returns the sentence with the given \a body (without the \c $ and the checksum): \c $, the body,
    \c *, the checksum in two hexadecimal digits and CR LF.
*/
QByteArray NmeaWriter::sentence(const QByteArray &body) {
    quint8 sum = 0;
    for (char c : body)
        sum ^= static_cast<quint8>(c);
    char checksum[4];
    std::snprintf(checksum, sizeof checksum, "%02X", sum);
    return '$' + body + '*' + QByteArray(checksum) + "\r\n";
}

/*!
    Returns a GGA sentence for \a info: time, position, a GPS fix with \a satellites in use and
    \a hdop, and the altitude of the coordinate, which is above mean sea level, with
    \a geoidSeparation, the height of the geoid above the ellipsoid.
*/
QByteArray NmeaWriter::gga(const QGeoPositionInfo &info, int satellites, double hdop, double geoidSeparation) {
    const QGeoCoordinate c = info.coordinate();
    const double altitude = c.type() == QGeoCoordinate::Coordinate3D ? c.altitude() : qQNaN();
    return sentence("GPGGA," + hhmmss(info.timestamp()) + ',' + angle(c.latitude(), true) + ',' +
                    angle(c.longitude(), false) + ",1," + QByteArray::number(satellites).rightJustified(2, '0') + ',' +
                    number(hdop, 1) + ',' + number(altitude, 2) + ",M," + number(geoidSeparation, 1) + ",M,,");
}

/*!
    Returns an RMC sentence for \a info: time, a valid status, position, ground speed in knots and
    direction from its attributes, and date.
*/
QByteArray NmeaWriter::rmc(const QGeoPositionInfo &info) {
    const QGeoCoordinate c = info.coordinate();
    const double speed = info.attribute(QGeoPositionInfo::GroundSpeed) / kKnot;
    const QDate d = info.timestamp().toUTC().date();
    char date[8];
    std::snprintf(date, sizeof date, "%02d%02d%02d", d.day(), d.month(), d.year() % 100);
    return sentence("GPRMC," + hhmmss(info.timestamp()) + ",A," + angle(c.latitude(), true) + ',' +
                    angle(c.longitude(), false) + ',' + number(speed, 2) + ',' +
                    number(info.attribute(QGeoPositionInfo::Direction), 1) + ',' + QByteArray(date) + ",,,A");
}

/*!
    Returns a VTG sentence with the direction and ground speed of \a info.
*/
QByteArray NmeaWriter::vtg(const QGeoPositionInfo &info) {
    const double speed = info.attribute(QGeoPositionInfo::GroundSpeed);
    return sentence("GPVTG," + number(info.attribute(QGeoPositionInfo::Direction), 1) + ",T,,M," +
                    number(speed / kKnot, 2) + ",N," + number(speed * 3.6, 2) + ",K,A");
}

/*!
    Returns a GSA sentence for a 3D fix with the dilutions of precision \a pdop, \a hdop and \a vdop.
*/
QByteArray NmeaWriter::gsa(double pdop, double hdop, double vdop) {
    return sentence("GPGSA,A,3,,,,,,,,,,,,," + number(pdop, 1) + ',' + number(hdop, 1) + ',' + number(vdop, 1));
}

/*!
    \class QtTwin::GnssReceiver
    \inmodule NtTwin
    \brief The GnssReceiver class is a simulated GNSS receiver: a position source for a twin entity.

    The receiver follows a trajectory, the true position of the entity it is mounted on as a
    function of time, and reports fixes with the errors of its GnssErrorModel. It is a
    QGeoPositionInfoSource, so code written against Qt Positioning runs on simulated positions
    unchanged; with setNmeaOutput() it also writes NMEA 0183, for software that reads a receiver on
    a serial port.

    \code
    QtTwin::SimulationClock clock;
    QtTwin::GnssReceiver receiver;
    receiver.setClock(&clock);
    receiver.setTrajectory([](const QDateTime &t) { return vehicle.positionAt(t); });
    receiver.setDeniedAreas({QGeoCircle(QGeoCoordinate(51.19, -1.82), 800.0)});   // a jammer
    receiver.setUpdateInterval(200);
    connect(&receiver, &QGeoPositionInfoSource::positionUpdated, this, &Navigator::onFix);
    receiver.startUpdates();
    clock.start();
    \endcode

    With a SimulationClock, fixes are taken at every multiple of the update interval of
    simulation time, so a simulation stepped with SimulationClock::advance() is deterministic.
    Without one, a timer takes them in real time at the current UTC time.

    Inside a denied area there is no fix: updates stop, errorOccurred() reports
    UpdateTimeoutError once, and updates resume when the trajectory leaves the area.

    Altitudes are reported above mean sea level, as a receiver reports them: the ellipsoidal
    height of the trajectory minus geoidUndulation().

    registerSource() makes the receiver available to
    QGeoPositionInfoSource::createSource() as \c nttwin.

    \differences
    Qt has no GNSS simulation; this class is part of Nt Twin. Satellite geometry, multipath and
    spoofing are not modelled yet.
    \enddifferences

    \sa QtTwin::GnssErrorModel, QtTwin::SimulationClock, QtTwin::NmeaWriter
*/

/*!
    \typedef QtTwin::GnssReceiver::Trajectory
    A function that returns the true position at a time in UTC.
*/

/*!
    Constructs a receiver with the default error model and the given \a parent. It has no
    trajectory until setTrajectory() or setTruePosition() is called.
*/
GnssReceiver::GnssReceiver(QObject *parent)
    : QGeoPositionInfoSource(parent), m_random(m_model.seed), m_timer(new QTimer(this)),
      m_requestTimer(new QTimer(this)) {
    m_requestTimer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] { produce(QDateTime::currentDateTimeUtc()); });
    connect(m_requestTimer, &QTimer::timeout, this, [this] {
        m_requested = false;
        m_error = UpdateTimeoutError;
        emit errorOccurred(UpdateTimeoutError);
    });
}

/*!
    Destroys the receiver.
*/
GnssReceiver::~GnssReceiver() = default;

/*!
    Sets the true position as a function of time to \a trajectory.
*/
void GnssReceiver::setTrajectory(const Trajectory &trajectory) { m_trajectory = trajectory; }

/*!
    Makes the receiver stand still at \a position.
*/
void GnssReceiver::setTruePosition(const GeoPosition &position) {
    m_trajectory = [position](const QDateTime &) { return position; };
}

/*!
    Takes fixes in the simulation time of \a clock instead of in real time. The receiver does not
    take ownership. Pass \c nullptr to go back to real time.
*/
void GnssReceiver::setClock(SimulationClock *clock) {
    if (m_clock == clock)
        return;
    if (m_clock)
        disconnect(m_clock, nullptr, this, nullptr);
    m_clock = clock;
    m_nextSample = QDateTime();
    if (clock)
        connect(clock, &SimulationClock::timeChanged, this, &GnssReceiver::onClockTime);
    if (m_running) {
        m_timer->stop();
        if (!clock)
            m_timer->start(interval());
        else
            onClockTime(clock->time());
    }
}

/*!
    Returns the clock set with setClock(), or \c nullptr.
*/
SimulationClock *GnssReceiver::clock() const { return m_clock; }

/*!
    Sets the error model to \a model, and restarts its random numbers from the model's seed.
*/
void GnssReceiver::setErrorModel(const GnssErrorModel &model) {
    m_model = model;
    m_random.seed(model.seed);
    m_hasSpare = false;
    m_bias = {};
    m_lastSample = QDateTime();
}

/*!
    \fn QtTwin::GnssErrorModel QtTwin::GnssReceiver::errorModel() const
    Returns the error model.
*/

/*!
    Sets the areas in which the receiver has no fix, such as the range of a jammer, to \a areas.
*/
void GnssReceiver::setDeniedAreas(const QList<QGeoShape> &areas) { m_denied = areas; }

/*!
    \fn QList<QGeoShape> QtTwin::GnssReceiver::deniedAreas() const
    Returns the areas in which the receiver has no fix.
*/

/*!
    \fn void QtTwin::GnssReceiver::setGeoidUndulation(double metres)
    Sets the height of the geoid above the ellipsoid at the receiver to \a metres, which is
    subtracted from the ellipsoidal height to report the altitude above mean sea level. The
    default is 0.
*/

/*!
    \fn double QtTwin::GnssReceiver::geoidUndulation() const
    Returns the height of the geoid above the ellipsoid in metres.
*/

/*!
    Writes every fix as GGA, GSA, RMC and VTG sentences to \a device, which must be open for
    writing. The receiver does not take ownership. Pass \c nullptr to stop.
*/
void GnssReceiver::setNmeaOutput(QIODevice *device) { m_nmea = device; }

/*!
    Returns the device set with setNmeaOutput(), or \c nullptr.
*/
QIODevice *GnssReceiver::nmeaOutput() const { return m_nmea; }

/*!
    Returns the fix the receiver would report at \a time, advancing its random numbers and bias as
    a real fix does, or an invalid position if \a time is inside a denied area or there is no
    trajectory. Nothing is emitted.
*/
QGeoPositionInfo GnssReceiver::sample(const QDateTime &time) {
    if (!m_trajectory)
        return QGeoPositionInfo();
    const GeoPosition truth = m_trajectory(time);
    for (const QGeoShape &area : std::as_const(m_denied)) {
        if (area.contains(QGeoCoordinate(truth.latitude, truth.longitude)))
            return QGeoPositionInfo();
    }

    // The bias: b = b * phi + sigma * sqrt(1 - phi^2) * w, started from its stationary distribution.
    const double sigmas[3] = {m_model.horizontalBias, m_model.horizontalBias, m_model.verticalBias};
    if (!m_lastSample.isValid()) {
        for (std::size_t i = 0; i < 3; ++i)
            m_bias[i] = sigmas[i] * gaussian();
    } else {
        const double dt = static_cast<double>(m_lastSample.msecsTo(time)) / 1000.0;
        const double phi = m_model.correlationTime > 0.0 ? std::exp(-std::fabs(dt) / m_model.correlationTime) : 0.0;
        for (std::size_t i = 0; i < 3; ++i)
            m_bias[i] = m_bias[i] * phi + sigmas[i] * std::sqrt(1.0 - phi * phi) * gaussian();
    }
    m_lastSample = time;
    const Vector3d error(m_bias[0] + m_model.horizontalNoise * gaussian(),
                         m_bias[1] + m_model.horizontalNoise * gaussian(),
                         m_bias[2] + m_model.verticalNoise * gaussian());

    const Ellipsoid wgs84 = Ellipsoid::wgs84();
    const LocalFrame frame(truth);
    const GeoPosition measured = wgs84.fromEcef(frame.toEcef(error));

    QGeoPositionInfo info(QGeoCoordinate(measured.latitude, measured.longitude, measured.height - m_undulation),
                          time.toUTC());
    // Velocity from the trajectory over the last second.
    const Vector3d before = frame.toLocal(wgs84.toEcef(m_trajectory(time.addMSecs(-1000))));
    const double ve = -before.x;
    const double vn = -before.y;
    const double speed = std::hypot(ve, vn);
    info.setAttribute(QGeoPositionInfo::GroundSpeed, speed);
    info.setAttribute(QGeoPositionInfo::VerticalSpeed, -before.z);
    if (speed > 0.05) {
        double direction = std::atan2(ve, vn) * 180.0 / kPi;
        if (direction < 0.0)
            direction += 360.0;
        info.setAttribute(QGeoPositionInfo::Direction, direction);
    }
    const double horizontal =
        std::sqrt(m_model.horizontalBias * m_model.horizontalBias + m_model.horizontalNoise * m_model.horizontalNoise);
    info.setAttribute(QGeoPositionInfo::HorizontalAccuracy, std::sqrt(2.0) * horizontal);
    info.setAttribute(QGeoPositionInfo::VerticalAccuracy, std::sqrt(m_model.verticalBias * m_model.verticalBias +
                                                                    m_model.verticalNoise * m_model.verticalNoise));
    return info;
}

/*!
    \reimp

    The default interval, 0, means one fix per second.
*/
void GnssReceiver::setUpdateInterval(int msec) {
    QGeoPositionInfoSource::setUpdateInterval(msec > 0 ? std::max(msec, minimumUpdateInterval()) : 0);
    if (m_running && !m_clock)
        m_timer->start(interval());
}

/*!
    \reimp
*/
QGeoPositionInfo GnssReceiver::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const {
    Q_UNUSED(fromSatellitePositioningMethodsOnly);
    return m_lastKnown;
}

/*!
    \reimp
*/
QGeoPositionInfoSource::PositioningMethods GnssReceiver::supportedPositioningMethods() const {
    return SatellitePositioningMethods;
}

/*!
    \reimp

    The receiver supports up to 100 fixes per second.
*/
int GnssReceiver::minimumUpdateInterval() const { return 10; }

/*!
    \fn QGeoPositionInfoSource::Error QtTwin::GnssReceiver::error() const
    \reimp
*/

/*!
    Registers the receiver with QGeoPositionInfoSource as the source \c nttwin, with priority -1 so
    that a real source is preferred by QGeoPositionInfoSource::createDefaultSource(). The parameters
    \c latitude, \c longitude and \c height (ellipsoidal, in metres) place the receiver; \c seed
    sets the seed of its error model.
*/
void GnssReceiver::registerSource() {
    QGeoPositionInfoSource::registerSource(
        QStringLiteral("nttwin"),
        [](const QVariantMap &parameters, QObject *parent) -> QGeoPositionInfoSource * {
            auto *receiver = new GnssReceiver(parent);
            receiver->setTruePosition(GeoPosition(parameters.value(QStringLiteral("latitude")).toDouble(),
                                                  parameters.value(QStringLiteral("longitude")).toDouble(),
                                                  parameters.value(QStringLiteral("height")).toDouble()));
            if (parameters.contains(QStringLiteral("seed"))) {
                GnssErrorModel model;
                model.seed = parameters.value(QStringLiteral("seed")).toUInt();
                receiver->setErrorModel(model);
            }
            return receiver;
        },
        -1);
}

/*!
    \reimp
*/
void GnssReceiver::startUpdates() {
    if (m_running)
        return;
    if (!m_trajectory) {
        m_error = AccessError;
        emit errorOccurred(AccessError);
        return;
    }
    m_running = true;
    m_error = NoError;
    if (m_clock) {
        onClockTime(m_clock->time());
    } else {
        produce(QDateTime::currentDateTimeUtc());
        m_timer->start(interval());
    }
}

/*!
    \reimp
*/
void GnssReceiver::stopUpdates() {
    m_running = false;
    m_timer->stop();
}

/*!
    \reimp
*/
void GnssReceiver::requestUpdate(int timeout) {
    if (!m_trajectory) {
        m_error = AccessError;
        emit errorOccurred(AccessError);
        return;
    }
    if (m_requested)
        return;
    m_requested = true;
    m_error = NoError;
    m_requestTimer->start(timeout > 0 ? timeout : kDefaultRequestTimeout);
    if (m_clock) {
        m_nextSample = m_clock->time();
        onClockTime(m_clock->time());
    } else {
        produce(QDateTime::currentDateTimeUtc());
    }
}

void GnssReceiver::onClockTime(const QDateTime &time) {
    if (!m_running && !m_requested)
        return;
    if (!m_nextSample.isValid() || time < m_lastSample)
        m_nextSample = time;
    while ((m_running || m_requested) && m_nextSample <= time) {
        produce(m_nextSample);
        m_nextSample = m_nextSample.addMSecs(interval());
    }
}

void GnssReceiver::produce(const QDateTime &time) {
    const QGeoPositionInfo info = sample(time);
    if (!info.isValid()) {
        if (m_trajectory && !m_denying) {
            m_denying = true;
            m_error = UpdateTimeoutError;
            emit errorOccurred(UpdateTimeoutError);
        }
        return;
    }
    m_denying = false;
    m_lastKnown = info;
    if (m_nmea && m_nmea->isWritable()) {
        const double pdop = std::hypot(m_model.hdop, m_model.vdop);
        m_nmea->write(NmeaWriter::gga(info, m_model.satellites, m_model.hdop, m_undulation) +
                      NmeaWriter::gsa(pdop, m_model.hdop, m_model.vdop) + NmeaWriter::rmc(info) +
                      NmeaWriter::vtg(info));
    }
    if (m_requested) {
        m_requested = false;
        m_requestTimer->stop();
        emit positionUpdated(info);
        return;
    }
    if (m_running)
        emit positionUpdated(info);
}

// A standard normal variate (Box-Muller), from the seeded generator.
double GnssReceiver::gaussian() {
    if (m_hasSpare) {
        m_hasSpare = false;
        return m_spareGaussian;
    }
    double u = 0.0;
    do {
        u = m_random.generateDouble();
    } while (u <= 0.0);
    const double v = m_random.generateDouble();
    const double r = std::sqrt(-2.0 * std::log(u));
    m_spareGaussian = r * std::sin(2.0 * kPi * v);
    m_hasSpare = true;
    return r * std::cos(2.0 * kPi * v);
}

int GnssReceiver::interval() const { return updateInterval() > 0 ? updateInterval() : 1000; }

} // namespace QtTwin

#include "moc_qtwingnss.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
