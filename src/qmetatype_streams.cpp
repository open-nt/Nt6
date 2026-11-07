// MIT License - see LICENSE
// QDataStream and QDebug operators for the value types that QVariant stores.
#include <QtCore/qbitarray.h>
#include <QtCore/qdatastream.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qline.h>
#include <QtCore/qmargins.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qurl.h>
#include <QtCore/quuid.h>
#include <QtCore/qversionnumber.h>

#include <limits>

// QDataStream

QDataStream &operator<<(QDataStream &out, const QDate &date) { return out << static_cast<qint64>(date.toJulianDay()); }

QDataStream &operator>>(QDataStream &in, QDate &date) {
    qint64 jd = 0;
    in >> jd;
    date = jd == std::numeric_limits<qint64>::min() ? QDate() : QDate::fromJulianDay(jd);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QTime &time) {
    return out << (time.isNull() ? 0xffffffffu : static_cast<quint32>(time.msecsSinceStartOfDay()));
}

QDataStream &operator>>(QDataStream &in, QTime &time) {
    quint32 ms = 0;
    in >> ms;
    time = ms == 0xffffffffu ? QTime() : QTime::fromMSecsSinceStartOfDay(static_cast<int>(ms));
    return in;
}

QDataStream &operator<<(QDataStream &out, const QDateTime &dateTime) {
    out << dateTime.date() << dateTime.time();
    out << static_cast<qint8>(dateTime.timeSpec());
    if (dateTime.timeSpec() == Qt::OffsetFromUTC)
        out << static_cast<qint32>(dateTime.offsetFromUtc());
    return out;
}

QDataStream &operator>>(QDataStream &in, QDateTime &dateTime) {
    QDate date;
    QTime time;
    qint8 spec = 0;
    in >> date >> time >> spec;
    qint32 offset = 0;
    if (spec == Qt::OffsetFromUTC)
        in >> offset;
    if (in.status() != QDataStream::Ok)
        return in;
    if (spec != Qt::LocalTime && spec != Qt::UTC && spec != Qt::OffsetFromUTC) {
        in.setStatus(QDataStream::ReadCorruptData);
        return in;
    }
    dateTime = date.isValid() ? QDateTime(date, time, static_cast<Qt::TimeSpec>(spec), offset) : QDateTime();
    return in;
}

QDataStream &operator<<(QDataStream &out, const QUrl &url) { return out << url.toEncoded(); }

QDataStream &operator>>(QDataStream &in, QUrl &url) {
    QByteArray encoded;
    in >> encoded;
    url = QUrl::fromEncoded(encoded, QUrl::StrictMode);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QUuid &uuid) {
    out << static_cast<quint32>(uuid.data1) << static_cast<quint16>(uuid.data2) << static_cast<quint16>(uuid.data3);
    for (unsigned char b : uuid.data4)
        out << static_cast<quint8>(b);
    return out;
}

QDataStream &operator>>(QDataStream &in, QUuid &uuid) {
    QUuid u;
    quint32 d1 = 0;
    quint16 d2 = 0;
    quint16 d3 = 0;
    in >> d1 >> d2 >> d3;
    u.data1 = d1;
    u.data2 = d2;
    u.data3 = d3;
    for (unsigned char &b : u.data4) {
        quint8 v = 0;
        in >> v;
        b = v;
    }
    if (in.status() == QDataStream::Ok)
        uuid = u;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QVersionNumber &version) { return out << version.segments(); }

QDataStream &operator>>(QDataStream &in, QVersionNumber &version) {
    QList<int> segments;
    in >> segments;
    if (in.status() == QDataStream::Ok)
        version = QVersionNumber(segments);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QBitArray &bits) {
    const qsizetype n = bits.size();
    out << static_cast<quint32>(n);
    for (qsizetype i = 0; i < n; i += 8) {
        quint8 byte = 0;
        for (qsizetype j = 0; j < 8 && i + j < n; ++j)
            if (bits.testBit(i + j))
                byte = static_cast<quint8>(byte | (1u << j));
        out << byte;
    }
    return out;
}

QDataStream &operator>>(QDataStream &in, QBitArray &bits) {
    quint32 count = 0;
    in >> count;
    if (in.status() != QDataStream::Ok)
        return in;
    const qsizetype n = static_cast<qsizetype>(count);
    QBitArray result(n);
    for (qsizetype i = 0; i < n; i += 8) {
        quint8 byte = 0;
        in >> byte;
        if (in.status() != QDataStream::Ok)
            return in;
        for (qsizetype j = 0; j < 8 && i + j < n; ++j)
            result.setBit(i + j, ((static_cast<unsigned>(byte) >> j) & 1u) != 0);
    }
    bits = result;
    return in;
}

QDataStream &operator<<(QDataStream &out, const QPoint &p) {
    return out << static_cast<qint32>(p.x()) << static_cast<qint32>(p.y());
}

QDataStream &operator>>(QDataStream &in, QPoint &p) {
    qint32 x = 0;
    qint32 y = 0;
    in >> x >> y;
    p = QPoint(x, y);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QPointF &p) { return out << double(p.x()) << double(p.y()); }

QDataStream &operator>>(QDataStream &in, QPointF &p) {
    double x = 0;
    double y = 0;
    in >> x >> y;
    p = QPointF(x, y);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QSize &s) {
    return out << static_cast<qint32>(s.width()) << static_cast<qint32>(s.height());
}

QDataStream &operator>>(QDataStream &in, QSize &s) {
    qint32 w = 0;
    qint32 h = 0;
    in >> w >> h;
    s = QSize(w, h);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QSizeF &s) { return out << double(s.width()) << double(s.height()); }

QDataStream &operator>>(QDataStream &in, QSizeF &s) {
    double w = 0;
    double h = 0;
    in >> w >> h;
    s = QSizeF(w, h);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QLine &l) { return out << l.p1() << l.p2(); }

QDataStream &operator>>(QDataStream &in, QLine &l) {
    QPoint a;
    QPoint b;
    in >> a >> b;
    l = QLine(a, b);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QLineF &l) { return out << l.p1() << l.p2(); }

QDataStream &operator>>(QDataStream &in, QLineF &l) {
    QPointF a;
    QPointF b;
    in >> a >> b;
    l = QLineF(a, b);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QRect &r) {
    return out << static_cast<qint32>(r.left()) << static_cast<qint32>(r.top()) << static_cast<qint32>(r.right())
               << static_cast<qint32>(r.bottom());
}

QDataStream &operator>>(QDataStream &in, QRect &r) {
    qint32 x1 = 0;
    qint32 y1 = 0;
    qint32 x2 = 0;
    qint32 y2 = 0;
    in >> x1 >> y1 >> x2 >> y2;
    r = QRect(QPoint(x1, y1), QPoint(x2, y2));
    return in;
}

QDataStream &operator<<(QDataStream &out, const QRectF &r) {
    return out << double(r.x()) << double(r.y()) << double(r.width()) << double(r.height());
}

QDataStream &operator>>(QDataStream &in, QRectF &r) {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
    in >> x >> y >> w >> h;
    r = QRectF(x, y, w, h);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QMargins &m) {
    return out << static_cast<qint32>(m.left()) << static_cast<qint32>(m.top()) << static_cast<qint32>(m.right())
               << static_cast<qint32>(m.bottom());
}

QDataStream &operator>>(QDataStream &in, QMargins &m) {
    qint32 l = 0;
    qint32 t = 0;
    qint32 r = 0;
    qint32 b = 0;
    in >> l >> t >> r >> b;
    m = QMargins(l, t, r, b);
    return in;
}

QDataStream &operator<<(QDataStream &out, const QMarginsF &m) {
    return out << double(m.left()) << double(m.top()) << double(m.right()) << double(m.bottom());
}

QDataStream &operator>>(QDataStream &in, QMarginsF &m) {
    double l = 0;
    double t = 0;
    double r = 0;
    double b = 0;
    in >> l >> t >> r >> b;
    m = QMarginsF(l, t, r, b);
    return in;
}

// QDebug

namespace {

// Writes what `f` writes without spaces between the parts, then restores the caller's setting.
template <typename F> QDebug write(QDebug &d, F f) {
    const bool spaces = d.autoInsertSpaces();
    d.nospace();
    f(d);
    d.setAutoInsertSpaces(spaces);
    return d.maybeSpace();
}

std::string quoted(const QString &s) { return "\"" + s.toStdString() + "\""; }

std::string timeText(const QTime &t) {
    return t.isValid() ? t.toString(QStringLiteral("HH:mm:ss.zzz")).toStdString() : std::string();
}

} // namespace

QDebug operator<<(QDebug d, const QDate &date) {
    return write(
        d, [&](QDebug &o) { o << "QDate(" << quoted(date.isValid() ? date.toString(Qt::ISODate) : QString()) << ")"; });
}

QDebug operator<<(QDebug d, const QTime &time) {
    return write(d, [&](QDebug &o) { o << "QTime(\"" << timeText(time) << "\")"; });
}

QDebug operator<<(QDebug d, const QDateTime &dateTime) {
    return write(d, [&](QDebug &o) {
        o << "QDateTime(";
        if (!dateTime.isValid()) {
            o << "Invalid";
        } else {
            o << dateTime.date().toString(Qt::ISODate).toStdString() << " " << timeText(dateTime.time());
            switch (dateTime.timeSpec()) {
            case Qt::UTC:
                o << " UTC Qt::UTC";
                break;
            case Qt::OffsetFromUTC: {
                const int offset = dateTime.offsetFromUtc();
                const int abs = offset < 0 ? -offset : offset;
                o << " UTC" << (offset < 0 ? "-" : "+") << QString::number(abs / 3600).toStdString() << ":"
                  << QString::number(abs / 60 % 60).rightJustified(2, QLatin1Char('0')).toStdString()
                  << " Qt::OffsetFromUTC";
                break;
            }
            default:
                o << " Qt::LocalTime";
                break;
            }
        }
        o << ")";
    });
}

QDebug operator<<(QDebug d, const QUrl &url) {
    return write(d, [&](QDebug &o) { o << "QUrl(" << quoted(url.toString()) << ")"; });
}

QDebug operator<<(QDebug d, const QUuid &uuid) {
    return write(d, [&](QDebug &o) { o << "QUuid(" << quoted(uuid.toString()) << ")"; });
}

QDebug operator<<(QDebug d, const QVersionNumber &version) {
    return write(d, [&](QDebug &o) { o << "QVersionNumber(" << version.toString().toStdString() << ")"; });
}

QDebug operator<<(QDebug d, const QBitArray &bits) {
    std::string text;
    for (qsizetype i = 0; i < bits.size(); ++i)
        text += bits.testBit(i) ? '1' : '0';
    return write(d, [&](QDebug &o) { o << "QBitArray(" << text << ")"; });
}

QDebug operator<<(QDebug d, const QPoint &p) {
    return write(d, [&](QDebug &o) { o << "QPoint(" << p.x() << "," << p.y() << ")"; });
}

QDebug operator<<(QDebug d, const QPointF &p) {
    return write(d, [&](QDebug &o) { o << "QPointF(" << p.x() << "," << p.y() << ")"; });
}

QDebug operator<<(QDebug d, const QSize &s) {
    return write(d, [&](QDebug &o) { o << "QSize(" << s.width() << "x" << s.height() << ")"; });
}

QDebug operator<<(QDebug d, const QSizeF &s) {
    return write(d, [&](QDebug &o) { o << "QSizeF(" << s.width() << "x" << s.height() << ")"; });
}

QDebug operator<<(QDebug d, const QLine &l) {
    return write(d, [&](QDebug &o) { o << "QLine(" << l.p1() << "," << l.p2() << ")"; });
}

QDebug operator<<(QDebug d, const QLineF &l) {
    return write(d, [&](QDebug &o) { o << "QLineF(" << l.p1() << "," << l.p2() << ")"; });
}

QDebug operator<<(QDebug d, const QRect &r) {
    return write(
        d, [&](QDebug &o) { o << "QRect(" << r.x() << "," << r.y() << " " << r.width() << "x" << r.height() << ")"; });
}

QDebug operator<<(QDebug d, const QRectF &r) {
    return write(
        d, [&](QDebug &o) { o << "QRectF(" << r.x() << "," << r.y() << " " << r.width() << "x" << r.height() << ")"; });
}

QDebug operator<<(QDebug d, const QMargins &m) {
    return write(d, [&](QDebug &o) {
        o << "QMargins(" << m.left() << ", " << m.top() << ", " << m.right() << ", " << m.bottom() << ")";
    });
}

QDebug operator<<(QDebug d, const QMarginsF &m) {
    return write(d, [&](QDebug &o) {
        o << "QMarginsF(" << m.left() << ", " << m.top() << ", " << m.right() << ", " << m.bottom() << ")";
    });
}
