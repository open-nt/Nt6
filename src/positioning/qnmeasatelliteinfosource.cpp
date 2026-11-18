// MIT License - see LICENSE
//
// QNmeaSatelliteInfoSource reads GSV (satellites in view) and GSA (satellites in use) sentences.
// A receiver sends one GSV set of up to nine messages per satellite system and one GSA per system
// for each fix. The latest complete set of every system is kept; an update is delivered when a
// sentence with a new time of day (GGA, RMC, GLL, ZDA) starts the next fix, or when the data read
// so far is exhausted. In simulation mode the recording is replayed one fix per
// SimulationUpdateInterval.
#include "qnmeasatelliteinfosource.h"
#include "qnmea_p.h"

#include <QtCore/qiodevice.h>
#include <QtCore/qtimer.h>

#include <algorithm>

namespace {

constexpr int kDefaultRequestTimeout = 10000; // milliseconds

QGeoSatelliteInfo::SatelliteSystem systemOfTalker(const QByteArray &talker) {
    if (talker == "GP")
        return QGeoSatelliteInfo::GPS;
    if (talker == "GL")
        return QGeoSatelliteInfo::GLONASS;
    if (talker == "GA")
        return QGeoSatelliteInfo::GALILEO;
    if (talker == "GB" || talker == "BD")
        return QGeoSatelliteInfo::BEIDOU;
    if (talker == "GQ" || talker == "QZ")
        return QGeoSatelliteInfo::QZSS;
    if (talker == "GN")
        return QGeoSatelliteInfo::Multiple;
    return QGeoSatelliteInfo::Undefined;
}

// The time-of-day field of the sentences that start a fix, or an empty array.
QByteArray fixTime(const QList<QByteArray> &f) {
    const QByteArray type = f.first().right(3);
    if (type == "GGA" || type == "RMC" || type == "ZDA")
        return f.value(1);
    if (type == "GLL")
        return f.value(5);
    return QByteArray();
}

} // namespace

/*!
    \class QNmeaSatelliteInfoSource
    \inmodule NtPositioning
    \brief The QNmeaSatelliteInfoSource class reads satellite information from NMEA 0183 data.

    The source reads the GSV sentences, which list the satellites in view with their elevation,
    azimuth and signal strength, and the GSA sentences, which list those used for the fix, from
    any QIODevice. GPS, GLONASS, Galileo, BeiDou and QZSS are told apart by the talker of each
    sentence, or for a combined (GN) GSA by its system ID or the satellite numbers.

    \code
    QNmeaSatelliteInfoSource satellites(QNmeaSatelliteInfoSource::RealTimeMode);
    satellites.setDevice(&port);
    QObject::connect(&satellites, &QGeoSatelliteInfoSource::satellitesInUseUpdated,
                     [](const QList<QGeoSatelliteInfo> &used) { qDebug() << used.size() << "satellites in use"; });
    satellites.startUpdates();
    \endcode

    A device can feed both this source and a QNmeaPositionInfoSource only if each gets its own
    copy of the data, since each reads the device to its end.

    \differences
    In simulation mode the recording is read when updates start, and fixes are replayed one per
    \l{QNmeaSatelliteInfoSource::SimulationUpdateInterval}{SimulationUpdateInterval}, not at the
    pace of their timestamps.
    \enddifferences

    \sa QNmeaPositionInfoSource, QGeoSatelliteInfo
*/

/*!
    \enum QNmeaSatelliteInfoSource::UpdateMode

    \value RealTimeMode The device delivers live data.
    \value SimulationMode The device holds a recording, replayed one fix per
           SimulationUpdateInterval.
*/

/*!
    \enum QNmeaSatelliteInfoSource::SatelliteInfoParseStatus

    \value NotParsed The sentence is not a GSV sentence.
    \value PartiallyParsed A GSV message that is not the last of its set.
    \value FullyParsed The last GSV message of its set.
*/

/*!
    \variable QNmeaSatelliteInfoSource::SimulationUpdateInterval
    The name of the backend property that sets, in milliseconds, how often a fix of the recording
    is replayed in simulation mode. The default is 100.
*/

/*!
    Constructs a source that reads in \a mode, with the given \a parent. Set a device with
    setDevice() before starting it.
*/
QNmeaSatelliteInfoSource::QNmeaSatelliteInfoSource(UpdateMode mode, QObject *parent)
    : QGeoSatelliteInfoSource(parent), m_mode(mode), m_requestTimer(new QTimer(this)),
      m_simulationTimer(new QTimer(this)) {
    m_requestTimer->setSingleShot(true);
    m_simulationTimer->setSingleShot(true);
    connect(m_requestTimer, &QTimer::timeout, this, [this] {
        m_requested = false;
        setError(UpdateTimeoutError);
    });
    connect(m_simulationTimer, &QTimer::timeout, this, [this] { simulateNext(); });
}

/*!
    Destroys the source. The device is not closed or deleted.
*/
QNmeaSatelliteInfoSource::~QNmeaSatelliteInfoSource() = default;

/*!
    \fn QNmeaSatelliteInfoSource::UpdateMode QNmeaSatelliteInfoSource::updateMode() const
    Returns the update mode given to the constructor.
*/

/*!
    Sets the device to read NMEA data from to \a source. The source does not take ownership; it
    opens the device for reading when updates start if it is not open yet. The device cannot be
    changed while updates are running.
*/
void QNmeaSatelliteInfoSource::setDevice(QIODevice *source) {
    if (source == m_device || m_running)
        return;
    if (m_device)
        disconnect(m_device, nullptr, this, nullptr);
    m_device = source;
    if (source && m_mode == RealTimeMode) {
        connect(source, &QIODevice::readyRead, this, &QNmeaSatelliteInfoSource::readAvailable);
        connect(source, &QIODevice::aboutToClose, this, [this] {
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
QIODevice *QNmeaSatelliteInfoSource::device() const { return m_device; }

/*!
    \reimp
*/
void QNmeaSatelliteInfoSource::setUpdateInterval(int msec) {
    QGeoSatelliteInfoSource::setUpdateInterval(msec > 0 ? std::max(msec, minimumUpdateInterval()) : 0);
}

/*!
    \reimp
*/
int QNmeaSatelliteInfoSource::minimumUpdateInterval() const { return 2; }

/*!
    \fn QGeoSatelliteInfoSource::Error QNmeaSatelliteInfoSource::error() const
    \reimp
*/

/*!
    \reimp

    The source supports SimulationUpdateInterval, a positive number of milliseconds.
*/
bool QNmeaSatelliteInfoSource::setBackendProperty(const QString &name, const QVariant &value) {
    if (name != SimulationUpdateInterval)
        return false;
    bool ok = false;
    const int interval = value.toInt(&ok);
    if (!ok || interval <= 0)
        return false;
    m_simulationInterval = interval;
    return true;
}

/*!
    \reimp
*/
QVariant QNmeaSatelliteInfoSource::backendProperty(const QString &name) const {
    return name == SimulationUpdateInterval ? QVariant(m_simulationInterval) : QVariant();
}

/*!
    \reimp
*/
void QNmeaSatelliteInfoSource::startUpdates() {
    if (m_running || !openDevice())
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
void QNmeaSatelliteInfoSource::stopUpdates() {
    m_running = false;
    if (!m_requested)
        m_simulationTimer->stop();
}

/*!
    \reimp
*/
void QNmeaSatelliteInfoSource::requestUpdate(int timeout) {
    if (timeout < 0 || (timeout > 0 && timeout < minimumUpdateInterval())) {
        setError(UpdateTimeoutError);
        return;
    }
    if (m_requested || !openDevice())
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
    Parses the NMEA sentence of \a size bytes at \a data. If it is a GSA sentence, fills
    \a pnrsInUse with the numbers of the satellites used for the fix and returns their system;
    otherwise returns QGeoSatelliteInfo::Undefined. A combined (GN) sentence names its system in
    the system ID of NMEA 4.10; without one, the system is taken from the satellite numbers (1 to
    32 GPS, 65 to 96 GLONASS), or is QGeoSatelliteInfo::Multiple.
*/
QGeoSatelliteInfo::SatelliteSystem QNmeaSatelliteInfoSource::parseSatellitesInUseFromNmea(const char *data, int size,
                                                                                          QList<int> &pnrsInUse) {
    const QList<QByteArray> f = QtPositioningPrivate::nmeaFields(data, size);
    if (f.isEmpty() || f.first().right(3) != "GSA")
        return QGeoSatelliteInfo::Undefined;
    pnrsInUse.clear();
    for (qsizetype i = 3; i <= 14 && i < f.size(); ++i) {
        bool ok = false;
        const int prn = f.at(i).toInt(&ok);
        if (ok && prn > 0)
            pnrsInUse.append(prn);
    }
    QGeoSatelliteInfo::SatelliteSystem system = systemOfTalker(f.first().left(2));
    if (system == QGeoSatelliteInfo::Multiple) {
        switch (f.value(18).toInt()) {
        case 1:
            return QGeoSatelliteInfo::GPS;
        case 2:
            return QGeoSatelliteInfo::GLONASS;
        case 3:
            return QGeoSatelliteInfo::GALILEO;
        case 4:
            return QGeoSatelliteInfo::BEIDOU;
        case 5:
            return QGeoSatelliteInfo::QZSS;
        default:
            break;
        }
        if (!pnrsInUse.isEmpty()) {
            const auto [low, high] = std::minmax_element(pnrsInUse.begin(), pnrsInUse.end());
            if (*low >= 1 && *high <= 32)
                return QGeoSatelliteInfo::GPS;
            if (*low >= 65 && *high <= 96)
                return QGeoSatelliteInfo::GLONASS;
        }
    }
    return system;
}

/*!
    Parses the NMEA sentence of \a size bytes at \a data. If it is a GSV message, appends the
    satellites it lists to \a infos, sets \a system to their system, and returns FullyParsed for
    the last message of a set or PartiallyParsed for the others; otherwise returns NotParsed.

    Reimplement this function, or parseSatellitesInUseFromNmea(), to read proprietary sentences.
*/
QNmeaSatelliteInfoSource::SatelliteInfoParseStatus
QNmeaSatelliteInfoSource::parseSatelliteInfoFromNmea(const char *data, int size, QList<QGeoSatelliteInfo> &infos,
                                                     QGeoSatelliteInfo::SatelliteSystem &system) {
    const QList<QByteArray> f = QtPositioningPrivate::nmeaFields(data, size);
    if (f.isEmpty() || f.first().right(3) != "GSV")
        return NotParsed;
    bool ok = false;
    const int total = f.value(1).toInt(&ok);
    if (!ok || total < 1)
        return NotParsed;
    const int number = f.value(2).toInt(&ok);
    if (!ok || number < 1)
        return NotParsed;
    system = systemOfTalker(f.first().left(2));
    for (qsizetype i = 4; i + 3 < f.size(); i += 4) {
        const int prn = f.at(i).toInt(&ok);
        if (!ok)
            continue;
        QGeoSatelliteInfo info;
        info.setSatelliteSystem(system);
        info.setSatelliteIdentifier(prn);
        bool has = false;
        const int elevation = f.at(i + 1).toInt(&has);
        if (has)
            info.setAttribute(QGeoSatelliteInfo::Elevation, elevation);
        const int azimuth = f.at(i + 2).toInt(&has);
        if (has)
            info.setAttribute(QGeoSatelliteInfo::Azimuth, azimuth);
        const int snr = f.at(i + 3).toInt(&has);
        info.setSignalStrength(has ? snr : -1);
        infos.append(info);
    }
    return number >= total ? FullyParsed : PartiallyParsed;
}

bool QNmeaSatelliteInfoSource::openDevice() {
    if (!m_device || (!m_device->isOpen() && !m_device->open(QIODevice::ReadOnly)) || !m_device->isReadable()) {
        setError(AccessError);
        return false;
    }
    if (m_mode == SimulationMode && m_simulation.isEmpty()) {
        m_collecting = true;
        m_buffer = m_device->readAll();
        m_buffer.append('\n');
        readAvailable();
        m_collecting = false;
    }
    return true;
}

void QNmeaSatelliteInfoSource::readAvailable() {
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
    flush();
}

void QNmeaSatelliteInfoSource::processLine(const QByteArray &line) {
    const int size = static_cast<int>(line.size());
    const QList<QByteArray> f = QtPositioningPrivate::nmeaFields(line.constData(), size);
    if (f.isEmpty())
        return;

    // A new time of day starts the next fix: deliver what the previous one reported.
    const QByteArray time = fixTime(f);
    if (!time.isEmpty()) {
        if (time != m_lastTime)
            flush();
        m_lastTime = time;
        return;
    }

    QList<QGeoSatelliteInfo> infos;
    QGeoSatelliteInfo::SatelliteSystem system = QGeoSatelliteInfo::Undefined;
    const SatelliteInfoParseStatus status = parseSatelliteInfoFromNmea(line.constData(), size, infos, system);
    if (status != NotParsed) {
        if (f.value(2).toInt() == 1)
            m_partialInView[system].clear();
        m_partialInView[system].append(infos);
        if (status == FullyParsed) {
            m_inView[system] = m_partialInView.take(system);
            m_changed = true;
        }
        return;
    }
    QList<int> used;
    system = parseSatellitesInUseFromNmea(line.constData(), size, used);
    if (system != QGeoSatelliteInfo::Undefined) {
        m_inUse[system] = used;
        m_changed = true;
    }
}

QNmeaSatelliteInfoSource::Snapshot QNmeaSatelliteInfoSource::snapshot() const {
    Snapshot s;
    for (const QList<QGeoSatelliteInfo> &list : m_inView)
        s.inView.append(list);
    for (auto it = m_inUse.constBegin(); it != m_inUse.constEnd(); ++it) {
        for (int prn : it.value()) {
            QGeoSatelliteInfo found;
            found.setSatelliteSystem(static_cast<QGeoSatelliteInfo::SatelliteSystem>(it.key()));
            found.setSatelliteIdentifier(prn);
            for (const QGeoSatelliteInfo &seen : std::as_const(s.inView)) {
                if (seen.satelliteIdentifier() == prn &&
                    (it.key() == QGeoSatelliteInfo::Multiple || seen.satelliteSystem() == it.key())) {
                    found = seen;
                    break;
                }
            }
            s.inUse.append(found);
        }
    }
    return s;
}

void QNmeaSatelliteInfoSource::flush() {
    if (!m_changed)
        return;
    m_changed = false;
    if (m_collecting)
        m_simulation.append(snapshot());
    else
        deliver(snapshot());
}

void QNmeaSatelliteInfoSource::deliver(const Snapshot &s) {
    if (m_requested) {
        m_requested = false;
        m_requestTimer->stop();
    } else if (!m_running) {
        return;
    }
    emit satellitesInViewUpdated(s.inView);
    emit satellitesInUseUpdated(s.inUse);
}

void QNmeaSatelliteInfoSource::simulateNext() {
    if ((!m_running && !m_requested) || m_simulationIndex < 0 || m_simulationIndex >= m_simulation.size()) {
        m_simulationIndex = -1;
        return;
    }
    deliver(m_simulation.at(m_simulationIndex++));
    if (m_simulationIndex < m_simulation.size() && m_running)
        m_simulationTimer->start(std::max(m_simulationInterval, updateInterval()));
}

void QNmeaSatelliteInfoSource::setError(Error error) {
    m_error = error;
    if (error != NoError)
        emit errorOccurred(error);
}

#include "moc_qnmeasatelliteinfosource.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
