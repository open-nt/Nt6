// MIT License - see LICENSE
#include <QtGui/qvulkaninstance.h>

#include "qvulkan_p.h"

#if QT_CONFIG(vulkan)

#include "qpa/qplatformintegration_p.h"
#include "qpa/qplatformwindow_p.h"
#include "qwindow_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qlibrary.h>
#include <QtGui/qwindow.h>

#include <map>
#include <mutex>
#include <vector>

namespace QtPrivate::Vulkan {

namespace {

template <typename F> void resolve(QLibrary *lib, const char *name, F *out) {
    *out = reinterpret_cast<F>(lib->resolve(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename F> void resolveGlobal(const Loader &loader, const char *name, F *out) {
    *out = reinterpret_cast<F>(loader.getInstanceProcAddr(nullptr, name)); // NOLINT
}

} // namespace

const Loader *loader() {
    static Loader api;
    static bool loaded = false;
    static std::once_flag once;
    std::call_once(once, [] {
        static QLibrary *lib = nullptr; // kept loaded for the life of the process
        for (const auto &[name, version] :
             {std::pair<const char *, int>{"vulkan", 1}, {"vulkan", -1}, {"vulkan-1", -1}, {"MoltenVK", -1}}) {
            auto *candidate = version >= 0 ? new QLibrary(QString::fromLatin1(name), version)
                                           : new QLibrary(QString::fromLatin1(name));
            if (candidate->load()) {
                lib = candidate;
                break;
            }
            delete candidate;
        }
        if (!lib)
            return;
        resolve(lib, "vkGetInstanceProcAddr", &api.getInstanceProcAddr);
        if (!api.getInstanceProcAddr)
            return;
        resolveGlobal(api, "vkCreateInstance", &api.createInstance);
        resolveGlobal(api, "vkEnumerateInstanceLayerProperties", &api.enumerateInstanceLayerProperties);
        resolveGlobal(api, "vkEnumerateInstanceExtensionProperties", &api.enumerateInstanceExtensionProperties);
        resolveGlobal(api, "vkEnumerateInstanceVersion", &api.enumerateInstanceVersion);
        loaded = api.createInstance && api.enumerateInstanceLayerProperties && api.enumerateInstanceExtensionProperties;
    });
    return loaded ? &api : nullptr;
}

} // namespace QtPrivate::Vulkan

using QtPrivate::Vulkan::Loader;

struct QVulkanInstance::Private {
    VkInstance vkInst = VK_NULL_HANDLE;
    VkInstance externalInstance = VK_NULL_HANDLE;
    bool ownsInstance = false;
    Flags flags;
    QByteArrayList layers;
    QByteArrayList extensions;
    QVersionNumber apiVersion;
    VkResult errorCode = VK_SUCCESS;
    std::unique_ptr<QVulkanFunctions> functions;
    std::map<VkDevice, std::unique_ptr<QVulkanDeviceFunctions>> deviceFunctions;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupport = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessenger = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessenger = nullptr;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    std::vector<DebugUtilsFilter> debugFilters;
};

namespace {

template <typename F> F procAddr(const Loader &loader, VkInstance inst, const char *name) {
    return reinterpret_cast<F>(loader.getInstanceProcAddr(inst, name)); // NOLINT
}

// Receives the messages of the validation layers and of the driver: the filters see them first, and what they do not
// take is printed, as Qt does, as a debug message or a warning by severity.
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT types,
                                             const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData) {
    auto *filters = static_cast<std::vector<QVulkanInstance::DebugUtilsFilter> *>(userData);
    const QVulkanInstance::DebugMessageSeverityFlags severityFlags(
        static_cast<QVulkanInstance::DebugMessageSeverityFlag>(severity));
    const QVulkanInstance::DebugMessageTypeFlags typeFlags(static_cast<QVulkanInstance::DebugMessageTypeFlag>(types));
    for (const auto &filter : *filters) {
        if (filter && filter(severityFlags, typeFlags, data))
            return VK_FALSE;
    }
    const char *message = data && data->pMessage ? data->pMessage : "";
    if (severity & (VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT))
        qWarning("vkDebug: %s", message);
    else
        qDebug("vkDebug: %s", message);
    return VK_FALSE;
}

} // namespace

/*!
    \class QVulkanLayer
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanLayer struct describes a Vulkan layer.

    \sa QVulkanInstance::supportedLayers()
*/

/*!
    \variable QVulkanLayer::name
    The name of the layer, such as \c VK_LAYER_KHRONOS_validation.
*/

/*!
    \variable QVulkanLayer::version
    The implementation version of the layer.
*/

/*!
    \variable QVulkanLayer::specVersion
    The Vulkan version the layer was written against.
*/

/*!
    \variable QVulkanLayer::description
    A description of the layer.
*/

/*!
    \class QVulkanExtension
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanExtension struct describes a Vulkan extension.

    \sa QVulkanInstance::supportedExtensions(), QVulkanWindow::supportedDeviceExtensions()
*/

/*!
    \variable QVulkanExtension::name
    The name of the extension, such as \c VK_KHR_ray_tracing_pipeline.
*/

/*!
    \variable QVulkanExtension::version
    The version of the extension.
*/

/*!
    \class QVulkanInfoVector
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanInfoVector class is a list of layers or extensions with a lookup by name.

    QVulkanInstance::supportedLayers(), QVulkanInstance::supportedExtensions() and
    QVulkanWindow::supportedDeviceExtensions() return one, so that a program can ask
    \c{if (extensions.contains("VK_KHR_ray_tracing_pipeline"))} before enabling an extension.

    \differences
    None.
    \enddifferences
*/

/*!
    \class QVulkanInstance
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanInstance class represents a native Vulkan instance, enabling Vulkan rendering onto a QWindow.

    A QVulkanInstance wraps a \c VkInstance. Set the layers, extensions and API version wanted, then call
    create(); the loader library is opened at that moment, so a program runs where there is no Vulkan and
    create() tells it so. The Vulkan functions of the instance are reached through functions(), those of a
    device through deviceFunctions(), without linking with a Vulkan library:

    \code
    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 2));
    if (!inst.create())
        qFatal("No Vulkan: error %d", inst.errorCode());

    QVulkanFunctions *f = inst.functions();
    uint32_t count = 0;
    f->vkEnumeratePhysicalDevices(inst.vkInstance(), &count, nullptr);
    \endcode

    Extensions of the instance, such as \c VK_KHR_surface and the surface extension of the platform, are
    enabled as the platform needs them; ask for more with setExtensions(). Layers and extensions that are not
    supported are dropped with a warning instead of failing create(), so a program can request
    \c VK_LAYER_KHRONOS_validation and run where it is not installed. Unless NoDebugOutputRedirect is set, the
    warnings and errors of the layers and the driver are printed with qWarning() through
    \c VK_EXT_debug_utils, filtered by installDebugOutputFilter().

    A window presents through the instance given to QWindow::setVulkanInstance(); surfaceForWindow() makes
    the \c VkSurfaceKHR of such a window. QVulkanWindow does all of that and the swapchain. The instance must
    outlive the windows that use it, and a program that uses several instances must give each window its own.

    \differences
    The default API version is the highest the loader supports up to 1.3, not 1.0, so the Vulkan 1.1 to 1.3
    functions of QVulkanFunctions and QVulkanDeviceFunctions work without setApiVersion(). There is no
    handle() (no QPlatformVulkanInstance), no \c NoPortabilityDrivers behaviour (the flag is accepted) and no
    \c VK_EXT_debug_report filter (\c DebugFilter); only the \c VK_EXT_debug_utils filter is there. The
    \c xcb platform presents; the \c offscreen platform creates no surfaces, so there the instance serves
    headless rendering and compute.
    \enddifferences

    \sa QVulkanWindow, QVulkanFunctions, QVulkanDeviceFunctions, QWindow::setVulkanInstance()
*/

/*!
    \enum QVulkanInstance::Flag

    What create() does besides making the instance.

    \value NoDebugOutputRedirect Do not enable \c VK_EXT_debug_utils, so the warnings and errors of the validation
           layers are not printed by Nt.
    \value NoPortabilityDrivers Accepted for Qt compatibility; Nt does not enumerate portability drivers on any
           platform yet.
*/

/*!
    \enum QVulkanInstance::DebugMessageSeverityFlag

    The severity of a message from \c VK_EXT_debug_utils, as seen by a DebugUtilsFilter.

    \value VerboseSeverity Diagnostic output.
    \value InfoSeverity Information, such as the creation of a resource.
    \value WarningSeverity Possible misuse that is not an error.
    \value ErrorSeverity A violation of the specification.
*/

/*!
    \enum QVulkanInstance::DebugMessageTypeFlag

    What a message from \c VK_EXT_debug_utils is about, as seen by a DebugUtilsFilter.

    \value GeneralMessage Not tied to the specification or performance.
    \value ValidationMessage A violation of the specification or a possible mistake.
    \value PerformanceMessage A use of Vulkan that is not optimal.
*/

/*!
    \typedef QVulkanInstance::DebugUtilsFilter

    A function that sees each debug message before it is printed; \c message is a
    \c{const VkDebugUtilsMessengerCallbackDataEXT *}. Returning \c true drops the message.

    \sa installDebugOutputFilter()
*/

/*!
    Constructs a new instance. Nothing is created until create() is called.
*/
QVulkanInstance::QVulkanInstance() : m_d(new Private) {}

/*!
    Destroys the instance; see destroy().
*/
QVulkanInstance::~QVulkanInstance() { destroy(); }

/*!
    Returns the layers the loader supports. Works before create().

    \sa setLayers(), supportedExtensions()
*/
QVulkanInfoVector<QVulkanLayer> QVulkanInstance::supportedLayers() {
    QVulkanInfoVector<QVulkanLayer> result;
    const Loader *loader = QtPrivate::Vulkan::loader();
    if (!loader)
        return result;
    uint32_t count = 0;
    if (loader->enumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS || count == 0)
        return result;
    std::vector<VkLayerProperties> props(count);
    if (loader->enumerateInstanceLayerProperties(&count, props.data()) != VK_SUCCESS)
        return result;
    for (uint32_t i = 0; i < count; ++i) {
        const VkLayerProperties &p = props[i];
        QVulkanLayer layer;
        layer.name = QByteArray(p.layerName);
        layer.version = p.implementationVersion;
        layer.specVersion = QVersionNumber(static_cast<int>(VK_API_VERSION_MAJOR(p.specVersion)),
                                           static_cast<int>(VK_API_VERSION_MINOR(p.specVersion)),
                                           static_cast<int>(VK_API_VERSION_PATCH(p.specVersion)));
        layer.description = QByteArray(p.description);
        result.append(layer);
    }
    return result;
}

/*!
    Returns the instance extensions the loader supports. Works before create().

    \sa setExtensions(), supportedLayers()
*/
QVulkanInfoVector<QVulkanExtension> QVulkanInstance::supportedExtensions() {
    QVulkanInfoVector<QVulkanExtension> result;
    const Loader *loader = QtPrivate::Vulkan::loader();
    if (!loader)
        return result;
    uint32_t count = 0;
    if (loader->enumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
        return result;
    std::vector<VkExtensionProperties> props(count);
    if (loader->enumerateInstanceExtensionProperties(nullptr, &count, props.data()) != VK_SUCCESS)
        return result;
    for (uint32_t i = 0; i < count; ++i) {
        QVulkanExtension ext;
        ext.name = QByteArray(props[i].extensionName);
        ext.version = props[i].specVersion;
        result.append(ext);
    }
    return result;
}

/*!
    Returns the highest Vulkan API version the loader supports, 1.0 with a loader that has no
    \c vkEnumerateInstanceVersion, or a null version when there is no loader. Works before create().

    \sa setApiVersion(), apiVersion()
*/
QVersionNumber QVulkanInstance::supportedApiVersion() const {
    const Loader *loader = QtPrivate::Vulkan::loader();
    if (!loader)
        return {};
    uint32_t version = VK_API_VERSION_1_0;
    if (loader->enumerateInstanceVersion && loader->enumerateInstanceVersion(&version) != VK_SUCCESS)
        version = VK_API_VERSION_1_0;
    return QVersionNumber(static_cast<int>(VK_API_VERSION_MAJOR(version)),
                          static_cast<int>(VK_API_VERSION_MINOR(version)),
                          static_cast<int>(VK_API_VERSION_PATCH(version)));
}

/*!
    Makes create() adopt \a existingVkInstance instead of creating one. The instance is not destroyed by
    destroy(); the layers, extensions and API version set here are not applied to it, but the ones given
    with setLayers(), setExtensions() and setApiVersion() are reported, so set them to what the instance was
    made with.

    \sa vkInstance()
*/
void QVulkanInstance::setVkInstance(VkInstance existingVkInstance) { m_d->externalInstance = existingVkInstance; }

/*!
    Sets the \a flags for create().

    \sa flags()
*/
void QVulkanInstance::setFlags(Flags flags) { m_d->flags = flags; }

/*!
    Sets the \a layers to enable in create(). Layers the loader does not support are dropped with a warning.

    \sa layers(), supportedLayers()
*/
void QVulkanInstance::setLayers(const QByteArrayList &layers) { m_d->layers = layers; }

/*!
    Sets the instance \a extensions to enable in create(), besides the ones the platform needs to present.
    Extensions the loader does not support are dropped with a warning.

    \sa extensions(), supportedExtensions()
*/
void QVulkanInstance::setExtensions(const QByteArrayList &extensions) { m_d->extensions = extensions; }

/*!
    Sets the Vulkan API version to request in create() to \a vulkanVersion. The default is the highest
    the loader supports, up to 1.3; a program that uses ray tracing or other Vulkan 1.2 features sets 1.2 or
    more. Unlike OpenGL, a higher version does not take anything away.

    \sa apiVersion(), supportedApiVersion()
*/
void QVulkanInstance::setApiVersion(const QVersionNumber &vulkanVersion) { m_d->apiVersion = vulkanVersion; }

/*!
    Creates the Vulkan instance, or adopts the one given to setVkInstance(). Returns \c true on success;
    otherwise errorCode() tells why, and \c VK_ERROR_INITIALIZATION_FAILED means that there is no loader
    library. An instance that is valid already is destroyed first.

    \sa destroy(), isValid(), errorCode()
*/
bool QVulkanInstance::create() {
    if (isValid())
        destroy();
    Private &d = *m_d;
    const Loader *loader = QtPrivate::Vulkan::loader();
    if (!loader) {
        qWarning("QVulkanInstance: Failed to load the Vulkan loader library");
        d.errorCode = VK_ERROR_INITIALIZATION_FAILED;
        return false;
    }

    QByteArrayList enabledExtensions;
    bool debugUtils = false;
    if (d.externalInstance) {
        d.vkInst = d.externalInstance;
        d.ownsInstance = false;
        enabledExtensions = d.extensions;
        debugUtils = !d.flags.testFlag(NoDebugOutputRedirect) && enabledExtensions.contains("VK_EXT_debug_utils");
    } else {
        const QVersionNumber supported = supportedApiVersion();
        QVersionNumber requested = d.apiVersion;
        if (requested.isNull())
            requested = qMin(supported, QVersionNumber(1, 3));
        if (requested.majorVersion() == 0)
            requested = QVersionNumber(1, 0);

        const QVulkanInfoVector<QVulkanLayer> layers = supportedLayers();
        QByteArrayList enabledLayers;
        for (const QByteArray &name : d.layers) {
            if (layers.contains(name))
                enabledLayers.append(name);
            else
                qWarning("QVulkanInstance: Layer %s is not supported and is ignored", name.constData());
        }

        const QVulkanInfoVector<QVulkanExtension> extensions = supportedExtensions();
        for (const QByteArray &name : d.extensions) {
            if (extensions.contains(name)) {
                if (!enabledExtensions.contains(name))
                    enabledExtensions.append(name);
            } else {
                qWarning("QVulkanInstance: Extension %s is not supported and is ignored", name.constData());
            }
        }
        if (extensions.contains("VK_KHR_surface") && !enabledExtensions.contains("VK_KHR_surface"))
            enabledExtensions.append("VK_KHR_surface");
        if (QPlatformIntegration *integration = QtPrivate::GuiAccess::integration()) {
            if (const char *ext = integration->vulkanSurfaceExtension()) {
                if (extensions.contains(ext) && !enabledExtensions.contains(ext))
                    enabledExtensions.append(ext);
            }
        }
        if (!d.flags.testFlag(NoDebugOutputRedirect) && extensions.contains("VK_EXT_debug_utils")) {
            debugUtils = true;
            if (!enabledExtensions.contains("VK_EXT_debug_utils"))
                enabledExtensions.append("VK_EXT_debug_utils");
        }

        std::vector<const char *> layerNames;
        for (const QByteArray &name : enabledLayers)
            layerNames.push_back(name.constData());
        std::vector<const char *> extensionNames;
        for (const QByteArray &name : enabledExtensions)
            extensionNames.push_back(name.constData());

        const QByteArray appName =
            QCoreApplication::instance() ? QCoreApplication::applicationName().toUtf8() : QByteArray("Nt");
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = appName.constData();
        appInfo.pEngineName = "Nt";
        appInfo.engineVersion = VK_MAKE_API_VERSION(0, QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
        appInfo.apiVersion = VK_MAKE_API_VERSION(0, static_cast<uint32_t>(requested.majorVersion()),
                                                 static_cast<uint32_t>(qMax(0, requested.minorVersion())), 0);

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &appInfo;
        info.enabledLayerCount = static_cast<uint32_t>(layerNames.size());
        info.ppEnabledLayerNames = layerNames.data();
        info.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
        info.ppEnabledExtensionNames = extensionNames.data();

        VkInstance inst = VK_NULL_HANDLE;
        const VkResult result = loader->createInstance(&info, nullptr, &inst);
        if (result != VK_SUCCESS || !inst) {
            qWarning("QVulkanInstance: Failed to create Vulkan instance: %d", static_cast<int>(result));
            d.errorCode = result == VK_SUCCESS ? VK_ERROR_INITIALIZATION_FAILED : result;
            return false;
        }
        d.vkInst = inst;
        d.ownsInstance = true;
        d.layers = enabledLayers;
        d.apiVersion = QVersionNumber(requested.majorVersion(), qMax(0, requested.minorVersion()));
    }
    d.extensions = enabledExtensions;
    d.errorCode = VK_SUCCESS;

    d.functions.reset(new QVulkanFunctions(loader->getInstanceProcAddr, d.vkInst));
    d.getDeviceProcAddr = procAddr<PFN_vkGetDeviceProcAddr>(*loader, d.vkInst, "vkGetDeviceProcAddr");
    d.destroyInstance = procAddr<PFN_vkDestroyInstance>(*loader, d.vkInst, "vkDestroyInstance");
    d.getPhysicalDeviceSurfaceSupport =
        procAddr<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(*loader, d.vkInst, "vkGetPhysicalDeviceSurfaceSupportKHR");
    if (debugUtils) {
        d.createDebugUtilsMessenger =
            procAddr<PFN_vkCreateDebugUtilsMessengerEXT>(*loader, d.vkInst, "vkCreateDebugUtilsMessengerEXT");
        d.destroyDebugUtilsMessenger =
            procAddr<PFN_vkDestroyDebugUtilsMessengerEXT>(*loader, d.vkInst, "vkDestroyDebugUtilsMessengerEXT");
        if (d.createDebugUtilsMessenger && d.destroyDebugUtilsMessenger) {
            VkDebugUtilsMessengerCreateInfoEXT info{};
            info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            // Warnings and errors, as Qt: the loader and the layers chatter at the lower severities.
            info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            info.pfnUserCallback = debugCallback;
            info.pUserData = &d.debugFilters;
            if (d.createDebugUtilsMessenger(d.vkInst, &info, nullptr, &d.debugMessenger) != VK_SUCCESS)
                d.debugMessenger = VK_NULL_HANDLE;
        }
    }
    return true;
}

/*!
    Destroys the Vulkan instance, unless it was adopted with setVkInstance(), and the functions objects.
    Destroy the devices, and the windows that present through the instance, first.

    \sa create()
*/
void QVulkanInstance::destroy() {
    Private &d = *m_d;
    if (!d.vkInst)
        return;
    if (d.debugMessenger && d.destroyDebugUtilsMessenger)
        d.destroyDebugUtilsMessenger(d.vkInst, d.debugMessenger, nullptr);
    d.debugMessenger = VK_NULL_HANDLE;
    d.deviceFunctions.clear();
    d.functions.reset();
    if (d.ownsInstance && d.destroyInstance)
        d.destroyInstance(d.vkInst, nullptr);
    d.vkInst = VK_NULL_HANDLE;
    d.ownsInstance = false;
    d.getDeviceProcAddr = nullptr;
    d.destroyInstance = nullptr;
    d.getPhysicalDeviceSurfaceSupport = nullptr;
    d.createDebugUtilsMessenger = nullptr;
    d.destroyDebugUtilsMessenger = nullptr;
}

/*!
    Returns \c true if create() succeeded and destroy() has not been called since.
*/
bool QVulkanInstance::isValid() const { return m_d->vkInst != VK_NULL_HANDLE; }

/*!
    Returns the result of the last create(): \c VK_SUCCESS, the error \c vkCreateInstance returned, or
    \c VK_ERROR_INITIALIZATION_FAILED when there is no loader library.
*/
VkResult QVulkanInstance::errorCode() const { return m_d->errorCode; }

/*!
    Returns the \c VkInstance, or \c VK_NULL_HANDLE before create().
*/
VkInstance QVulkanInstance::vkInstance() const { return m_d->vkInst; }

/*!
    Returns the flags given to setFlags().
*/
QVulkanInstance::Flags QVulkanInstance::flags() const { return m_d->flags; }

/*!
    Returns the layers that were enabled, once the instance is valid; before that, the ones requested.
*/
QByteArrayList QVulkanInstance::layers() const { return m_d->layers; }

/*!
    Returns the extensions that were enabled, including the ones added for the platform, once the instance
    is valid; before that, the ones requested.
*/
QByteArrayList QVulkanInstance::extensions() const { return m_d->extensions; }

/*!
    Returns the API version that was requested, once the instance is valid; before that, the one given to
    setApiVersion(), which is null by default.
*/
QVersionNumber QVulkanInstance::apiVersion() const { return m_d->apiVersion; }

/*!
    Returns the function called \a name through \c vkGetInstanceProcAddr for the instance, or \c nullptr if
    there is none. This is how the functions of an extension are reached:

    \code
    auto createAccelerationStructure = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        inst.getInstanceProcAddr("vkCreateAccelerationStructureKHR"));
    \endcode

    Functions that take a device are faster through \c vkGetDeviceProcAddr; see
    QVulkanFunctions::vkGetDeviceProcAddr().
*/
PFN_vkVoidFunction QVulkanInstance::getInstanceProcAddr(const char *name) {
    const Loader *loader = QtPrivate::Vulkan::loader();
    if (!loader || !m_d->vkInst || !name)
        return nullptr;
    return loader->getInstanceProcAddr(m_d->vkInst, name);
}

/*!
    Returns the instance-level functions, or \c nullptr before create(). The object belongs to the instance.
*/
QVulkanFunctions *QVulkanInstance::functions() const { return m_d->functions.get(); }

/*!
    Returns the device-level functions of \a device, resolving them the first time. The object belongs to
    the instance and lives until resetDeviceFunctions() or destroy().

    \sa resetDeviceFunctions()
*/
QVulkanDeviceFunctions *QVulkanInstance::deviceFunctions(VkDevice device) {
    Private &d = *m_d;
    if (!d.vkInst || !device || !d.getDeviceProcAddr)
        return nullptr;
    auto it = d.deviceFunctions.find(device);
    if (it == d.deviceFunctions.end())
        it = d.deviceFunctions
                 .emplace(device, std::unique_ptr<QVulkanDeviceFunctions>(
                                      new QVulkanDeviceFunctions(d.getDeviceProcAddr, device)))
                 .first;
    return it->second.get();
}

/*!
    Drops the functions object of \a device. Call it after the device is destroyed, so that a device created
    later with the same handle gets fresh function pointers.

    \sa deviceFunctions()
*/
void QVulkanInstance::resetDeviceFunctions(VkDevice device) { m_d->deviceFunctions.erase(device); }

/*!
    Returns the \c VkSurfaceKHR of \a window, creating it the first time through the instance set with
    QWindow::setVulkanInstance(), or \c VK_NULL_HANDLE when the window has no instance, or the platform
    cannot present with Vulkan (the \c offscreen platform). The surface belongs to the window and is
    destroyed with it, before its native window.
*/
VkSurfaceKHR QVulkanInstance::surfaceForWindow(QWindow *window) {
    if (!window)
        return VK_NULL_HANDLE;
    QVulkanInstance *inst = window->vulkanInstance();
    if (!inst || !inst->isValid()) {
        qWarning("QVulkanInstance::surfaceForWindow: the window has no valid QVulkanInstance");
        return VK_NULL_HANDLE;
    }
    const Loader *loader = QtPrivate::Vulkan::loader();
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    if (!loader || !integration)
        return VK_NULL_HANDLE;
    window->create();
    QPlatformWindow *platformWindow = QtPrivate::GuiAccess::platformWindow(window);
    if (!platformWindow)
        return VK_NULL_HANDLE;
    if (!platformWindow->vulkanSurface)
        integration->createVulkanSurface(window, inst->vkInstance(),
                                         reinterpret_cast<QVulkanGetInstanceProcAddr>(loader->getInstanceProcAddr));
    return QtPrivate::Vulkan::handleFromU64<VkSurfaceKHR>(platformWindow->vulkanSurface);
}

/*!
    Returns \c true if queue family \a queueFamilyIndex of \a physicalDevice can present to \a window.
*/
bool QVulkanInstance::supportsPresent(VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, QWindow *window) {
    if (!m_d->getPhysicalDeviceSurfaceSupport)
        return false;
    VkSurfaceKHR surface = surfaceForWindow(window);
    if (!surface)
        return false;
    VkBool32 supported = VK_FALSE;
    return m_d->getPhysicalDeviceSurfaceSupport(physicalDevice, queueFamilyIndex, surface, &supported) == VK_SUCCESS &&
           supported;
}

/*!
    Called by QVulkanWindow before it queues a present for \a window. Nothing to do on the platforms Nt has.
*/
void QVulkanInstance::presentAboutToBeQueued(QWindow *window) { (void)window; }

/*!
    Called by QVulkanWindow after it queued a present for \a window. Programs that present themselves call
    it too: the driver may have read events of the window system while presenting, and the platform makes
    sure that they are handled.
*/
void QVulkanInstance::presentQueued(QWindow *window) {
    (void)window;
    if (QPlatformIntegration *integration = QtPrivate::GuiAccess::integration())
        integration->checkQueuedEvents();
}

/*!
    Installs \a filter, which sees each warning and error of \c VK_EXT_debug_utils before Nt prints it, and
    drops the message by returning \c true. Filters can be installed before or after create().

    \sa clearDebugOutputFilters()
*/
void QVulkanInstance::installDebugOutputFilter(DebugUtilsFilter filter) {
    if (filter)
        m_d->debugFilters.push_back(std::move(filter));
}

/*!
    Removes every filter installed with installDebugOutputFilter().
*/
void QVulkanInstance::clearDebugOutputFilters() { m_d->debugFilters.clear(); }

#endif // QT_CONFIG(vulkan)
