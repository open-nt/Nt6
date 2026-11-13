// MIT License - see LICENSE
#pragma once

#include <QtCore/qeventloop.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>

#include <functional>
#include <type_traits>
#include <utility>

class QAbstractEventDispatcher;
class QAbstractNativeEventFilter;
class QPermission;
class QTranslator;

class QCoreApplication : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString applicationName READ applicationName WRITE setApplicationName NOTIFY applicationNameChanged)
    Q_PROPERTY(
        QString applicationVersion READ applicationVersion WRITE setApplicationVersion NOTIFY applicationVersionChanged)
    Q_PROPERTY(QString organizationName READ organizationName WRITE setOrganizationName NOTIFY organizationNameChanged)
    Q_PROPERTY(
        QString organizationDomain READ organizationDomain WRITE setOrganizationDomain NOTIFY organizationDomainChanged)
    Q_PROPERTY(bool quitLockEnabled READ isQuitLockEnabled WRITE setQuitLockEnabled)
public:
    QCoreApplication(int &argc, char **argv);
    ~QCoreApplication() override;

    static QCoreApplication *instance();
    static bool instanceExists() noexcept;
    static bool startingUp();
    static bool closingDown();
    static QAbstractEventDispatcher *eventDispatcher();

    // The command line as passed to the constructor, decoded as UTF-8.
    static QStringList arguments();
    static QString applicationFilePath();
    static QString applicationDirPath();
    static qint64 applicationPid();
    static QStringList libraryPaths();
    static void setLibraryPaths(const QStringList &paths);
    static void addLibraryPath(const QString &path);
    static void removeLibraryPath(const QString &path);
    // Defaults to the file name of argv[0].
    static void setApplicationName(const QString &name);
    static QString applicationName();
    static void setOrganizationName(const QString &orgName);
    static QString organizationName();
    static void setOrganizationDomain(const QString &orgDomain);
    static QString organizationDomain();
    static void setApplicationVersion(const QString &version);
    static QString applicationVersion();

    static int exec();
    static void exit(int returnCode = 0);
    static void setQuitLockEnabled(bool enabled);
    static bool isQuitLockEnabled();
    static void processEvents(QEventLoop::ProcessEventsFlags flags = QEventLoop::AllEvents);
    static void processEvents(QEventLoop::ProcessEventsFlags flags, int ms);
    static void processEvents(QEventLoop::ProcessEventsFlags flags, QDeadlineTimer deadline);
    static void sendPostedEvents(QObject *receiver = nullptr, int eventType = 0);

    // Takes ownership of `event`.
    static void postEvent(QObject *receiver, QEvent *event);
    static bool sendEvent(QObject *receiver, QEvent *event);
    // eventType 0 removes every pending event for `receiver`.
    static void removePostedEvents(QObject *receiver, int eventType = 0);

    virtual bool notify(QObject *receiver, QEvent *event);
    static bool installTranslator(QTranslator *translationFile);
    static bool removeTranslator(QTranslator *translationFile);
    static QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1);

    static void setAttribute(Qt::ApplicationAttribute attribute, bool on = true);
    static bool testAttribute(Qt::ApplicationAttribute attribute);
    static void setSetuidAllowed(bool allow);
    static bool isSetuidAllowed();

    void installNativeEventFilter(QAbstractNativeEventFilter *filterObj);
    void removeNativeEventFilter(QAbstractNativeEventFilter *filterObj);

    Qt::PermissionStatus checkPermission(const QPermission &permission);
    template <typename Functor>
    void requestPermission(const QPermission &permission,
                           const typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type *context,
                           Functor &&func) {
        using Context = typename QtPrivate::InvokeContext<std::decay_t<Functor>>::type;
        if constexpr (std::is_member_function_pointer_v<std::decay_t<Functor>>) {
            auto *object = const_cast<Context *>(context); // NOLINT(cppcoreguidelines-pro-type-const-cast): as Qt
            requestPermissionImpl(permission, context,
                                  [object, f = std::forward<Functor>(func)](const QPermission &p) { (object->*f)(p); });
        } else {
            requestPermissionImpl(permission, context,
                                  [f = std::forward<Functor>(func)](const QPermission &p) mutable { f(p); });
        }
    }
    template <typename Functor> void requestPermission(const QPermission &permission, Functor &&func) {
        requestPermissionImpl(permission, nullptr,
                              [f = std::forward<Functor>(func)](const QPermission &p) mutable { f(p); });
    }

public slots:
    static void quit();

signals:
    /*!
        This signal is emitted by exec() after the event loop has stopped,
        just before exec() returns. Use it to release resources.

        \sa quit()
    */
    void aboutToQuit() { emitSignal(&QCoreApplication::aboutToQuit); }
    /*!
        \since 4.0
        This signal is emitted when setApplicationName() changes the application name.
    */
    void applicationNameChanged() { emitSignal(&QCoreApplication::applicationNameChanged); }
    /*!
        \since 4.0
        This signal is emitted when setApplicationVersion() changes the application version.
    */
    void applicationVersionChanged() { emitSignal(&QCoreApplication::applicationVersionChanged); }
    /*!
        \since 4.0
        This signal is emitted when setOrganizationName() changes the name of the organization.
    */
    void organizationNameChanged() { emitSignal(&QCoreApplication::organizationNameChanged); }
    /*!
        \since 4.0
        This signal is emitted when setOrganizationDomain() changes the domain of the organization.
    */
    void organizationDomainChanged() { emitSignal(&QCoreApplication::organizationDomainChanged); }

private:
    void requestPermissionImpl(const QPermission &permission, const QObject *context,
                               std::function<void(const QPermission &)> callback);
};

#define qApp QCoreApplication::instance()

using QtStartUpFunction = void (*)();
using QtCleanUpFunction = void (*)();
// Calls the function from the constructor of the application object, or at once if there is one already.
void qAddPreRoutine(QtStartUpFunction function);
// Calls the function when the application object is destroyed; the routines run last added, first called.
void qAddPostRoutine(QtCleanUpFunction function);
// The name of the application: QCoreApplication::applicationName().
QString qAppName();
void qRemovePostRoutine(QtCleanUpFunction function);
// Registers a function of the file to run when the application object is constructed.
#define Q_COREAPP_STARTUP_FUNCTION(AFUNC)                                                                              \
    static void AFUNC##_ctor_function() { qAddPreRoutine(AFUNC); }                                                     \
    namespace {                                                                                                        \
    static const struct AFUNC##_ctor_class_ {                                                                          \
        AFUNC##_ctor_class_() { AFUNC##_ctor_function(); }                                                             \
    } AFUNC##_ctor_instance_;                                                                                          \
    }

#define Q_DECLARE_TR_FUNCTIONS(context)                                                                                \
public:                                                                                                                \
    static QString tr(const char *sourceText, const char *disambiguation = nullptr, int n = -1) {                      \
        return QCoreApplication::translate(#context, sourceText, disambiguation, n);                                   \
    }                                                                                                                  \
                                                                                                                       \
private:
