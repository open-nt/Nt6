// MIT License - see LICENSE
//
// ntgltfview's side of NVIDIA DLSS Super Resolution, through the NGX SDK. The SDK is proprietary and is not part of
// Nt: configure with -DNT_DLSS_SDK_DIR=<a checkout of github.com/NVIDIA/DLSS> to build it in. Without it every
// function here says that DLSS is not there, and nothing else in the viewer changes.
#pragma once

#include <QByteArrayList>
#include <QSize>
#include <QString>
#include <QVulkanInstance>

class QVulkanInstance;

namespace Dlss {

// How far DLSS upscales: from the window's size (Dlaa, which only improves the anti-aliasing) down to a quarter.
enum class Quality { Dlaa, Quality, Balanced, Performance };

const char *qualityName(Quality quality);

// Whether the viewer was built with the SDK.
bool builtIn();

// The extensions NGX needs of the instance and of the device (the driver's own, such as VK_NVX_binary_import).
bool requiredExtensions(QByteArrayList *instanceExtensions, QByteArrayList *deviceExtensions);

// An image NGX reads or writes.
struct ImageRef {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    QSize size;
};

// What one frame hands to DLSS: the color, depth and motion vectors at the render size, and where the result goes.
struct Frame {
    ImageRef color;
    ImageRef depth;
    ImageRef motion;
    ImageRef output;
    float jitterX = 0.0f; // the sub-pixel offset of this frame's rays, in render pixels
    float jitterY = 0.0f;
    bool reset = false; // the history does not apply: the scene or the camera jumped
};

// NGX on one device, and the DLSS feature for one render and output size.
class Session {
public:
    Session() = default;
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    // Starts NGX on the device. `libraryDir` holds NVIDIA's libnvidia-ngx-dlss. On failure, `error` says why.
    bool init(QVulkanInstance *instance, VkPhysicalDevice physicalDevice, VkDevice device, const QString &libraryDir,
              QString *error);
    // Whether NGX runs and the GPU and driver have Super Resolution.
    bool available() const { return m_available; }
    // The size to render at for DLSS to produce `display`.
    QSize renderSize(const QSize &display, Quality quality) const;
    // Makes the feature for this size, recording its set-up into `cmd`; false if NGX refuses.
    bool create(VkCommandBuffer cmd, const QSize &render, const QSize &display, Quality quality);
    bool ready() const { return m_feature != nullptr; }
    void release();
    // Records the upscaling into `cmd`; the images must be in the general layout.
    bool evaluate(VkCommandBuffer cmd, const Frame &frame);
    // Ends NGX; the device must be idle.
    void shutdown();

private:
    VkDevice m_device = VK_NULL_HANDLE;
    bool m_started = false;
    bool m_available = false;
    bool m_evaluateFailed = false;
    void *m_params = nullptr;  // NVSDK_NGX_Parameter *
    void *m_feature = nullptr; // NVSDK_NGX_Handle *
};

} // namespace Dlss
