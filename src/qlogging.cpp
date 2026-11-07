// MIT License - see LICENSE
#include <QtCore/qcoreapplication.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/qlogging.h>
#include <QtCore/qthread.h>

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include "qlogging_p.h"

#if defined(__linux__)
#include <sys/syscall.h>
#endif

namespace {

const char *const kDefaultPattern = "%{if-category}%{category}: %{endif}%{message}";

const std::chrono::steady_clock::time_point g_processStart = std::chrono::steady_clock::now();

QtMessageHandler g_handler = nullptr;
std::mutex g_handlerMutex;
thread_local bool t_inHandler = false;

std::string vformat(const char *fmt, va_list ap) {
    va_list copy;
    va_copy(copy, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, ap);
    std::string text(static_cast<std::size_t>(n < 0 ? 0 : n), '\0');
    if (n > 0)
        std::vsnprintf(text.data(), text.size() + 1, fmt, copy);
    va_end(copy);
    return text;
}

bool isDefaultCategory(const char *category) { return !category || std::strcmp(category, "default") == 0; }

// One piece of a message pattern.
struct Token {
    enum Kind {
        Literal, Message, Category, File, Line, Function, Type, Pid, ThreadId, QThreadPtr, AppName,
        TimeIso, TimeFormat, TimeProcess, TimeBoot, Backtrace,
        IfDebug, IfInfo, IfWarning, IfCritical, IfFatal, IfCategory, EndIf
    };
    Kind kind;
    std::string text;
};

std::vector<Token> parsePattern(const std::string &pattern) {
    static const struct {
        const char *name;
        Token::Kind kind;
    } kPlaceholders[] = {
        {"message", Token::Message}, {"category", Token::Category}, {"file", Token::File},
        {"line", Token::Line}, {"function", Token::Function}, {"type", Token::Type},
        {"pid", Token::Pid}, {"threadid", Token::ThreadId}, {"qthreadptr", Token::QThreadPtr},
        {"appname", Token::AppName}, {"time process", Token::TimeProcess},
        {"time boot", Token::TimeBoot}, {"if-debug", Token::IfDebug}, {"if-info", Token::IfInfo},
        {"if-warning", Token::IfWarning}, {"if-critical", Token::IfCritical},
        {"if-fatal", Token::IfFatal}, {"if-category", Token::IfCategory}, {"endif", Token::EndIf},
    };

    std::vector<Token> tokens;
    std::string literal;
    const auto flush = [&] {
        if (!literal.empty())
            tokens.push_back({Token::Literal, literal});
        literal.clear();
    };
    for (std::size_t i = 0; i < pattern.size();) {
        const std::size_t close = pattern.find('}', i);
        if (pattern.compare(i, 2, "%{") != 0 || close == std::string::npos) {
            literal.push_back(pattern[i++]);
            continue;
        }
        const std::string name = pattern.substr(i + 2, close - i - 2);
        bool known = false;
        for (const auto &placeholder : kPlaceholders) {
            if (name == placeholder.name) {
                flush();
                tokens.push_back({placeholder.kind, {}});
                known = true;
                break;
            }
        }
        if (!known && name == "time") {
            flush();
            tokens.push_back({Token::TimeIso, {}});
            known = true;
        } else if (!known && name.compare(0, 5, "time ") == 0) {
            flush();
            tokens.push_back({Token::TimeFormat, name.substr(5)});
            known = true;
        } else if (!known && name.compare(0, 9, "backtrace") == 0) {
            flush();
            tokens.push_back({Token::Backtrace, {}});
            known = true;
        }
        if (known) {
            i = close + 1;
        } else {
            std::fprintf(stderr, "Unknown placeholder %s in the message pattern\n", name.c_str());
            literal.append(pattern, i, close + 1 - i);
            i = close + 1;
        }
    }
    flush();
    return tokens;
}

struct MessagePattern {
    std::vector<Token> tokens;
    bool fromEnvironment = false;
    bool initialised = false;
};

MessagePattern &pattern() {
    static MessagePattern p;
    return p;
}

std::mutex g_patternMutex;

void ensurePattern() {
    MessagePattern &p = pattern();
    if (p.initialised)
        return;
    const char *env = std::getenv("QT_MESSAGE_PATTERN");
    p.fromEnvironment = env && *env;
    p.tokens = parsePattern(p.fromEnvironment ? env : kDefaultPattern);
    p.initialised = true;
}

// Reduces a compiler's pretty function name to the qualified function name:
// "void ns::Class::method(int) const" becomes "ns::Class::method".
std::string cleanupFunction(const char *info) {
    if (!info)
        return "unknown";
    std::string s = info;
    // The argument list starts at the first '(' that is not part of a template argument list.
    int angle = 0;
    std::size_t paren = std::string::npos;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '<')
            ++angle;
        else if (s[i] == '>')
            --angle;
        else if (s[i] == '(' && angle == 0) {
            paren = i;
            break;
        }
    }
    if (paren != std::string::npos)
        s.erase(paren);
    // The return type is everything before the last space outside template arguments.
    angle = 0;
    std::size_t cut = std::string::npos;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '<')
            ++angle;
        else if (s[i] == '>')
            --angle;
        else if (s[i] == ' ' && angle == 0)
            cut = i;
    }
    if (cut != std::string::npos)
        s.erase(0, cut + 1);
    return s;
}

std::string formatTimeSince(long long microseconds) {
    char buffer[48];
    std::snprintf(buffer, sizeof buffer, "%lld.%06lld", microseconds / 1000000, microseconds % 1000000);
    return buffer;
}

const char *typeName(QtMsgType type) {
    switch (type) {
    case QtDebugMsg: return "debug";
    case QtInfoMsg: return "info";
    case QtWarningMsg: return "warning";
    case QtCriticalMsg: return "critical";
    case QtFatalMsg: return "fatal";
    }
    return "debug";
}

std::string formatMessage(const std::vector<Token> &tokens, QtMsgType type, const QMessageLogContext &context,
                          const std::string &message) {
    std::string out;
    // Each open %{if-...} adds whether its condition holds; text is written only while all do.
    std::vector<bool> conditions;
    const auto active = [&] { return std::all_of(conditions.begin(), conditions.end(), [](bool c) { return c; }); };
    for (const Token &token : tokens) {
        switch (token.kind) {
        case Token::IfDebug: conditions.push_back(type == QtDebugMsg); continue;
        case Token::IfInfo: conditions.push_back(type == QtInfoMsg); continue;
        case Token::IfWarning: conditions.push_back(type == QtWarningMsg); continue;
        case Token::IfCritical: conditions.push_back(type == QtCriticalMsg); continue;
        case Token::IfFatal: conditions.push_back(type == QtFatalMsg); continue;
        case Token::IfCategory: conditions.push_back(!isDefaultCategory(context.category)); continue;
        case Token::EndIf:
            if (!conditions.empty())
                conditions.pop_back();
            continue;
        default: break;
        }
        if (!active())
            continue;
        switch (token.kind) {
        case Token::Literal: out += token.text; break;
        case Token::Message: out += message; break;
        case Token::Category: out += context.category ? context.category : "default"; break;
        case Token::File: out += context.file ? context.file : "unknown"; break;
        case Token::Line: out += std::to_string(context.line); break;
        case Token::Function: out += cleanupFunction(context.function); break;
        case Token::Type: out += typeName(type); break;
        case Token::Pid: out += std::to_string(static_cast<long>(::getpid())); break;
        case Token::ThreadId:
#if defined(__linux__)
            out += std::to_string(static_cast<long>(::syscall(SYS_gettid)));
#else
            out += std::to_string(reinterpret_cast<std::uintptr_t>(QThread::currentThreadId()));
#endif
            break;
        case Token::QThreadPtr: {
            char buffer[32];
            std::snprintf(buffer, sizeof buffer, "0x%llx",
                          static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(QThread::currentThread())));
            out += buffer;
            break;
        }
        case Token::AppName: out += QCoreApplication::applicationName().toStdString(); break;
        case Token::TimeIso: out += QDateTime::currentDateTime().toString(Qt::ISODate).toStdString(); break;
        case Token::TimeFormat: out += QDateTime::currentDateTime().toString(QString::fromStdString(token.text)).toStdString(); break;
        case Token::TimeProcess:
            out += formatTimeSince(std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::steady_clock::now() - g_processStart).count());
            break;
        case Token::TimeBoot: {
            timespec ts{};
            #if defined(CLOCK_BOOTTIME)
            ::clock_gettime(CLOCK_BOOTTIME, &ts);
#else
            ::clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
            out += formatTimeSince(static_cast<long long>(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000);
            break;
        }
        default: break;
        }
    }
    return out;
}

std::string format(QtMsgType type, const QMessageLogContext &context, const std::string &message) {
    std::vector<Token> tokens;
    {
        const std::lock_guard<std::mutex> lock(g_patternMutex);
        ensurePattern();
        tokens = pattern().tokens;
    }
    return formatMessage(tokens, type, context, message);
}

void defaultHandler(QtMsgType type, const QMessageLogContext &context, const std::string &message) {
    std::string line = format(type, context, message);
    line.push_back('\n');
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

struct HandlerScope {
    HandlerScope() { t_inHandler = true; }
    ~HandlerScope() { t_inHandler = false; }
    HandlerScope(const HandlerScope &) = delete;
    HandlerScope &operator=(const HandlerScope &) = delete;
};

bool fatalFor(QtMsgType type) {
    static const bool warnings = std::getenv("QT_FATAL_WARNINGS") != nullptr;
    static const bool criticals = std::getenv("QT_FATAL_CRITICALS") != nullptr;
    return type == QtFatalMsg || (type == QtCriticalMsg && (warnings || criticals)) || (type == QtWarningMsg && warnings);
}

} // namespace

/*!
    \class QMessageLogContext
    \brief The QMessageLogContext class carries information about the origin of a log message.

    An instance of this class is passed to a message handler installed with
    \c qInstallMessageHandler(). The \c qDebug(), \c qInfo(), \c qWarning(),
    \c qCritical() and \c qFatal() macros fill in the file, line and function
    of the statement, and the \c qCDebug() family adds the name of the
    category.

    \differences
    The class can be copied, and \c category is \c{"default"} rather than
    \c nullptr when a message has no category. Define
    \c QT_NO_MESSAGELOGCONTEXT to leave the file, line and function out.
    \enddifferences

    \sa QMessageLogger, QDebug, QLoggingCategory
*/

/*!
    \class QNoDebug
    \brief The QNoDebug class is a stream that discards everything written to it.

    When \c QT_NO_DEBUG_OUTPUT, \c QT_NO_INFO_OUTPUT or \c QT_NO_WARNING_OUTPUT
    is defined, \c qDebug(), \c qInfo() or \c qWarning() produces a QNoDebug,
    so that the statement compiles to nothing.

    \differences
    There are no stream manipulators.
    \enddifferences

    \sa QDebug
*/

/*!
    \class QMessageLogger
    \brief The QMessageLogger class generates log messages.

    QMessageLogger is what the \c qDebug(), \c qInfo(), \c qWarning(),
    \c qCritical() and \c qFatal() macros construct, with the file, line and
    function of the statement. Use the macros rather than the class:

    \code
    qInfo() << "listening on port" << port;
    qWarning("cannot open %s: %s", path, strerror(errno));
    \endcode

    Each function has two forms. One takes a \c printf() style format string
    and arguments, and emits the message at once. The other takes no message
    and returns a QDebug stream, which emits the message when it is
    destroyed. Both also accept a QLoggingCategory, or a function that returns
    one, in front of the message; the \c qCDebug() family of macros does that.

    A fatal message aborts the process after the handler has run.

    \differences
    The \c QT_NO_DEBUG_OUTPUT, \c QT_NO_INFO_OUTPUT and \c QT_NO_WARNING_OUTPUT
    macros are honoured, but \c QT_MESSAGELOGCONTEXT is not needed: the
    context is always present unless \c QT_NO_MESSAGELOGCONTEXT is defined.
    \enddifferences

    \sa QDebug, QLoggingCategory, qInstallMessageHandler()
*/

void qt_message_output(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    // The default category is filtered here, since qDebug() and friends have no category to check.
    if (type != QtFatalMsg && isDefaultCategory(context.category) && !QLoggingCategory::defaultCategory()->isEnabled(type))
        return;

    QtMessageHandler handler = nullptr;
    {
        const std::lock_guard<std::mutex> lock(g_handlerMutex);
        handler = g_handler;
    }
    // A message logged from inside the handler goes to the default handler, not back into this one.
    if (handler && !t_inHandler) {
        const HandlerScope scope;
        handler(type, context, message);
    } else {
        defaultHandler(type, context, message.toStdString());
    }
    if (fatalFor(type))
        std::abort();
}

/*!
    Installs \a handler as the message handler and returns the previous
    handler, or \c nullptr if there was none. Pass \c nullptr to restore the
    default handler, which writes each message to \c stderr in the format set
    by qSetMessagePattern().

    The handler is called for every message that is not filtered out, from
    the thread that logged it. A message logged from inside the handler is
    written by the default handler.

    \sa qSetMessagePattern(), qFormatLogMessage(), QMessageLogContext
*/
QtMessageHandler qInstallMessageHandler(QtMessageHandler handler) {
    const std::lock_guard<std::mutex> lock(g_handlerMutex);
    QtMessageHandler old = g_handler;
    g_handler = handler;
    return old;
}

/*!
    Changes the format of the messages written by the default message
    handler to \a messagePattern. The pattern is text in which these
    placeholders are replaced:

    \table
    \header
        \li Placeholder
        \li Replaced by
    \row
        \li \c{%{appname}}
        \li QCoreApplication::applicationName()
    \row
        \li \c{%{category}}
        \li The logging category
    \row
        \li \c{%{file}}, \c{%{line}}, \c{%{function}}
        \li Where the message was logged; \c unknown when not known. The function is the qualified name, without the return type and the parameters.
    \row
        \li \c{%{message}}
        \li The message
    \row
        \li \c{%{pid}}, \c{%{threadid}}, \c{%{qthreadptr}}
        \li The process ID, the operating system thread ID, and the address of the QThread
    \row
        \li \c{%{type}}
        \li \c debug, \c info, \c warning, \c critical or \c fatal
    \row
        \li \c{%{time}}, \c{%{time format}}
        \li The local date and time, in ISO 8601 form or in the given QDateTime::toString() format
    \row
        \li \c{%{time process}}, \c{%{time boot}}
        \li Seconds since the process started, or since the system booted
    \row
        \li \c{%{if-debug}}, \c{%{if-info}}, \c{%{if-warning}}, \c{%{if-critical}}, \c{%{if-fatal}}, \c{%{if-category}} ... \c{%{endif}}
        \li The text in between, only for that type of message, or only if the message has a category other than \c{"default"}
    \endtable

    The default pattern is
    \c{"%{if-category}%{category}: %{endif}%{message}"}. The
    \c QT_MESSAGE_PATTERN environment variable sets the pattern too, and takes
    precedence over this function.

    \differences
    \c{%{backtrace}} is accepted but produces no text.
    \enddifferences

    \sa qInstallMessageHandler(), qFormatLogMessage()
*/
void qSetMessagePattern(const QString &messagePattern) {
    const std::lock_guard<std::mutex> lock(g_patternMutex);
    ensurePattern();
    MessagePattern &p = pattern();
    if (!p.fromEnvironment)
        p.tokens = parsePattern(messagePattern.isEmpty() ? kDefaultPattern : messagePattern.toStdString());
}

/*!
    Returns \a str formatted according to the current message pattern, as
    the default message handler would write it, for a message of the given
    \a type and \a context. A custom message handler can call this to keep
    the standard format.

    \sa qSetMessagePattern(), qInstallMessageHandler()
*/
QString qFormatLogMessage(QtMsgType type, const QMessageLogContext &context, const QString &str) {
    return QString::fromStdString(format(type, context, str.toStdString()));
}

QDebug QMessageLogger::makeStream(QtMsgType type, const QLoggingCategory *cat) const {
    QDebug stream(type);
    stream.s->file = m_context.file;
    stream.s->line = m_context.line;
    stream.s->function = m_context.function;
    stream.s->category = cat ? cat->categoryName() : (m_context.category ? m_context.category : "default");
    stream.s->output = type == QtFatalMsg || !cat || cat->isEnabled(type);
    return stream;
}

void QMessageLogger::logFormatted(QtMsgType type, const QLoggingCategory *cat, const char *msg, va_list ap) const {
    if (type != QtFatalMsg && cat && !cat->isEnabled(type))
        return;
    QMessageLogContext context = m_context;
    if (cat)
        context.category = cat->categoryName();
    qt_message_output(type, context, QString::fromStdString(vformat(msg, ap)));
}

#define NT_LOGGER_METHODS(name, TYPE, NORETURN)                                                        \
    void QMessageLogger::name(const char *msg, ...) const {                                            \
        va_list ap;                                                                                    \
        va_start(ap, msg);                                                                             \
        logFormatted(TYPE, nullptr, msg, ap);                                                          \
        va_end(ap);                                                                                    \
        NORETURN                                                                                       \
    }                                                                                                  \
    void QMessageLogger::name(const QLoggingCategory &cat, const char *msg, ...) const {               \
        va_list ap;                                                                                    \
        va_start(ap, msg);                                                                             \
        logFormatted(TYPE, &cat, msg, ap);                                                             \
        va_end(ap);                                                                                    \
        NORETURN                                                                                       \
    }                                                                                                  \
    void QMessageLogger::name(CategoryFunction catFunc, const char *msg, ...) const {                  \
        va_list ap;                                                                                    \
        va_start(ap, msg);                                                                             \
        logFormatted(TYPE, &catFunc(), msg, ap);                                                       \
        va_end(ap);                                                                                    \
        NORETURN                                                                                       \
    }                                                                                                  \
    QDebug QMessageLogger::name() const { return makeStream(TYPE, nullptr); }                          \
    QDebug QMessageLogger::name(const QLoggingCategory &cat) const { return makeStream(TYPE, &cat); }  \
    QDebug QMessageLogger::name(CategoryFunction catFunc) const { return makeStream(TYPE, &catFunc()); }

#define NT_NO_NORETURN
#define NT_ABORT std::abort();

NT_LOGGER_METHODS(debug, QtDebugMsg, NT_NO_NORETURN)
NT_LOGGER_METHODS(info, QtInfoMsg, NT_NO_NORETURN)
NT_LOGGER_METHODS(warning, QtWarningMsg, NT_NO_NORETURN)
NT_LOGGER_METHODS(critical, QtCriticalMsg, NT_NO_NORETURN)
NT_LOGGER_METHODS(fatal, QtFatalMsg, NT_ABORT)

#undef NT_LOGGER_METHODS
#undef NT_NO_NORETURN
#undef NT_ABORT
