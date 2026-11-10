// MIT License - see LICENSE
#include <QtCore/qversionnumber.h>

/*!
    \class QVersionNumber
    \brief The QVersionNumber class contains a version number with an arbitrary number of segments.

    A version number such as \c 6.5.1 is stored as a list of integer
    segments. Versions compare segment by segment, and a missing trailing
    segment counts as 0, so \c 1.2 and \c 1.2.0 compare equal.

    \code
    QVersionNumber v = QVersionNumber::fromString("6.5.1-beta");
    v.majorVersion();     // 6
    v < QVersionNumber(6, 6); // true
    \endcode

    \differences
    The segments are always held in a QList<int>; there is no constructor
    taking a \c QSpan or a \c QVarLengthArray. A segment that does not fit in
    an \c int ends parsing in fromString().
    \enddifferences

    \sa QVersionNumber::compare()
*/

/*!
    Returns a copy of the version number without its trailing zero segments.
    For example \c 1.2.0.0 becomes \c 1.2.

    \sa isNormalized()
*/
QVersionNumber QVersionNumber::normalized() const {
    QList<int> s = m;
    while (!s.isEmpty() && s.last() == 0)
        s.removeLast();
    return QVersionNumber(s);
}

/*!
    Returns \c true if this version number is a prefix of \a other, which
    means all of its segments equal the first segments of \a other.

    \sa commonPrefix()
*/
bool QVersionNumber::isPrefixOf(const QVersionNumber &other) const noexcept {
    if (segmentCount() > other.segmentCount())
        return false;
    for (qsizetype i = 0; i < segmentCount(); ++i)
        if (m.at(i) != other.m.at(i))
            return false;
    return true;
}

/*!
    Returns the segments joined with dots, for example \c{"6.5.1"}. A null
    version number gives an empty string.

    \sa fromString()
*/
QString QVersionNumber::toString() const {
    QString r;
    for (qsizetype i = 0; i < m.size(); ++i) {
        if (i)
            r.append(QChar(u'.'));
        r.append(QString::number(m.at(i)));
    }
    return r;
}

/*!
    Compares \a a and \a b segment by segment, treating a missing trailing
    segment as 0. Returns a negative number if \a a is smaller, 0 if they are
    equal and a positive number if \a a is greater.
*/
int QVersionNumber::compare(const QVersionNumber &a, const QVersionNumber &b) noexcept {
    const qsizetype common = qMin(a.segmentCount(), b.segmentCount());
    for (qsizetype i = 0; i < common; ++i) {
        if (a.m.at(i) != b.m.at(i))
            return a.m.at(i) < b.m.at(i) ? -1 : 1;
    }
    // The shorter version is padded with zero segments.
    const bool aLonger = a.segmentCount() > b.segmentCount();
    const QVersionNumber &longer = aLonger ? a : b;
    for (qsizetype i = common; i < longer.segmentCount(); ++i) {
        const int s = longer.m.at(i);
        if (s != 0)
            return (s > 0) == aLonger ? 1 : -1;
    }
    return 0;
}

/*!
    Returns the longest version number that is a prefix of both \a a and
    \a b.

    \sa isPrefixOf()
*/
QVersionNumber QVersionNumber::commonPrefix(const QVersionNumber &a, const QVersionNumber &b) {
    QList<int> s;
    const qsizetype common = qMin(a.segmentCount(), b.segmentCount());
    for (qsizetype i = 0; i < common && a.m.at(i) == b.m.at(i); ++i)
        s.append(a.m.at(i));
    return QVersionNumber(s);
}

// Parses the leading version of string; the rest is described at QVersionNumber::fromString().
static QVersionNumber parseVersion(QStringView string, qsizetype *suffixIndex) {
    QList<int> seg;
    qsizetype i = 0, consumed = 0;
    const qsizetype n = string.size();
    while (i < n && QtPrivate::isAsciiDigit(string[i])) {
        qlonglong v = 0;
        bool overflow = false;
        qsizetype start = i;
        while (i < n && QtPrivate::isAsciiDigit(string[i])) {
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
        if (i + 1 < n && string[i] == QChar(u'.') && QtPrivate::isAsciiDigit(string[i + 1]))
            ++i;
        else
            break;
    }
    if (suffixIndex)
        *suffixIndex = consumed;
    return QVersionNumber(seg);
}

/*!
    Parses the leading dot-separated decimal numbers of \a string into a
    version number. Parsing stops at the first character that does not belong
    to the version, such as a suffix like \c{-beta}. If \a suffixIndex is not
    \c nullptr, it is set to the index in \a string where the version ends.
    Returns a null version number if \a string does not start with a digit.

    \a string can be a QString, a QStringView, a QLatin1StringView, or UTF-8
    text in a QByteArray or a \c{const char *}:

    \code
    qsizetype suffix = 0;
    const QVersionNumber v = QVersionNumber::fromString("6.5.1-beta", &suffix);
    // v is 6.5.1 and suffix is 5, the index of "-beta"
    \endcode

    \sa toString()
*/
QVersionNumber QVersionNumber::fromString(QAnyStringView string, qsizetype *suffixIndex) {
    // The version is ASCII, so an index into the UTF-16 text is the same index in any encoding.
    const QString utf16 = string.toString();
    return parseVersion(QStringView(utf16), suffixIndex);
}
