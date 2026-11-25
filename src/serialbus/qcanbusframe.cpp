// MIT License - see LICENSE
#include <QtSerialBus/qcanbusframe.h>

#include <QtCore/qdatastream.h>

namespace {

constexpr quint32 kMaximumBaseId = 0x7ff;
constexpr quint32 kMaximumExtendedId = 0x1fffffff;

bool isFdLength(qsizetype length) {
    switch (length) {
    case 12:
    case 16:
    case 20:
    case 24:
    case 32:
    case 48:
    case 64:
        return true;
    default:
        return length >= 0 && length <= 8;
    }
}

} // namespace

/*!
    \class QCanBusFrame
    \inmodule NtSerialBus
    \brief The QCanBusFrame class is a frame of a CAN bus: its identifier, payload and flags.
    \since 6.0

    A data frame carries up to 8 bytes on a classic CAN bus and up to 64 on CAN FD, under an
    identifier of 11 bits (base format) or 29 bits (extended format):

    \code
    QCanBusFrame frame(0x123, QByteArray::fromHex("cafe"));
    device->writeFrame(frame);
    \endcode

    \differences
    The layout that operator<<() writes to a QDataStream is Nt's own and is not read by Qt.
    \enddifferences

    \sa QCanBusDevice
*/

/*!
    \typedef QCanBusFrame::FrameId

    The identifier of a frame, of 11 or 29 bits.
*/

/*!
    \enum QCanBusFrame::FrameType

    This enum describes the type of a frame.

    \value UnknownFrame The type is not known.
    \value DataFrame A frame that carries data.
    \value ErrorFrame A frame that reports errors of the bus; error() tells which.
    \value RemoteRequestFrame A request for a data frame of the same identifier.
    \value InvalidFrame An invalid frame, such as readFrame() returns when there is none.
*/

/*!
    \enum QCanBusFrame::FrameError

    This enum describes the errors that an error frame reports.

    \value NoError No error.
    \value TransmissionTimeoutError A frame could not be sent in time.
    \value LostArbitrationError The controller lost arbitration.
    \value ControllerError The controller reports a problem, such as an overflow or the error
           passive state.
    \value ProtocolViolationError A violation of the CAN protocol.
    \value TransceiverError A problem of the transceiver.
    \value MissingAcknowledgmentError No node acknowledged a frame.
    \value BusOffError The controller went bus off.
    \value BusError A bus error.
    \value ControllerRestartError The controller restarted.
    \value UnknownError Another error.
    \value AnyError Every error, for filters.
*/

/*!
    \class QCanBusFrame::TimeStamp
    \inmodule NtSerialBus
    \brief The QCanBusFrame::TimeStamp class tells when a frame was received.
    \since 6.0

    \differences
    None.
    \enddifferences
*/

/*!
    \fn QCanBusFrame::TimeStamp::TimeStamp(qint64 s, qint64 usec)

    Constructs a time stamp of \a s seconds and \a usec microseconds.
*/

/*!
    \fn QCanBusFrame::TimeStamp QCanBusFrame::TimeStamp::fromMicroSeconds(qint64 usec)

    Returns the time stamp of \a usec microseconds.
*/

/*!
    \fn qint64 QCanBusFrame::TimeStamp::seconds() const

    Returns the seconds of the time stamp.
*/

/*!
    \fn qint64 QCanBusFrame::TimeStamp::microSeconds() const

    Returns the microseconds of the time stamp, past seconds().
*/

/*!
    Constructs an empty frame of \a type.
*/
QCanBusFrame::QCanBusFrame(FrameType type) noexcept : m_type(type) {}

/*!
    Constructs a data frame with \a identifier and the payload \a data. An identifier above 0x7FF
    selects the extended format.
*/
QCanBusFrame::QCanBusFrame(FrameId identifier, const QByteArray &data) : m_payload(data) { setFrameId(identifier); }

/*!
    Returns \c true if the frame can be sent: a known type, an identifier that fits its format,
    and a payload of a length that the bus allows (up to 8 bytes, or one of the lengths of CAN FD
    up to 64).
*/
bool QCanBusFrame::isValid() const noexcept {
    if (m_type == InvalidFrame || m_invalidId)
        return false;
    if (!m_extended && m_id > kMaximumBaseId && m_type != ErrorFrame)
        return false;
    const qsizetype length = m_payload.size();
    if (m_flexibleDataRate) {
        if (m_type == RemoteRequestFrame)
            return false;
        return isFdLength(length);
    }
    return length <= 8;
}

/*!
    Sets the type of the frame to \a newType.
*/
void QCanBusFrame::setFrameType(FrameType newType) noexcept { m_type = newType; }

/*!
    Sets the identifier of the frame to \a newFrameId, which selects the extended format if it is
    above 0x7FF. An identifier above 0x1FFFFFFF makes the frame invalid.
*/
void QCanBusFrame::setFrameId(FrameId newFrameId) {
    if (newFrameId > kMaximumExtendedId) {
        m_invalidId = true;
        m_id = 0;
        return;
    }
    m_invalidId = false;
    m_id = newFrameId;
    m_extended = newFrameId > kMaximumBaseId;
}

/*!
    Returns the identifier of the frame, or 0 for an error frame.
*/
QCanBusFrame::FrameId QCanBusFrame::frameId() const noexcept { return m_type == ErrorFrame ? 0 : m_id; }

/*!
    Sets the payload of the frame to \a data. For a remote request, the length of \a data is the
    length that is asked for.
*/
void QCanBusFrame::setPayload(const QByteArray &data) { m_payload = data; }

/*!
    Returns the errors of an error frame, or \c NoError for another frame.
*/
QCanBusFrame::FrameErrors QCanBusFrame::error() const noexcept {
    if (m_type != ErrorFrame)
        return NoError;
    return FrameErrors::fromInt(int(m_id & quint32(AnyError)));
}

/*!
    Sets the errors of an error frame to \a e. It does nothing for another frame.
*/
void QCanBusFrame::setError(FrameErrors e) {
    if (m_type != ErrorFrame)
        return;
    m_id = quint32(int(e)) & quint32(AnyError);
}

/*!
    Sets whether the frame is a CAN FD frame to \a isFlexibleData. Turning it off clears the bit
    rate switch and the error state indicator.
*/
void QCanBusFrame::setFlexibleDataRateFormat(bool isFlexibleData) noexcept {
    m_flexibleDataRate = isFlexibleData;
    if (!isFlexibleData) {
        m_bitrateSwitch = false;
        m_errorStateIndicator = false;
    }
}

/*!
    Sets whether the payload of a CAN FD frame is sent at the data bit rate to \a bitrateSwitch.
    Setting it makes the frame a CAN FD frame.
*/
void QCanBusFrame::setBitrateSwitch(bool bitrateSwitch) noexcept {
    m_bitrateSwitch = bitrateSwitch;
    if (bitrateSwitch)
        m_flexibleDataRate = true;
}

/*!
    Sets the error state indicator of a CAN FD frame to \a errorStateIndicator. Setting it makes
    the frame a CAN FD frame.
*/
void QCanBusFrame::setErrorStateIndicator(bool errorStateIndicator) noexcept {
    m_errorStateIndicator = errorStateIndicator;
    if (errorStateIndicator)
        m_flexibleDataRate = true;
}

/*!
    \fn bool QCanBusFrame::hasLocalEcho() const

    Returns \c true if the frame is one that this application sent, received back because the
    device was configured with \c QCanBusDevice::ReceiveOwnKey.
*/

/*!
    \fn void QCanBusFrame::setLocalEcho(bool localEcho)

    Marks the frame as sent by this application if \a localEcho is \c true.
*/

/*!
    \fn QCanBusFrame::FrameType QCanBusFrame::frameType() const

    Returns the type of the frame.
*/

/*!
    \fn bool QCanBusFrame::hasExtendedFrameFormat() const

    Returns \c true if the frame has a 29-bit identifier.
*/

/*!
    \fn void QCanBusFrame::setExtendedFrameFormat(bool isExtended)

    Sets whether the frame has a 29-bit identifier to \a isExtended.
*/

/*!
    \fn QByteArray QCanBusFrame::payload() const

    Returns the payload of the frame.
*/

/*!
    \fn void QCanBusFrame::setTimeStamp(QCanBusFrame::TimeStamp ts)

    Sets the time stamp of the frame to \a ts.
*/

/*!
    \fn QCanBusFrame::TimeStamp QCanBusFrame::timeStamp() const

    Returns when the frame was received.
*/

/*!
    \fn bool QCanBusFrame::hasFlexibleDataRateFormat() const

    Returns \c true if the frame is a CAN FD frame.
*/

/*!
    \fn bool QCanBusFrame::hasBitrateSwitch() const

    Returns \c true if the payload of the CAN FD frame is sent at the data bit rate.
*/

/*!
    \fn bool QCanBusFrame::hasErrorStateIndicator() const

    Returns \c true if the sender of the CAN FD frame was error passive.
*/

/*!
    Returns the frame as text: the identifier in hexadecimal, right-aligned in eight places (padded
    with spaces for the base format and zeros for the extended one), the length in brackets and the
    payload in hexadecimal, as in

    \code
         123   [4]  01 02 03 04
    12345678   [8]  01 02 03 04 05 06 07 08
         123   [5]  Remote Request
    \endcode

    An error frame is \c (Error), an invalid one \c (Invalid) and one of unknown type \c (Unknown).
*/
QString QCanBusFrame::toString() const {
    switch (m_type) {
    case ErrorFrame:
        return QStringLiteral("(Error)");
    case InvalidFrame:
        return QStringLiteral("(Invalid)");
    case UnknownFrame:
        return QStringLiteral("(Unknown)");
    default:
        break;
    }
    QString text = m_extended ? QStringLiteral("%1").arg(m_id, 8, 16, QLatin1Char('0')).toUpper()
                              : QStringLiteral("%1").arg(QString::number(m_id, 16).toUpper(), 8);
    text += QStringLiteral("%1").arg(QStringLiteral("[%1]").arg(m_payload.size()), 6);
    if (m_type == RemoteRequestFrame)
        return text + QStringLiteral("  Remote Request");
    if (!m_payload.isEmpty()) {
        text += QStringLiteral("  ");
        text += QString::fromLatin1(m_payload.toHex(' ').toUpper());
    }
    return text;
}

// Writes frame to the stream out and returns it. (ndoc cannot document global functions yet.)
QDataStream &operator<<(QDataStream &out, const QCanBusFrame &frame) {
    out << quint32(frame.m_type) << frame.m_id << frame.m_payload << frame.m_timeStamp.seconds()
        << frame.m_timeStamp.microSeconds() << frame.m_extended << frame.m_flexibleDataRate << frame.m_bitrateSwitch
        << frame.m_errorStateIndicator << frame.m_localEcho << frame.m_invalidId;
    return out;
}

// Reads a frame from the stream in into frame and returns the stream.
QDataStream &operator>>(QDataStream &in, QCanBusFrame &frame) {
    quint32 type = 0;
    qint64 seconds = 0;
    qint64 microSeconds = 0;
    in >> type >> frame.m_id >> frame.m_payload >> seconds >> microSeconds >> frame.m_extended >>
        frame.m_flexibleDataRate >> frame.m_bitrateSwitch >> frame.m_errorStateIndicator >> frame.m_localEcho >>
        frame.m_invalidId;
    frame.m_type = type <= QCanBusFrame::InvalidFrame ? QCanBusFrame::FrameType(type) : QCanBusFrame::InvalidFrame;
    frame.m_timeStamp = QCanBusFrame::TimeStamp(seconds, microSeconds);
    return in;
}
