// MIT License - see LICENSE
#include "qcommsreliable_p.h"

#include "qcommsprotocol_p.h"

#include <algorithm>

namespace QtComms {

namespace {

// Fragments further ahead than this are dropped; the sender sends them again.
constexpr int kReceiveWindow = 1024;

} // namespace

bool ReliableChannel::send(const QByteArray &message, qint64 now, const Sender &sender) {
    if (message.size() > Protocol::kMaxMessage || m_failed)
        return false;
    qsizetype offset = 0;
    do {
        Pending p;
        p.fragment.seq = m_nextSeq++;
        p.fragment.data = message.mid(offset, Protocol::kMaxFragment);
        offset += p.fragment.data.size();
        p.fragment.more = offset < message.size();
        p.sentAt = now;
        p.sends = 1;
        m_pending.insert(p.fragment.seq, p);
        sender(p.fragment);
    } while (offset < message.size());
    return true;
}

QList<QByteArray> ReliableChannel::receive(const Fragment &fragment) {
    QList<QByteArray> delivered;
    const int distance = difference(fragment.seq, m_expected);
    if (distance < 0) {
        m_hasReceived = true; // a duplicate: the acknowledgement that follows covers it
        return delivered;
    }
    if (distance >= kReceiveWindow)
        return delivered; // too far ahead
    m_hasReceived = true;
    m_buffer.insert(fragment.seq, fragment);
    for (auto it = m_buffer.find(m_expected); it != m_buffer.end(); it = m_buffer.find(m_expected)) {
        m_partial.append(it->data);
        const bool more = it->more;
        m_buffer.erase(it);
        ++m_expected;
        if (m_partial.size() > Protocol::kMaxMessage) {
            m_partial.clear(); // a peer that sends more is broken; drop the message
            continue;
        }
        if (!more) {
            delivered.append(m_partial);
            m_partial.clear();
        }
    }
    return delivered;
}

quint32 ReliableChannel::ackBits() const {
    quint32 bits = 0;
    for (int i = 0; i < 32; ++i) {
        if (m_buffer.contains(static_cast<quint16>(m_expected + 1 + i)))
            bits |= 1u << i;
    }
    return bits;
}

void ReliableChannel::acknowledge(quint16 ack, quint32 bits, qint64 now) {
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        // ack is the next sequence the other side expects; bit i says ack + 1 + i has arrived too.
        const int ahead = difference(it.key(), ack);
        const bool acked = ahead < 0 || (ahead >= 1 && ahead <= 32 && (bits & (1u << (ahead - 1))) != 0);
        if (!acked) {
            ++it;
            continue;
        }
        if (it->sends == 1) {
            // Only packets sent once give an unambiguous round-trip time (Karn's rule).
            const auto sample = static_cast<double>(now - it->sentAt);
            m_srtt = m_srtt < 0.0 ? sample : m_srtt * 0.875 + sample * 0.125;
        }
        it = m_pending.erase(it);
    }
}

int ReliableChannel::retransmissionTimeout() const {
    const double base = m_srtt < 0.0 ? 100.0 : m_srtt;
    return std::clamp(static_cast<int>(base * 2.0) + 10, 50, 2000);
}

void ReliableChannel::resend(qint64 now, const Sender &sender) {
    const int rto = retransmissionTimeout();
    for (Pending &p : m_pending) {
        if (now - p.sentAt < static_cast<qint64>(rto) * std::min(p.sends, 4))
            continue;
        if (p.sends >= kMaxSends) {
            m_failed = true;
            return;
        }
        ++p.sends;
        p.sentAt = now;
        sender(p.fragment);
    }
}

} // namespace QtComms
