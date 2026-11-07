// MIT License - see LICENSE
#include <QtCore/qversionnumber.h>

QVersionNumber QVersionNumber::normalized() const {
    QList<int> s = m;
    while (!s.isEmpty() && s.last() == 0)
        s.removeLast();
    return QVersionNumber(s);
}

bool QVersionNumber::isPrefixOf(const QVersionNumber &other) const noexcept {
    if (segmentCount() > other.segmentCount())
        return false;
    for (qsizetype i = 0; i < segmentCount(); ++i)
        if (m.at(i) != other.m.at(i))
            return false;
    return true;
}

QString QVersionNumber::toString() const {
    QString r;
    for (qsizetype i = 0; i < m.size(); ++i) {
        if (i)
            r.append(QChar(u'.'));
        r.append(QString::number(m.at(i)));
    }
    return r;
}

int QVersionNumber::compare(const QVersionNumber &a, const QVersionNumber &b) noexcept {
    const qsizetype common = qMin(a.segmentCount(), b.segmentCount());
    for (qsizetype i = 0; i < common; ++i) {
        const int d = a.m.at(i) - b.m.at(i);
        if (d != 0)
            return d;
    }
    // Extra zero segments compare equal to an absent segment only in value:
    // the longer version is greater unless its next segment is negative.
    if (a.segmentCount() == b.segmentCount())
        return 0;
    const QVersionNumber &longer = a.segmentCount() > b.segmentCount() ? a : b;
    const int sign = a.segmentCount() > b.segmentCount() ? 1 : -1;
    return longer.m.at(common) < 0 ? -sign : sign;
}

QVersionNumber QVersionNumber::commonPrefix(const QVersionNumber &a, const QVersionNumber &b) {
    QList<int> s;
    const qsizetype common = qMin(a.segmentCount(), b.segmentCount());
    for (qsizetype i = 0; i < common && a.m.at(i) == b.m.at(i); ++i)
        s.append(a.m.at(i));
    return QVersionNumber(s);
}

QVersionNumber QVersionNumber::fromString(QStringView string, qsizetype *suffixIndex) {
    QList<int> seg;
    qsizetype i = 0, consumed = 0;
    const qsizetype n = string.size();
    while (i < n && string[i].isDigit()) {
        qlonglong v = 0;
        bool overflow = false;
        qsizetype start = i;
        while (i < n && string[i].isDigit()) {
            v = v * 10 + (string[i].unicode() - u'0');
            if (v > std::numeric_limits<int>::max()) {
                overflow = true;
                break;
            }
            ++i;
        }
        if (overflow) {
            i = start;
            break;
        }
        seg.append(static_cast<int>(v));
        consumed = i;
        if (i + 1 < n && string[i] == QChar(u'.') && string[i + 1].isDigit())
            ++i;
        else
            break;
    }
    if (suffixIndex)
        *suffixIndex = consumed;
    return QVersionNumber(seg);
}
