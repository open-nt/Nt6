// MIT License - see LICENSE
#pragma once

#include <QtCore/qdebug.h>
#include <QtCore/qlogging.h>
#include <QtCore/qstring.h>

#include <atomic>
#include <string>

class QLoggingCategory {
public:
    /*!
        Constructs a category named \a category. Messages below the given
        \a severityLevel are disabled to begin with: the default,
        \c QtDebugMsg, enables all types, \c QtInfoMsg disables debug
        messages, \c QtWarningMsg disables debug and info messages, and
        \c QtCriticalMsg enables only critical ones. Filter rules and the
        installed filter then adjust that.
    */
    explicit QLoggingCategory(const char *category, QtMsgType severityLevel = QtDebugMsg);
    ~QLoggingCategory();

    QLoggingCategory(const QLoggingCategory &) = delete;
    QLoggingCategory &operator=(const QLoggingCategory &) = delete;

    /*!
        Returns \c true if debug messages are enabled for this category.

        \sa isEnabled()
    */
    bool isDebugEnabled() const { return m_enabled[kDebug].load(std::memory_order_relaxed); }
    /*!
        Returns \c true if info messages are enabled for this category.

        \sa isEnabled()
    */
    bool isInfoEnabled() const { return m_enabled[kInfo].load(std::memory_order_relaxed); }
    /*!
        Returns \c true if warning messages are enabled for this category.

        \sa isEnabled()
    */
    bool isWarningEnabled() const { return m_enabled[kWarning].load(std::memory_order_relaxed); }
    /*!
        Returns \c true if critical messages are enabled for this category.

        \sa isEnabled()
    */
    bool isCriticalEnabled() const { return m_enabled[kCritical].load(std::memory_order_relaxed); }

    bool isEnabled(QtMsgType type) const;
    void setEnabled(QtMsgType type, bool enable);

    /*!
        Returns the name of the category.
    */
    const char *categoryName() const { return m_name.c_str(); }

    /*!
        Returns a reference to this category. This lets a category object be
        used where a category function is expected, as in \c{qCDebug(category)}.
    */
    QLoggingCategory &operator()() { return *this; }
    /*!
        \overload
    */
    const QLoggingCategory &operator()() const { return *this; }

    static QLoggingCategory *defaultCategory();

    /*!
        The type of a filter function: it is called with a category whose
        rules have been applied and may change which types are enabled.
    */
    using CategoryFilter = void (*)(QLoggingCategory *);
    static CategoryFilter installFilter(CategoryFilter filter);
    static void setFilterRules(const QString &rules);

private:
    enum { kDebug, kInfo, kWarning, kCritical, kTypes };
    friend class QLoggingRegistry;

    std::string m_name;
    QtMsgType m_severity;
    std::atomic<bool> m_enabled[kTypes];
};

#define Q_DECLARE_LOGGING_CATEGORY(name) const QLoggingCategory &name();
#define Q_DECLARE_EXPORTED_LOGGING_CATEGORY(name, EXPORT_MACRO) EXPORT_MACRO const QLoggingCategory &name();
#define Q_LOGGING_CATEGORY(name, ...)           \
    const QLoggingCategory &name() {            \
        static const QLoggingCategory category(__VA_ARGS__); \
        return category;                        \
    }

// A category that only the file it is defined in can use.
#define Q_STATIC_LOGGING_CATEGORY(name, ...) static Q_LOGGING_CATEGORY(name, __VA_ARGS__)

// The first argument of a category macro is the category; the rest, if any, goes to the logger.
#define NT_LOG_CATEGORY_(first, ...) first

#define NT_LOG_CATEGORY_MACRO_(enabledFn, method, ...)                                              \
    for (bool qt_category_enabled = NT_LOG_CATEGORY_(__VA_ARGS__, 0)().enabledFn();                 \
         qt_category_enabled; qt_category_enabled = false)                                          \
    QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC,                      \
                   NT_LOG_CATEGORY_(__VA_ARGS__, 0)().categoryName()).method(__VA_ARGS__)

#if defined(QT_NO_DEBUG_OUTPUT)
#define qCDebug(...) for (bool qt_category_enabled = false; qt_category_enabled; qt_category_enabled = false) QMessageLogger().noDebug()
#else
#define qCDebug(...) NT_LOG_CATEGORY_MACRO_(isDebugEnabled, debug, __VA_ARGS__)
#endif
#if defined(QT_NO_INFO_OUTPUT)
#define qCInfo(...) for (bool qt_category_enabled = false; qt_category_enabled; qt_category_enabled = false) QMessageLogger().noDebug()
#else
#define qCInfo(...) NT_LOG_CATEGORY_MACRO_(isInfoEnabled, info, __VA_ARGS__)
#endif
#if defined(QT_NO_WARNING_OUTPUT)
#define qCWarning(...) for (bool qt_category_enabled = false; qt_category_enabled; qt_category_enabled = false) QMessageLogger().noDebug()
#else
#define qCWarning(...) NT_LOG_CATEGORY_MACRO_(isWarningEnabled, warning, __VA_ARGS__)
#endif
#define qCCritical(...) NT_LOG_CATEGORY_MACRO_(isCriticalEnabled, critical, __VA_ARGS__)
#define qCFatal(...) QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE, QT_MESSAGELOG_FUNC, NT_LOG_CATEGORY_(__VA_ARGS__, 0)().categoryName()).fatal(__VA_ARGS__)
