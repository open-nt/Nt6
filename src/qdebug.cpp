// MIT License - see LICENSE
#include <QtCore/qdebug.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlogging.h>

#include <cstdint>
#include <cstdio>

#include "qlogging_p.h"

namespace {

std::string escaped(const std::string &s) {
    std::string r;
    for (char c : s) {
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default: r.push_back(c);
        }
    }
    return r;
}

} // namespace

/*!
    \class QDebug
    \brief The QDebug class provides an output stream for debugging information.

    QDebug is used to write human-readable diagnostics. Obtain a stream from
    \c qDebug(), \c qInfo(), \c qWarning() or \c qCritical() and write values
    to it with \c{<<}. The message is emitted when the stream, and every copy
    of it, is destroyed, so a statement such as

    \code
    qDebug() << "value:" << 42 << QString("text");
    \endcode

    produces one message, \c{value: 42 "text"}. Spaces are inserted between
    values unless disabled with nospace(), and strings are quoted unless
    disabled with noquote().

    A message is written to \c stderr followed by a newline, in the format set
    with \c qSetMessagePattern(), unless a handler was installed with
    \c qInstallMessageHandler(). That handler then receives the message type
    (\c QtDebugMsg, \c QtInfoMsg, \c QtWarningMsg, \c QtCriticalMsg or
    \c QtFatalMsg), a QMessageLogContext and the text instead. A \c QtFatalMsg
    message aborts the process after it is handled.

    The same functions also have printf-style forms, such as
    \c{qDebug("count: %d", n)}, and \c qFatal() takes a format string. A
    QLoggingCategory selects which messages are written; see \c qCDebug().

    \differences
    There are no stream manipulators such as \c endl and no \c{<<} for
    QMetaEnum. The containers supported are QList, QSet, QMap, QMultiMap,
    QHash, QMultiHash, \c std::vector, \c std::list and \c std::pair; a
    QList is written without the \c QList prefix. QFlags values are written
    as hexadecimal bits, because Nt does not look up \c Q_FLAG names here.
    The verbosity is stored but the built-in operators ignore it.
    \enddifferences

    \sa QMessageLogger, QMessageLogContext, QDebugStateSaver, QLoggingCategory
*/

/*!
    Constructs a debug stream that emits a message of the given \a type when
    it is destroyed.
*/
QDebug::QDebug(QtMsgType type) : s(std::make_shared<State>()) { s->type = type; }

/*!
    Constructs a debug stream that appends its text to \a string when it is
    destroyed, instead of emitting a message. A space written after the last
    value is kept.
*/
QDebug::QDebug(QString *string) : s(std::make_shared<State>()) { s->string = string; }

/*!
    Constructs a debug stream that writes its text to \a device, as UTF-8,
    when it is destroyed, instead of emitting a message. A space written after
    the last value is kept.
*/
QDebug::QDebug(QIODevice *device) : s(std::make_shared<State>()) { s->device = device; }

/*!
    Destroys the stream. If this is the last copy of the stream, the
    accumulated message is emitted, with a trailing space removed, and
    for QtFatalMsg the process is aborted. A stream for a QString or a
    QIODevice writes its text there instead.
*/
QDebug::~QDebug() {
    if (s.use_count() != 1)
        return;
    if (s->string) {
        *s->string += QString::fromStdString(s->buf);
        return;
    }
    if (s->device) {
        s->device->write(s->buf.data(), static_cast<qint64>(s->buf.size()));
        return;
    }
    if (!s->output && s->type != QtFatalMsg)
        return;
    if (s->space && !s->buf.empty() && s->buf.back() == ' ')
        s->buf.pop_back();
    const QMessageLogContext context(s->file, s->line, s->function, s->category.empty() ? "default" : s->category.c_str());
    qt_message_output(s->type, context, QString::fromStdString(s->buf));
}

QDebug &QDebug::operator<<(char v) {
    maybeQuote('\'');
    s->buf.push_back(v);
    maybeQuote('\'');
    return maybeSpace();
}

QDebug &QDebug::operator<<(const void *p) {
    char b[32];
    std::snprintf(b, sizeof b, "0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(p)));
    return put(b);
}

QDebug &QDebug::operator<<(QChar c) { return *this << QString(c); }

QDebug &QDebug::operator<<(const QString &v) {
    maybeQuote();
    s->buf += s->quote ? escaped(v.toStdString()) : v.toStdString();
    maybeQuote();
    return maybeSpace();
}

QDebug &QDebug::operator<<(const QByteArray &v) {
    maybeQuote();
    s->buf += s->quote ? escaped(v.toStdString()) : v.toStdString();
    maybeQuote();
    return maybeSpace();
}

QDebug &QDebug::operator<<(const QObject *o) {
    if (!o)
        return put("QObject(0x0)");
    char b[64];
    std::snprintf(b, sizeof b, "QObject(0x%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(o)));
    std::string r = b;
    if (!o->objectName().isEmpty())
        r += ", name = \"" + escaped(o->objectName().toStdString()) + "\"";
    return put(r + ")");
}

/*!
    \class QDebugStateSaver
    \brief The QDebugStateSaver class saves and restores the formatting state of a QDebug stream.

    Construct one at the start of an \c{operator<<(QDebug, const T &)} that
    changes the spacing or quoting of the stream. The settings are restored
    when it is destroyed, so the caller's settings are not affected:

    \code
    QDebug operator<<(QDebug debug, const Point &p) {
        QDebugStateSaver saver(debug);
        debug.nospace() << "Point(" << p.x << ", " << p.y << ')';
        return debug;
    }
    \endcode

    \differences
    The state is the automatic spacing, the quoting and the verbosity. There
    is no integer base.
    \enddifferences

    \sa QDebug
*/

/*!
    Saves the formatting state of \a dbg.
*/
QDebugStateSaver::QDebugStateSaver(QDebug &dbg)
    : m_debug(dbg), m_space(dbg.s->space), m_quote(dbg.s->quote), m_verbosity(dbg.s->verbosity) {}

/*!
    Restores the formatting state of the stream.
*/
QDebugStateSaver::~QDebugStateSaver() {
    m_debug.s->space = m_space;
    m_debug.s->quote = m_quote;
    m_debug.s->verbosity = m_verbosity;
}
