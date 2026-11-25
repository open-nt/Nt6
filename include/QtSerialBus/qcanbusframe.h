// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>

class QDataStream;

class QCanBusFrame {
public:
    using FrameId = quint32;

    enum FrameType {
        UnknownFrame = 0x0,
        DataFrame = 0x1,
        ErrorFrame = 0x2,
        RemoteRequestFrame = 0x3,
        InvalidFrame = 0x4
    };

    enum FrameError {
        NoError = 0,
        TransmissionTimeoutError = (1 << 0),
        LostArbitrationError = (1 << 1),
        ControllerError = (1 << 2),
        ProtocolViolationError = (1 << 3),
        TransceiverError = (1 << 4),
        MissingAcknowledgmentError = (1 << 5),
        BusOffError = (1 << 6),
        BusError = (1 << 7),
        ControllerRestartError = (1 << 8),
        UnknownError = (1 << 9),
        AnyError = 0x1FFFFFFF
    };
    Q_DECLARE_FLAGS(FrameErrors, FrameError)

    class TimeStamp {
    public:
        constexpr TimeStamp(qint64 s = 0, qint64 usec = 0) noexcept : m_seconds(s), m_microSeconds(usec) {}
        static constexpr TimeStamp fromMicroSeconds(qint64 usec) noexcept {
            return TimeStamp(usec / 1000000, usec % 1000000);
        }
        constexpr qint64 seconds() const noexcept { return m_seconds; }
        constexpr qint64 microSeconds() const noexcept { return m_microSeconds; }

    private:
        qint64 m_seconds;
        qint64 m_microSeconds;
    };

    explicit QCanBusFrame(FrameType type = DataFrame) noexcept;
    explicit QCanBusFrame(FrameId identifier, const QByteArray &data);

    bool isValid() const noexcept;
    FrameType frameType() const noexcept { return m_type; }
    void setFrameType(FrameType newType) noexcept;
    bool hasExtendedFrameFormat() const noexcept { return m_extended; }
    void setExtendedFrameFormat(bool isExtended) noexcept { m_extended = isExtended; }
    void setFrameId(FrameId newFrameId);
    FrameId frameId() const noexcept;
    void setPayload(const QByteArray &data);
    QByteArray payload() const { return m_payload; }
    void setTimeStamp(TimeStamp ts) noexcept { m_timeStamp = ts; }
    TimeStamp timeStamp() const noexcept { return m_timeStamp; }
    FrameErrors error() const noexcept;
    void setError(FrameErrors e);
    bool hasFlexibleDataRateFormat() const noexcept { return m_flexibleDataRate; }
    void setFlexibleDataRateFormat(bool isFlexibleData) noexcept;
    bool hasBitrateSwitch() const noexcept { return m_bitrateSwitch; }
    void setBitrateSwitch(bool bitrateSwitch) noexcept;
    bool hasErrorStateIndicator() const noexcept { return m_errorStateIndicator; }
    void setErrorStateIndicator(bool errorStateIndicator) noexcept;
    bool hasLocalEcho() const noexcept { return m_localEcho; }
    void setLocalEcho(bool localEcho) noexcept { m_localEcho = localEcho; }
    QString toString() const;

private:
    FrameId m_id = 0;
    FrameType m_type = DataFrame;
    QByteArray m_payload;
    TimeStamp m_timeStamp;
    bool m_extended = false;
    bool m_flexibleDataRate = false;
    bool m_bitrateSwitch = false;
    bool m_errorStateIndicator = false;
    bool m_localEcho = false;
    bool m_invalidId = false;

    friend QDataStream &operator<<(QDataStream &out, const QCanBusFrame &frame);
    friend QDataStream &operator>>(QDataStream &in, QCanBusFrame &frame);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QCanBusFrame::FrameErrors)
Q_DECLARE_METATYPE(QCanBusFrame)
Q_DECLARE_METATYPE(QCanBusFrame::FrameErrors)

QDataStream &operator<<(QDataStream &out, const QCanBusFrame &frame);
QDataStream &operator>>(QDataStream &in, QCanBusFrame &frame);
