// MIT License - see LICENSE
//
// NMEA 0183 sentence framing shared by the NMEA sources. Not part of the public API.
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>

namespace QtPositioningPrivate {

// Returns the comma-separated fields of the sentence of size bytes at data, the first being the
// talker and type ("GPGGA"), without the leading '$' and the checksum. Returns an empty list if
// the text is not a sentence or its checksum is wrong. A sentence without a checksum is accepted.
inline QList<QByteArray> nmeaFields(const char *data, int size) {
    QByteArray sentence = QByteArray(data, size).trimmed();
    if (sentence.size() < 6 || (sentence.at(0) != '$' && sentence.at(0) != '!'))
        return {};
    const qsizetype star = sentence.indexOf('*');
    if (star >= 0) {
        quint8 sum = 0;
        for (qsizetype i = 1; i < star; ++i)
            sum ^= static_cast<quint8>(sentence.at(i));
        bool ok = false;
        const uint given = sentence.mid(star + 1, 2).toUInt(&ok, 16);
        if (!ok || given != sum)
            return {};
        sentence.truncate(star);
    }
    QList<QByteArray> fields = sentence.mid(1).split(',');
    if (fields.first().size() < 5)
        return {};
    return fields;
}

} // namespace QtPositioningPrivate
