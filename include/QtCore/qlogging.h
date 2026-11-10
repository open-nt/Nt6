// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>

#include <cstdarg>

#ifndef Q_FUNC_INFO
#define Q_FUNC_INFO __PRETTY_FUNCTION__
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NT_PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define NT_PRINTF_FORMAT(fmt, args)
#endif

class QDebug;
class QLoggingCategory;

enum QtMsgType { QtDebugMsg, QtWarningMsg, QtCriticalMsg, QtFatalMsg, QtInfoMsg, QtSystemMsg = QtCriticalMsg };

class QMessageLogContext {
public:
    /*!
        Constructs an empty context: no file, line or function, in the category
        \c{"default"}.
    */
    constexpr QMessageLogContext() noexcept = default;
    /*!
        Constructs a context for a message from the given \a fileName,
        \a lineNumber and \a functionName, in the category \a categoryName.
    */
    constexpr QMessageLogContext(const char *fileName, int lineNumber, const char *functionName,
                                 const char *categoryName) noexcept
        : line(lineNumber), file(fileName), function(functionName), category(categoryName) {}

    /*!
        The version of the context. It is \c 2, as in Qt 6.
    */
    int version = 2;
    /*!
        The line number the message came from, or \c 0.
    */
    int line = 0;
    /*!
        The name of the source file the message came from, or \c nullptr.
    */
    const char *file = nullptr;
    /*!
        The name of the function the message came from, or \c nullptr. It is
        the compiler's pretty function name, so it includes the return type
        and the parameters; the \c{%{function}} message pattern placeholder
        leaves only the qualified name.
    */
    const char *function = nullptr;
    /*!
        The logging category of the message. It is \c{"default"} for messages
        written without a category.
    */
    const char *category = "default";
};

using QtMessageHandler = void (*)(QtMsgType, const QMessageLogContext &, const QString &);
QtMessageHandler qInstallMessageHandler(QtMessageHandler handler);

void qSetMessagePattern(const QString &messagePattern);
// The text the system has for an error number; for errno if errorCode is -1.
QString qt_error_string(int errorCode = -1);
// Logs a critical message that ends with the text of an error number, in parentheses: of errno, or of code.
#if defined(__GNUC__) || defined(__clang__)
void qErrnoWarning(const char *msg, ...) __attribute__((format(printf, 1, 2)));
void qErrnoWarning(int code, const char *msg, ...) __attribute__((format(printf, 2, 3)));
#else
void qErrnoWarning(const char *msg, ...);
void qErrnoWarning(int code, const char *msg, ...);
#endif
QString qFormatLogMessage(QtMsgType type, const QMessageLogContext &context, const QString &str);

#ifdef QT_NO_MESSAGELOGCONTEXT
#define QT_MESSAGELOG_FILE static_cast<const char *>(nullptr)
#define QT_MESSAGELOG_LINE 0
#define QT_MESSAGELOG_FUNC static_cast<const char *>(nullptr)
#else
#define QT_MESSAGELOG_FILE __FILE__
#define QT_MESSAGELOG_LINE __LINE__
#define QT_MESSAGELOG_FUNC Q_FUNC_INFO
#endif

class QNoDebug {
public:
    /*!
        Constructs a stream that discards everything written to it.
    */
    constexpr QNoDebug() noexcept = default;
    /*!
        Discards the value and returns a reference to the stream.
    */
    template <typename T>
    QNoDebug &operator<<(const T &) noexcept {
        return *this;
    }
    /*!
        Does nothing, and returns a reference to the stream.
    */
    QNoDebug &space() noexcept { return *this; }
    /*!
        Does nothing, and returns a reference to the stream.
    */
    QNoDebug &nospace() noexcept { return *this; }
    /*!
        Does nothing, and returns a reference to the stream.
    */
    QNoDebug &quote() noexcept { return *this; }
    /*!
        Does nothing, and returns a reference to the stream.
    */
    QNoDebug &noquote() noexcept { return *this; }
    /*!
        Does nothing, and returns a reference to the stream.
    */
    QNoDebug &maybeSpace() noexcept { return *this; }
};

class QMessageLogger {
public:
    /*!
        \typedef QMessageLogger::CategoryFunction

        A function that returns a logging category, such as one declared with
        \c Q_DECLARE_LOGGING_CATEGORY().
    */
    using CategoryFunction = const QLoggingCategory &(*)();

    /*!
        Constructs a logger with no file, line or function, in the category
        \c{"default"}.
    */
    constexpr QMessageLogger() noexcept = default;
    /*!
        Constructs a logger for messages from the given \a file, \a line and
        \a function, in the category \c{"default"}.
    */
    constexpr QMessageLogger(const char *file, int line, const char *function) noexcept
        : m_context(file, line, function, "default") {}
    /*!
        Constructs a logger for messages from the given \a file, \a line and
        \a function, in the category named \a category.
    */
    constexpr QMessageLogger(const char *file, int line, const char *function, const char *category) noexcept
        : m_context(file, line, function, category) {}

    /*!
        Logs the message \a msg, formatted like \c printf(), as a debug message.
    */
    void debug(const char *msg, ...) const NT_PRINTF_FORMAT(2, 3);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a debug message in the
        category \a cat if debug messages are enabled for the category.
    */
    void debug(const QLoggingCategory &cat, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a debug message in the
        category returned by \a catFunc if debug messages are enabled for the category.
    */
    void debug(CategoryFunction catFunc, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        Returns a stream for a debug message. Write the message to it with \c{<<}; it is emitted
        when the stream is destroyed.
    */
    QDebug debug() const;
    /*!
        \overload

        Returns a stream for a debug message in the category \a cat. Nothing is written
        if the category does not enable debug messages.
    */
    QDebug debug(const QLoggingCategory &cat) const;
    /*!
        \overload

        Returns a stream for a debug message in the category returned by \a catFunc.
    */
    QDebug debug(CategoryFunction catFunc) const;

    /*!
        Logs the message \a msg, formatted like \c printf(), as an info message.
    */
    void info(const char *msg, ...) const NT_PRINTF_FORMAT(2, 3);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as an info message in the
        category \a cat if info messages are enabled for the category.
    */
    void info(const QLoggingCategory &cat, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as an info message in the
        category returned by \a catFunc if info messages are enabled for the category.
    */
    void info(CategoryFunction catFunc, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        Returns a stream for an info message. Write the message to it with \c{<<}; it is emitted
        when the stream is destroyed.
    */
    QDebug info() const;
    /*!
        \overload

        Returns a stream for an info message in the category \a cat. Nothing is written
        if the category does not enable info messages.
    */
    QDebug info(const QLoggingCategory &cat) const;
    /*!
        \overload

        Returns a stream for an info message in the category returned by \a catFunc.
    */
    QDebug info(CategoryFunction catFunc) const;

    /*!
        Logs the message \a msg, formatted like \c printf(), as a warning message.
    */
    void warning(const char *msg, ...) const NT_PRINTF_FORMAT(2, 3);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a warning message in the
        category \a cat if warning messages are enabled for the category.
    */
    void warning(const QLoggingCategory &cat, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a warning message in the
        category returned by \a catFunc if warning messages are enabled for the category.
    */
    void warning(CategoryFunction catFunc, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        Returns a stream for a warning message. Write the message to it with \c{<<}; it is emitted
        when the stream is destroyed.
    */
    QDebug warning() const;
    /*!
        \overload

        Returns a stream for a warning message in the category \a cat. Nothing is written
        if the category does not enable warning messages.
    */
    QDebug warning(const QLoggingCategory &cat) const;
    /*!
        \overload

        Returns a stream for a warning message in the category returned by \a catFunc.
    */
    QDebug warning(CategoryFunction catFunc) const;

    /*!
        Logs the message \a msg, formatted like \c printf(), as a critical message.
    */
    void critical(const char *msg, ...) const NT_PRINTF_FORMAT(2, 3);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a critical message in the
        category \a cat if critical messages are enabled for the category.
    */
    void critical(const QLoggingCategory &cat, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a critical message in the
        category returned by \a catFunc if critical messages are enabled for the category.
    */
    void critical(CategoryFunction catFunc, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        Returns a stream for a critical message. Write the message to it with \c{<<}; it is emitted
        when the stream is destroyed.
    */
    QDebug critical() const;
    /*!
        \overload

        Returns a stream for a critical message in the category \a cat. Nothing is written
        if the category does not enable critical messages.
    */
    QDebug critical(const QLoggingCategory &cat) const;
    /*!
        \overload

        Returns a stream for a critical message in the category returned by \a catFunc.
    */
    QDebug critical(CategoryFunction catFunc) const;

    /*!
        Logs the message \a msg, formatted like \c printf(), as a fatal message. Then the process is aborted.
    */
    [[noreturn]] void fatal(const char *msg, ...) const NT_PRINTF_FORMAT(2, 3);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a fatal message in the
        category \a cat. Then the process is aborted.
    */
    [[noreturn]] void fatal(const QLoggingCategory &cat, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        \overload

        Logs the message \a msg, formatted like \c printf(), as a fatal message in the
        category returned by \a catFunc. Then the process is aborted.
    */
    [[noreturn]] void fatal(CategoryFunction catFunc, const char *msg, ...) const NT_PRINTF_FORMAT(3, 4);
    /*!
        Returns a stream for a fatal message. Write the message to it with \c{<<}; it is emitted
        when the stream is destroyed. Then the process is aborted.
    */
    QDebug fatal() const;
    /*!
        \overload

        Returns a stream for a fatal message in the category \a cat. Nothing is written
        if the category does not enable fatal messages.
    */
    QDebug fatal(const QLoggingCategory &cat) const;
    /*!
        \overload

        Returns a stream for a fatal message in the category returned by \a catFunc.
    */
    QDebug fatal(CategoryFunction catFunc) const;

    /*!
        Does nothing. This is what \c qDebug(), \c qInfo() and \c qWarning()
        call when \c QT_NO_DEBUG_OUTPUT, \c QT_NO_INFO_OUTPUT or
        \c QT_NO_WARNING_OUTPUT is defined, so that the message is compiled out.
    */
    void noDebug(const char *, ...) const noexcept {}
    /*!
        \overload

        Returns a stream that discards everything written to it.
    */
    QNoDebug noDebug() const noexcept { return QNoDebug(); }

private:
    QDebug makeStream(QtMsgType type, const QLoggingCategory *cat) const;
    void logFormatted(QtMsgType type, const QLoggingCategory *cat, const char *msg, va_list ap) const;

    QMessageLogContext m_context;
};

#if defined(QT_NO_DEBUG_OUTPUT)
#define qDebug QMessageLogger().noDebug
#else
#define qDebug QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).debug
#endif
#if defined(QT_NO_INFO_OUTPUT)
#define qInfo QMessageLogger().noDebug
#else
#define qInfo QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).info
#endif
#if defined(QT_NO_WARNING_OUTPUT)
#define qWarning QMessageLogger().noDebug
#else
#define qWarning QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).warning
#endif
#define qCritical QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).critical
#define qFatal QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC).fatal
