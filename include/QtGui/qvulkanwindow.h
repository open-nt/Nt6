// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#if QT_CONFIG(vulkan) || defined(Q_QDOC)

#include <QtCore/qlist.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qvulkaninstance.h>
#include <QtGui/qwindow.h>

#include <functional>
#include <memory>

class QVulkanWindow;

class QVulkanWindowRenderer {
public:
    virtual ~QVulkanWindowRenderer();

    virtual void preInitResources();
    virtual void initResources();
    virtual void initSwapChainResources();
    virtual void releaseSwapChainResources();
    virtual void releaseResources();

    virtual void startNextFrame() = 0;

    virtual void physicalDeviceLost();
    virtual void logicalDeviceLost();
};

class QVulkanWindow : public QWindow {
    Q_OBJECT
public:
    enum Flag { PersistentResources = 0x01 };
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit QVulkanWindow(QWindow *parent = nullptr);
    ~QVulkanWindow() override;

    void setFlags(Flags flags);
    Flags flags() const;

    QList<VkPhysicalDeviceProperties> availablePhysicalDevices();
    void setPhysicalDeviceIndex(int idx);

    QVulkanInfoVector<QVulkanExtension> supportedDeviceExtensions();
    void setDeviceExtensions(const QByteArrayList &extensions);

    using QueueCreateInfoModifier =
        std::function<void(const VkQueueFamilyProperties *, uint32_t, QList<VkDeviceQueueCreateInfo> &)>;
    void setQueueCreateInfoModifier(const QueueCreateInfoModifier &modifier);

    using EnabledFeaturesModifier = std::function<void(VkPhysicalDeviceFeatures &)>;
    void setEnabledFeaturesModifier(const EnabledFeaturesModifier &modifier);
    using EnabledFeatures2Modifier = std::function<void(VkPhysicalDeviceFeatures2 &)>;
    void setEnabledFeaturesModifier(EnabledFeatures2Modifier modifier);

    void setPreferredColorFormats(const QList<VkFormat> &formats);

    QList<int> supportedSampleCounts();
    void setSampleCount(int sampleCount);

    bool isValid() const;

    virtual QVulkanWindowRenderer *createRenderer();
    void frameReady();

    VkPhysicalDevice physicalDevice() const;
    const VkPhysicalDeviceProperties *physicalDeviceProperties() const;
    VkDevice device() const;
    VkQueue graphicsQueue() const;
    uint32_t graphicsQueueFamilyIndex() const;
    VkCommandPool graphicsCommandPool() const;
    uint32_t hostVisibleMemoryIndex() const;
    uint32_t deviceLocalMemoryIndex() const;
    VkRenderPass defaultRenderPass() const;

    VkFormat colorFormat() const;
    VkFormat depthStencilFormat() const;
    QSize swapChainImageSize() const;

    VkCommandBuffer currentCommandBuffer() const;
    VkFramebuffer currentFramebuffer() const;
    int currentFrame() const;

    static const int MAX_CONCURRENT_FRAME_COUNT = 3;
    int concurrentFrameCount() const;

    int swapChainImageCount() const;
    int currentSwapChainImageIndex() const;
    VkImage swapChainImage(int idx) const;
    VkImageView swapChainImageView(int idx) const;
    VkImage depthStencilImage() const;
    VkImageView depthStencilImageView() const;
    VkSampleCountFlagBits sampleCountFlagBits() const;
    VkImage msaaColorImage(int idx) const;
    VkImageView msaaColorImageView(int idx) const;

    bool supportsGrab() const;
    QImage grab();

    QMatrix4x4 clipCorrectionMatrix();

signals:
    /*!
        This signal is emitted when grab() has read a frame back into \a image.
    */
    void frameGrabbed(const QImage &image) { emitSignal(&QVulkanWindow::frameGrabbed, image); }

protected:
    void exposeEvent(QExposeEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    Q_DISABLE_COPY(QVulkanWindow)
    struct Data;
    std::unique_ptr<Data> m_d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QVulkanWindow::Flags)

#endif // QT_CONFIG(vulkan)
