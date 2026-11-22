// MIT License - see LICENSE
//
// The reliable channel between two peers of an Nt session: selective repeat over UDP with a 16-bit sequence
// number, a cumulative acknowledgement (the next sequence expected) plus a 32-bit bitfield of the ones received
// after it,
// retransmission on a timeout derived from the round-trip time, in-order delivery and fragmentation of messages
// up to 64 KiB. It holds no socket: the session sends what it returns. Not part of the public API.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmap.h>

#include <functional>

namespace QtComms {

class ReliableChannel {
public:
    struct Fragment {
        quint16 seq = 0;
        bool more = false;
        QByteArray data;
    };
    using Sender = std::function<void(const Fragment &fragment)>;

    static constexpr int kMaxSends = 10;

    // Splits message into fragments, records them as unacknowledged and sends them. Returns false if the message
    // is too large.
    bool send(const QByteArray &message, qint64 now, const Sender &sender);
    // Takes one received fragment and returns the messages it completes, in order.
    QList<QByteArray> receive(const Fragment &fragment);
    // Takes an acknowledgement from the other side.
    void acknowledge(quint16 ack, quint32 bits, qint64 now);
    // Sends again what has waited longer than the retransmission timeout.
    void resend(qint64 now, const Sender &sender);

    bool hasReceived() const { return m_hasReceived; }
    quint16 ack() const { return m_expected; }
    quint32 ackBits() const;
    bool failed() const { return m_failed; }
    bool idle() const { return m_pending.isEmpty(); }
    int roundTripTime() const { return m_srtt < 0 ? -1 : static_cast<int>(m_srtt); }
    int retransmissionTimeout() const;

    static int difference(quint16 a, quint16 b) { return static_cast<qint16>(static_cast<quint16>(a - b)); }

private:
    struct Pending {
        Fragment fragment;
        qint64 sentAt = 0;
        int sends = 0;
    };

    quint16 m_nextSeq = 0;
    QMap<quint16, Pending> m_pending;
    bool m_failed = false;
    double m_srtt = -1.0;

    quint16 m_expected = 0;
    bool m_hasReceived = false;
    QMap<quint16, Fragment> m_buffer; // received out of order, by sequence number
    QByteArray m_partial;
};

} // namespace QtComms
