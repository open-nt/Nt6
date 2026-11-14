// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#if QT_CONFIG(vulkan) || defined(Q_QDOC)

#include <QtCore/qbytearraylist.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qversionnumber.h>
#include <QtGui/qvulkanfunctions.h>

#include <algorithm>
#include <functional>
#include <memory>

class QWindow;

struct QVulkanLayer {
    QByteArray name;
    uint32_t version;
    QVersionNumber specVersion;
    QByteArray description;
};

inline bool operator==(const QVulkanLayer &lhs, const QVulkanLayer &rhs) noexcept {
    return lhs.name == rhs.name && lhs.version == rhs.version && lhs.specVersion == rhs.specVersion;
}
inline bool operator!=(const QVulkanLayer &lhs, const QVulkanLayer &rhs) noexcept { return !(lhs == rhs); }

struct QVulkanExtension {
    QByteArray name;
    uint32_t version;
};

inline bool operator==(const QVulkanExtension &lhs, const QVulkanExtension &rhs) noexcept {
    return lhs.name == rhs.name && lhs.version == rhs.version;
}
inline bool operator!=(const QVulkanExtension &lhs, const QVulkanExtension &rhs) noexcept { return !(lhs == rhs); }

template <typename T> class QVulkanInfoVector : public QList<T> {
public:
    /*!
        Returns \c true if the list has an entry called \a name.
    */
    bool contains(const QByteArray &name) const {
        return std::any_of(this->cbegin(), this->cend(), [&name](const T &entry) { return entry.name == name; });
    }
    /*!
        Returns \c true if the list has an entry called \a name with a version of at least \a minVersion.
    */
    bool contains(const QByteArray &name, int minVersion) const {
        return minVersion >= 0 && std::any_of(this->cbegin(), this->cend(), [&name, minVersion](const T &entry) {
                   return entry.name == name && entry.version >= static_cast<uint32_t>(minVersion);
               });
    }
};

class QVulkanInstance {
public:
    QVulkanInstance();
    ~QVulkanInstance();

    enum Flag { NoDebugOutputRedirect = 0x01, NoPortabilityDrivers = 0x02 };
    Q_DECLARE_FLAGS(Flags, Flag)

    QVulkanInfoVector<QVulkanLayer> supportedLayers();
    QVulkanInfoVector<QVulkanExtension> supportedExtensions();
    QVersionNumber supportedApiVersion() const;

    void setVkInstance(VkInstance existingVkInstance);
    void setFlags(Flags flags);
    void setLayers(const QByteArrayList &layers);
    void setExtensions(const QByteArrayList &extensions);
    void setApiVersion(const QVersionNumber &vulkanVersion);

    bool create();
    void destroy();
    bool isValid() const;
    VkResult errorCode() const;

    VkInstance vkInstance() const;

    Flags flags() const;
    QByteArrayList layers() const;
    QByteArrayList extensions() const;
    QVersionNumber apiVersion() const;

    PFN_vkVoidFunction getInstanceProcAddr(const char *name);

    QVulkanFunctions *functions() const;
    QVulkanDeviceFunctions *deviceFunctions(VkDevice device);
    void resetDeviceFunctions(VkDevice device);

    static VkSurfaceKHR surfaceForWindow(QWindow *window);
    bool supportsPresent(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, QWindow *window);

    void presentAboutToBeQueued(QWindow *window);
    void presentQueued(QWindow *window);

    enum DebugMessageSeverityFlag {
        VerboseSeverity = 0x01,
        InfoSeverity = 0x02,
        WarningSeverity = 0x04,
        ErrorSeverity = 0x08
    };
    Q_DECLARE_FLAGS(DebugMessageSeverityFlags, DebugMessageSeverityFlag)
    enum DebugMessageTypeFlag { GeneralMessage = 0x01, ValidationMessage = 0x02, PerformanceMessage = 0x04 };
    Q_DECLARE_FLAGS(DebugMessageTypeFlags, DebugMessageTypeFlag)
    using DebugUtilsFilter = std::function<bool(DebugMessageSeverityFlags, DebugMessageTypeFlags, const void *message)>;
    void installDebugOutputFilter(DebugUtilsFilter filter);
    void clearDebugOutputFilters();

private:
    Q_DISABLE_COPY(QVulkanInstance)
    struct Private;
    std::unique_ptr<Private> m_d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVulkanInstance::Flags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QVulkanInstance::DebugMessageSeverityFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(QVulkanInstance::DebugMessageTypeFlags)

#endif // QT_CONFIG(vulkan)
