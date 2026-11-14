// MIT License - see LICENSE
#include <QtGui/qvulkanwindow.h>

#if QT_CONFIG(vulkan)

#include "qvulkan_p.h"

#include <QtCore/qcoreevent.h>
#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace {

template <typename F> F deviceProc(QVulkanFunctions *f, VkDevice dev, const char *name) {
    return reinterpret_cast<F>(f->vkGetDeviceProcAddr(dev, name)); // NOLINT
}

template <typename F> F instanceProc(QVulkanInstance *inst, const char *name) {
    return reinterpret_cast<F>(inst->getInstanceProcAddr(name)); // NOLINT
}

constexpr int kFrameLag = 2; // frames that may be in flight at once

} // namespace

struct QVulkanWindow::Data {
    enum Status { StatusUninitialized, StatusFail, StatusDeviceReady, StatusReady };

    // What the program sets before the window is shown.
    Flags flags;
    int physDevIndex = 0;
    QByteArrayList requestedDevExtensions;
    QueueCreateInfoModifier queueCreateInfoModifier;
    EnabledFeaturesModifier featuresModifier;
    EnabledFeatures2Modifier features2Modifier;
    QList<VkFormat> requestedColorFormats;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    std::unique_ptr<QVulkanWindowRenderer> renderer;

    Status status = StatusUninitialized;
    QVulkanInstance *inst = nullptr;
    QVulkanFunctions *f = nullptr;
    QVulkanDeviceFunctions *df = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // The physical devices, enumerated once.
    QList<VkPhysicalDevice> physDevs;
    QList<VkPhysicalDeviceProperties> physDevProps;

    // The device.
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memProps{};
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue gfxQueue = VK_NULL_HANDLE;
    uint32_t gfxQueueFamilyIdx = 0;
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    uint32_t hostVisibleMemIndex = 0;
    uint32_t deviceLocalMemIndex = 0;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkFormat dsFormat = VK_FORMAT_D24_UNORM_S8_UINT;
    VkRenderPass defaultRenderPass = VK_NULL_HANDLE;

    // The surface and swapchain functions, which are extensions.
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getSurfaceCapabilities = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getSurfaceFormats = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getSurfacePresentModes = nullptr;
    PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
    PFN_vkDestroySwapchainKHR destroySwapchain = nullptr;
    PFN_vkGetSwapchainImagesKHR getSwapchainImages = nullptr;
    PFN_vkAcquireNextImageKHR acquireNextImage = nullptr;
    PFN_vkQueuePresentKHR queuePresent = nullptr;

    // The swapchain.
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    QSize swapChainImageSize;
    bool swapChainSupportsReadBack = false;
    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkImage msaaImage = VK_NULL_HANDLE;
        VkImageView msaaImageView = VK_NULL_HANDLE;
        VkDeviceMemory msaaMem = VK_NULL_HANDLE;
    };
    std::vector<ImageResources> imageRes;
    VkImage dsImage = VK_NULL_HANDLE;
    VkDeviceMemory dsMem = VK_NULL_HANDLE;
    VkImageView dsView = VK_NULL_HANDLE;
    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        bool imageSemWaitable = false;
    };
    std::array<FrameResources, kFrameLag> frameRes;
    uint32_t currentImage = 0;
    int currentFrame = 0;
    bool framePending = false;

    // Reading a frame back for grab().
    bool frameGrabbing = false;
    QImage frameGrabTargetImage;
    VkImage frameGrabImage = VK_NULL_HANDLE;
    VkDeviceMemory frameGrabImageMem = VK_NULL_HANDLE;

    QMatrix4x4 clipCorrect;

    // The steps, in order; each returns false when it fails and sets `status`.
    bool enumeratePhysicalDevices(QVulkanWindow *w);
    void init(QVulkanWindow *w);
    void reset();
    bool recreateSwapChain(QVulkanWindow *w);
    void releaseSwapChain();
    bool createDefaultRenderPass();
    bool createTransientImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImage *image,
                              VkDeviceMemory *mem, VkImageView *view);
    bool createReadbackImage();
    void ensureStarted(QVulkanWindow *w);
    void beginFrame(QVulkanWindow *w);
    void endFrame(QVulkanWindow *w);
    void addReadback();
    void finishReadback();
    uint32_t memoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted, uint32_t fallback) const;
    QSize surfaceSize(QVulkanWindow *w, VkSurfaceCapabilitiesKHR *caps) const;
};

/*!
    \class QVulkanWindowRenderer
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanWindowRenderer class is used to implement the application-specific rendering logic for a
    Vulkan-based QWindow.

    A QVulkanWindow creates its renderer with QVulkanWindow::createRenderer() and calls it as the window's
    resources come and go: initResources() once the device is made, initSwapChainResources() each time the
    swapchain is made, their release counterparts in the reverse order, and startNextFrame() for each frame.
    The frame ends when the renderer calls QVulkanWindow::frameReady(); to render continuously, it calls
    QWindow::requestUpdate() too:

    \snippet vulkan/rtriangle.cpp frame

    \differences
    None.
    \enddifferences

    \sa QVulkanWindow
*/

/*!
    Destroys the renderer.
*/
QVulkanWindowRenderer::~QVulkanWindowRenderer() = default;

/*!
    Called before the device is created. Reimplement to set device extensions, features or the sample count
    on the window, with QVulkanWindow::setDeviceExtensions(), QVulkanWindow::setEnabledFeaturesModifier() and
    QVulkanWindow::setSampleCount(). The default does nothing.
*/
void QVulkanWindowRenderer::preInitResources() {}

/*!
    Called when the device has been created. Reimplement to make the resources that do not depend on the
    swapchain: pipelines, buffers and descriptor sets. The default does nothing.
*/
void QVulkanWindowRenderer::initResources() {}

/*!
    Called when the swapchain has been created, and again after each resize. Reimplement to make what
    depends on QVulkanWindow::swapChainImageSize(). The default does nothing.
*/
void QVulkanWindowRenderer::initSwapChainResources() {}

/*!
    Called before the swapchain is destroyed, with the device idle. The default does nothing.
*/
void QVulkanWindowRenderer::releaseSwapChainResources() {}

/*!
    Called before the device is destroyed, with the device idle. The default does nothing.
*/
void QVulkanWindowRenderer::releaseResources() {}

/*!
    \fn void QVulkanWindowRenderer::startNextFrame()

    Called when a frame is to be rendered: QVulkanWindow::currentCommandBuffer() is recording, and
    QVulkanWindow::currentFramebuffer() is the target of QVulkanWindow::defaultRenderPass(). Record the frame
    and call QVulkanWindow::frameReady(), which submits and presents it.
*/

/*!
    Called when the physical device is lost. The window releases its resources and tries again when it is
    next exposed. The default does nothing.
*/
void QVulkanWindowRenderer::physicalDeviceLost() {}

/*!
    Called when the logical device is lost, which also drops every resource made from it. The window makes a
    new device and calls initResources() again. The default does nothing.
*/
void QVulkanWindowRenderer::logicalDeviceLost() {}

/*!
    \class QVulkanWindow
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanWindow class is a convenience subclass of QWindow to perform Vulkan rendering.

    QVulkanWindow does what every Vulkan program does before it draws: it picks a physical device and a
    queue that can present to the window, creates the logical device with \c VK_KHR_swapchain, the swapchain,
    a depth-stencil buffer, a default render pass and the command buffers, fences and semaphores of each
    frame, and recreates the swapchain when the window is resized. The program draws through a
    QVulkanWindowRenderer that createRenderer() returns:

    \snippet vulkan/rtriangle.cpp window

    The window needs a QVulkanInstance before it is shown:

    \snippet vulkan/rtriangle.cpp main

    Each frame, the renderer records into currentCommandBuffer(). A rasterizing renderer begins
    defaultRenderPass() on currentFramebuffer(), whose color attachment is the swapchain image and whose
    layout ends as \c VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; a renderer that writes the swapchain image some other
    way, as the ray tracing example does with a copy, moves it from \c VK_IMAGE_LAYOUT_UNDEFINED to
    \c VK_IMAGE_LAYOUT_PRESENT_SRC_KHR itself. The command buffer waits for the image at
    \c VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT.

    Device extensions and features are chosen in QVulkanWindowRenderer::preInitResources(), or before the
    window is shown, with setDeviceExtensions() and setEnabledFeaturesModifier(). The second overload of the
    latter takes a \c VkPhysicalDeviceFeatures2, so ray tracing and other features that live in extension
    structures are enabled by chaining them to its \c pNext.

    \differences
    The graphics queue must be able to present (true of every desktop driver); separate present queues are
    not supported. grab() returns a QImage::Format_RGB32 image, not \c Format_RGBA8888_Premultiplied. Two
    frames can be in flight at once, so concurrentFrameCount() is 2. Only the \c xcb platform presents.
    \enddifferences

    \sa QVulkanWindowRenderer, QVulkanInstance
*/

/*!
    \enum QVulkanWindow::Flag

    \value PersistentResources Keep the device, the renderer's resources and the swapchain while the window is
           not exposed. By default everything is released then, and made again when the window shows.
*/

/*!
    \typedef QVulkanWindow::QueueCreateInfoModifier

    A function that is given the queue families of the physical device, their count, and the list of
    \c VkDeviceQueueCreateInfo the window is about to create the device with, which it can extend.

    \sa setQueueCreateInfoModifier()
*/

/*!
    \typedef QVulkanWindow::EnabledFeaturesModifier

    A function that is given the \c VkPhysicalDeviceFeatures the device is about to be created with, filled with
    what the physical device supports except \c robustBufferAccess, and changes it.

    \sa setEnabledFeaturesModifier()
*/

/*!
    \typedef QVulkanWindow::EnabledFeatures2Modifier

    A function that is given a \c VkPhysicalDeviceFeatures2 whose \c features are filled as for
    EnabledFeaturesModifier, and chains extension feature structures to its \c pNext. The structures must
    outlive the call to create the device, so keep them in the renderer or capture them by reference.

    \sa setEnabledFeaturesModifier()
*/

/*!
    \variable QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT

    The most frames that can be in flight at once; see concurrentFrameCount().
*/

/*!
    Constructs a window with the given \a parent, which Nt ignores since windows have no parents. The
    surface type is QSurface::VulkanSurface.
*/
QVulkanWindow::QVulkanWindow(QWindow *parent) : m_d(new Data) {
    (void)parent;
    setSurfaceType(QSurface::VulkanSurface);
    m_d->clipCorrect =
        QMatrix4x4(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f);
}

/*!
    Destroys the window and releases its Vulkan resources, with the renderer.
*/
QVulkanWindow::~QVulkanWindow() {
    m_d->reset();
    m_d->renderer.reset();
}

/*!
    Sets the \a flags of the window. Call it before the window is shown.

    \sa flags()
*/
void QVulkanWindow::setFlags(Flags flags) { m_d->flags = flags; }

/*!
    Returns the flags of the window.
*/
QVulkanWindow::Flags QVulkanWindow::flags() const { return m_d->flags; }

bool QVulkanWindow::Data::enumeratePhysicalDevices(QVulkanWindow *w) {
    if (!physDevs.isEmpty())
        return true;
    inst = w->vulkanInstance();
    if (!inst || !inst->isValid()) {
        qWarning("QVulkanWindow: Attempted to initialize without a valid QVulkanInstance");
        return false;
    }
    f = inst->functions();
    uint32_t count = 0;
    VkResult result = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        qWarning("QVulkanWindow: No physical devices found (%d)", static_cast<int>(result));
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    result = f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, devices.data());
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to enumerate physical devices: %d", static_cast<int>(result));
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties props{};
        f->vkGetPhysicalDeviceProperties(devices[i], &props);
        physDevs.append(devices[i]);
        physDevProps.append(props);
    }
    return true;
}

/*!
    Returns the properties of the physical devices of the window's QVulkanInstance, in the order of their
    indexes for setPhysicalDeviceIndex(). Works before the window is shown.
*/
QList<VkPhysicalDeviceProperties> QVulkanWindow::availablePhysicalDevices() {
    m_d->enumeratePhysicalDevices(this);
    return m_d->physDevProps;
}

/*!
    Makes the window use physical device \a idx of availablePhysicalDevices(); the default is 0. Call it
    before the window is shown.
*/
void QVulkanWindow::setPhysicalDeviceIndex(int idx) {
    if (m_d->dev) {
        qWarning("QVulkanWindow: Attempted to set physical device when already initialized");
        return;
    }
    m_d->physDevIndex = idx;
}

/*!
    Returns the device extensions the chosen physical device supports. Works before the window is shown,
    typically in QVulkanWindowRenderer::preInitResources().

    \sa setDeviceExtensions()
*/
QVulkanInfoVector<QVulkanExtension> QVulkanWindow::supportedDeviceExtensions() {
    QVulkanInfoVector<QVulkanExtension> result;
    if (!m_d->enumeratePhysicalDevices(this) || m_d->physDevIndex < 0 || m_d->physDevIndex >= m_d->physDevs.size())
        return result;
    VkPhysicalDevice physDev = m_d->physDevs.at(m_d->physDevIndex);
    uint32_t count = 0;
    if (m_d->f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, nullptr) != VK_SUCCESS || count == 0)
        return result;
    std::vector<VkExtensionProperties> props(count);
    if (m_d->f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, props.data()) != VK_SUCCESS)
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
    Sets the device \a extensions to enable, besides \c VK_KHR_swapchain. Extensions the device does not
    support are dropped with a warning. Call it before the window is shown, or in
    QVulkanWindowRenderer::preInitResources().

    \sa supportedDeviceExtensions()
*/
void QVulkanWindow::setDeviceExtensions(const QByteArrayList &extensions) {
    if (m_d->dev) {
        qWarning("QVulkanWindow: Attempted to set device extensions when already initialized");
        return;
    }
    m_d->requestedDevExtensions = extensions;
}

/*!
    Sets \a modifier, which can add queues to create with the device. Call it before the window is shown.
*/
void QVulkanWindow::setQueueCreateInfoModifier(const QueueCreateInfoModifier &modifier) {
    m_d->queueCreateInfoModifier = modifier;
}

/*!
    Sets \a modifier, which decides the \c VkPhysicalDeviceFeatures the device is created with. Call it before
    the window is shown.
*/
void QVulkanWindow::setEnabledFeaturesModifier(const EnabledFeaturesModifier &modifier) {
    m_d->featuresModifier = modifier;
    m_d->features2Modifier = nullptr;
}

/*!
    \overload

    Sets \a modifier, which decides the \c VkPhysicalDeviceFeatures2 the device is created with, extension
    features included:

    \snippet vulkan/rtriangle.cpp features
*/
void QVulkanWindow::setEnabledFeaturesModifier(EnabledFeatures2Modifier modifier) {
    m_d->features2Modifier = std::move(modifier);
    m_d->featuresModifier = nullptr;
}

/*!
    Sets the color \a formats to prefer for the swapchain, most wanted first; the first one the surface
    supports is used. The default prefers \c VK_FORMAT_B8G8R8A8_UNORM. Call it before the window is shown.

    \sa colorFormat()
*/
void QVulkanWindow::setPreferredColorFormats(const QList<VkFormat> &formats) {
    if (m_d->dev) {
        qWarning("QVulkanWindow: Attempted to set preferred color formats when already initialized");
        return;
    }
    m_d->requestedColorFormats = formats;
}

/*!
    Returns the sample counts the chosen physical device supports for color and depth-stencil attachments,
    such as 1, 2, 4 and 8.

    \sa setSampleCount()
*/
QList<int> QVulkanWindow::supportedSampleCounts() {
    QList<int> result;
    if (!m_d->enumeratePhysicalDevices(this) || m_d->physDevIndex < 0 || m_d->physDevIndex >= m_d->physDevs.size())
        return result;
    const VkPhysicalDeviceLimits &limits = m_d->physDevProps.at(m_d->physDevIndex).limits;
    const VkSampleCountFlags counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts &
                                      limits.framebufferStencilSampleCounts;
    for (int bit = 1; bit <= 64; bit *= 2) {
        if (counts & static_cast<VkSampleCountFlags>(bit))
            result.append(bit);
    }
    return result;
}

/*!
    Sets the number of samples per pixel of the color and depth-stencil buffers to \a sampleCount, one of
    supportedSampleCounts(). With more than 1, the renderer draws into msaaColorImage() and the render pass
    resolves into the swapchain image. Call it before the window is shown.

    \sa sampleCountFlagBits()
*/
void QVulkanWindow::setSampleCount(int sampleCount) {
    if (m_d->dev) {
        qWarning("QVulkanWindow: Attempted to set sample count when already initialized");
        return;
    }
    sampleCount = qBound(1, sampleCount, 64);
    int bits = 1;
    while (bits * 2 <= sampleCount)
        bits *= 2;
    m_d->sampleCount = static_cast<VkSampleCountFlagBits>(bits);
}

/*!
    Returns \c true if the device and the swapchain exist, so that the window can render.
*/
bool QVulkanWindow::isValid() const { return m_d->status == Data::StatusReady; }

/*!
    Returns a new renderer for the window, which the window owns, or \c nullptr, in which case each frame
    only clears the window to black. Reimplement it to render.
*/
QVulkanWindowRenderer *QVulkanWindow::createRenderer() { return nullptr; }

uint32_t QVulkanWindow::Data::memoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted, uint32_t fallback) const {
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & wanted) == wanted)
            return i;
    }
    return fallback;
}

void QVulkanWindow::Data::init(QVulkanWindow *w) {
    status = StatusFail;
    if (!enumeratePhysicalDevices(w))
        return;
    if (physDevIndex < 0 || physDevIndex >= physDevs.size()) {
        qWarning("QVulkanWindow: Invalid physical device index %d", physDevIndex);
        return;
    }
    surface = QVulkanInstance::surfaceForWindow(w);
    if (!surface) {
        qWarning("QVulkanWindow: Failed to create a Vulkan surface for the window");
        return;
    }
    physDev = physDevs.at(physDevIndex);
    f->vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);

    getSurfaceCapabilities =
        instanceProc<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(inst, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    getSurfaceFormats =
        instanceProc<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(inst, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    getSurfacePresentModes =
        instanceProc<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(inst, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    if (!getSurfaceCapabilities || !getSurfaceFormats || !getSurfacePresentModes) {
        qWarning("QVulkanWindow: The instance has no VK_KHR_surface");
        return;
    }

    // The renderer may ask for extensions, features and a sample count now.
    if (!renderer)
        renderer.reset(w->createRenderer());
    if (renderer)
        renderer->preInitResources();

    // A queue family that draws and presents.
    uint32_t familyCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &familyCount, families.data());
    gfxQueueFamilyIdx = UINT32_MAX;
    for (uint32_t i = 0; i < familyCount; ++i) {
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && inst->supportsPresent(physDev, i, w)) {
            gfxQueueFamilyIdx = i;
            break;
        }
    }
    if (gfxQueueFamilyIdx == UINT32_MAX) {
        qWarning("QVulkanWindow: No queue family with graphics and present support");
        return;
    }

    QList<VkDeviceQueueCreateInfo> queueInfos;
    const float priority = 0.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    queueInfos.append(queueInfo);
    if (queueCreateInfoModifier)
        queueCreateInfoModifier(families.data(), familyCount, queueInfos);

    const QVulkanInfoVector<QVulkanExtension> supported = w->supportedDeviceExtensions();
    QByteArrayList devExtensions;
    if (supported.contains("VK_KHR_swapchain"))
        devExtensions.append("VK_KHR_swapchain");
    for (const QByteArray &name : requestedDevExtensions) {
        if (!supported.contains(name))
            qWarning("QVulkanWindow: Device extension %s is not supported and is ignored", name.constData());
        else if (!devExtensions.contains(name))
            devExtensions.append(name);
    }
    std::vector<const char *> extensionNames;
    for (const QByteArray &name : devExtensions)
        extensionNames.push_back(name.constData());

    VkPhysicalDeviceFeatures features{};
    f->vkGetPhysicalDeviceFeatures(physDev, &features);
    features.robustBufferAccess = VK_FALSE;
    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features = features;

    VkDeviceCreateInfo devInfo{};
    devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    devInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    devInfo.pQueueCreateInfos = queueInfos.constData();
    devInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
    devInfo.ppEnabledExtensionNames = extensionNames.data();
    if (features2Modifier) {
        features2Modifier(features2);
        devInfo.pNext = &features2;
    } else {
        if (featuresModifier)
            featuresModifier(features);
        devInfo.pEnabledFeatures = &features;
    }
    VkResult result = f->vkCreateDevice(physDev, &devInfo, nullptr, &dev);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create device: %d", static_cast<int>(result));
        if (result == VK_ERROR_DEVICE_LOST && renderer)
            renderer->physicalDeviceLost();
        dev = VK_NULL_HANDLE;
        return;
    }
    df = inst->deviceFunctions(dev);
    df->vkGetDeviceQueue(dev, gfxQueueFamilyIdx, 0, &gfxQueue);

    createSwapchain = deviceProc<PFN_vkCreateSwapchainKHR>(f, dev, "vkCreateSwapchainKHR");
    destroySwapchain = deviceProc<PFN_vkDestroySwapchainKHR>(f, dev, "vkDestroySwapchainKHR");
    getSwapchainImages = deviceProc<PFN_vkGetSwapchainImagesKHR>(f, dev, "vkGetSwapchainImagesKHR");
    acquireNextImage = deviceProc<PFN_vkAcquireNextImageKHR>(f, dev, "vkAcquireNextImageKHR");
    queuePresent = deviceProc<PFN_vkQueuePresentKHR>(f, dev, "vkQueuePresentKHR");
    if (!createSwapchain || !destroySwapchain || !getSwapchainImages || !acquireNextImage || !queuePresent) {
        qWarning("QVulkanWindow: The device has no VK_KHR_swapchain");
        reset();
        return;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = gfxQueueFamilyIdx;
    result = df->vkCreateCommandPool(dev, &poolInfo, nullptr, &cmdPool);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create command pool: %d", static_cast<int>(result));
        reset();
        return;
    }

    hostVisibleMemIndex = memoryType(~0u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     memoryType(~0u, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0));
    deviceLocalMemIndex = memoryType(~0u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

    // The color format: the first preferred one the surface has, else BGRA8, else what the surface offers first.
    uint32_t formatCount = 0;
    getSurfaceFormats(physDev, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount)
        getSurfaceFormats(physDev, surface, &formatCount, formats.data());
    colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
    colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    if (!formats.empty() && formats[0].format != VK_FORMAT_UNDEFINED) {
        bool found = false;
        for (VkFormat wanted : requestedColorFormats) {
            for (const VkSurfaceFormatKHR &sf : formats) {
                if (sf.format == wanted) {
                    colorFormat = sf.format;
                    colorSpace = sf.colorSpace;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found) {
            auto it = std::find_if(formats.begin(), formats.end(),
                                   [](const VkSurfaceFormatKHR &sf) { return sf.format == VK_FORMAT_B8G8R8A8_UNORM; });
            if (it == formats.end())
                it = formats.begin();
            colorFormat = it->format;
            colorSpace = it->colorSpace;
        }
    }

    // A depth-stencil format the device renders to.
    dsFormat = VK_FORMAT_UNDEFINED;
    for (VkFormat candidate :
         {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT}) {
        VkFormatProperties props{};
        f->vkGetPhysicalDeviceFormatProperties(physDev, candidate, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            dsFormat = candidate;
            break;
        }
    }
    if (dsFormat == VK_FORMAT_UNDEFINED) {
        qWarning("QVulkanWindow: No depth-stencil format is supported");
        reset();
        return;
    }

    if (!createDefaultRenderPass()) {
        reset();
        return;
    }

    if (renderer)
        renderer->initResources();
    status = StatusDeviceReady;
}

bool QVulkanWindow::Data::createDefaultRenderPass() {
    const bool msaa = sampleCount > VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentDescription attachments[3]{};
    // 0: the swapchain image, presented at the end; also the resolve target with multisampling.
    attachments[0].format = colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = msaa ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    // 1: depth-stencil.
    attachments[1].format = dsFormat;
    attachments[1].samples = sampleCount;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    // 2: the multisampled color buffer, resolved into 0.
    attachments[2].format = colorFormat;
    attachments[2].samples = sampleCount;
    attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference resolveRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference dsRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    if (msaa)
        colorRef.attachment = 2;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &dsRef;
    if (msaa)
        subpass.pResolveAttachments = &resolveRef;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = msaa ? 3 : 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    const VkResult result = df->vkCreateRenderPass(dev, &info, nullptr, &defaultRenderPass);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create render pass: %d", static_cast<int>(result));
        defaultRenderPass = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

QSize QVulkanWindow::Data::surfaceSize(QVulkanWindow *w, VkSurfaceCapabilitiesKHR *caps) const {
    std::memset(caps, 0, sizeof(*caps));
    if (getSurfaceCapabilities(physDev, surface, caps) != VK_SUCCESS)
        return {};
    if (caps->currentExtent.width != UINT32_MAX)
        return QSize(static_cast<int>(caps->currentExtent.width), static_cast<int>(caps->currentExtent.height));
    const qreal dpr = w->devicePixelRatio();
    return QSize(qRound(w->width() * dpr), qRound(w->height() * dpr));
}

// A depth-stencil or multisampled color image in device-local memory, with its view.
bool QVulkanWindow::Data::createTransientImage(VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect,
                                               VkImage *image, VkDeviceMemory *mem, VkImageView *view) {
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = format;
    imgInfo.extent.width = static_cast<uint32_t>(swapChainImageSize.width());
    imgInfo.extent.height = static_cast<uint32_t>(swapChainImageSize.height());
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = sampleCount;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = usage | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult result = df->vkCreateImage(dev, &imgInfo, nullptr, image);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create image: %d", static_cast<int>(result));
        return false;
    }
    VkMemoryRequirements req{};
    df->vkGetImageMemoryRequirements(dev, *image, &req);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = memoryType(
        req.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memoryType(req.memoryTypeBits, 0, 0)));
    result = df->vkAllocateMemory(dev, &allocInfo, nullptr, mem);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to allocate image memory: %d", static_cast<int>(result));
        return false;
    }
    df->vkBindImageMemory(dev, *image, *mem, 0);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = *image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    result = df->vkCreateImageView(dev, &viewInfo, nullptr, view);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create image view: %d", static_cast<int>(result));
        return false;
    }
    return true;
}

bool QVulkanWindow::Data::recreateSwapChain(QVulkanWindow *w) {
    VkSurfaceCapabilitiesKHR caps{};
    const QSize size = surfaceSize(w, &caps);
    if (size.isEmpty())
        return false; // minimized: nothing to render into

    if (dev)
        df->vkDeviceWaitIdle(dev);
    if (renderer && status == StatusReady)
        renderer->releaseSwapChainResources();
    VkSwapchainKHR oldSwapChain = swapChain;
    swapChain = VK_NULL_HANDLE;
    releaseSwapChain();
    swapChainImageSize = size;
    status = StatusDeviceReady;

    uint32_t bufferCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        bufferCount = std::min(bufferCount, caps.maxImageCount);
    swapChainSupportsReadBack = (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (swapChainSupportsReadBack)
        usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; // renderers that copy into the swapchain image
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & static_cast<VkCompositeAlphaFlagsKHR>(compositeAlpha))) {
        for (uint32_t bit = 1; bit <= VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR; bit <<= 1) {
            if (caps.supportedCompositeAlpha & bit) {
                compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(bit);
                break;
            }
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    if (w->requestedFormat().swapInterval() == 0) {
        uint32_t modeCount = 0;
        getSurfacePresentModes(physDev, surface, &modeCount, nullptr);
        std::vector<VkPresentModeKHR> modes(modeCount);
        if (modeCount)
            getSurfacePresentModes(physDev, surface, &modeCount, modes.data());
        if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != modes.end())
            presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != modes.end())
            presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    VkSwapchainCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.surface = surface;
    info.minImageCount = bufferCount;
    info.imageFormat = colorFormat;
    info.imageColorSpace = colorSpace;
    info.imageExtent.width = static_cast<uint32_t>(size.width());
    info.imageExtent.height = static_cast<uint32_t>(size.height());
    info.imageArrayLayers = 1;
    info.imageUsage = usage;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                            : caps.currentTransform;
    info.compositeAlpha = compositeAlpha;
    info.presentMode = presentMode;
    info.clipped = VK_TRUE;
    info.oldSwapchain = oldSwapChain;
    VkResult result = createSwapchain(dev, &info, nullptr, &swapChain);
    if (oldSwapChain)
        destroySwapchain(dev, oldSwapChain, nullptr);
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create swapchain: %d", static_cast<int>(result));
        swapChain = VK_NULL_HANDLE;
        return false;
    }

    uint32_t imageCount = 0;
    getSwapchainImages(dev, swapChain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    getSwapchainImages(dev, swapChain, &imageCount, images.data());
    imageRes.resize(imageCount);

    const bool msaa = sampleCount > VK_SAMPLE_COUNT_1_BIT;
    if (!createTransientImage(dsFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, &dsImage, &dsMem, &dsView))
        return false;

    VkCommandBufferAllocateInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.commandPool = cmdPool;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandBufferCount = 1;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    for (uint32_t i = 0; i < imageCount; ++i) {
        ImageResources &image = imageRes[i];
        image.image = images[i];
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = colorFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (df->vkCreateImageView(dev, &viewInfo, nullptr, &image.imageView) != VK_SUCCESS) {
            qWarning("QVulkanWindow: Failed to create swapchain image view");
            return false;
        }
        if (df->vkAllocateCommandBuffers(dev, &cmdInfo, &image.cmdBuf) != VK_SUCCESS ||
            df->vkCreateFence(dev, &fenceInfo, nullptr, &image.cmdFence) != VK_SUCCESS) {
            qWarning("QVulkanWindow: Failed to create command buffer or fence");
            return false;
        }
        image.cmdFenceWaitable = false;
        if (msaa && !createTransientImage(colorFormat, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                                          &image.msaaImage, &image.msaaMem, &image.msaaImageView))
            return false;
        VkImageView attachments[3] = {image.imageView, dsView, image.msaaImageView};
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = defaultRenderPass;
        fbInfo.attachmentCount = msaa ? 3 : 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = static_cast<uint32_t>(size.width());
        fbInfo.height = static_cast<uint32_t>(size.height());
        fbInfo.layers = 1;
        if (df->vkCreateFramebuffer(dev, &fbInfo, nullptr, &image.fb) != VK_SUCCESS) {
            qWarning("QVulkanWindow: Failed to create framebuffer");
            return false;
        }
    }

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (FrameResources &frame : frameRes) {
        if (df->vkCreateFence(dev, &fenceInfo, nullptr, &frame.fence) != VK_SUCCESS ||
            df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.imageSem) != VK_SUCCESS ||
            df->vkCreateSemaphore(dev, &semInfo, nullptr, &frame.drawSem) != VK_SUCCESS) {
            qWarning("QVulkanWindow: Failed to create frame synchronization objects");
            return false;
        }
        frame.fenceWaitable = false;
        frame.imageSemWaitable = false;
    }
    currentFrame = 0;
    status = StatusReady;
    if (renderer)
        renderer->initSwapChainResources();
    return true;
}

// Destroys everything made by recreateSwapChain(), the swapchain included. The device must be idle.
void QVulkanWindow::Data::releaseSwapChain() {
    if (!dev)
        return;
    for (FrameResources &frame : frameRes) {
        if (frame.fence) {
            if (frame.fenceWaitable)
                df->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            df->vkDestroyFence(dev, frame.fence, nullptr);
        }
        if (frame.imageSem)
            df->vkDestroySemaphore(dev, frame.imageSem, nullptr);
        if (frame.drawSem)
            df->vkDestroySemaphore(dev, frame.drawSem, nullptr);
        frame = FrameResources();
    }
    for (ImageResources &image : imageRes) {
        if (image.cmdFence) {
            if (image.cmdFenceWaitable)
                df->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
            df->vkDestroyFence(dev, image.cmdFence, nullptr);
        }
        if (image.fb)
            df->vkDestroyFramebuffer(dev, image.fb, nullptr);
        if (image.cmdBuf)
            df->vkFreeCommandBuffers(dev, cmdPool, 1, &image.cmdBuf);
        if (image.imageView)
            df->vkDestroyImageView(dev, image.imageView, nullptr);
        if (image.msaaImageView)
            df->vkDestroyImageView(dev, image.msaaImageView, nullptr);
        if (image.msaaImage)
            df->vkDestroyImage(dev, image.msaaImage, nullptr);
        if (image.msaaMem)
            df->vkFreeMemory(dev, image.msaaMem, nullptr);
    }
    imageRes.clear();
    if (dsView)
        df->vkDestroyImageView(dev, dsView, nullptr);
    if (dsImage)
        df->vkDestroyImage(dev, dsImage, nullptr);
    if (dsMem)
        df->vkFreeMemory(dev, dsMem, nullptr);
    dsView = VK_NULL_HANDLE;
    dsImage = VK_NULL_HANDLE;
    dsMem = VK_NULL_HANDLE;
    if (frameGrabImage)
        df->vkDestroyImage(dev, frameGrabImage, nullptr);
    if (frameGrabImageMem)
        df->vkFreeMemory(dev, frameGrabImageMem, nullptr);
    frameGrabImage = VK_NULL_HANDLE;
    frameGrabImageMem = VK_NULL_HANDLE;
    if (swapChain) {
        destroySwapchain(dev, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }
    framePending = false;
    if (status == StatusReady)
        status = StatusDeviceReady;
}

// Releases everything: the swapchain, the renderer's resources and the device.
void QVulkanWindow::Data::reset() {
    if (!dev) {
        if (status != StatusFail)
            status = StatusUninitialized;
        return;
    }
    df->vkDeviceWaitIdle(dev);
    if (renderer && status == StatusReady)
        renderer->releaseSwapChainResources();
    releaseSwapChain();
    if (renderer)
        renderer->releaseResources();
    if (defaultRenderPass)
        df->vkDestroyRenderPass(dev, defaultRenderPass, nullptr);
    defaultRenderPass = VK_NULL_HANDLE;
    if (cmdPool)
        df->vkDestroyCommandPool(dev, cmdPool, nullptr);
    cmdPool = VK_NULL_HANDLE;
    df->vkDestroyDevice(dev, nullptr);
    inst->resetDeviceFunctions(dev);
    dev = VK_NULL_HANDLE;
    df = nullptr;
    gfxQueue = VK_NULL_HANDLE;
    swapChainImageSize = QSize();
    status = StatusUninitialized;
}

void QVulkanWindow::Data::ensureStarted(QVulkanWindow *w) {
    if (status == StatusFail) // try again: the instance or the surface may be there now
        status = StatusUninitialized;
    if (status == StatusUninitialized)
        init(w);
    if (status == StatusDeviceReady || status == StatusReady) {
        VkSurfaceCapabilitiesKHR caps{};
        const QSize size = surfaceSize(w, &caps);
        if (status == StatusDeviceReady || size != swapChainImageSize)
            recreateSwapChain(w);
    }
    if (status == StatusReady)
        w->requestUpdate();
}

void QVulkanWindow::Data::beginFrame(QVulkanWindow *w) {
    if (framePending || !w->isExposed())
        return;
    if (status != StatusReady) {
        ensureStarted(w);
        if (status != StatusReady)
            return;
    }
    VkSurfaceCapabilitiesKHR caps{};
    const QSize size = surfaceSize(w, &caps);
    if (size.isEmpty())
        return;
    if (size != swapChainImageSize && !recreateSwapChain(w))
        return;

    FrameResources &frame = frameRes[static_cast<std::size_t>(currentFrame)];
    if (frame.fenceWaitable) { // the acquire of this slot's previous frame
        df->vkWaitForFences(dev, 1, &frame.fence, VK_TRUE, UINT64_MAX);
        df->vkResetFences(dev, 1, &frame.fence);
        frame.fenceWaitable = false;
    }
    const VkResult result = acquireNextImage(dev, swapChain, UINT64_MAX, frame.imageSem, frame.fence, &currentImage);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        frame.fenceWaitable = true;
        frame.imageSemWaitable = true;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        if (recreateSwapChain(w))
            w->requestUpdate();
        return;
    } else {
        qWarning("QVulkanWindow: Failed to acquire swapchain image: %d", static_cast<int>(result));
        if (result == VK_ERROR_DEVICE_LOST) {
            if (renderer)
                renderer->logicalDeviceLost();
            reset();
        }
        w->requestUpdate();
        return;
    }

    ImageResources &image = imageRes[currentImage];
    if (image.cmdFenceWaitable) { // the last submit that used this image's command buffer
        df->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
        df->vkResetFences(dev, 1, &image.cmdFence);
        image.cmdFenceWaitable = false;
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (df->vkBeginCommandBuffer(image.cmdBuf, &beginInfo) != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to begin command buffer");
        return;
    }
    framePending = true;

    if (renderer) {
        renderer->startNextFrame();
    } else { // nothing to draw: clear to black
        VkClearValue clearValues[3]{};
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};
        clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = defaultRenderPass;
        rpInfo.framebuffer = image.fb;
        rpInfo.renderArea.extent.width = static_cast<uint32_t>(swapChainImageSize.width());
        rpInfo.renderArea.extent.height = static_cast<uint32_t>(swapChainImageSize.height());
        rpInfo.clearValueCount = sampleCount > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
        rpInfo.pClearValues = clearValues;
        df->vkCmdBeginRenderPass(image.cmdBuf, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        df->vkCmdEndRenderPass(image.cmdBuf);
        endFrame(w);
    }
}

// Records the copy of the swapchain image into the linear readback image, for grab().
void QVulkanWindow::Data::addReadback() {
    ImageResources &image = imageRes[currentImage];
    VkImageMemoryBarrier barriers[2]{};
    for (VkImageMemoryBarrier &b : barriers) {
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.layerCount = 1;
    }
    barriers[0].image = image.image;
    barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[1].image = frameGrabImage;
    barriers[1].srcAccessMask = 0;
    barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    df->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                             nullptr, 0, nullptr, 2, barriers);

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.srcSubresource.layerCount = 1;
    copy.dstSubresource = copy.srcSubresource;
    copy.extent.width = static_cast<uint32_t>(swapChainImageSize.width());
    copy.extent.height = static_cast<uint32_t>(swapChainImageSize.height());
    copy.extent.depth = 1;
    df->vkCmdCopyImage(image.cmdBuf, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frameGrabImage,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barriers[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barriers[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
    df->vkCmdPipelineBarrier(image.cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                             nullptr, 2, barriers);
}

bool QVulkanWindow::Data::createReadbackImage() {
    if (frameGrabImage)
        return true;
    VkImageCreateInfo imgInfo{};
    imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = colorFormat;
    imgInfo.extent.width = static_cast<uint32_t>(swapChainImageSize.width());
    imgInfo.extent.height = static_cast<uint32_t>(swapChainImageSize.height());
    imgInfo.extent.depth = 1;
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (df->vkCreateImage(dev, &imgInfo, nullptr, &frameGrabImage) != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to create readback image");
        frameGrabImage = VK_NULL_HANDLE;
        return false;
    }
    VkMemoryRequirements req{};
    df->vkGetImageMemoryRequirements(dev, frameGrabImage, &req);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex =
        memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0));
    if (df->vkAllocateMemory(dev, &allocInfo, nullptr, &frameGrabImageMem) != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to allocate readback memory");
        df->vkDestroyImage(dev, frameGrabImage, nullptr);
        frameGrabImage = VK_NULL_HANDLE;
        frameGrabImageMem = VK_NULL_HANDLE;
        return false;
    }
    df->vkBindImageMemory(dev, frameGrabImage, frameGrabImageMem, 0);
    return true;
}

// Waits for the frame that copied into the readback image and turns its pixels into frameGrabTargetImage.
void QVulkanWindow::Data::finishReadback() {
    ImageResources &image = imageRes[currentImage];
    if (image.cmdFenceWaitable) {
        df->vkWaitForFences(dev, 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
        df->vkResetFences(dev, 1, &image.cmdFence);
        image.cmdFenceWaitable = false;
    }
    VkImageSubresource subres{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout layout{};
    df->vkGetImageSubresourceLayout(dev, frameGrabImage, &subres, &layout);
    void *mapped = nullptr;
    if (df->vkMapMemory(dev, frameGrabImageMem, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to map readback memory");
        return;
    }
    const bool rgba = colorFormat == VK_FORMAT_R8G8B8A8_UNORM || colorFormat == VK_FORMAT_R8G8B8A8_SRGB;
    QImage &out = frameGrabTargetImage;
    out = QImage(swapChainImageSize, QImage::Format_RGB32);
    const auto *src = static_cast<const uchar *>(mapped) + layout.offset;
    for (int y = 0; y < out.height(); ++y) {
        const uchar *row = src + static_cast<std::size_t>(y) * layout.rowPitch;
        auto *dst = reinterpret_cast<QRgb *>(out.bits() + static_cast<qsizetype>(y) * out.bytesPerLine());
        if (rgba) {
            for (int x = 0; x < out.width(); ++x, row += 4)
                dst[x] = qRgb(row[0], row[1], row[2]);
        } else { // BGRA, the memory layout of Format_RGB32
            std::memcpy(dst, row, static_cast<std::size_t>(out.width()) * 4);
            for (int x = 0; x < out.width(); ++x)
                dst[x] |= 0xff000000u;
        }
    }
    df->vkUnmapMemory(dev, frameGrabImageMem);
}

void QVulkanWindow::Data::endFrame(QVulkanWindow *w) {
    ImageResources &image = imageRes[currentImage];
    FrameResources &frame = frameRes[static_cast<std::size_t>(currentFrame)];
    const bool grabbing = frameGrabbing && swapChainSupportsReadBack && createReadbackImage();
    if (grabbing)
        addReadback();
    if (df->vkEndCommandBuffer(image.cmdBuf) != VK_SUCCESS)
        qWarning("QVulkanWindow: Failed to end command buffer");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &image.cmdBuf;
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    if (frame.imageSemWaitable) {
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &frame.imageSem;
        submit.pWaitDstStageMask = &waitStage;
    }
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &frame.drawSem;
    VkResult result = df->vkQueueSubmit(gfxQueue, 1, &submit, image.cmdFence);
    frame.imageSemWaitable = false;
    framePending = false;
    if (result != VK_SUCCESS) {
        qWarning("QVulkanWindow: Failed to submit to graphics queue: %d", static_cast<int>(result));
        frameGrabbing = false;
        if (result == VK_ERROR_DEVICE_LOST) {
            if (renderer)
                renderer->logicalDeviceLost();
            reset();
            w->requestUpdate();
        }
        return;
    }
    image.cmdFenceWaitable = true;

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &frame.drawSem;
    present.swapchainCount = 1;
    present.pSwapchains = &swapChain;
    present.pImageIndices = &currentImage;
    inst->presentAboutToBeQueued(w);
    result = queuePresent(gfxQueue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain(w);
        w->requestUpdate();
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        qWarning("QVulkanWindow: Failed to present: %d", static_cast<int>(result));
        if (result == VK_ERROR_DEVICE_LOST) {
            if (renderer)
                renderer->logicalDeviceLost();
            reset();
            w->requestUpdate();
        }
    }
    inst->presentQueued(w);

    if (grabbing && dev)
        finishReadback();
    frameGrabbing = false;
    currentFrame = (currentFrame + 1) % kFrameLag;
}

/*!
    Ends the frame that QVulkanWindowRenderer::startNextFrame() records: submits the command buffer and
    presents the swapchain image. The renderer must call it once per frame, and may call
    QWindow::requestUpdate() after it to render the next frame.
*/
void QVulkanWindow::frameReady() {
    if (!m_d->framePending) {
        qWarning("QVulkanWindow: frameReady() called without a frame pending");
        return;
    }
    m_d->endFrame(this);
}

/*!
    Returns the physical device, once the window has been shown.
*/
VkPhysicalDevice QVulkanWindow::physicalDevice() const { return m_d->physDev; }

/*!
    Returns the properties of the physical device, or \c nullptr before the window has been shown.
*/
const VkPhysicalDeviceProperties *QVulkanWindow::physicalDeviceProperties() const {
    if (m_d->physDevIndex < 0 || m_d->physDevIndex >= m_d->physDevProps.size() || !m_d->physDev)
        return nullptr;
    return &m_d->physDevProps.at(m_d->physDevIndex);
}

/*!
    Returns the logical device, or \c VK_NULL_HANDLE before QVulkanWindowRenderer::initResources().
*/
VkDevice QVulkanWindow::device() const { return m_d->dev; }

/*!
    Returns the graphics queue, which also presents.
*/
VkQueue QVulkanWindow::graphicsQueue() const { return m_d->gfxQueue; }

/*!
    Returns the family index of graphicsQueue().
*/
uint32_t QVulkanWindow::graphicsQueueFamilyIndex() const { return m_d->gfxQueueFamilyIdx; }

/*!
    Returns the command pool that the frame command buffers are allocated from, with
    \c VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; renderers may allocate their own command buffers from it.
*/
VkCommandPool QVulkanWindow::graphicsCommandPool() const { return m_d->cmdPool; }

/*!
    Returns a memory type index that is host visible and coherent, for buffers the program writes.
*/
uint32_t QVulkanWindow::hostVisibleMemoryIndex() const { return m_d->hostVisibleMemIndex; }

/*!
    Returns a memory type index that is device local, for images and buffers the device alone touches.
*/
uint32_t QVulkanWindow::deviceLocalMemoryIndex() const { return m_d->deviceLocalMemIndex; }

/*!
    Returns the render pass that currentFramebuffer() is made for: attachment 0 is the swapchain image, cleared
    at the start and in \c VK_IMAGE_LAYOUT_PRESENT_SRC_KHR at the end, attachment 1 the depth-stencil buffer,
    and with multisampling attachment 2 the multisampled color buffer, resolved into attachment 0. A renderer
    that uses its own render pass does not have to use it, but then must leave the swapchain image in
    \c VK_IMAGE_LAYOUT_PRESENT_SRC_KHR.
*/
VkRenderPass QVulkanWindow::defaultRenderPass() const { return m_d->defaultRenderPass; }

/*!
    Returns the format of the swapchain images.

    \sa setPreferredColorFormats()
*/
VkFormat QVulkanWindow::colorFormat() const { return m_d->colorFormat; }

/*!
    Returns the format of the depth-stencil buffer: \c VK_FORMAT_D24_UNORM_S8_UINT where the device has it.
*/
VkFormat QVulkanWindow::depthStencilFormat() const { return m_d->dsFormat; }

/*!
    Returns the size of the swapchain images in pixels, which is the window size times its device pixel
    ratio.
*/
QSize QVulkanWindow::swapChainImageSize() const { return m_d->swapChainImageSize; }

/*!
    Returns the command buffer of the frame being recorded, between QVulkanWindowRenderer::startNextFrame()
    and frameReady(); \c VK_NULL_HANDLE otherwise.
*/
VkCommandBuffer QVulkanWindow::currentCommandBuffer() const {
    if (!m_d->framePending || m_d->currentImage >= m_d->imageRes.size())
        return VK_NULL_HANDLE;
    return m_d->imageRes[m_d->currentImage].cmdBuf;
}

/*!
    Returns the framebuffer of the frame being recorded, for defaultRenderPass(); \c VK_NULL_HANDLE when no
    frame is being recorded.
*/
VkFramebuffer QVulkanWindow::currentFramebuffer() const {
    if (!m_d->framePending || m_d->currentImage >= m_d->imageRes.size())
        return VK_NULL_HANDLE;
    return m_d->imageRes[m_d->currentImage].fb;
}

/*!
    Returns the index of the frame being recorded, from 0 to concurrentFrameCount() - 1. Resources that a
    frame writes while the device still reads the previous frame's, such as uniform buffers, are kept once per
    index.
*/
int QVulkanWindow::currentFrame() const { return m_d->currentFrame; }

/*!
    Returns the number of frames that can be in flight at once, which is 2. It is at most
    MAX_CONCURRENT_FRAME_COUNT.
*/
int QVulkanWindow::concurrentFrameCount() const { return kFrameLag; }

/*!
    Returns the number of images in the swapchain, once it exists.
*/
int QVulkanWindow::swapChainImageCount() const { return static_cast<int>(m_d->imageRes.size()); }

/*!
    Returns the index of the swapchain image of the frame being recorded.
*/
int QVulkanWindow::currentSwapChainImageIndex() const { return static_cast<int>(m_d->currentImage); }

/*!
    Returns swapchain image \a idx, or \c VK_NULL_HANDLE if there is none.
*/
VkImage QVulkanWindow::swapChainImage(int idx) const {
    return idx >= 0 && static_cast<std::size_t>(idx) < m_d->imageRes.size()
               ? m_d->imageRes[static_cast<std::size_t>(idx)].image
               : VK_NULL_HANDLE;
}

/*!
    Returns the view of swapchain image \a idx, or \c VK_NULL_HANDLE if there is none.
*/
VkImageView QVulkanWindow::swapChainImageView(int idx) const {
    return idx >= 0 && static_cast<std::size_t>(idx) < m_d->imageRes.size()
               ? m_d->imageRes[static_cast<std::size_t>(idx)].imageView
               : VK_NULL_HANDLE;
}

/*!
    Returns the depth-stencil image, shared by every frame.
*/
VkImage QVulkanWindow::depthStencilImage() const { return m_d->dsImage; }

/*!
    Returns the view of the depth-stencil image.
*/
VkImageView QVulkanWindow::depthStencilImageView() const { return m_d->dsView; }

/*!
    Returns the sample count as a \c VkSampleCountFlagBits, for pipelines that draw into the default render pass.

    \sa setSampleCount()
*/
VkSampleCountFlagBits QVulkanWindow::sampleCountFlagBits() const { return m_d->sampleCount; }

/*!
    Returns the multisampled color image for swapchain image \a idx, or \c VK_NULL_HANDLE without
    multisampling.
*/
VkImage QVulkanWindow::msaaColorImage(int idx) const {
    return idx >= 0 && static_cast<std::size_t>(idx) < m_d->imageRes.size()
               ? m_d->imageRes[static_cast<std::size_t>(idx)].msaaImage
               : VK_NULL_HANDLE;
}

/*!
    Returns the view of msaaColorImage() \a idx, or \c VK_NULL_HANDLE without multisampling.
*/
VkImageView QVulkanWindow::msaaColorImageView(int idx) const {
    return idx >= 0 && static_cast<std::size_t>(idx) < m_d->imageRes.size()
               ? m_d->imageRes[static_cast<std::size_t>(idx)].msaaImageView
               : VK_NULL_HANDLE;
}

/*!
    Returns \c true if the swapchain images can be read back, so that grab() works.
*/
bool QVulkanWindow::supportsGrab() const { return m_d->swapChainSupportsReadBack; }

/*!
    Renders a frame through the renderer, which must call frameReady() before it returns, presents it and
    returns its pixels as a QImage::Format_RGB32 image, also emitted as frameGrabbed(). Returns a null image
    if the window is not valid, a frame is pending or supportsGrab() is \c false. Rendering is waited for, so
    use it for tests and screenshots, not every frame.
*/
QImage QVulkanWindow::grab() {
    if (!isValid()) {
        qWarning("QVulkanWindow: Attempted to call grab() without a valid window");
        return {};
    }
    if (m_d->framePending) {
        qWarning("QVulkanWindow: Attempted to call grab() while a frame is still pending");
        return {};
    }
    if (!m_d->swapChainSupportsReadBack) {
        qWarning("QVulkanWindow: Attempted to call grab() with a swapchain that does not support reading back");
        return {};
    }
    m_d->frameGrabTargetImage = QImage();
    m_d->frameGrabbing = true;
    m_d->beginFrame(this);
    if (m_d->framePending) {
        qWarning("QVulkanWindow: grab() needs the renderer to call frameReady() from startNextFrame()");
        m_d->frameGrabbing = false;
        return {};
    }
    const QImage image = m_d->frameGrabTargetImage;
    if (!image.isNull())
        emit frameGrabbed(image);
    return image;
}

/*!
    Returns the matrix that turns OpenGL clip coordinates into Vulkan's: it flips Y and maps depth from
    -1..1 to 0..1. Multiply the projection matrix by it, as in \c{clipCorrectionMatrix() * projection}.
*/
QMatrix4x4 QVulkanWindow::clipCorrectionMatrix() { return m_d->clipCorrect; }

/*!
    \reimp
*/
void QVulkanWindow::exposeEvent(QExposeEvent *event) {
    (void)event;
    if (isExposed()) {
        m_d->ensureStarted(this);
    } else if (!m_d->flags.testFlag(PersistentResources)) {
        m_d->reset();
    }
}

/*!
    \reimp
*/
void QVulkanWindow::resizeEvent(QResizeEvent *event) {
    (void)event;
    if (isExposed() && m_d->status == Data::StatusReady)
        requestUpdate(); // the next frame recreates the swapchain at the new size
}

/*!
    \reimp
*/
bool QVulkanWindow::event(QEvent *event) {
    if (event->type() == QEvent::UpdateRequest) {
        QWindow::event(event); // lets the next requestUpdate() post again
        m_d->beginFrame(this);
        return true;
    }
    return QWindow::event(event);
}

#include "moc_qvulkanwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc

#endif // QT_CONFIG(vulkan)
