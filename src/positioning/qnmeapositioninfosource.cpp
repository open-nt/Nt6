// MIT License - see LICENSE
//
// QNmeaPositionInfoSource reads NMEA 0183 from a QIODevice. The sentences a receiver sends for one
// fix (GGA, RMC, GSA, VTG, ...) share a time of day; they are merged into one update, which is
// delivered when a sentence of a new epoch arrives or when the data read so far is exhausted. In
// simulation mode the whole device is read first and the updates are replayed with the time gaps
// recorded in the log.
#include "qnmeapositioninfosource.h"
#include "qnmea_p.h"

#include <QtCore/qiodevice.h>
#include <QtCore/qtimer.h>

#include <cmath>
#include <cstdlib>

namespace {

constexpr int kDefaultRequestTimeout = 10000; // milliseconds
constexpr double kKnot = 1852.0 / 3600.0;     // metres per second

// Parses "ddmm.mmmm" (latitude) or "dddmm.mmmm" (longitude) with its hemisphere letter.
bool parseAngle(const QByteArray &value, const QByteArray &hemisphere, double *degrees) {
    bool ok = false;
    const double raw = value.toDouble(&ok);
    if (!ok || hemisphere.isEmpty())
        return false;
    const double whole = std::floor(raw / 100.0);
    double result = whole + (raw - whole * 100.0) / 60.0;
    const char h = hemisphere.at(0);
    if (h == 'S' || h == 'W')
        result = -result;
    else if (h != 'N' && h != 'E')
        return false;
    *degrees = result;
    return true;
}

bool parseTime(const QByteArray &value, QTime *time) {
    if (value.size() < 6)
        return false;
    bool ok = false;
    const int h = value.left(2).toInt(&ok);
    if (!ok)
        return false;
    const int m = value.mid(2, 2).toInt(&ok);
    if (!ok)
        return false;
    const double s = value.mid(4).toDouble(&ok);
    if (!ok)
        return false;
    const int whole = static_cast<int>(s);
    *time = QTime(h, m, whole, static_cast<int>(std::lround((s - whole) * 1000.0)) % 1000);
    return time->isValid();
}

bool parseDate(const QByteArray &value, QDate *date) {
    if (value.size() != 6)
        return false;
    bool ok = false;
    const int d = value.left(2).toInt(&ok);
    const int m = ok ? value.mid(2, 2).toInt(&ok) : 0;
    const int y = ok ? value.mid(4, 2).toInt(&ok) : 0;
    if (!ok)
        return false;
    *date = QDate(y < 80 ? 2000 + y : 1900 + y, m, d);
    return date->isValid();
}

double toDouble(const QByteArray &value) {
    bool ok = false;
    const double v = value.toDouble(&ok);
    return ok ? v : qQNaN();
}

} // namespace

/*!
    \class QNmeaPositionInfoSource
    \inmodule NtPositioning
    \brief The QNmeaPositionInfoSource class reads positions from NMEA 0183 data.

    Most GNSS receivers report their fixes as NMEA 0183 sentences over a serial port, USB or the
    network. Give the source a QIODevice, such as a QSerialPort, a QTcpSocket or a QFile with a
    recorded log, and it delivers one QGeoPositionInfo per fix:

    \code
    QSerialPort port(QStringLiteral("/dev/ttyACM0"));
    port.setBaudRate(QSerialPort::Baud9600);
    port.open(QIODevice::ReadOnly);

    QNmeaPositionInfoSource source(QNmeaPositionInfoSource::RealTimeMode);
    source.setDevice(&port);
    source.setUserEquivalentRangeError(5.1);   // metres, for HorizontalAccuracy from HDOP
    QObject::connect(&source, &QGeoPositionInfoSource::positionUpdated, [](const QGeoPositionInfo &info) {
        qDebug() << info.coordinate() << info.timestamp();
    });
    source.startUpdates();
    \endcode

    The sentences GGA, RMC, GLL, VTG, GSA and ZDA are read, from any talker (GP, GN, GL, GA, GB,
    ...). Sentences with a wrong checksum are ignored. The sentences of one fix are merged: RMC
    gives the date, speed and course, GGA the altitude and HDOP. Altitudes are as the receiver
    reports them in GGA, above mean sea level. Timestamps are in UTC.

    In \l{QNmeaPositionInfoSource::SimulationMode}{SimulationMode} the device is read to its end
    when updates start, and the fixes are replayed with the gaps between their timestamps, so a
    recorded drive plays back in real time.

    \differences
    Satellite information (GSV) is read by QNmeaSatelliteInfoSource, not by this class. A fix is
    delivered when the next epoch starts or when the data read so far ends, so a receiver that
    sends its sentences in separate writes may deliver one fix as two updates.
    \enddifferences

    \sa QGeoPositionInfoSource
*/

/*!
    \enum QNmeaPositionInfoSource::UpdateMode

    \value RealTimeMode The device delivers live data; each fix is delivered as it is read.
    \value SimulationMode The device holds a recording; it is replayed at the pace of its
           timestamps.
*/

/*!
    Constructs a source that reads in \a updateMode, with the given \a parent. Set a device with
    setDevice() before starting it.
*/
QNmeaPositionInfoSource::QNmeaPositionInfoSource(UpdateMode updateMode, QObject *parent)
    : QGeoPositionInfoSource(parent), m_mode(updateMode), m_requestTimer(new QTimer(this)),
      m_intervalTimer(new QTimer(this)), m_simulationTimer(new QTimer(this)) {
    m_requestTimer->setSingleShot(true);
    m_intervalTimer->setSingleShot(true);
    m_simulationTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout, this, [this] {
        m_requested = false;
        setError(UpdateTimeoutError);
    });
    connect(m_intervalTimer, &QTimer::timeout, this, [this] { emitThrottled(); });
    connect(m_simulationTimer, &QTimer::timeout, this, [this] { simulateNext(); });
}

/*!
    Destroys the source. The device is not closed or deleted.
*/
QNmeaPositionInfoSource::~QNmeaPositionInfoSource() = default;

/*!
    \fn void QNmeaPositionInfoSource::setUserEquivalentRangeError(double uere)
    Sets the user equivalent range error of the receiver to \a uere metres. When it is set, the
    horizontal and vertical accuracy of each update is the HDOP or VDOP times this value.
*/

/*!
    \fn double QNmeaPositionInfoSource::userEquivalentRangeError() const
    Returns the user equivalent range error in metres, or NaN if it is not set.
*/

/*!
    \fn QNmeaPositionInfoSource::UpdateMode QNmeaPositionInfoSource::updateMode() const
    Returns the update mode given to the constructor.
*/

/*!
    Sets the device to read NMEA data from to \a device. The source does not take ownership. It
    opens the device for reading when updates start if it is not open yet. The device cannot be
    changed while updates are running.
*/
void QNmeaPositionInfoSource::setDevice(QIODevice *device) {
    if (device == m_device || m_running)
        return;
    if (m_device)
        disconnect(m_device, nullptr, this, nullptr);
    m_device = device;
    if (device && m_mode == RealTimeMode) {
        connect(device, &QIODevice::readyRead, this, &QNmeaPositionInfoSource::readAvailable);
        connect(device, &QIODevice::aboutToClose, this, [this] {
            if (m_running || m_requested)
                setError(ClosedError);
        });
    }
    m_buffer.clear();
    m_simulation.clear();
    m_simulationIndex = -1;
}

/*!
    Returns the device, or \c nullptr if none is set.
*/
QIODevice *QNmeaPositionInfoSource::device() const { return m_device; }

/*!
    \reimp
*/
void QNmeaPositionInfoSource::setUpdateInterval(int msec) {
    QGeoPositionInfoSource::setUpdateInterval(msec > 0 ? std::max(msec, minimumUpdateInterval()) : 0);
}

/*!
    \reimp
*/
QGeoPositionInfo QNmeaPositionInfoSource::lastKnownPosition(bool fromSatellitePositioningMethodsOnly) const {
    Q_UNUSED(fromSatellitePositioningMethodsOnly); // NMEA positions are all from satellites
    return m_lastKnown;
}

/*!
    \reimp
*/
QGeoPositionInfoSource::PositioningMethods QNmeaPositionInfoSource::supportedPositioningMethods() const {
    return SatellitePositioningMethods;
}

/*!
    \reimp
*/
int QNmeaPositionInfoSource::minimumUpdateInterval() const { return 2; }

/*!
    \fn QGeoPositionInfoSource::Error QNmeaPositionInfoSource::error() const
    \reimp
*/

/*!
    \reimp
*/
void QNmeaPositionInfoSource::startUpdates() {
    if (m_running)
        return;
    if (!openDevice())
        return;
    m_running = true;
    m_error = NoError;
    if (m_mode == SimulationMode) {
        if (m_simulationIndex < 0 || m_simulationIndex >= m_simulation.size())
            m_simulationIndex = 0;
        if (!m_simulationTimer->isActive())
            m_simulationTimer->start(0);
    } else {
        readAvailable();
    }
}

/*!
    \reimp
*/
void QNmeaPositionInfoSource::stopUpdates() {
    m_running = false;
    m_intervalTimer->stop();
    if (!m_requested)
        m_simulationTimer->stop();
}

/*!
    \reimp
*/
void QNmeaPositionInfoSource::requestUpdate(int timeout) {
    if (timeout < 0 || (timeout > 0 && timeout < minimumUpdateInterval())) {
        setError(UpdateTimeoutError);
        return;
    }
    if (m_requested)
        return;
    if (!openDevice())
        return;
    m_requested = true;
    m_error = NoError;
    m_requestTimer->start(timeout > 0 ? timeout : kDefaultRequestTimeout);
    if (m_mode == SimulationMode) {
        if (m_simulationIndex < 0 || m_simulationIndex >= m_simulation.size())
            m_simulationIndex = 0;
        if (!m_simulationTimer->isActive())
            m_simulationTimer->start(0);
    } else {
        readAvailable();
    }
}

/*!
    Parses the NMEA sentence of \a size bytes at \a data into \a posInfo, and returns \c true if it
    is a sentence the source understands. \a hasFix is set to \c false if the sentence reports that
    the receiver has no fix. Sentences without a time of day (VTG, GSA) leave the timestamp
    invalid; their values are merged into the fix of the sentences around them.

    Reimplement this function to read proprietary sentences, and call the base implementation for
    the others.
*/
bool QNmeaPositionInfoSource::parsePosInfoFromNmeaData(const char *data, int size, QGeoPositionInfo *posInfo,
                                                       bool *hasFix) {
    *hasFix = true;
    const QList<QByteArray> f = QtPositioningPrivate::nmeaFields(data, size);
    if (f.isEmpty())
        return false;
    const QByteArray type = f.first().right(3);
    auto field = [&f](qsizetype i) { return i < f.size() ? f.at(i) : QByteArray(); };

    QTime time;
    QDate date;
    QGeoCoordinate coordinate;
    double lat = 0.0;
    double lon = 0.0;
    auto position = [&](qsizetype latIndex) {
        if (parseAngle(field(latIndex), field(latIndex + 1), &lat) &&
            parseAngle(field(latIndex + 2), field(latIndex + 3), &lon))
            coordinate = QGeoCoordinate(lat, lon);
    };
    auto accuracy = [&](QGeoPositionInfo::Attribute attribute, const QByteArray &dop) {
        const double value = toDouble(dop);
        if (!std::isnan(value) && !std::isnan(m_uere))
            posInfo->setAttribute(attribute, value * m_uere);
    };

    if (type == "RMC") {
        parseTime(field(1), &time);
        *hasFix = field(2) == "A" && field(12) != "N";
        position(3);
        const double knots = toDouble(field(7));
        if (!std::isnan(knots))
            posInfo->setAttribute(QGeoPositionInfo::GroundSpeed, knots * kKnot);
        const double course = toDouble(field(8));
        if (!std::isnan(course))
            posInfo->setAttribute(QGeoPositionInfo::Direction, course);
        if (parseDate(field(9), &date))
            m_date = date;
        const double variation = toDouble(field(10));
        if (!std::isnan(variation))
            posInfo->setAttribute(QGeoPositionInfo::MagneticVariation, field(11) == "W" ? -variation : variation);
    } else if (type == "GGA") {
        parseTime(field(1), &time);
        position(2);
        *hasFix = field(6).toInt() > 0;
        const double altitude = toDouble(field(9));
        if (coordinate.isValid() && !std::isnan(altitude))
            coordinate.setAltitude(altitude);
        accuracy(QGeoPositionInfo::HorizontalAccuracy, field(8));
    } else if (type == "GLL") {
        position(1);
        parseTime(field(5), &time);
        *hasFix = field(6) == "A" && field(7) != "N";
    } else if (type == "VTG") {
        const double course = toDouble(field(1));
        if (!std::isnan(course))
            posInfo->setAttribute(QGeoPositionInfo::Direction, course);
        const double kmh = toDouble(field(7));
        const double knots = toDouble(field(5));
        if (!std::isnan(kmh))
            posInfo->setAttribute(QGeoPositionInfo::GroundSpeed, kmh / 3.6);
        else if (!std::isnan(knots))
            posInfo->setAttribute(QGeoPositionInfo::GroundSpeed, knots * kKnot);
    } else if (type == "GSA") {
        *hasFix = field(2).toInt() >= 2;
        accuracy(QGeoPositionInfo::HorizontalAccuracy, field(16));
        accuracy(QGeoPositionInfo::VerticalAccuracy, field(17));
    } else if (type == "ZDA") {
        parseTime(field(1), &time);
        const QDate zda(field(4).toInt(), field(3).toInt(), field(2).toInt());
        if (zda.isValid())
            m_date = zda;
    } else {
        return false;
    }

    posInfo->setCoordinate(coordinate);
    if (time.isValid()) {
        const QDate day = date.isValid() ? date : (m_date.isValid() ? m_date : QDateTime::currentDateTimeUtc().date());
        posInfo->setTimestamp(QDateTime(day, time, Qt::UTC));
    }
    return true;
}

bool QNmeaPositionInfoSource::openDevice() {
    if (!m_device) {
        setError(AccessError);
        return false;
    }
    if (!m_device->isOpen() && !m_device->open(QIODevice::ReadOnly)) {
        setError(AccessError);
        return false;
    }
    if (!m_device->isReadable()) {
        setError(AccessError);
        return false;
    }
    if (m_mode == SimulationMode) {
        if (m_simulation.isEmpty()) {
            // Read the whole recording and turn it into a list of fixes.
            m_collecting = true;
            m_buffer = m_device->readAll();
            m_buffer.append('\n');
            readAvailable();
            m_collecting = false;
        }
    }
    return true;
}

void QNmeaPositionInfoSource::readAvailable() {
    if (!m_collecting) {
        if (!m_device || (!m_running && !m_requested))
            return;
        m_buffer.append(m_device->readAll());
    }
    qsizetype start = 0;
    for (;;) {
        const qsizetype end = m_buffer.indexOf('\n', start);
        if (end < 0)
            break;
        processLine(m_buffer.mid(start, end - start));
        start = end + 1;
    }
    m_buffer.remove(0, start);
    flushEpoch();
}

void QNmeaPositionInfoSource::processLine(const QByteArray &line) {
    QGeoPositionInfo info;
    bool hasFix = true;
    if (!parsePosInfoFromNmeaData(line.constData(), static_cast<int>(line.size()), &info, &hasFix))
        return;
    if (info.timestamp().isValid()) {
        if (m_epoch.started && m_epoch.info.timestamp().time() != info.timestamp().time())
            flushEpoch();
        if (!m_epoch.started) {
            m_epoch.started = true;
            m_epoch.info.setTimestamp(info.timestamp());
        } else if (info.timestamp().date() != m_epoch.info.timestamp().date()) {
            m_epoch.info.setTimestamp(info.timestamp()); // RMC knows the date better than GGA
        }
    } else if (!m_epoch.started) {
        // VTG or GSA before any timed sentence: keep its values for the coming epoch.
        m_epoch.started = true;
    }
    const QGeoCoordinate c = info.coordinate();
    if (c.isValid()) {
        QGeoCoordinate merged(c.latitude(), c.longitude());
        if (c.type() == QGeoCoordinate::Coordinate3D)
            merged.setAltitude(c.altitude());
        else if (m_epoch.info.coordinate().type() == QGeoCoordinate::Coordinate3D)
            merged.setAltitude(m_epoch.info.coordinate().altitude());
        m_epoch.info.setCoordinate(merged);
        m_epoch.hasPosition = true;
    }
    for (int a = QGeoPositionInfo::Direction; a <= QGeoPositionInfo::DirectionAccuracy; ++a) {
        const auto attribute = static_cast<QGeoPositionInfo::Attribute>(a);
        if (info.hasAttribute(attribute))
            m_epoch.info.setAttribute(attribute, info.attribute(attribute));
    }
    if (!hasFix)
        m_epoch.fix = false;
}

void QNmeaPositionInfoSource::flushEpoch() {
    const Epoch epoch = m_epoch;
    m_epoch = Epoch();
    if (!epoch.started || !epoch.hasPosition || !epoch.fix || !epoch.info.timestamp().isValid())
        return;
    if (m_collecting)
        m_simulation.append(epoch.info);
    else
        deliver(epoch.info);
}

void QNmeaPositionInfoSource::deliver(const QGeoPositionInfo &info) {
    m_lastKnown = info;
    if (m_requested) {
        m_requested = false;
        m_requestTimer->stop();
        m_lastEmitted = info.timestamp();
        emit positionUpdated(info);
        return;
    }
    if (!m_running)
        return;
    m_latest = info;
    if (updateInterval() > 0 && m_intervalTimer->isActive())
        return; // emitThrottled() delivers the latest one when the interval ends
    emitThrottled();
}

void QNmeaPositionInfoSource::emitThrottled() {
    if (!m_running || !m_latest.isValid() || m_latest.timestamp() == m_lastEmitted)
        return;
    m_lastEmitted = m_latest.timestamp();
    if (updateInterval() > 0 && m_mode == RealTimeMode)
        m_intervalTimer->start(updateInterval());
    emit positionUpdated(m_latest);
}

void QNmeaPositionInfoSource::simulateNext() {
    if (!m_running && !m_requested)
        return;
    if (m_simulationIndex < 0 || m_simulationIndex >= m_simulation.size()) {
        m_simulationIndex = -1;
        return;
    }
    const QGeoPositionInfo info = m_simulation.at(m_simulationIndex);
    // With an update interval, skip the fixes that fall inside it.
    qsizetype next = m_simulationIndex + 1;
    if (updateInterval() > 0) {
        while (next < m_simulation.size() &&
               info.timestamp().msecsTo(m_simulation.at(next).timestamp()) < updateInterval())
            ++next;
    }
    deliver(info);
    m_simulationIndex = next;
    if (next < m_simulation.size() && (m_running || m_requested)) {
        const qint64 gap = info.timestamp().msecsTo(m_simulation.at(next).timestamp());
        m_simulationTimer->start(static_cast<int>(std::clamp<qint64>(gap, 0, 60 * 60 * 1000)));
    }
}

void QNmeaPositionInfoSource::setError(Error error) {
    m_error = error;
    if (error != NoError)
        emit errorOccurred(error);
}

#include "moc_qnmeapositioninfosource.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
