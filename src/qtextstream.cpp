// MIT License - see LICENSE
#include <QtCore/qbuffer.h>
#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>

#include "numparse_p.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

/*
    Differences from Qt:
    - Text on devices and byte arrays is always UTF-8 (a leading BOM is not detected and no BOM is
      written); there is no setEncoding()/setAutoDetectUnicode()/setLocale(). Numbers always use the
      "C" locale: '.' as decimal point and no group separators.
    - Line breaks are never translated; readLine() splits on '\n' and strips one trailing '\r'.
    - Output to a QString* always appends; the string is not a seekable output device.
    - pos() on a device is derived from the device position minus the UTF-8 size of the text that
      was read ahead, which is exact for valid UTF-8 input.
    - Reading a number reads the longest valid numeric prefix ("12abc" yields 12 and leaves "abc").
*/

namespace {

constexpr qsizetype kWriteBufferLimit = 16384;
constexpr qsizetype kReadChunk = 4096;

// A QIODevice over a stdio stream; QTextStream(FILE *) uses it.
class StdioDevice : public QIODevice {
public:
    explicit StdioDevice(FILE *f) : m_file(f), m_seekable(std::ftell(f) != -1) {}

    bool isSequential() const override { return !m_seekable; }
    qint64 pos() const override { return m_seekable ? static_cast<qint64>(std::ftell(m_file)) : 0; }
    bool seek(qint64 p) override {
        QIODevice::seek(p);
        return m_seekable && std::fseek(m_file, static_cast<long>(p), SEEK_SET) == 0;
    }
    void flushFile() { std::fflush(m_file); }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        if (m_seekable)
            return static_cast<qint64>(std::fread(data, 1, static_cast<std::size_t>(maxSize), m_file));
        // Terminals and pipes: return at the end of a line instead of waiting for a full buffer.
        qint64 n = 0;
        while (n < maxSize) {
            const int c = std::getc(m_file);
            if (c == EOF)
                break;
            data[n++] = static_cast<char>(c);
            if (c == '\n')
                break;
        }
        return n;
    }
    qint64 writeData(const char *data, qint64 maxSize) override {
        return static_cast<qint64>(std::fwrite(data, 1, static_cast<std::size_t>(maxSize), m_file));
    }

private:
    FILE *m_file;
    bool m_seekable;
};

QString emptyString() { return QString(qsizetype(0), QChar()); }

// Number of leading bytes of `b` that do not end in a truncated UTF-8 sequence.
qsizetype completeUtf8Prefix(const QByteArray &b) {
    const qsizetype n = b.size();
    qsizetype i = n;
    int back = 0;
    while (i > 0 && back < 3 && (static_cast<uchar>(b[i - 1]) & 0xC0) == 0x80) {
        --i;
        ++back;
    }
    if (i == 0)
        return n;
    const uchar lead = static_cast<uchar>(b[i - 1]);
    const int need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    return need > back + 1 ? i - 1 : n;
}

int digitValue(QChar c) {
    const char16_t u = c.unicode();
    if (u >= u'0' && u <= u'9')
        return u - u'0';
    if (u >= u'a' && u <= u'z')
        return u - u'a' + 10;
    if (u >= u'A' && u <= u'Z')
        return u - u'A' + 10;
    return -1;
}

int effectiveBase(int base) { return base == 0 || (base >= 2 && base <= 36) ? base : 10; }

// Parses sign, optional base prefix and digits; false on garbage or overflow.
bool parseMagnitude(const QString &tok, int configuredBase, bool *negative, qulonglong *magnitude) {
    const int cfg = effectiveBase(configuredBase);
    qsizetype i = 0;
    *negative = false;
    if (i < tok.size() && (tok[i] == u'-' || tok[i] == u'+')) {
        *negative = tok[i] == u'-';
        ++i;
    }
    int base = cfg == 0 ? 10 : cfg;
    if (tok.size() > i + 2 && tok[i] == u'0') {
        const char16_t x = static_cast<char16_t>(tok[i + 1].unicode() | 0x20);
        if (x == u'x' && (cfg == 0 || cfg == 16)) {
            base = 16;
            i += 2;
        } else if (x == u'b' && (cfg == 0 || cfg == 2)) {
            base = 2;
            i += 2;
        }
    }
    if (cfg == 0 && base == 10 && tok.size() > i + 1 && tok[i] == u'0')
        base = 8;
    if (i >= tok.size())
        return false;
    qulonglong v = 0;
    for (; i < tok.size(); ++i) {
        const int d = digitValue(tok[i]);
        if (d < 0 || d >= base)
            return false;
        if (v > (std::numeric_limits<qulonglong>::max() - static_cast<qulonglong>(d)) / static_cast<qulonglong>(base))
            return false;
        v = v * static_cast<qulonglong>(base) + static_cast<qulonglong>(d);
    }
    *magnitude = v;
    return true;
}

QString formatReal(double v, char fmt, int precision, bool forcePoint) {
    if (precision < 0)
        precision = 6;
    const char spec[] = {'%', '.', '*', fmt, '\0'};
    const int len = std::snprintf(nullptr, 0, spec, precision, v);
    std::vector<char> buf(static_cast<std::size_t>(len < 0 ? 0 : len) + 1);
    std::snprintf(buf.data(), buf.size(), spec, precision, v);
    QString s = QString::fromLatin1(buf.data(), len < 0 ? 0 : len);
    if (forcePoint && !s.contains(QChar(u'.'))) {
        const qsizetype e = std::max(s.indexOf(QChar(u'e')), s.indexOf(QChar(u'E')));
        s.insert(e < 0 ? s.size() : e, QChar(u'.'));
    }
    return s;
}

} // namespace

QTextStream::QTextStream() = default;

QTextStream::QTextStream(QIODevice *device) : m_dev(device) {}

QTextStream::QTextStream(FILE *fileHandle, QIODeviceBase::OpenMode mode) {
    auto dev = std::make_unique<StdioDevice>(fileHandle);
    dev->open(mode);
    attachOwned(dev.release());
}

QTextStream::QTextStream(QString *string, QIODeviceBase::OpenMode mode) : m_str(string), m_strMode(mode) {}

QTextStream::QTextStream(QByteArray *array, QIODeviceBase::OpenMode mode) {
    auto buf = std::make_unique<QBuffer>(array);
    buf->open(mode);
    attachOwned(buf.release());
}

QTextStream::QTextStream(const QByteArray &array, QIODeviceBase::OpenMode mode) {
    auto buf = std::make_unique<QBuffer>();
    buf->setData(array);
    buf->open(mode);
    attachOwned(buf.release());
}

QTextStream::~QTextStream() { flush(); }

void QTextStream::attachOwned(QIODevice *d) {
    m_owned.reset(d);
    m_dev = d;
}

void QTextStream::setDevice(QIODevice *device) {
    flush();
    m_owned.reset();
    m_dev = device;
    m_str = nullptr;
    m_read.clear();
    m_readPos = 0;
    m_partial.clear();
}

void QTextStream::setString(QString *string, QIODeviceBase::OpenMode mode) {
    flush();
    m_owned.reset();
    m_dev = nullptr;
    m_str = string;
    m_strMode = mode;
    m_read.clear();
    m_readPos = 0;
    m_partial.clear();
}

void QTextStream::reset() {
    m_align = AlignRight;
    m_pad = QChar(u' ');
    m_width = 0;
    m_flags = NumberFlags();
    m_base = 10;
    m_notation = SmartNotation;
    m_precision = 6;
}

void QTextStream::flush() {
    flushWrite();
    if (!m_dev)
        return;
    if (auto *f = dynamic_cast<QFileDevice *>(m_dev))
        f->flush();
    else if (auto *s = dynamic_cast<StdioDevice *>(m_dev))
        s->flushFile();
}

void QTextStream::flushWrite() const {
    if (m_write.isEmpty())
        return;
    if (!m_dev) {
        m_write.clear();
        return;
    }
    const bool readAhead = m_readPos < m_read.size() || !m_partial.isEmpty();
    if (readAhead && !m_dev->isSequential()) {
        // Writes go where the application logically is, not where the read-ahead ended.
        const qint64 logical = pos();
        m_read.clear();
        m_readPos = 0;
        m_partial.clear();
        m_dev->seek(logical);
    }
    const qint64 n = m_dev->write(m_write.constData(), m_write.size());
    if (n != m_write.size() && m_status == Ok)
        m_status = WriteFailed;
    m_write.clear();
}

bool QTextStream::fill() const {
    if (!m_dev || !m_dev->isReadable())
        return false;
    flushWrite();
    if (m_readPos > 0) {
        m_read.remove(0, m_readPos);
        m_readPos = 0;
    }
    char chunk[kReadChunk];
    const qint64 n = m_dev->read(chunk, sizeof chunk);
    if (n <= 0) {
        if (m_partial.isEmpty())
            return false;
        m_read.append(QString::fromUtf8(m_partial));
        m_partial.clear();
        return true;
    }
    QByteArray bytes = m_partial;
    bytes.append(chunk, n);
    const qsizetype complete = completeUtf8Prefix(bytes);
    m_partial = bytes.mid(complete);
    m_read.append(QString::fromUtf8(bytes.constData(), complete));
    return true;
}

bool QTextStream::available(qsizetype n) const {
    if (m_str)
        return (m_strMode & QIODeviceBase::ReadOnly) && m_readPos + n <= m_str->size();
    while (m_read.size() - m_readPos < n)
        if (!fill())
            return false;
    return true;
}

bool QTextStream::atEnd() const {
    if (m_str)
        return !((m_strMode & QIODeviceBase::ReadOnly) && m_readPos < m_str->size());
    return !available(1);
}

qint64 QTextStream::pos() const {
    if (m_str)
        return m_readPos;
    if (!m_dev)
        return 0;
    const qint64 pending = m_write.size();
    const QString unread = m_read.mid(m_readPos);
    return m_dev->pos() - unread.toUtf8().size() - m_partial.size() + pending;
}

bool QTextStream::seek(qint64 position) {
    if (m_str) {
        if (position < 0 || position > m_str->size())
            return false;
        m_readPos = static_cast<qsizetype>(position);
        return true;
    }
    if (!m_dev)
        return false;
    flushWrite();
    m_read.clear();
    m_readPos = 0;
    m_partial.clear();
    return m_dev->seek(position);
}

void QTextStream::skipWhiteSpace() {
    const QString &buf = m_str ? *m_str : m_read;
    while (available(1) && buf[m_readPos].isSpace())
        ++m_readPos;
}

QString QTextStream::nextToken() {
    skipWhiteSpace();
    if (!available(1))
        return QString();
    const QString &buf = m_str ? *m_str : m_read;
    qsizetype n = 0;
    while (available(n + 1) && !buf[m_readPos + n].isSpace())
        ++n;
    const QString tok = buf.mid(m_readPos, n);
    m_readPos += n;
    return tok;
}

QString QTextStream::scanInteger() {
    skipWhiteSpace();
    if (!available(1))
        return QString();
    const QString &buf = m_str ? *m_str : m_read;
    const int cfg = effectiveBase(m_base);
    qsizetype n = 0;
    if (buf[m_readPos] == u'-' || buf[m_readPos] == u'+')
        n = 1;
    int base = cfg == 0 ? 10 : cfg;
    if (available(n + 3) && buf[m_readPos + n] == u'0') {
        const char16_t x = static_cast<char16_t>(buf[m_readPos + n + 1].unicode() | 0x20);
        int prefixBase = 0;
        if (x == u'x' && (cfg == 0 || cfg == 16))
            prefixBase = 16;
        else if (x == u'b' && (cfg == 0 || cfg == 2))
            prefixBase = 2;
        const int d = digitValue(buf[m_readPos + n + 2]);
        if (prefixBase && d >= 0 && d < prefixBase) {
            base = prefixBase;
            n += 2;
        }
    }
    if (cfg == 0 && base == 10 && available(n + 2) && buf[m_readPos + n] == u'0') {
        const int d = digitValue(buf[m_readPos + n + 1]);
        if (d >= 0 && d < 8)
            base = 8;
    }
    const qsizetype digitsStart = n;
    while (available(n + 1)) {
        const int d = digitValue(buf[m_readPos + n]);
        if (d < 0 || d >= base)
            break;
        ++n;
    }
    if (n == digitsStart)
        return emptyString();
    const QString tok = buf.mid(m_readPos, n);
    m_readPos += n;
    return tok;
}

QString QTextStream::scanReal() {
    skipWhiteSpace();
    if (!available(1))
        return QString();
    const QString &buf = m_str ? *m_str : m_read;
    auto at = [&](qsizetype i) { return available(i + 1) ? buf[m_readPos + i] : QChar(); };
    auto isDigit = [](QChar c) { return c.unicode() >= u'0' && c.unicode() <= u'9'; };
    auto lower = [](QChar c) { return static_cast<char16_t>(c.unicode() | 0x20); };

    qsizetype n = 0;
    if (at(0) == u'-' || at(0) == u'+')
        n = 1;
    qsizetype end = -1;
    const char16_t first = lower(at(n));
    if (first == u'i' || first == u'n') {
        const char16_t *word = first == u'i' ? u"inf" : u"nan";
        bool match = true;
        for (qsizetype i = 0; i < 3; ++i)
            match = match && lower(at(n + i)) == word[i];
        if (match) {
            end = n + 3;
            if (first == u'i') {
                const char16_t *rest = u"inity";
                bool full = true;
                for (qsizetype i = 0; i < 5; ++i)
                    full = full && lower(at(end + i)) == rest[i];
                if (full)
                    end += 5;
            }
        }
    } else {
        qsizetype digits = 0;
        while (isDigit(at(n))) {
            ++n;
            ++digits;
        }
        if (at(n) == u'.') {
            ++n;
            while (isDigit(at(n))) {
                ++n;
                ++digits;
            }
        }
        if (digits > 0) {
            end = n;
            if (lower(at(n)) == u'e') {
                qsizetype e = n + 1;
                if (at(e) == u'-' || at(e) == u'+')
                    ++e;
                if (isDigit(at(e))) {
                    while (isDigit(at(e)))
                        ++e;
                    end = e;
                }
            }
        }
    }
    if (end < 0)
        return emptyString();
    const QString tok = buf.mid(m_readPos, end);
    m_readPos += end;
    return tok;
}

qlonglong QTextStream::parseSigned(const QString &s, bool *ok) const {
    bool neg = false;
    qulonglong mag = 0;
    *ok = !s.isEmpty() && parseMagnitude(s, m_base, &neg, &mag);
    if (!*ok)
        return 0;
    constexpr qulonglong limit = static_cast<qulonglong>(std::numeric_limits<qlonglong>::max());
    if (neg) {
        *ok = mag <= limit + 1;
        return *ok ? static_cast<qlonglong>(0ULL - mag) : 0;
    }
    *ok = mag <= limit;
    return *ok ? static_cast<qlonglong>(mag) : 0;
}

qulonglong QTextStream::parseUnsigned(const QString &s, bool *ok) const {
    bool neg = false;
    qulonglong mag = 0;
    *ok = !s.isEmpty() && parseMagnitude(s, m_base, &neg, &mag) && !(neg && mag != 0);
    return *ok ? mag : 0;
}

QTextStream &QTextStream::operator>>(QChar &c) {
    skipWhiteSpace();
    if (!available(1)) {
        c = QChar();
        setStatus(ReadPastEnd);
        return *this;
    }
    c = (m_str ? *m_str : m_read)[m_readPos++];
    return *this;
}

QTextStream &QTextStream::operator>>(char &c) {
    QChar ch;
    *this >> ch;
    c = ch.toLatin1();
    return *this;
}

QTextStream &QTextStream::operator>>(double &f) {
    const QString tok = scanReal();
    f = 0;
    if (tok.isEmpty()) {
        setStatus(tok.isNull() ? ReadPastEnd : ReadCorruptData);
        return *this;
    }
    const std::string a = tok.toLatin1().toStdString();
    f = std::strtod(a.c_str(), nullptr);
    return *this;
}

QTextStream &QTextStream::operator>>(float &f) {
    double d = 0;
    *this >> d;
    f = static_cast<float>(d);
    return *this;
}

QTextStream &QTextStream::operator>>(QString &s) {
    s = nextToken();
    if (s.isNull())
        setStatus(ReadPastEnd);
    return *this;
}

QTextStream &QTextStream::operator>>(QByteArray &array) {
    const QString tok = nextToken();
    if (tok.isNull()) {
        array.clear();
        setStatus(ReadPastEnd);
    } else {
        array = tok.toUtf8();
    }
    return *this;
}

bool QTextStream::readLineInto(QString *line, qint64 maxlen) {
    if (!available(1)) {
        if (line)
            line->clear();
        return false;
    }
    const QString &buf = m_str ? *m_str : m_read;
    qsizetype n = 0;
    bool found = false;
    while (maxlen <= 0 || n < maxlen) {
        if (!available(n + 1))
            break;
        if (buf[m_readPos + n] == u'\n') {
            found = true;
            break;
        }
        ++n;
    }
    QString text = n > 0 ? buf.mid(m_readPos, n) : emptyString();
    m_readPos += n + (found ? 1 : 0);
    if (found && text.endsWith(QChar(u'\r')))
        text.chop(1);
    if (line)
        *line = text;
    return true;
}

QString QTextStream::readLine(qint64 maxlen) {
    QString line;
    if (!readLineInto(&line, maxlen))
        return QString();
    return line;
}

QString QTextStream::readAll() {
    QString r;
    if (m_str) {
        if ((m_strMode & QIODeviceBase::ReadOnly) && m_readPos < m_str->size())
            r = m_str->mid(m_readPos);
        m_readPos = std::max(m_readPos, m_str->size());
    } else {
        while (fill()) {
        }
        r = m_read.mid(m_readPos);
        m_read.clear();
        m_readPos = 0;
    }
    return r.isNull() ? emptyString() : r;
}

QString QTextStream::read(qint64 maxlen) {
    if (maxlen <= 0)
        return emptyString();
    available(static_cast<qsizetype>(maxlen));
    const QString &buf = m_str ? *m_str : m_read;
    const qsizetype have = std::max<qsizetype>(0, buf.size() - m_readPos);
    const qsizetype n = std::min<qsizetype>(have, static_cast<qsizetype>(maxlen));
    QString r = n > 0 ? buf.mid(m_readPos, n) : emptyString();
    m_readPos += n;
    return r;
}

QString QTextStream::padded(const QString &sign, const QString &body) const {
    const qsizetype len = sign.size() + body.size();
    const qsizetype pad = m_width > len ? m_width - len : 0;
    if (pad == 0)
        return sign + body;
    switch (m_align) {
    case AlignLeft:
        return sign + body + QString(pad, m_pad);
    case AlignCenter:
        return QString(pad / 2, m_pad) + sign + body + QString(pad - pad / 2, m_pad);
    case AlignAccountingStyle:
        return sign + QString(pad, m_pad) + body;
    case AlignRight:
        break;
    }
    return QString(pad, m_pad) + sign + body;
}

void QTextStream::rawPut(const QString &text) {
    if (m_str) {
        if (m_strMode & QIODeviceBase::WriteOnly)
            m_str->append(text);
        return;
    }
    if (!m_dev)
        return;
    m_write.append(text.toUtf8());
    if (m_write.size() >= kWriteBufferLimit)
        flushWrite();
}

void QTextStream::put(const QString &field) { rawPut(padded(QString(), field)); }

QTextStream &QTextStream::writeInteger(bool negative, qulonglong magnitude) {
    const int base = m_base >= 2 && m_base <= 36 ? m_base : 10;
    QString digits = QString::fromLatin1(QtPrivate::intToString(magnitude, false, base).c_str());
    if (m_flags & UppercaseDigits)
        digits = digits.toUpper();
    QString prefix;
    if (negative)
        prefix = QString(QChar(u'-'));
    else if (m_flags & ForceSign)
        prefix = QString(QChar(u'+'));
    if (m_flags & ShowBase) {
        const bool upper = m_flags & UppercaseBase;
        if (base == 16)
            prefix += upper ? QString::fromLatin1("0X") : QString::fromLatin1("0x");
        else if (base == 2)
            prefix += upper ? QString::fromLatin1("0B") : QString::fromLatin1("0b");
        else if (base == 8)
            prefix += QString(QChar(u'0'));
    }
    rawPut(padded(prefix, digits));
    return *this;
}

QTextStream &QTextStream::operator<<(QChar c) {
    put(QString(c));
    return *this;
}

QTextStream &QTextStream::operator<<(const QString &s) {
    put(s);
    return *this;
}

QTextStream &QTextStream::operator<<(double f) {
    const bool upper = m_flags & UppercaseBase;
    QString sign;
    QString body;
    if (std::isnan(f)) {
        body = QString::fromLatin1(upper ? "NAN" : "nan");
    } else {
        if (std::signbit(f))
            sign = QString(QChar(u'-'));
        else if (m_flags & ForceSign)
            sign = QString(QChar(u'+'));
        const double mag = std::fabs(f);
        if (std::isinf(mag)) {
            body = QString::fromLatin1(upper ? "INF" : "inf");
        } else {
            char fmt = 'g';
            if (m_notation == FixedNotation)
                fmt = 'f';
            else if (m_notation == ScientificNotation)
                fmt = 'e';
            if (upper)
                fmt = static_cast<char>(fmt - 32);
            body = formatReal(mag, fmt, m_precision, m_flags & ForcePoint);
        }
    }
    rawPut(padded(sign, body));
    return *this;
}

QTextStream &QTextStream::operator<<(const void *ptr) {
    const NumberFlags oldFlags = m_flags;
    const int oldBase = m_base;
    m_flags |= ShowBase;
    m_base = 16;
    writeInteger(false, static_cast<qulonglong>(reinterpret_cast<quintptr>(ptr)));
    m_flags = oldFlags;
    m_base = oldBase;
    return *this;
}

namespace Qt {

QTextStream &bin(QTextStream &s) { s.setIntegerBase(2); return s; }
QTextStream &oct(QTextStream &s) { s.setIntegerBase(8); return s; }
QTextStream &dec(QTextStream &s) { s.setIntegerBase(10); return s; }
QTextStream &hex(QTextStream &s) { s.setIntegerBase(16); return s; }

QTextStream &showbase(QTextStream &s) { s.setNumberFlags(s.numberFlags() | QTextStream::ShowBase); return s; }
QTextStream &noshowbase(QTextStream &s) { s.setNumberFlags(s.numberFlags() & ~QTextStream::NumberFlags(QTextStream::ShowBase)); return s; }
QTextStream &forcesign(QTextStream &s) { s.setNumberFlags(s.numberFlags() | QTextStream::ForceSign); return s; }
QTextStream &noforcesign(QTextStream &s) { s.setNumberFlags(s.numberFlags() & ~QTextStream::NumberFlags(QTextStream::ForceSign)); return s; }
QTextStream &forcepoint(QTextStream &s) { s.setNumberFlags(s.numberFlags() | QTextStream::ForcePoint); return s; }
QTextStream &noforcepoint(QTextStream &s) { s.setNumberFlags(s.numberFlags() & ~QTextStream::NumberFlags(QTextStream::ForcePoint)); return s; }
QTextStream &uppercasebase(QTextStream &s) { s.setNumberFlags(s.numberFlags() | QTextStream::UppercaseBase); return s; }
QTextStream &lowercasebase(QTextStream &s) { s.setNumberFlags(s.numberFlags() & ~QTextStream::NumberFlags(QTextStream::UppercaseBase)); return s; }
QTextStream &uppercasedigits(QTextStream &s) { s.setNumberFlags(s.numberFlags() | QTextStream::UppercaseDigits); return s; }
QTextStream &lowercasedigits(QTextStream &s) { s.setNumberFlags(s.numberFlags() & ~QTextStream::NumberFlags(QTextStream::UppercaseDigits)); return s; }

QTextStream &fixed(QTextStream &s) { s.setRealNumberNotation(QTextStream::FixedNotation); return s; }
QTextStream &scientific(QTextStream &s) { s.setRealNumberNotation(QTextStream::ScientificNotation); return s; }

QTextStream &left(QTextStream &s) { s.setFieldAlignment(QTextStream::AlignLeft); return s; }
QTextStream &right(QTextStream &s) { s.setFieldAlignment(QTextStream::AlignRight); return s; }
QTextStream &center(QTextStream &s) { s.setFieldAlignment(QTextStream::AlignCenter); return s; }

QTextStream &endl(QTextStream &s) {
    s << QChar(u'\n');
    s.flush();
    return s;
}
QTextStream &flush(QTextStream &s) { s.flush(); return s; }
QTextStream &reset(QTextStream &s) { s.reset(); return s; }
QTextStream &ws(QTextStream &s) { s.skipWhiteSpace(); return s; }

} // namespace Qt
