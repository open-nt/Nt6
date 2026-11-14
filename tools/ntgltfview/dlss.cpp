// MIT License - see LICENSE
#include "dlss.h"

#include <QDir>

#include <cstdio>
#include <string>

#ifdef NT_HAVE_DLSS
#include <nvsdk_ngx_helpers.h>
#include <nvsdk_ngx_helpers_vk.h>
#endif

namespace Dlss {

const char *qualityName(Quality quality) {
    switch (quality) {
    case Quality::Dlaa:
        return "dlaa";
    case Quality::Quality:
        return "quality";
    case Quality::Balanced:
        return "balanced";
    case Quality::Performance:
        return "performance";
    }
    return "";
}

#ifdef NT_HAVE_DLSS

namespace {

NVSDK_NGX_PerfQuality_Value ngxQuality(Quality quality) {
    switch (quality) {
    case Quality::Dlaa:
        return NVSDK_NGX_PerfQuality_Value_DLAA;
    case Quality::Quality:
        return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case Quality::Balanced:
        return NVSDK_NGX_PerfQuality_Value_Balanced;
    case Quality::Performance:
        return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    }
    return NVSDK_NGX_PerfQuality_Value_Balanced;
}

std::wstring widen(const QString &text) {
    std::wstring wide;
    for (const QChar c : text)
        wide.push_back(static_cast<wchar_t>(c.unicode()));
    return wide;
}

NVSDK_NGX_Resource_VK resource(const ImageRef &image, bool readWrite) {
    return NVSDK_NGX_Create_ImageView_Resource_VK(image.view, image.image, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                                                  image.format, static_cast<unsigned int>(image.size.width()),
                                                  static_cast<unsigned int>(image.size.height()), readWrite);
}

} // namespace

bool builtIn() { return true; }

bool requiredExtensions(QByteArrayList *instanceExtensions, QByteArrayList *deviceExtensions) {
    unsigned int instanceCount = 0;
    unsigned int deviceCount = 0;
    const char **instanceNames = nullptr;
    const char **deviceNames = nullptr;
    if (NVSDK_NGX_FAILED(
            NVSDK_NGX_VULKAN_RequiredExtensions(&instanceCount, &instanceNames, &deviceCount, &deviceNames)))
        return false;
    for (unsigned int i = 0; i < instanceCount; ++i)
        instanceExtensions->append(instanceNames[i]);
    for (unsigned int i = 0; i < deviceCount; ++i)
        deviceExtensions->append(deviceNames[i]);
    return true;
}

Session::~Session() { shutdown(); }

bool Session::init(QVulkanInstance *instance, VkPhysicalDevice physicalDevice, VkDevice device,
                   const QString &libraryDir, QString *error) {
    const QString dataDir = QDir::tempPath() + QStringLiteral("/ntgltfview-ngx");
    QDir().mkpath(dataDir);
    const std::wstring data = widen(dataDir);
    const std::wstring library = widen(libraryDir);
    const wchar_t *paths[1] = {library.c_str()};
    NVSDK_NGX_FeatureCommonInfo info{};
    info.PathListInfo.Path = const_cast<wchar_t **>(paths);
    info.PathListInfo.Length = 1;
    auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(instance->getInstanceProcAddr("vkGetInstanceProcAddr"));
    auto gdpa = reinterpret_cast<PFN_vkGetDeviceProcAddr>(instance->getInstanceProcAddr("vkGetDeviceProcAddr"));
    NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_Init_with_ProjectID(
        "7d1c5a3e-2b64-4f0a-9e18-5c3a7b9d2e41", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "6.0", data.c_str(),
        instance->vkInstance(), physicalDevice, device, gipa, gdpa, &info, NVSDK_NGX_Version_API);
    if (NVSDK_NGX_FAILED(result)) {
        *error = QStringLiteral("NGX did not start (0x%1)").arg(static_cast<unsigned>(result), 0, 16);
        return false;
    }
    m_started = true;
    m_device = device;
    NVSDK_NGX_Parameter *params = nullptr;
    result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&params);
    if (NVSDK_NGX_FAILED(result)) {
        *error = QStringLiteral("NGX capabilities unavailable (0x%1)").arg(static_cast<unsigned>(result), 0, 16);
        return false;
    }
    m_params = params;
    int available = 0;
    int needsDriver = 0;
    NVSDK_NGX_Parameter_GetI(params, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    NVSDK_NGX_Parameter_GetI(params, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsDriver);
    m_available = available != 0;
    if (!m_available)
        *error = needsDriver ? QStringLiteral("the driver is too old for DLSS")
                             : QStringLiteral("this GPU has no DLSS Super Resolution");
    return m_available;
}

QSize Session::renderSize(const QSize &display, Quality quality) const {
    if (quality == Quality::Dlaa || !m_params)
        return display;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int maxWidth = 0;
    unsigned int maxHeight = 0;
    unsigned int minWidth = 0;
    unsigned int minHeight = 0;
    float sharpness = 0.0f;
    const NVSDK_NGX_Result result = NGX_DLSS_GET_OPTIMAL_SETTINGS(
        static_cast<NVSDK_NGX_Parameter *>(m_params), static_cast<unsigned int>(display.width()),
        static_cast<unsigned int>(display.height()), ngxQuality(quality), &width, &height, &maxWidth, &maxHeight,
        &minWidth, &minHeight, &sharpness);
    if (NVSDK_NGX_SUCCEED(result) && width > 0 && height > 0)
        return QSize(static_cast<int>(width), static_cast<int>(height));
    return QSize(display.width() * 58 / 100, display.height() * 58 / 100);
}

bool Session::create(VkCommandBuffer cmd, const QSize &render, const QSize &display, Quality quality) {
    release();
    NVSDK_NGX_DLSS_Create_Params params{};
    params.Feature.InWidth = static_cast<unsigned int>(render.width());
    params.Feature.InHeight = static_cast<unsigned int>(render.height());
    params.Feature.InTargetWidth = static_cast<unsigned int>(display.width());
    params.Feature.InTargetHeight = static_cast<unsigned int>(display.height());
    params.Feature.InPerfQualityValue = ngxQuality(quality);
    // The color is the tone mapped image, the motion vectors are at the render size and carry no jitter.
    params.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    NVSDK_NGX_Handle *handle = nullptr;
    const NVSDK_NGX_Result result = NGX_VULKAN_CREATE_DLSS_EXT1(m_device, cmd, 1, 1, &handle,
                                                                static_cast<NVSDK_NGX_Parameter *>(m_params), &params);
    if (NVSDK_NGX_FAILED(result)) {
        std::printf("ntgltfview: could not create DLSS (0x%x)\n", static_cast<unsigned>(result));
        return false;
    }
    m_feature = handle;
    m_evaluateFailed = false;
    return true;
}

void Session::release() {
    if (m_feature)
        NVSDK_NGX_VULKAN_ReleaseFeature(static_cast<NVSDK_NGX_Handle *>(m_feature));
    m_feature = nullptr;
}

bool Session::evaluate(VkCommandBuffer cmd, const Frame &frame) {
    if (!m_feature)
        return false;
    NVSDK_NGX_Resource_VK color = resource(frame.color, false);
    NVSDK_NGX_Resource_VK depth = resource(frame.depth, false);
    NVSDK_NGX_Resource_VK motion = resource(frame.motion, false);
    NVSDK_NGX_Resource_VK output = resource(frame.output, true);
    NVSDK_NGX_VK_DLSS_Eval_Params eval{};
    eval.Feature.pInColor = &color;
    eval.Feature.pInOutput = &output;
    eval.pInDepth = &depth;
    eval.pInMotionVectors = &motion;
    eval.InJitterOffsetX = frame.jitterX;
    eval.InJitterOffsetY = frame.jitterY;
    eval.InRenderSubrectDimensions = {static_cast<unsigned int>(frame.color.size.width()),
                                      static_cast<unsigned int>(frame.color.size.height())};
    eval.InReset = frame.reset ? 1 : 0;
    eval.InMVScaleX = 1.0f;
    eval.InMVScaleY = 1.0f;
    eval.InPreExposure = 1.0f;
    eval.InExposureScale = 1.0f;
    const NVSDK_NGX_Result result = NGX_VULKAN_EVALUATE_DLSS_EXT(cmd, static_cast<NVSDK_NGX_Handle *>(m_feature),
                                                                 static_cast<NVSDK_NGX_Parameter *>(m_params), &eval);
    if (NVSDK_NGX_FAILED(result)) {
        if (!m_evaluateFailed)
            std::printf("ntgltfview: DLSS evaluate failed (0x%x)\n", static_cast<unsigned>(result));
        m_evaluateFailed = true;
        return false;
    }
    return true;
}

void Session::shutdown() {
    release();
    if (m_params)
        NVSDK_NGX_VULKAN_DestroyParameters(static_cast<NVSDK_NGX_Parameter *>(m_params));
    m_params = nullptr;
    if (m_started)
        NVSDK_NGX_VULKAN_Shutdown1(m_device);
    m_started = false;
    m_available = false;
}

#else

bool builtIn() { return false; }

bool requiredExtensions(QByteArrayList *, QByteArrayList *) { return false; }

Session::~Session() = default;

bool Session::init(QVulkanInstance *, VkPhysicalDevice, VkDevice, const QString &, QString *error) {
    *error = QStringLiteral("ntgltfview was built without the DLSS SDK (configure with -DNT_DLSS_SDK_DIR)");
    return false;
}

QSize Session::renderSize(const QSize &display, Quality) const { return display; }

bool Session::create(VkCommandBuffer, const QSize &, const QSize &, Quality) { return false; }

void Session::release() {}

bool Session::evaluate(VkCommandBuffer, const Frame &) { return false; }

void Session::shutdown() {}

#endif

} // namespace Dlss
