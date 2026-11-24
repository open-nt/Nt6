// MIT License - see LICENSE
// The Vulkan backend of QRhi. Frames are offscreen and synchronous: endOffscreenFrame() submits the command buffer and
// waits for it, so there is one frame in flight and nothing to track across frames.
#include "qrhi_p.h"

#if QT_CONFIG(vulkan)

#include <QtCore/qdebug.h>
#include <QtGui/qvulkaninstance.h>
#include <rhi/qrhivulkan.h>

#include <algorithm>
#include <cstring>
#include <map>

namespace {

class VulkanRhi;

quint32 u32(std::size_t v) { return static_cast<quint32>(v); }

VkFormat vkFormatFor(QRhiTexture::Format format, bool srgb) {
    switch (format) {
    case QRhiTexture::RGBA8:
        return srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    case QRhiTexture::BGRA8:
        return srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    case QRhiTexture::R8:
        return VK_FORMAT_R8_UNORM;
    case QRhiTexture::RG8:
        return VK_FORMAT_R8G8_UNORM;
    case QRhiTexture::R16:
        return VK_FORMAT_R16_UNORM;
    case QRhiTexture::RG16:
        return VK_FORMAT_R16G16_UNORM;
    case QRhiTexture::RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case QRhiTexture::RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case QRhiTexture::R16F:
        return VK_FORMAT_R16_SFLOAT;
    case QRhiTexture::R32F:
        return VK_FORMAT_R32_SFLOAT;
    case QRhiTexture::RGB10A2:
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case QRhiTexture::D16:
        return VK_FORMAT_D16_UNORM;
    case QRhiTexture::D24:
        return VK_FORMAT_X8_D24_UNORM_PACK32;
    case QRhiTexture::D24S8:
        return VK_FORMAT_D24_UNORM_S8_UINT;
    case QRhiTexture::D32F:
        return VK_FORMAT_D32_SFLOAT;
    case QRhiTexture::UnknownFormat:
        break;
    }
    return VK_FORMAT_UNDEFINED;
}

bool hasStencil(QRhiTexture::Format format) { return format == QRhiTexture::D24S8; }

VkImageAspectFlags aspectFor(QRhiTexture::Format format) {
    if (!QRhiImplementation::isDepthFormat(format))
        return VK_IMAGE_ASPECT_COLOR_BIT;
    return hasStencil(format) ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT) : VK_IMAGE_ASPECT_DEPTH_BIT;
}

// The aspect for sampling and copies: depth alone for a depth-stencil format.
VkImageAspectFlags samplingAspect(QRhiTexture::Format format) {
    return QRhiImplementation::isDepthFormat(format) ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT)
                                                     : static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT);
}

VkSampleCountFlagBits samplesFor(int count) {
    switch (count) {
    case 2:
        return VK_SAMPLE_COUNT_2_BIT;
    case 4:
        return VK_SAMPLE_COUNT_4_BIT;
    case 8:
        return VK_SAMPLE_COUNT_8_BIT;
    case 16:
        return VK_SAMPLE_COUNT_16_BIT;
    default:
        return VK_SAMPLE_COUNT_1_BIT;
    }
}

VkShaderStageFlags stageFlagsFor(QRhiShaderResourceBinding::StageFlags stages) {
    VkShaderStageFlags out = 0;
    if (stages.testFlag(QRhiShaderResourceBinding::VertexStage))
        out |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages.testFlag(QRhiShaderResourceBinding::TessellationControlStage))
        out |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages.testFlag(QRhiShaderResourceBinding::TessellationEvaluationStage))
        out |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages.testFlag(QRhiShaderResourceBinding::GeometryStage))
        out |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages.testFlag(QRhiShaderResourceBinding::FragmentStage))
        out |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages.testFlag(QRhiShaderResourceBinding::ComputeStage))
        out |= VK_SHADER_STAGE_COMPUTE_BIT;
    return out;
}

VkCompareOp compareOpFor(QRhiGraphicsPipeline::CompareOp op) {
    static const VkCompareOp map[] = {VK_COMPARE_OP_NEVER,
                                      VK_COMPARE_OP_LESS,
                                      VK_COMPARE_OP_EQUAL,
                                      VK_COMPARE_OP_LESS_OR_EQUAL,
                                      VK_COMPARE_OP_GREATER,
                                      VK_COMPARE_OP_NOT_EQUAL,
                                      VK_COMPARE_OP_GREATER_OR_EQUAL,
                                      VK_COMPARE_OP_ALWAYS};
    return map[op];
}

VkStencilOp stencilOpFor(QRhiGraphicsPipeline::StencilOp op) {
    static const VkStencilOp map[] = {VK_STENCIL_OP_ZERO,
                                      VK_STENCIL_OP_KEEP,
                                      VK_STENCIL_OP_REPLACE,
                                      VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                      VK_STENCIL_OP_DECREMENT_AND_CLAMP,
                                      VK_STENCIL_OP_INVERT,
                                      VK_STENCIL_OP_INCREMENT_AND_WRAP,
                                      VK_STENCIL_OP_DECREMENT_AND_WRAP};
    return map[op];
}

VkBlendFactor blendFactorFor(QRhiGraphicsPipeline::BlendFactor f) {
    static const VkBlendFactor map[] = {VK_BLEND_FACTOR_ZERO,
                                        VK_BLEND_FACTOR_ONE,
                                        VK_BLEND_FACTOR_SRC_COLOR,
                                        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                                        VK_BLEND_FACTOR_DST_COLOR,
                                        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                                        VK_BLEND_FACTOR_SRC_ALPHA,
                                        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                        VK_BLEND_FACTOR_DST_ALPHA,
                                        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
                                        VK_BLEND_FACTOR_CONSTANT_COLOR,
                                        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
                                        VK_BLEND_FACTOR_CONSTANT_ALPHA,
                                        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
                                        VK_BLEND_FACTOR_SRC_ALPHA_SATURATE};
    return map[f];
}

VkBlendOp blendOpFor(QRhiGraphicsPipeline::BlendOp op) {
    static const VkBlendOp map[] = {VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT, VK_BLEND_OP_REVERSE_SUBTRACT,
                                    VK_BLEND_OP_MIN, VK_BLEND_OP_MAX};
    return map[op];
}

VkFormat vertexFormatFor(QRhiVertexInputAttribute::Format f) {
    static const VkFormat map[] = {
        VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32_SFLOAT,
        VK_FORMAT_R8G8B8A8_UNORM,      VK_FORMAT_R8G8_UNORM,       VK_FORMAT_R8_UNORM,      VK_FORMAT_R32G32B32A32_UINT,
        VK_FORMAT_R32G32B32_UINT,      VK_FORMAT_R32G32_UINT,      VK_FORMAT_R32_UINT,      VK_FORMAT_R32G32B32A32_SINT,
        VK_FORMAT_R32G32B32_SINT,      VK_FORMAT_R32G32_SINT,      VK_FORMAT_R32_SINT};
    return map[f];
}

// ---------------------------------------------------------------------------------------------------------------------

class VulkanBuffer;
class VulkanTexture;
class VulkanCommandBuffer;
class VulkanSampler;
class VulkanRenderBuffer;
class VulkanRenderPassDescriptor;
class VulkanTextureRenderTarget;
class VulkanShaderResourceBindings;
class VulkanGraphicsPipeline;
class VulkanComputePipeline;

class VulkanRhi final : public QRhiImplementation {
public:
    ~VulkanRhi() override;
    bool create(QRhi::Flags flags) override;
    void destroy() override;
    QRhi::Implementation backend() const override { return QRhi::Vulkan; }
    QString driverInfo() const override {
        return QString::fromUtf8(physProps.deviceName) + QStringLiteral(" (Vulkan %1.%2)")
                                                             .arg(VK_API_VERSION_MAJOR(physProps.apiVersion))
                                                             .arg(VK_API_VERSION_MINOR(physProps.apiVersion));
    }

    QRhiGraphicsPipeline *createGraphicsPipeline() override;
    QRhiComputePipeline *createComputePipeline() override;
    QRhiShaderResourceBindings *createShaderResourceBindings() override;
    QRhiBuffer *createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size) override;
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount) override;
    QRhiTexture *createTexture(QRhiTexture::Format format, const QSize &pixelSize, int depth, int arraySize,
                               int sampleCount, QRhiTexture::Flags textureFlags) override;
    QRhiSampler *createSampler(QRhiSampler::Filter mag, QRhiSampler::Filter min, QRhiSampler::Filter mip,
                               QRhiSampler::AddressMode u, QRhiSampler::AddressMode v,
                               QRhiSampler::AddressMode w) override;
    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags targetFlags) override;

    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **out, QRhi::BeginFrameFlags) override;
    QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags) override;
    QRhi::FrameOpResult finish() override {
        if (dev != VK_NULL_HANDLE)
            df->vkDeviceWaitIdle(dev);
        return QRhi::FrameOpSuccess;
    }

    QList<int> supportedSampleCounts() const override {
        QList<int> counts;
        const VkSampleCountFlags mask =
            physProps.limits.framebufferColorSampleCounts & physProps.limits.framebufferDepthSampleCounts;
        for (int c : {1, 2, 4, 8, 16})
            if (mask & static_cast<VkSampleCountFlags>(samplesFor(c)))
                counts.append(c);
        return counts;
    }
    int ubufAlignment() const override { return static_cast<int>(physProps.limits.minUniformBufferOffsetAlignment); }
    bool isYUpInFramebuffer() const override { return false; }
    bool isYUpInNDC() const override { return false; }
    bool isClipDepthZeroToOne() const override { return true; }
    QMatrix4x4 clipSpaceCorrMatrix() const override {
        // From OpenGL clip space: y points down and z goes from 0 to 1.
        QMatrix4x4 m;
        m(1, 1) = -1.0f;
        m(2, 2) = 0.5f;
        m(2, 3) = 0.5f;
        return m;
    }
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags textureFlags) const override;
    bool isFeatureSupported(QRhi::Feature feature) const override;
    int resourceLimit(QRhi::ResourceLimit limit) const override;
    const QRhiNativeHandles *nativeHandles() override { return &handles; }
    bool isDeviceLost() const override { return deviceLost; }
    void releaseCachedResources() override {}
    int currentFrameSlot() const override { return 0; }

    void executeUpdates(QRhiCommandBuffer *cbase, QRhiResourceUpdateBatch *batch) override;

    // ---- helpers ----
    quint32 findMemoryType(quint32 typeBits, VkMemoryPropertyFlags wanted) const {
        for (quint32 i = 0; i < memProps.memoryTypeCount; ++i)
            if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & wanted) == wanted)
                return i;
        return UINT32_MAX;
    }
    bool allocate(const VkMemoryRequirements &req, VkMemoryPropertyFlags wanted, VkDeviceMemory *memory) {
        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = req.size;
        info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, wanted);
        if (info.memoryTypeIndex == UINT32_MAX && (wanted & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
            info.memoryTypeIndex = findMemoryType(
                req.memoryTypeBits, wanted & ~static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
        if (info.memoryTypeIndex == UINT32_MAX) {
            qWarning("QRhiVulkan: no memory type for the resource");
            return false;
        }
        const VkResult result = df->vkAllocateMemory(dev, &info, nullptr, memory);
        if (result != VK_SUCCESS) {
            qWarning("QRhiVulkan: vkAllocateMemory failed: %d", result);
            return false;
        }
        return true;
    }
    bool makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer *buffer,
                    VkDeviceMemory *memory, void **mapped);
    void destroyBuffer(VkBuffer *buffer, VkDeviceMemory *memory, void **mapped) {
        if (*mapped)
            df->vkUnmapMemory(dev, *memory);
        *mapped = nullptr;
        if (*buffer != VK_NULL_HANDLE)
            df->vkDestroyBuffer(dev, *buffer, nullptr);
        if (*memory != VK_NULL_HANDLE)
            df->vkFreeMemory(dev, *memory, nullptr);
        *buffer = VK_NULL_HANDLE;
        *memory = VK_NULL_HANDLE;
    }
    void transition(VulkanTexture *tex, VkImageLayout newLayout, bool discard = false);
    void transitionImage(VkImage image, VkImageAspectFlags aspect, int levels, int layers, VkImageLayout from,
                         VkImageLayout to);
    VkImageLayout restingLayout(const VulkanTexture *tex) const;
    bool oneShot(const std::function<void(VkCommandBuffer)> &record);

    struct Staging {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void *mapped = nullptr;
    };
    struct PendingRead {
        Staging staging;
        QRhiReadbackResult *result = nullptr;
        QRhiTexture::Format format = QRhiTexture::UnknownFormat;
        QSize size;
        quint32 bytes = 0;
        VulkanBuffer *buffer = nullptr; // for a read of a dynamic buffer, which is read in place
        quint32 offset = 0;
    };

    QVulkanInstance *inst = nullptr;
    QVulkanFunctions *f = nullptr;
    QVulkanDeviceFunctions *df = nullptr;
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    VkDevice dev = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    quint32 queueFamily = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physProps = {};
    VkPhysicalDeviceMemoryProperties memProps = {};
    VkPhysicalDeviceFeatures enabledFeatures = {};
    QRhiVulkanNativeHandles handles;
    bool ownsDevice = true;
    bool deviceLost = false;
    bool inFrame = false;
    std::unique_ptr<VulkanCommandBuffer> cmd;
    std::vector<Staging> staging;
    std::vector<PendingRead> pendingReads;
    QByteArrayList requestedExtensions;
};

class VulkanBuffer final : public QRhiBuffer {
public:
    VulkanBuffer(VulkanRhi *rhi, Type type, UsageFlags usage, quint32 size);
    ~VulkanBuffer() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Buffer; }
    void destroy() override;
    bool create() override;
    NativeBuffer nativeBuffer() override {
        NativeBuffer nb;
        nb.objects[0] = reinterpret_cast<const void *>(buffer);
        nb.slotCount = 1;
        return nb;
    }

    VulkanRhi *vk;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr; // for dynamic buffers
};

class VulkanTexture final : public QRhiTexture {
public:
    VulkanTexture(VulkanRhi *rhi, Format format, const QSize &size, int depth, int arraySize, int sampleCount,
                  Flags flags);
    ~VulkanTexture() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Texture; }
    void destroy() override;
    bool create() override;
    NativeTexture nativeTexture() override {
        NativeTexture nt;
        nt.object = reinterpret_cast<const void *>(image);
        nt.layout = static_cast<int>(layout);
        return nt;
    }
    bool createFrom(NativeTexture src) override;

    int layerCount() const {
        return flags().testFlag(CubeMap) ? 6 : (flags().testFlag(TextureArray) ? std::max(1, arraySize()) : 1);
    }
    int levelCount() const { return flags().testFlag(MipMapped) ? QRhi::mipLevelsForSize(pixelSize()) : 1; }
    VkImageView attachmentView(int layer, int level);

    VulkanRhi *vk;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::map<int, VkImageView> attachmentViews;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
    bool owned = true;
};

class VulkanSampler final : public QRhiSampler {
public:
    VulkanSampler(VulkanRhi *rhi, Filter mag, Filter min, Filter mip, AddressMode u, AddressMode v, AddressMode w)
        : QRhiSampler(rhi, mag, min, mip, u, v, w), vk(rhi) {}
    ~VulkanSampler() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Sampler; }
    void destroy() override;
    bool create() override;

    VulkanRhi *vk;
    VkSampler sampler = VK_NULL_HANDLE;
};

class VulkanRenderBuffer final : public QRhiRenderBuffer {
public:
    VulkanRenderBuffer(VulkanRhi *rhi, Type type, const QSize &size, int samples)
        : QRhiRenderBuffer(rhi, type, size, samples), vk(rhi) {}
    ~VulkanRenderBuffer() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::RenderBuffer; }
    void destroy() override;
    bool create() override;

    VulkanRhi *vk;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat vkFormat = VK_FORMAT_UNDEFINED;
};

class VulkanRenderPassDescriptor final : public QRhiRenderPassDescriptor {
public:
    VulkanRenderPassDescriptor(VulkanRhi *rhi, const QRhiTextureRenderTargetDescription &desc,
                               QRhiTextureRenderTarget::Flags flags)
        : QRhiRenderPassDescriptor(rhi), vk(rhi), description(desc), targetFlags(flags) {}
    ~VulkanRenderPassDescriptor() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::RenderPassDescriptor; }
    void destroy() override;
    bool create() override;
    bool isCompatible(const QRhiRenderPassDescriptor *other) const override;

    VulkanRhi *vk;
    QRhiTextureRenderTargetDescription description;
    QRhiTextureRenderTarget::Flags targetFlags;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFormat> colorFormats;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    int samples = 1;
    int colorCount = 0;
    bool hasDepth = false;
    int resolveCount = 0;
};

class VulkanTextureRenderTarget final : public QRhiTextureRenderTarget {
public:
    VulkanTextureRenderTarget(VulkanRhi *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags)
        : QRhiTextureRenderTarget(rhi, desc, flags), vk(rhi) {}
    ~VulkanTextureRenderTarget() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::TextureRenderTarget; }
    void destroy() override;
    QSize pixelSize() const override { return size; }
    float devicePixelRatio() const override { return 1.0f; }
    int sampleCount() const override { return samples; }
    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override;
    bool create() override;

    VulkanRhi *vk;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    QSize size;
    int samples = 1;
};

class VulkanShaderResourceBindings final : public QRhiShaderResourceBindings {
public:
    VulkanShaderResourceBindings(VulkanRhi *rhi) : QRhiShaderResourceBindings(rhi), vk(rhi) {}
    ~VulkanShaderResourceBindings() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::ShaderResourceBindings; }
    void destroy() override;
    bool create() override;

    VulkanRhi *vk;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};

class VulkanGraphicsPipeline final : public QRhiGraphicsPipeline {
public:
    explicit VulkanGraphicsPipeline(VulkanRhi *rhi) : QRhiGraphicsPipeline(rhi), vk(rhi) {}
    ~VulkanGraphicsPipeline() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::GraphicsPipeline; }
    void destroy() override;
    bool create() override;

    VulkanRhi *vk;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

class VulkanComputePipeline final : public QRhiComputePipeline {
public:
    explicit VulkanComputePipeline(VulkanRhi *rhi) : QRhiComputePipeline(rhi), vk(rhi) {}
    ~VulkanComputePipeline() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::ComputePipeline; }
    void destroy() override;
    bool create() override;

    VulkanRhi *vk;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

class VulkanCommandBuffer final : public QRhiCommandBuffer {
public:
    explicit VulkanCommandBuffer(VulkanRhi *rhi) : QRhiCommandBuffer(rhi), vk(rhi) {}
    ~VulkanCommandBuffer() override = default;
    QRhiResource::Type resourceType() const override { return QRhiResource::CommandBuffer; }
    void destroy() override {}

    void resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates) override;
    void beginPass(QRhiRenderTarget *rt, const QColor &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue, QRhiResourceUpdateBatch *resourceUpdates,
                   BeginPassFlags flags) override;
    void endPass(QRhiResourceUpdateBatch *resourceUpdates) override;
    void setGraphicsPipeline(QRhiGraphicsPipeline *ps) override;
    void setShaderResources(QRhiShaderResourceBindings *srb, int dynamicOffsetCount,
                            const std::pair<int, quint32> *dynamicOffsets) override;
    void setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings, QRhiBuffer *indexBuf,
                        quint32 indexOffset, IndexFormat indexFormat) override;
    void setViewport(const QRhiViewport &viewport) override;
    void setScissor(const QRhiScissor &scissor) override;
    void setBlendConstants(const QColor &c) override;
    void setStencilRef(quint32 refValue) override;
    void draw(quint32 vertexCount, quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) override;
    void drawIndexed(quint32 indexCount, quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset,
                     quint32 firstInstance) override;
    void beginComputePass(QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags flags) override;
    void endComputePass(QRhiResourceUpdateBatch *resourceUpdates) override;
    void setComputePipeline(QRhiComputePipeline *ps) override;
    void dispatch(int x, int y, int z) override;

    VulkanRhi *vk;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    bool recording = false;
    bool inPass = false;
    bool inCompute = false;
    VulkanGraphicsPipeline *graphics = nullptr;
    VulkanComputePipeline *compute = nullptr;
    VulkanTextureRenderTarget *target = nullptr;
    VulkanRenderPassDescriptor *targetPass = nullptr;
};

VulkanRhi::~VulkanRhi() = default;

QRhiGraphicsPipeline *VulkanRhi::createGraphicsPipeline() { return new VulkanGraphicsPipeline(this); }
QRhiComputePipeline *VulkanRhi::createComputePipeline() { return new VulkanComputePipeline(this); }
QRhiShaderResourceBindings *VulkanRhi::createShaderResourceBindings() { return new VulkanShaderResourceBindings(this); }
QRhiBuffer *VulkanRhi::createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size) {
    return new VulkanBuffer(this, type, usage, size);
}
QRhiRenderBuffer *VulkanRhi::createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount) {
    return new VulkanRenderBuffer(this, type, pixelSize, sampleCount);
}
QRhiTexture *VulkanRhi::createTexture(QRhiTexture::Format format, const QSize &pixelSize, int depth, int arraySize,
                                      int sampleCount, QRhiTexture::Flags textureFlags) {
    return new VulkanTexture(this, format, pixelSize, depth, arraySize, sampleCount, textureFlags);
}
QRhiSampler *VulkanRhi::createSampler(QRhiSampler::Filter mag, QRhiSampler::Filter min, QRhiSampler::Filter mip,
                                      QRhiSampler::AddressMode u, QRhiSampler::AddressMode v,
                                      QRhiSampler::AddressMode w) {
    return new VulkanSampler(this, mag, min, mip, u, v, w);
}
QRhiTextureRenderTarget *VulkanRhi::createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                              QRhiTextureRenderTarget::Flags targetFlags) {
    return new VulkanTextureRenderTarget(this, desc, targetFlags);
}
VkImageLayout VulkanRhi::restingLayout(const VulkanTexture *tex) const {
    return QRhiImplementation::isDepthFormat(tex->format()) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

// ---------------------------------------------------------------------------------------------------------------------
// Device

bool VulkanRhi::create(QRhi::Flags flags) {
    if (!inst || !inst->isValid()) {
        qWarning("QRhi::create: the QVulkanInstance is not valid");
        return false;
    }
    f = inst->functions();
    if (handles.dev != VK_NULL_HANDLE) { // imported
        physDev = handles.physDev;
        dev = handles.dev;
        queueFamily = handles.gfxQueueFamilyIdx;
        queue = handles.gfxQueue;
        ownsDevice = false;
        f->vkGetPhysicalDeviceProperties(physDev, &physProps);
    } else {
        quint32 count = 0;
        f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        f->vkEnumeratePhysicalDevices(inst->vkInstance(), &count, devices.data());
        int bestScore = -1;
        for (VkPhysicalDevice candidate : devices) {
            VkPhysicalDeviceProperties props;
            f->vkGetPhysicalDeviceProperties(candidate, &props);
            quint32 families = 0;
            f->vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, nullptr);
            std::vector<VkQueueFamilyProperties> familyProps(families);
            f->vkGetPhysicalDeviceQueueFamilyProperties(candidate, &families, familyProps.data());
            int family = -1;
            for (quint32 i = 0; i < families; ++i)
                if ((familyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                    (familyProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                    family = static_cast<int>(i);
                    break;
                }
            if (family < 0)
                continue;
            int score = 1;
            const bool software = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
            if (flags.testFlag(QRhi::PreferSoftwareRenderer))
                score += software ? 100 : 0;
            else
                score += props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 100 : (software ? 0 : 50);
            if (score > bestScore) {
                bestScore = score;
                physDev = candidate;
                queueFamily = static_cast<quint32>(family);
                physProps = props;
            }
        }
        if (physDev == VK_NULL_HANDLE) {
            qWarning("QRhi::create: no Vulkan device with a graphics and compute queue");
            return false;
        }
        VkPhysicalDeviceFeatures supported = {};
        f->vkGetPhysicalDeviceFeatures(physDev, &supported);
        enabledFeatures.fillModeNonSolid = supported.fillModeNonSolid;
        enabledFeatures.wideLines = supported.wideLines;
        enabledFeatures.samplerAnisotropy = supported.samplerAnisotropy;
        enabledFeatures.geometryShader = supported.geometryShader;
        enabledFeatures.tessellationShader = supported.tessellationShader;
        // Extensions the application asked for, if the device has them.
        quint32 extCount = 0;
        f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &extCount, exts.data());
        std::vector<const char *> enabled;
        for (const QByteArray &name : std::as_const(requestedExtensions)) {
            const bool has = std::any_of(exts.begin(), exts.end(),
                                         [&](const VkExtensionProperties &e) { return name == e.extensionName; });
            if (has)
                enabled.push_back(name.constData());
            else
                qWarning("QRhi::create: the device does not have the extension %s", name.constData());
        }
        const float priority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        VkDeviceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queueInfo;
        info.enabledExtensionCount = u32(enabled.size());
        info.ppEnabledExtensionNames = enabled.data();
        info.pEnabledFeatures = &enabledFeatures;
        const VkResult result = f->vkCreateDevice(physDev, &info, nullptr, &dev);
        if (result != VK_SUCCESS) {
            qWarning("QRhi::create: vkCreateDevice failed: %d", result);
            return false;
        }
    }
    df = inst->deviceFunctions(dev);
    if (!df)
        return false;
    if (ownsDevice)
        df->vkGetDeviceQueue(dev, queueFamily, 0, &queue);
    f->vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamily;
    if (df->vkCreateCommandPool(dev, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        return false;
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (df->vkCreateFence(dev, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
        return false;

    cmd = std::make_unique<VulkanCommandBuffer>(this);
    unregisterResource(cmd.get());
    VkCommandBufferAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    if (df->vkAllocateCommandBuffers(dev, &alloc, &cmd->cb) != VK_SUCCESS)
        return false;

    handles.physDev = physDev;
    handles.dev = dev;
    handles.gfxQueueFamilyIdx = queueFamily;
    handles.gfxQueueIdx = 0;
    handles.gfxQueue = queue;
    return true;
}

void VulkanRhi::destroy() {
    if (dev == VK_NULL_HANDLE)
        return;
    df->vkDeviceWaitIdle(dev);
    for (Staging &s : staging)
        destroyBuffer(&s.buffer, &s.memory, &s.mapped);
    staging.clear();
    for (PendingRead &p : pendingReads)
        destroyBuffer(&p.staging.buffer, &p.staging.memory, &p.staging.mapped);
    pendingReads.clear();
    if (cmd && cmd->cb != VK_NULL_HANDLE)
        df->vkFreeCommandBuffers(dev, pool, 1, &cmd->cb);
    cmd.reset();
    if (fence != VK_NULL_HANDLE)
        df->vkDestroyFence(dev, fence, nullptr);
    if (pool != VK_NULL_HANDLE)
        df->vkDestroyCommandPool(dev, pool, nullptr);
    fence = VK_NULL_HANDLE;
    pool = VK_NULL_HANDLE;
    if (ownsDevice)
        df->vkDestroyDevice(dev, nullptr);
    inst->resetDeviceFunctions(dev);
    dev = VK_NULL_HANDLE;
}

bool VulkanRhi::makeBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer *buffer,
                           VkDeviceMemory *memory, void **mapped) {
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (df->vkCreateBuffer(dev, &info, nullptr, buffer) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req;
    df->vkGetBufferMemoryRequirements(dev, *buffer, &req);
    if (!allocate(req, props, memory)) {
        df->vkDestroyBuffer(dev, *buffer, nullptr);
        *buffer = VK_NULL_HANDLE;
        return false;
    }
    df->vkBindBufferMemory(dev, *buffer, *memory, 0);
    if (mapped) {
        if (df->vkMapMemory(dev, *memory, 0, VK_WHOLE_SIZE, 0, mapped) != VK_SUCCESS) {
            destroyBuffer(buffer, memory, mapped);
            return false;
        }
    }
    return true;
}

void VulkanRhi::transitionImage(VkImage image, VkImageAspectFlags aspect, int levels, int layers, VkImageLayout from,
                                VkImageLayout to) {
    if (from == to)
        return;
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {aspect, 0, static_cast<quint32>(levels), 0, static_cast<quint32>(layers)};
    barrier.srcAccessMask =
        from == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : (VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT);
    barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    df->vkCmdPipelineBarrier(cmd->cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                             nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRhi::transition(VulkanTexture *tex, VkImageLayout newLayout, bool discard) {
    const VkImageLayout from = discard ? VK_IMAGE_LAYOUT_UNDEFINED : tex->layout;
    transitionImage(tex->image, aspectFor(tex->format()), tex->levelCount(), tex->layerCount(), from, newLayout);
    tex->layout = newLayout;
}

// Records and runs a command buffer of its own, for work outside of a frame (such as filling a descriptor image layout).
bool VulkanRhi::oneShot(const std::function<void(VkCommandBuffer)> &record) {
    VkCommandBufferAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VkCommandBuffer one = VK_NULL_HANDLE;
    if (df->vkAllocateCommandBuffers(dev, &alloc, &one) != VK_SUCCESS)
        return false;
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    df->vkBeginCommandBuffer(one, &begin);
    record(one);
    df->vkEndCommandBuffer(one);
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &one;
    df->vkResetFences(dev, 1, &fence);
    const bool ok = df->vkQueueSubmit(queue, 1, &submit, fence) == VK_SUCCESS &&
                    df->vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS;
    df->vkFreeCommandBuffers(dev, pool, 1, &one);
    return ok;
}

bool VulkanRhi::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags textureFlags) const {
    const VkFormat vkf = vkFormatFor(format, textureFlags.testFlag(QRhiTexture::sRGB));
    if (vkf == VK_FORMAT_UNDEFINED)
        return false;
    VkFormatProperties props;
    f->vkGetPhysicalDeviceFormatProperties(physDev, vkf, &props);
    VkFormatFeatureFlags need = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (textureFlags.testFlag(QRhiTexture::RenderTarget))
        need |= QRhiImplementation::isDepthFormat(format)
                    ? static_cast<VkFormatFeatureFlags>(VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                    : static_cast<VkFormatFeatureFlags>(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);
    return (props.optimalTilingFeatures & need) == need;
}

bool VulkanRhi::isFeatureSupported(QRhi::Feature feature) const {
    switch (feature) {
    case QRhi::MultisampleTexture:
    case QRhi::MultisampleRenderBuffer:
    case QRhi::Instancing:
    case QRhi::NonDynamicUniformBuffers:
    case QRhi::NPOTTextureRepeat:
    case QRhi::RedOrAlpha8IsRed:
    case QRhi::ElementIndexUint:
    case QRhi::Compute:
    case QRhi::BaseVertex:
    case QRhi::BaseInstance:
    case QRhi::TriangleFanTopology:
    case QRhi::ReadBackNonUniformBuffer:
    case QRhi::ReadBackNonBaseMipLevel:
    case QRhi::TexelFetch:
    case QRhi::RenderToNonBaseMipLevel:
    case QRhi::IntAttributes:
    case QRhi::ScreenSpaceDerivatives:
    case QRhi::ReadBackAnyTextureFormat:
    case QRhi::ImageDataStride:
    case QRhi::ThreeDimensionalTextures:
    case QRhi::TextureArrays:
    case QRhi::OneDimensionalTextures:
    case QRhi::OneDimensionalTextureMipmaps:
    case QRhi::ThreeDimensionalTextureMipmaps:
    case QRhi::CustomInstanceStepRate:
    case QRhi::VertexShaderPointSize:
    case QRhi::PrimitiveRestart:
    case QRhi::NonFourAlignedEffectiveIndexBufferOffset:
        return true;
    case QRhi::WideLines:
        return enabledFeatures.wideLines != VK_FALSE;
    case QRhi::NonFillPolygonMode:
        return enabledFeatures.fillModeNonSolid != VK_FALSE;
    case QRhi::Tessellation:
        return enabledFeatures.tessellationShader != VK_FALSE;
    case QRhi::GeometryShader:
        return enabledFeatures.geometryShader != VK_FALSE;
    default:
        return false;
    }
}

int VulkanRhi::resourceLimit(QRhi::ResourceLimit limit) const {
    const VkPhysicalDeviceLimits &l = physProps.limits;
    switch (limit) {
    case QRhi::TextureSizeMin:
        return 1;
    case QRhi::TextureSizeMax:
        return static_cast<int>(l.maxImageDimension2D);
    case QRhi::MaxColorAttachments:
        return static_cast<int>(l.maxColorAttachments);
    case QRhi::FramesInFlight:
    case QRhi::MaxAsyncReadbackFrames:
        return 1;
    case QRhi::MaxThreadGroupsPerDimension:
        return static_cast<int>(l.maxComputeWorkGroupCount[0]);
    case QRhi::MaxThreadsPerThreadGroup:
        return static_cast<int>(l.maxComputeWorkGroupInvocations);
    case QRhi::MaxThreadGroupX:
        return static_cast<int>(l.maxComputeWorkGroupSize[0]);
    case QRhi::MaxThreadGroupY:
        return static_cast<int>(l.maxComputeWorkGroupSize[1]);
    case QRhi::MaxThreadGroupZ:
        return static_cast<int>(l.maxComputeWorkGroupSize[2]);
    case QRhi::TextureArraySizeMax:
        return static_cast<int>(l.maxImageArrayLayers);
    case QRhi::MaxUniformBufferRange:
        return static_cast<int>(std::min<quint32>(l.maxUniformBufferRange, 0x7fffffffu));
    case QRhi::MaxVertexInputs:
        return static_cast<int>(l.maxVertexInputAttributes);
    case QRhi::MaxVertexOutputs:
        return static_cast<int>(l.maxVertexOutputComponents / 4);
    }
    return 0;
}

QRhi::FrameOpResult VulkanRhi::beginOffscreenFrame(QRhiCommandBuffer **out, QRhi::BeginFrameFlags) {
    if (inFrame) {
        qWarning("QRhi::beginOffscreenFrame: a frame is already in progress");
        return QRhi::FrameOpError;
    }
    df->vkResetCommandBuffer(cmd->cb, 0);
    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (df->vkBeginCommandBuffer(cmd->cb, &begin) != VK_SUCCESS)
        return QRhi::FrameOpError;
    cmd->recording = true;
    cmd->inPass = cmd->inCompute = false;
    cmd->graphics = nullptr;
    cmd->compute = nullptr;
    inFrame = true;
    *out = cmd.get();
    return QRhi::FrameOpSuccess;
}

QRhi::FrameOpResult VulkanRhi::endOffscreenFrame(QRhi::EndFrameFlags) {
    if (!inFrame) {
        qWarning("QRhi::endOffscreenFrame: no frame is in progress");
        return QRhi::FrameOpError;
    }
    inFrame = false;
    cmd->recording = false;
    df->vkEndCommandBuffer(cmd->cb);
    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd->cb;
    df->vkResetFences(dev, 1, &fence);
    VkResult result = df->vkQueueSubmit(queue, 1, &submit, fence);
    if (result == VK_SUCCESS)
        result = df->vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    if (result == VK_ERROR_DEVICE_LOST)
        deviceLost = true;
    for (PendingRead &p : pendingReads) {
        if (result == VK_SUCCESS) {
            const void *source = p.buffer ? static_cast<const char *>(p.buffer->mapped) + p.offset : p.staging.mapped;
            p.result->data = QByteArray(static_cast<const char *>(source), static_cast<qsizetype>(p.bytes));
            p.result->format = p.format;
            p.result->pixelSize = p.size;
            if (p.result->completed)
                p.result->completed();
        }
        destroyBuffer(&p.staging.buffer, &p.staging.memory, &p.staging.mapped);
    }
    pendingReads.clear();
    for (Staging &s : staging)
        destroyBuffer(&s.buffer, &s.memory, &s.mapped);
    staging.clear();
    return result == VK_SUCCESS ? QRhi::FrameOpSuccess : (deviceLost ? QRhi::FrameOpDeviceLost : QRhi::FrameOpError);
}

// ---------------------------------------------------------------------------------------------------------------------
// Buffer

VulkanBuffer::VulkanBuffer(VulkanRhi *rhi, Type type, UsageFlags usage, quint32 size)
    : QRhiBuffer(rhi, type, usage, size), vk(rhi) {}

void VulkanBuffer::destroy() {
    if (vk->dev != VK_NULL_HANDLE)
        vk->destroyBuffer(&buffer, &memory, &mapped);
}

bool VulkanBuffer::create() {
    destroy();
    if (size() == 0) {
        qWarning("QRhiBuffer: the size is 0");
        return false;
    }
    VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (usage().testFlag(VertexBuffer))
        usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (usage().testFlag(IndexBuffer))
        usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (usage().testFlag(UniformBuffer))
        usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (usage().testFlag(StorageBuffer))
        usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (type() == Dynamic)
        return vk->makeBuffer(size(), usageFlags,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &buffer,
                              &memory, &mapped);
    return vk->makeBuffer(size(), usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer, &memory, nullptr);
}

// ---------------------------------------------------------------------------------------------------------------------
// Texture

VulkanTexture::VulkanTexture(VulkanRhi *rhi, Format format, const QSize &size, int depth, int arraySize,
                             int sampleCount, Flags flags)
    : QRhiTexture(rhi, format, size, depth, arraySize, sampleCount, flags), vk(rhi) {}

void VulkanTexture::destroy() {
    if (vk->dev == VK_NULL_HANDLE)
        return;
    for (auto &entry : attachmentViews)
        vk->df->vkDestroyImageView(vk->dev, entry.second, nullptr);
    attachmentViews.clear();
    if (view != VK_NULL_HANDLE)
        vk->df->vkDestroyImageView(vk->dev, view, nullptr);
    if (owned && image != VK_NULL_HANDLE)
        vk->df->vkDestroyImage(vk->dev, image, nullptr);
    if (owned && memory != VK_NULL_HANDLE)
        vk->df->vkFreeMemory(vk->dev, memory, nullptr);
    view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
}

bool VulkanTexture::create() {
    destroy();
    owned = true;
    vkFormat = vkFormatFor(format(), flags().testFlag(sRGB));
    if (vkFormat == VK_FORMAT_UNDEFINED || pixelSize().isEmpty()) {
        qWarning("QRhiTexture: the format is unknown or the size is empty");
        return false;
    }
    const bool depthFormat = QRhiImplementation::isDepthFormat(format());
    if (depthFormat && format() == QRhiTexture::D24S8 && !vk->isTextureFormatSupported(format(), flags())) {
        vkFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    } else if (depthFormat && format() == QRhiTexture::D24 && !vk->isTextureFormatSupported(format(), flags())) {
        vkFormat = VK_FORMAT_D32_SFLOAT;
    }
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    const bool is3D = flags().testFlag(ThreeDimensional);
    info.imageType = is3D ? VK_IMAGE_TYPE_3D : (flags().testFlag(OneDimensional) ? VK_IMAGE_TYPE_1D : VK_IMAGE_TYPE_2D);
    info.format = vkFormat;
    info.extent = {static_cast<quint32>(pixelSize().width()),
                   static_cast<quint32>(is3D ? pixelSize().height() : std::max(1, pixelSize().height())),
                   static_cast<quint32>(is3D ? std::max(1, depth()) : 1)};
    if (info.imageType == VK_IMAGE_TYPE_1D)
        info.extent.height = 1;
    info.mipLevels = static_cast<quint32>(levelCount());
    info.arrayLayers = static_cast<quint32>(layerCount());
    info.samples = samplesFor(sampleCount());
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (flags().testFlag(RenderTarget))
        info.usage |= depthFormat ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (flags().testFlag(UsedWithLoadStore))
        info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (flags().testFlag(CubeMap))
        info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vk->df->vkCreateImage(vk->dev, &info, nullptr, &image) != VK_SUCCESS) {
        qWarning("QRhiTexture: vkCreateImage failed");
        return false;
    }
    VkMemoryRequirements req;
    vk->df->vkGetImageMemoryRequirements(vk->dev, image, &req);
    if (!vk->allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory)) {
        destroy();
        return false;
    }
    vk->df->vkBindImageMemory(vk->dev, image, memory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange = {depthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT, 0, info.mipLevels,
                                 0, info.arrayLayers};
    if (flags().testFlag(CubeMap))
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    else if (is3D)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
    else if (info.imageType == VK_IMAGE_TYPE_1D)
        viewInfo.viewType = flags().testFlag(TextureArray) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
    else
        viewInfo.viewType = flags().testFlag(TextureArray) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    if (vk->df->vkCreateImageView(vk->dev, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        destroy();
        return false;
    }
    return true;
}

bool VulkanTexture::createFrom(NativeTexture src) {
    destroy();
    image = reinterpret_cast<VkImage>(const_cast<void *>(src.object));
    layout = static_cast<VkImageLayout>(src.layout);
    owned = false;
    vkFormat = vkFormatFor(format(), flags().testFlag(sRGB));
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange = {samplingAspect(format()), 0, 1, 0, 1};
    return image != VK_NULL_HANDLE && vk->df->vkCreateImageView(vk->dev, &viewInfo, nullptr, &view) == VK_SUCCESS;
}

// The view of one layer and level with all aspects, for a framebuffer.
VkImageView VulkanTexture::attachmentView(int layer, int level) {
    const int key = layer * 64 + level;
    const auto it = attachmentViews.find(key);
    if (it != attachmentViews.end())
        return it->second;
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange = {aspectFor(format()), static_cast<quint32>(level), 1, static_cast<quint32>(layer), 1};
    VkImageView v = VK_NULL_HANDLE;
    if (vk->df->vkCreateImageView(vk->dev, &viewInfo, nullptr, &v) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    attachmentViews[key] = v;
    return v;
}

// ---------------------------------------------------------------------------------------------------------------------
// Sampler and render buffer

void VulkanSampler::destroy() {
    if (sampler != VK_NULL_HANDLE && vk->dev != VK_NULL_HANDLE)
        vk->df->vkDestroySampler(vk->dev, sampler, nullptr);
    sampler = VK_NULL_HANDLE;
}

bool VulkanSampler::create() {
    destroy();
    const auto filter = [](Filter fl) { return fl == Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR; };
    const auto address = [](AddressMode m) {
        return m == Repeat
                   ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                   : (m == Mirror ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    };
    VkSamplerCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = filter(magFilter());
    info.minFilter = filter(minFilter());
    info.mipmapMode = mipmapMode() == Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = address(addressU());
    info.addressModeV = address(addressV());
    info.addressModeW = address(addressW());
    info.maxLod = mipmapMode() == None ? 0.25f : VK_LOD_CLAMP_NONE;
    if (textureCompareOp() != Never) {
        info.compareEnable = VK_TRUE;
        info.compareOp =
            static_cast<VkCompareOp>(textureCompareOp() == Less             ? VK_COMPARE_OP_LESS
                                     : textureCompareOp() == Equal          ? VK_COMPARE_OP_EQUAL
                                     : textureCompareOp() == LessOrEqual    ? VK_COMPARE_OP_LESS_OR_EQUAL
                                     : textureCompareOp() == Greater        ? VK_COMPARE_OP_GREATER
                                     : textureCompareOp() == NotEqual       ? VK_COMPARE_OP_NOT_EQUAL
                                     : textureCompareOp() == GreaterOrEqual ? VK_COMPARE_OP_GREATER_OR_EQUAL
                                                                            : VK_COMPARE_OP_ALWAYS);
    }
    return vk->df->vkCreateSampler(vk->dev, &info, nullptr, &sampler) == VK_SUCCESS;
}

void VulkanRenderBuffer::destroy() {
    if (vk->dev == VK_NULL_HANDLE)
        return;
    if (view != VK_NULL_HANDLE)
        vk->df->vkDestroyImageView(vk->dev, view, nullptr);
    if (image != VK_NULL_HANDLE)
        vk->df->vkDestroyImage(vk->dev, image, nullptr);
    if (memory != VK_NULL_HANDLE)
        vk->df->vkFreeMemory(vk->dev, memory, nullptr);
    view = VK_NULL_HANDLE;
    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
}

bool VulkanRenderBuffer::create() {
    destroy();
    if (pixelSize().isEmpty())
        return false;
    const bool depth = type() == DepthStencil;
    vkFormat = depth ? (vk->isTextureFormatSupported(QRhiTexture::D24S8, QRhiTexture::RenderTarget)
                            ? VK_FORMAT_D24_UNORM_S8_UINT
                            : VK_FORMAT_D32_SFLOAT_S8_UINT)
                     : VK_FORMAT_R8G8B8A8_UNORM;
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = vkFormat;
    info.extent = {static_cast<quint32>(pixelSize().width()), static_cast<quint32>(pixelSize().height()), 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = samplesFor(sampleCount());
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                 VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vk->df->vkCreateImage(vk->dev, &info, nullptr, &image) != VK_SUCCESS)
        return false;
    VkMemoryRequirements req;
    vk->df->vkGetImageMemoryRequirements(vk->dev, image, &req);
    if (!vk->allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &memory) &&
        !vk->allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memory)) {
        destroy();
        return false;
    }
    vk->df->vkBindImageMemory(vk->dev, image, memory, 0);
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = vkFormat;
    viewInfo.subresourceRange = {
        depth ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
              : static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_COLOR_BIT),
        0, 1, 0, 1};
    return vk->df->vkCreateImageView(vk->dev, &viewInfo, nullptr, &view) == VK_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Render pass and render target

void VulkanRenderPassDescriptor::destroy() {
    if (renderPass != VK_NULL_HANDLE && vk->dev != VK_NULL_HANDLE)
        vk->df->vkDestroyRenderPass(vk->dev, renderPass, nullptr);
    renderPass = VK_NULL_HANDLE;
}

bool VulkanRenderPassDescriptor::create() {
    destroy();
    colorFormats.clear();
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference> colorRefs, resolveRefs;
    const bool keepColor = targetFlags.testFlag(QRhiTextureRenderTarget::PreserveColorContents);
    const bool keepDepth = targetFlags.testFlag(QRhiTextureRenderTarget::PreserveDepthStencilContents);
    colorCount = static_cast<int>(description.colorAttachmentCount());
    samples = 1;
    for (int i = 0; i < colorCount; ++i) {
        const QRhiColorAttachment *att = description.colorAttachmentAt(i);
        VkAttachmentDescription a = {};
        if (att->texture()) {
            auto *tex = static_cast<VulkanTexture *>(att->texture());
            a.format = vkFormatFor(tex->format(), tex->flags().testFlag(QRhiTexture::sRGB));
            a.samples = samplesFor(tex->sampleCount());
            samples = tex->sampleCount();
            a.finalLayout = att->resolveTexture() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else if (att->renderBuffer()) {
            auto *rb = static_cast<VulkanRenderBuffer *>(att->renderBuffer());
            a.format = VK_FORMAT_R8G8B8A8_UNORM;
            a.samples = samplesFor(rb->sampleCount());
            samples = rb->sampleCount();
            a.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        } else {
            qWarning("QRhiRenderPassDescriptor: a color attachment has neither texture nor render buffer");
            return false;
        }
        a.loadOp = keepColor ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = keepColor ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        colorFormats.push_back(a.format);
        colorRefs.push_back({u32(attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        attachments.push_back(a);
    }
    hasDepth = description.depthTexture() || description.depthStencilBuffer();
    VkAttachmentReference depthRef = {};
    depthFormat = VK_FORMAT_UNDEFINED;
    if (hasDepth) {
        VkAttachmentDescription a = {};
        if (description.depthTexture()) {
            auto *tex = static_cast<VulkanTexture *>(description.depthTexture());
            a.format = tex->vkFormat != VK_FORMAT_UNDEFINED ? tex->vkFormat : vkFormatFor(tex->format(), false);
            a.samples = samplesFor(tex->sampleCount());
            a.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        } else {
            auto *rb = static_cast<VulkanRenderBuffer *>(description.depthStencilBuffer());
            a.format = rb->vkFormat != VK_FORMAT_UNDEFINED ? rb->vkFormat : VK_FORMAT_D24_UNORM_S8_UINT;
            a.samples = samplesFor(rb->sampleCount());
            a.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        a.loadOp = keepDepth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = a.loadOp;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.initialLayout = keepDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        depthFormat = a.format;
        depthRef = {u32(attachments.size()), VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        attachments.push_back(a);
    }
    resolveCount = 0;
    for (int i = 0; i < colorCount; ++i) {
        const QRhiColorAttachment *att = description.colorAttachmentAt(i);
        if (!att->resolveTexture()) {
            resolveRefs.push_back({VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_UNDEFINED});
            continue;
        }
        auto *tex = static_cast<VulkanTexture *>(att->resolveTexture());
        VkAttachmentDescription a = {};
        a.format = vkFormatFor(tex->format(), tex->flags().testFlag(QRhiTexture::sRGB));
        a.samples = VK_SAMPLE_COUNT_1_BIT;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        resolveRefs.push_back({u32(attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        attachments.push_back(a);
        ++resolveCount;
    }
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = u32(colorRefs.size());
    subpass.pColorAttachments = colorRefs.data();
    subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;
    subpass.pResolveAttachments = resolveCount > 0 ? resolveRefs.data() : nullptr;
    VkSubpassDependency deps[2] = {};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    deps[0].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    deps[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    deps[1].srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    VkRenderPassCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = u32(attachments.size());
    info.pAttachments = attachments.data();
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 2;
    info.pDependencies = deps;
    return vk->df->vkCreateRenderPass(vk->dev, &info, nullptr, &renderPass) == VK_SUCCESS;
}

bool VulkanRenderPassDescriptor::isCompatible(const QRhiRenderPassDescriptor *other) const {
    const auto *o = static_cast<const VulkanRenderPassDescriptor *>(other);
    return o && o->colorFormats == colorFormats && o->depthFormat == depthFormat && o->samples == samples &&
           o->hasDepth == hasDepth && o->resolveCount == resolveCount;
}

void VulkanTextureRenderTarget::destroy() {
    if (framebuffer != VK_NULL_HANDLE && vk->dev != VK_NULL_HANDLE)
        vk->df->vkDestroyFramebuffer(vk->dev, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
}

QRhiRenderPassDescriptor *VulkanTextureRenderTarget::newCompatibleRenderPassDescriptor() {
    return new VulkanRenderPassDescriptor(vk, description(), flags());
}

bool VulkanTextureRenderTarget::create() {
    destroy();
    auto *rp = static_cast<VulkanRenderPassDescriptor *>(renderPassDescriptor());
    if (!rp || rp->renderPass == VK_NULL_HANDLE) {
        qWarning("QRhiTextureRenderTarget: the render pass descriptor was not set or not created");
        return false;
    }
    const QRhiTextureRenderTargetDescription desc = description();
    std::vector<VkImageView> views;
    size = QSize();
    samples = 1;
    const auto adopt = [this](const QSize &s) {
        if (size.isValid() && size != s) {
            qWarning("QRhiTextureRenderTarget: the attachments differ in size");
            return false;
        }
        size = s;
        return true;
    };
    for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i) {
        const QRhiColorAttachment *att = desc.colorAttachmentAt(i);
        if (att->texture()) {
            auto *tex = static_cast<VulkanTexture *>(att->texture());
            if (!tex->flags().testFlag(QRhiTexture::RenderTarget) || tex->image == VK_NULL_HANDLE) {
                qWarning("QRhiTextureRenderTarget: a color texture is not a created render target");
                return false;
            }
            if (!adopt(QRhiImplementation::levelSize(tex->pixelSize(), att->level())))
                return false;
            samples = tex->sampleCount();
            views.push_back(tex->attachmentView(att->layer(), att->level()));
        } else if (att->renderBuffer()) {
            auto *rb = static_cast<VulkanRenderBuffer *>(att->renderBuffer());
            if (!adopt(rb->pixelSize()))
                return false;
            samples = rb->sampleCount();
            views.push_back(rb->view);
        } else {
            return false;
        }
    }
    if (desc.depthTexture()) {
        auto *tex = static_cast<VulkanTexture *>(desc.depthTexture());
        if (!size.isValid() && !adopt(tex->pixelSize()))
            return false;
        views.push_back(tex->attachmentView(0, 0));
    } else if (desc.depthStencilBuffer()) {
        auto *rb = static_cast<VulkanRenderBuffer *>(desc.depthStencilBuffer());
        if (!size.isValid() && !adopt(rb->pixelSize()))
            return false;
        views.push_back(rb->view);
    }
    for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i)
        if (const QRhiColorAttachment *att = desc.colorAttachmentAt(i); att->resolveTexture())
            views.push_back(static_cast<VulkanTexture *>(att->resolveTexture())->attachmentView(0, 0));
    if (views.empty() || std::find(views.begin(), views.end(), VK_NULL_HANDLE) != views.end() || !size.isValid()) {
        qWarning("QRhiTextureRenderTarget: there are no usable attachments");
        return false;
    }
    VkFramebufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.renderPass = rp->renderPass;
    info.attachmentCount = u32(views.size());
    info.pAttachments = views.data();
    info.width = static_cast<quint32>(size.width());
    info.height = static_cast<quint32>(size.height());
    info.layers = 1;
    return vk->df->vkCreateFramebuffer(vk->dev, &info, nullptr, &framebuffer) == VK_SUCCESS;
}

// ---------------------------------------------------------------------------------------------------------------------
// Shader resource bindings

void VulkanShaderResourceBindings::destroy() {
    if (vk->dev == VK_NULL_HANDLE)
        return;
    if (pool != VK_NULL_HANDLE)
        vk->df->vkDestroyDescriptorPool(vk->dev, pool, nullptr);
    if (setLayout != VK_NULL_HANDLE)
        vk->df->vkDestroyDescriptorSetLayout(vk->dev, setLayout, nullptr);
    pool = VK_NULL_HANDLE;
    setLayout = VK_NULL_HANDLE;
    set = VK_NULL_HANDLE;
}

bool VulkanShaderResourceBindings::create() {
    destroy();
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    std::map<VkDescriptorType, quint32> counts;
    for (const QRhiShaderResourceBinding &b : m_bindings) {
        VkDescriptorSetLayoutBinding lb = {};
        lb.binding = static_cast<quint32>(b.binding());
        lb.descriptorCount = 1;
        lb.stageFlags = stageFlagsFor(b.stage());
        switch (b.type()) {
        case QRhiShaderResourceBinding::UniformBuffer:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            if (!b.buffer() || !static_cast<VulkanBuffer *>(b.buffer())->buffer) {
                qWarning("QRhiShaderResourceBindings: a uniform buffer binding has no created buffer");
                return false;
            }
            break;
        case QRhiShaderResourceBinding::BufferLoad:
        case QRhiShaderResourceBinding::BufferStore:
        case QRhiShaderResourceBinding::BufferLoadStore:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            if (!b.buffer() || !static_cast<VulkanBuffer *>(b.buffer())->buffer) {
                qWarning("QRhiShaderResourceBindings: a storage buffer binding has no created buffer");
                return false;
            }
            break;
        case QRhiShaderResourceBinding::SampledTexture:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if (!b.texture() || !b.sampler() || !static_cast<VulkanTexture *>(b.texture())->view ||
                !static_cast<VulkanSampler *>(b.sampler())->sampler) {
                qWarning("QRhiShaderResourceBindings: a sampled texture binding lacks a created texture or sampler");
                return false;
            }
            break;
        case QRhiShaderResourceBinding::Texture:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            break;
        case QRhiShaderResourceBinding::Sampler:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            break;
        case QRhiShaderResourceBinding::ImageLoad:
        case QRhiShaderResourceBinding::ImageStore:
        case QRhiShaderResourceBinding::ImageLoadStore:
            lb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            if (!b.texture() || !static_cast<VulkanTexture *>(b.texture())->view) {
                qWarning("QRhiShaderResourceBindings: an image binding has no created texture");
                return false;
            }
            break;
        }
        ++counts[lb.descriptorType];
        layoutBindings.push_back(lb);
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = u32(layoutBindings.size());
    layoutInfo.pBindings = layoutBindings.data();
    if (vk->df->vkCreateDescriptorSetLayout(vk->dev, &layoutInfo, nullptr, &setLayout) != VK_SUCCESS)
        return false;
    std::vector<VkDescriptorPoolSize> sizes;
    sizes.reserve(counts.size() + 1);
    for (const auto &entry : counts)
        sizes.push_back({entry.first, entry.second});
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = u32(sizes.size());
    poolInfo.pPoolSizes = sizes.data();
    if (sizes.empty()) { // a pool needs at least one size
        sizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = sizes.data();
    }
    if (vk->df->vkCreateDescriptorPool(vk->dev, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        destroy();
        return false;
    }
    VkDescriptorSetAllocateInfo alloc = {};
    alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc.descriptorPool = pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &setLayout;
    if (vk->df->vkAllocateDescriptorSets(vk->dev, &alloc, &set) != VK_SUCCESS) {
        destroy();
        return false;
    }
    // Images that shaders write have to be in the general layout.
    std::vector<VulkanTexture *> toGeneral;
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos(m_bindings.size());
    std::vector<VkDescriptorImageInfo> imageInfos(m_bindings.size());
    for (std::size_t i = 0; i < m_bindings.size(); ++i) {
        const QRhiShaderResourceBinding &b = m_bindings[i];
        VkWriteDescriptorSet w = {};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = static_cast<quint32>(b.binding());
        w.descriptorCount = 1;
        w.descriptorType = layoutBindings[i].descriptorType;
        if (b.buffer()) {
            auto *buf = static_cast<VulkanBuffer *>(b.buffer());
            bufferInfos[i] = {buf->buffer, b.bufferOffset(),
                              b.bufferSize() ? b.bufferSize() : buf->size() - b.bufferOffset()};
            w.pBufferInfo = &bufferInfos[i];
        } else if (b.texture() || b.sampler()) {
            auto *tex = static_cast<VulkanTexture *>(b.texture());
            auto *smp = static_cast<VulkanSampler *>(b.sampler());
            VkDescriptorImageInfo ii = {};
            ii.sampler = smp ? smp->sampler : VK_NULL_HANDLE;
            ii.imageView = tex ? tex->view : VK_NULL_HANDLE;
            if (b.type() == QRhiShaderResourceBinding::ImageLoad || b.type() == QRhiShaderResourceBinding::ImageStore ||
                b.type() == QRhiShaderResourceBinding::ImageLoadStore) {
                ii.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                toGeneral.push_back(tex);
            } else {
                ii.imageLayout = tex && QRhiImplementation::isDepthFormat(tex->format())
                                     ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                     : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            imageInfos[i] = ii;
            w.pImageInfo = &imageInfos[i];
        }
        writes.push_back(w);
    }
    if (!writes.empty())
        vk->df->vkUpdateDescriptorSets(vk->dev, u32(writes.size()), writes.data(), 0, nullptr);
    if (!toGeneral.empty()) {
        vk->oneShot([&](VkCommandBuffer one) {
            for (VulkanTexture *tex : toGeneral) {
                if (tex->layout == VK_IMAGE_LAYOUT_GENERAL)
                    continue;
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.oldLayout = tex->layout;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = tex->image;
                barrier.subresourceRange = {aspectFor(tex->format()), 0, static_cast<quint32>(tex->levelCount()), 0,
                                            static_cast<quint32>(tex->layerCount())};
                barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                vk->df->vkCmdPipelineBarrier(one, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                                             &barrier);
                tex->layout = VK_IMAGE_LAYOUT_GENERAL;
            }
        });
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Pipelines

// The module of the SPIR-V in the standard variant of the shader; VK_NULL_HANDLE if there is none.
VkShaderModule makeModule(VulkanRhi *vk, const QRhiShaderStage &stage, QByteArray *entryPoint) {
    const QShader shader = stage.shader();
    for (const QShaderKey &key : shader.availableShaders()) {
        if (key.source() != QShader::SpirvShader || key.sourceVariant() != stage.shaderVariant())
            continue;
        const QShaderCode code = shader.shader(key);
        const QByteArray spirv = code.shader(); // a copy that lives through the call
        if (spirv.isEmpty() || spirv.size() % 4 != 0)
            continue;
        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = static_cast<std::size_t>(spirv.size());
        info.pCode = reinterpret_cast<const quint32 *>(spirv.constData());
        VkShaderModule module = VK_NULL_HANDLE;
        if (vk->df->vkCreateShaderModule(vk->dev, &info, nullptr, &module) == VK_SUCCESS) {
            *entryPoint = code.entryPoint();
            return module;
        }
    }
    qWarning("QRhi: a shader has no SPIR-V variant that Vulkan accepts");
    return VK_NULL_HANDLE;
}

VkPipelineLayout makeLayout(VulkanRhi *vk, QRhiShaderResourceBindings *srb) {
    auto *vsrb = static_cast<VulkanShaderResourceBindings *>(srb);
    VkPipelineLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &vsrb->setLayout;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    vk->df->vkCreatePipelineLayout(vk->dev, &info, nullptr, &layout);
    return layout;
}

void VulkanGraphicsPipeline::destroy() {
    if (vk->dev == VK_NULL_HANDLE)
        return;
    if (pipeline != VK_NULL_HANDLE)
        vk->df->vkDestroyPipeline(vk->dev, pipeline, nullptr);
    if (layout != VK_NULL_HANDLE)
        vk->df->vkDestroyPipelineLayout(vk->dev, layout, nullptr);
    pipeline = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
}

bool VulkanGraphicsPipeline::create() {
    destroy();
    auto *rp = static_cast<VulkanRenderPassDescriptor *>(renderPassDescriptor());
    auto *srb = static_cast<VulkanShaderResourceBindings *>(shaderResourceBindings());
    if (shaderStageCount() == 0 || !rp || rp->renderPass == VK_NULL_HANDLE || !srb ||
        srb->setLayout == VK_NULL_HANDLE) {
        qWarning(
            "QRhiGraphicsPipeline: shader stages, created bindings and a created render pass descriptor are needed");
        return false;
    }
    layout = makeLayout(vk, srb);
    if (layout == VK_NULL_HANDLE)
        return false;

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkShaderModule> modules;
    std::vector<QByteArray> entryPoints(static_cast<std::size_t>(shaderStageCount()));
    for (qsizetype i = 0; i < shaderStageCount(); ++i) {
        const QRhiShaderStage *s = shaderStageAt(i);
        VkShaderModule module = makeModule(vk, *s, &entryPoints[static_cast<std::size_t>(i)]);
        if (module == VK_NULL_HANDLE) {
            for (VkShaderModule m : modules)
                vk->df->vkDestroyShaderModule(vk->dev, m, nullptr);
            destroy();
            return false;
        }
        modules.push_back(module);
        VkPipelineShaderStageCreateInfo si = {};
        si.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        switch (s->type()) {
        case QRhiShaderStage::Vertex:
            si.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case QRhiShaderStage::Fragment:
            si.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case QRhiShaderStage::TessellationControl:
            si.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        case QRhiShaderStage::TessellationEvaluation:
            si.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;
        case QRhiShaderStage::Geometry:
            si.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case QRhiShaderStage::Compute:
            si.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        }
        si.module = module;
        si.pName = entryPoints[static_cast<std::size_t>(i)].constData();
        stages.push_back(si);
    }

    const QRhiVertexInputLayout input = vertexInputLayout();
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    for (std::size_t i = 0; i < input.bindings().size(); ++i) {
        const QRhiVertexInputBinding &b = input.bindings()[i];
        bindings.push_back({u32(i), b.stride(),
                            b.classification() == QRhiVertexInputBinding::PerInstance ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                                                      : VK_VERTEX_INPUT_RATE_VERTEX});
    }
    for (const QRhiVertexInputAttribute &a : input.attributes())
        attributes.push_back({static_cast<quint32>(a.location()), static_cast<quint32>(a.binding()),
                              vertexFormatFor(a.format()), a.offset()});
    VkPipelineVertexInputStateCreateInfo vi = {};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = u32(bindings.size());
    vi.pVertexBindingDescriptions = bindings.data();
    vi.vertexAttributeDescriptionCount = u32(attributes.size());
    vi.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo ia = {};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    static const VkPrimitiveTopology topologies[] = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,     VK_PRIMITIVE_TOPOLOGY_POINT_LIST};
    ia.topology = topologies[topology()];

    VkPipelineViewportStateCreateInfo vp = {};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs = {};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = polygonMode() == Line ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    rs.cullMode =
        cullMode() == Front ? VK_CULL_MODE_FRONT_BIT : (cullMode() == Back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    rs.frontFace = frontFace() == CCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rs.lineWidth = lineWidth();

    VkPipelineMultisampleStateCreateInfo ms = {};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = samplesFor(sampleCount());

    VkPipelineDepthStencilStateCreateInfo ds = {};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = hasDepthTest();
    ds.depthWriteEnable = hasDepthWrite();
    ds.depthCompareOp = compareOpFor(depthOp());
    ds.stencilTestEnable = hasStencilTest();
    const auto stencilState = [this](const StencilOpState &s) {
        VkStencilOpState out = {};
        out.failOp = stencilOpFor(s.failOp);
        out.depthFailOp = stencilOpFor(s.depthFailOp);
        out.passOp = stencilOpFor(s.passOp);
        out.compareOp = compareOpFor(s.compareOp);
        out.compareMask = stencilReadMask();
        out.writeMask = stencilWriteMask();
        return out;
    };
    ds.front = stencilState(stencilFront());
    ds.back = stencilState(stencilBack());

    std::vector<VkPipelineColorBlendAttachmentState> blends;
    for (int i = 0; i < rp->colorCount; ++i) {
        const TargetBlend *tb = targetBlendAt(i);
        TargetBlend defaults;
        if (!tb)
            tb = &defaults;
        VkPipelineColorBlendAttachmentState bs = {};
        bs.blendEnable = tb->enable;
        bs.srcColorBlendFactor = blendFactorFor(tb->srcColor);
        bs.dstColorBlendFactor = blendFactorFor(tb->dstColor);
        bs.colorBlendOp = blendOpFor(tb->opColor);
        bs.srcAlphaBlendFactor = blendFactorFor(tb->srcAlpha);
        bs.dstAlphaBlendFactor = blendFactorFor(tb->dstAlpha);
        bs.alphaBlendOp = blendOpFor(tb->opAlpha);
        bs.colorWriteMask = (tb->colorWrite.testFlag(R) ? VK_COLOR_COMPONENT_R_BIT : 0) |
                            (tb->colorWrite.testFlag(G) ? VK_COLOR_COMPONENT_G_BIT : 0) |
                            (tb->colorWrite.testFlag(B) ? VK_COLOR_COMPONENT_B_BIT : 0) |
                            (tb->colorWrite.testFlag(A) ? VK_COLOR_COMPONENT_A_BIT : 0);
        blends.push_back(bs);
    }
    VkPipelineColorBlendStateCreateInfo cb = {};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = u32(blends.size());
    cb.pAttachments = blends.data();

    std::vector<VkDynamicState> dynamic = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    if (flags().testFlag(UsesBlendConstants))
        dynamic.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
    if (flags().testFlag(UsesStencilRef))
        dynamic.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
    VkPipelineDynamicStateCreateInfo dyn = {};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = u32(dynamic.size());
    dyn.pDynamicStates = dynamic.data();

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = u32(stages.size());
    info.pStages = stages.data();
    info.pVertexInputState = &vi;
    info.pInputAssemblyState = &ia;
    info.pViewportState = &vp;
    info.pRasterizationState = &rs;
    info.pMultisampleState = &ms;
    info.pDepthStencilState = &ds;
    info.pColorBlendState = &cb;
    info.pDynamicState = &dyn;
    info.layout = layout;
    info.renderPass = rp->renderPass;
    const VkResult result = vk->df->vkCreateGraphicsPipelines(vk->dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    for (VkShaderModule m : modules)
        vk->df->vkDestroyShaderModule(vk->dev, m, nullptr);
    if (result != VK_SUCCESS) {
        qWarning("QRhiGraphicsPipeline: vkCreateGraphicsPipelines failed: %d", result);
        destroy();
        return false;
    }
    return true;
}

void VulkanComputePipeline::destroy() {
    if (vk->dev == VK_NULL_HANDLE)
        return;
    if (pipeline != VK_NULL_HANDLE)
        vk->df->vkDestroyPipeline(vk->dev, pipeline, nullptr);
    if (layout != VK_NULL_HANDLE)
        vk->df->vkDestroyPipelineLayout(vk->dev, layout, nullptr);
    pipeline = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
}

bool VulkanComputePipeline::create() {
    destroy();
    auto *srb = static_cast<VulkanShaderResourceBindings *>(shaderResourceBindings());
    if (!srb || srb->setLayout == VK_NULL_HANDLE) {
        qWarning("QRhiComputePipeline: created bindings are needed");
        return false;
    }
    layout = makeLayout(vk, srb);
    QByteArray entry;
    VkShaderModule module = makeModule(vk, shaderStage(), &entry);
    if (layout == VK_NULL_HANDLE || module == VK_NULL_HANDLE) {
        destroy();
        return false;
    }
    VkComputePipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = module;
    info.stage.pName = entry.constData();
    info.layout = layout;
    const VkResult result = vk->df->vkCreateComputePipelines(vk->dev, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
    vk->df->vkDestroyShaderModule(vk->dev, module, nullptr);
    if (result != VK_SUCCESS) {
        destroy();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Resource updates

void VulkanRhi::executeUpdates(QRhiCommandBuffer *cbase, QRhiResourceUpdateBatch *batch) {
    auto *vcb = static_cast<VulkanCommandBuffer *>(cbase);
    if (!vcb->recording || vcb->inPass) {
        qWarning("QRhi: resource updates run outside of a pass, in a frame");
        return;
    }
    QRhiResourceUpdateBatchPrivate *d = batchPrivate(batch);
    VkCommandBuffer cb = vcb->cb;
    const auto memoryBarrier = [&] {
        VkMemoryBarrier mb = {};
        mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &mb,
                                 0, nullptr, 0, nullptr);
    };
    const auto makeStaging = [&](VkDeviceSize size, VkBufferUsageFlags usage, Staging *out) {
        const bool ok =
            makeBuffer(size, usage, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &out->buffer, &out->memory, &out->mapped);
        return ok;
    };

    for (const QRhiResourceUpdateBatchPrivate::BufferOp &op : d->bufferOps) {
        auto *buf = static_cast<VulkanBuffer *>(op.buf);
        if (buf->buffer == VK_NULL_HANDLE) {
            qWarning("QRhiResourceUpdateBatch: the buffer was not created");
            continue;
        }
        using Op = QRhiResourceUpdateBatchPrivate::BufferOp;
        if (op.type == Op::DynamicUpdate) {
            std::memcpy(static_cast<char *>(buf->mapped) + op.offset, op.data.constData(), op.size);
        } else if (op.type == Op::StaticUpload) {
            Staging s;
            if (!makeStaging(op.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &s))
                continue;
            std::memcpy(s.mapped, op.data.constData(), op.size);
            const VkBufferCopy region = {0, op.offset, op.size};
            df->vkCmdCopyBuffer(cb, s.buffer, buf->buffer, 1, &region);
            memoryBarrier();
            staging.push_back(s);
        } else {
            PendingRead p;
            p.result = op.result;
            p.bytes = op.size;
            p.format = QRhiTexture::UnknownFormat;
            if (buf->mapped) { // a dynamic buffer is host visible: read it in place after the frame
                p.buffer = buf;
                p.offset = op.offset;
            } else {
                if (!makeStaging(op.size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &p.staging))
                    continue;
                memoryBarrier();
                const VkBufferCopy region = {op.offset, 0, op.size};
                df->vkCmdCopyBuffer(cb, buf->buffer, p.staging.buffer, 1, &region);
                memoryBarrier();
            }
            pendingReads.push_back(p);
        }
    }

    for (const QRhiResourceUpdateBatchPrivate::TextureOp &op : d->textureOps) {
        using Op = QRhiResourceUpdateBatchPrivate::TextureOp;
        if (op.type == Op::Upload) {
            auto *tex = static_cast<VulkanTexture *>(op.dst);
            if (tex->image == VK_NULL_HANDLE) {
                qWarning("QRhiResourceUpdateBatch::uploadTexture: the texture was not created");
                continue;
            }
            const int bpp = QRhiImplementation::bytesPerPixel(tex->format());
            std::vector<VkBufferImageCopy> regions;
            QByteArray all;
            for (const QRhiTextureUploadEntry &entry : op.upload.entries()) {
                QSize size;
                const QByteArray pixels =
                    QRhiImplementation::pixelsForUpload(entry.description(), tex->format(), &size);
                const QSize levelSize = QRhiImplementation::levelSize(tex->pixelSize(), entry.level());
                const QPoint at = entry.description().destinationTopLeft();
                if (!size.isValid() || pixels.size() < static_cast<qsizetype>(size.width()) * size.height() * bpp ||
                    at.x() + size.width() > levelSize.width() || at.y() + size.height() > levelSize.height() ||
                    entry.level() >= tex->levelCount() || entry.layer() >= tex->layerCount()) {
                    qWarning("QRhiResourceUpdateBatch::uploadTexture: the pixels do not fit");
                    continue;
                }
                VkBufferImageCopy region = {};
                region.bufferOffset = static_cast<VkDeviceSize>(all.size());
                region.imageSubresource = {samplingAspect(tex->format()), static_cast<quint32>(entry.level()),
                                           static_cast<quint32>(entry.layer()), 1};
                region.imageOffset = {at.x(), at.y(), 0};
                region.imageExtent = {static_cast<quint32>(size.width()), static_cast<quint32>(size.height()), 1};
                regions.push_back(region);
                all.append(pixels.constData(), static_cast<qsizetype>(size.width()) * size.height() * bpp);
            }
            if (regions.empty())
                continue;
            Staging s;
            if (!makeStaging(static_cast<VkDeviceSize>(all.size()), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &s))
                continue;
            std::memcpy(s.mapped, all.constData(), static_cast<std::size_t>(all.size()));
            transition(tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            df->vkCmdCopyBufferToImage(cb, s.buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       u32(regions.size()), regions.data());
            transition(tex, restingLayout(tex));
            staging.push_back(s);
        } else if (op.type == Op::Copy) {
            auto *src = static_cast<VulkanTexture *>(op.src);
            auto *dst = static_cast<VulkanTexture *>(op.dst);
            if (src->image == VK_NULL_HANDLE || dst->image == VK_NULL_HANDLE) {
                qWarning("QRhiResourceUpdateBatch::copyTexture: a texture was not created");
                continue;
            }
            const QSize srcSize = QRhiImplementation::levelSize(src->pixelSize(), op.copy.sourceLevel());
            const QSize size = op.copy.pixelSize().isValid() ? op.copy.pixelSize() : srcSize;
            VkImageCopy region = {};
            region.srcSubresource = {aspectFor(src->format()), static_cast<quint32>(op.copy.sourceLevel()),
                                     static_cast<quint32>(op.copy.sourceLayer()), 1};
            region.srcOffset = {op.copy.sourceTopLeft().x(), op.copy.sourceTopLeft().y(), 0};
            region.dstSubresource = {aspectFor(dst->format()), static_cast<quint32>(op.copy.destinationLevel()),
                                     static_cast<quint32>(op.copy.destinationLayer()), 1};
            region.dstOffset = {op.copy.destinationTopLeft().x(), op.copy.destinationTopLeft().y(), 0};
            region.extent = {static_cast<quint32>(size.width()), static_cast<quint32>(size.height()), 1};
            const VkImageLayout srcBefore = src->layout;
            transition(src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            transition(dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            df->vkCmdCopyImage(cb, src->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst->image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
            transition(src, srcBefore == VK_IMAGE_LAYOUT_UNDEFINED ? restingLayout(src) : srcBefore);
            transition(dst, restingLayout(dst));
        } else if (op.type == Op::Read) {
            auto *tex = static_cast<VulkanTexture *>(op.read.texture());
            if (tex->image == VK_NULL_HANDLE) {
                qWarning("QRhiResourceUpdateBatch::readBackTexture: the texture was not created");
                continue;
            }
            const QSize size = QRhiImplementation::levelSize(tex->pixelSize(), op.read.level());
            const quint32 bytes = static_cast<quint32>(size.width()) * static_cast<quint32>(size.height()) *
                                  static_cast<quint32>(QRhiImplementation::bytesPerPixel(tex->format()));
            PendingRead p;
            p.result = op.result;
            p.format = tex->format();
            p.size = size;
            p.bytes = bytes;
            if (!makeStaging(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &p.staging))
                continue;
            const VkImageLayout before = tex->layout;
            transition(tex, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkBufferImageCopy region = {};
            region.imageSubresource = {samplingAspect(tex->format()), static_cast<quint32>(op.read.level()),
                                       static_cast<quint32>(op.read.layer()), 1};
            region.imageExtent = {static_cast<quint32>(size.width()), static_cast<quint32>(size.height()), 1};
            df->vkCmdCopyImageToBuffer(cb, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, p.staging.buffer, 1,
                                       &region);
            transition(tex, before == VK_IMAGE_LAYOUT_UNDEFINED              ? restingLayout(tex)
                            : before == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ? restingLayout(tex)
                                                                             : before);
            memoryBarrier();
            pendingReads.push_back(p);
        } else { // generate mips
            auto *tex = static_cast<VulkanTexture *>(op.dst);
            const int levels = tex->levelCount();
            if (tex->image == VK_NULL_HANDLE || levels < 2) {
                continue;
            }
            const VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
            transition(tex, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL); // level 0 as the source
            for (int level = 1; level < levels; ++level) {
                const QSize from = QRhiImplementation::levelSize(tex->pixelSize(), level - 1);
                const QSize to = QRhiImplementation::levelSize(tex->pixelSize(), level);
                // The level is a destination; the levels above stay sources.
                VkImageMemoryBarrier b = {};
                b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = tex->image;
                b.subresourceRange = {aspect, static_cast<quint32>(level), 1, 0,
                                      static_cast<quint32>(tex->layerCount())};
                b.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                         nullptr, 0, nullptr, 1, &b);
                VkImageBlit blit = {};
                blit.srcSubresource = {aspect, static_cast<quint32>(level - 1), 0,
                                       static_cast<quint32>(tex->layerCount())};
                blit.srcOffsets[1] = {from.width(), from.height(), 1};
                blit.dstSubresource = {aspect, static_cast<quint32>(level), 0, static_cast<quint32>(tex->layerCount())};
                blit.dstOffsets[1] = {to.width(), to.height(), 1};
                df->vkCmdBlitImage(cb, tex->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, tex->image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
                b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                                         nullptr, 0, nullptr, 1, &b);
            }
            transition(tex, restingLayout(tex));
        }
    }
    d->clear();
}

// ---------------------------------------------------------------------------------------------------------------------
// Command buffer

void VulkanCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates) {
    if (!resourceUpdates)
        return;
    vk->executeUpdates(this, resourceUpdates);
    vk->releaseBatch(resourceUpdates);
}

void VulkanCommandBuffer::beginPass(QRhiRenderTarget *rt, const QColor &colorClearValue,
                                    const QRhiDepthStencilClearValue &depthStencilClearValue,
                                    QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags) {
    if (!recording || inPass || inCompute) {
        qWarning("QRhiCommandBuffer::beginPass: not in a frame, or a pass is already in progress");
        return;
    }
    resourceUpdate(resourceUpdates);
    target = static_cast<VulkanTextureRenderTarget *>(rt);
    targetPass = target ? static_cast<VulkanRenderPassDescriptor *>(target->renderPassDescriptor()) : nullptr;
    if (!target || target->framebuffer == VK_NULL_HANDLE || !targetPass) {
        qWarning("QRhiCommandBuffer::beginPass: the render target was not created");
        return;
    }
    // Attachments that keep their contents are in the attachment layout when the pass begins.
    const QRhiTextureRenderTargetDescription desc = target->description();
    const bool keepColor = target->flags().testFlag(QRhiTextureRenderTarget::PreserveColorContents);
    const bool keepDepth = target->flags().testFlag(QRhiTextureRenderTarget::PreserveDepthStencilContents);
    for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i) {
        if (auto *tex = static_cast<VulkanTexture *>(desc.colorAttachmentAt(i)->texture()); tex && keepColor)
            vk->transition(tex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    if (auto *depth = static_cast<VulkanTexture *>(desc.depthTexture()); depth && keepDepth)
        vk->transition(depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    std::vector<VkClearValue> clears;
    for (int i = 0; i < targetPass->colorCount; ++i) {
        VkClearValue c = {};
        c.color.float32[0] = static_cast<float>(colorClearValue.redF());
        c.color.float32[1] = static_cast<float>(colorClearValue.greenF());
        c.color.float32[2] = static_cast<float>(colorClearValue.blueF());
        c.color.float32[3] = static_cast<float>(colorClearValue.alphaF());
        clears.push_back(c);
    }
    if (targetPass->hasDepth) {
        VkClearValue c = {};
        c.depthStencil = {depthStencilClearValue.depthClearValue(), depthStencilClearValue.stencilClearValue()};
        clears.push_back(c);
    }
    for (int i = 0; i < targetPass->resolveCount; ++i)
        clears.push_back(VkClearValue{});
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = targetPass->renderPass;
    info.framebuffer = target->framebuffer;
    info.renderArea.extent = {static_cast<quint32>(target->size.width()), static_cast<quint32>(target->size.height())};
    info.clearValueCount = u32(clears.size());
    info.pClearValues = clears.data();
    vk->df->vkCmdBeginRenderPass(cb, &info, VK_SUBPASS_CONTENTS_INLINE);
    inPass = true;
    graphics = nullptr;
}

void VulkanCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates) {
    if (!inPass) {
        qWarning("QRhiCommandBuffer::endPass: no pass is in progress");
        return;
    }
    vk->df->vkCmdEndRenderPass(cb);
    inPass = false;
    graphics = nullptr;
    // The render pass left its attachments in the layouts of its final state.
    if (target) {
        const QRhiTextureRenderTargetDescription desc = target->description();
        for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i) {
            const QRhiColorAttachment *att = desc.colorAttachmentAt(i);
            if (auto *tex = static_cast<VulkanTexture *>(att->texture()))
                tex->layout = att->resolveTexture() ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            if (auto *res = static_cast<VulkanTexture *>(att->resolveTexture()))
                res->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
        if (auto *depth = static_cast<VulkanTexture *>(desc.depthTexture()))
            depth->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
    target = nullptr;
    resourceUpdate(resourceUpdates);
}

void VulkanCommandBuffer::setGraphicsPipeline(QRhiGraphicsPipeline *ps) {
    if (!inPass) {
        qWarning("QRhiCommandBuffer::setGraphicsPipeline: not in a pass");
        return;
    }
    graphics = static_cast<VulkanGraphicsPipeline *>(ps);
    if (!graphics || graphics->pipeline == VK_NULL_HANDLE) {
        qWarning("QRhiCommandBuffer::setGraphicsPipeline: the pipeline was not created");
        graphics = nullptr;
        return;
    }
    vk->df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics->pipeline);
}

void VulkanCommandBuffer::setShaderResources(QRhiShaderResourceBindings *srb, int, const std::pair<int, quint32> *) {
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipelineBindPoint point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    QRhiShaderResourceBindings *own = nullptr;
    if (inCompute && compute) {
        layout = compute->layout;
        point = VK_PIPELINE_BIND_POINT_COMPUTE;
        own = compute->shaderResourceBindings();
    } else if (inPass && graphics) {
        layout = graphics->layout;
        own = graphics->shaderResourceBindings();
    }
    if (layout == VK_NULL_HANDLE) {
        qWarning("QRhiCommandBuffer::setShaderResources: no pipeline is set");
        return;
    }
    if (!srb)
        srb = own;
    if (!srb->isLayoutCompatible(own)) {
        qWarning("QRhiCommandBuffer::setShaderResources: the bindings do not match the pipeline");
        return;
    }
    auto *vsrb = static_cast<VulkanShaderResourceBindings *>(srb);
    vk->df->vkCmdBindDescriptorSets(cb, point, layout, 0, 1, &vsrb->set, 0, nullptr);
}

void VulkanCommandBuffer::setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings,
                                         QRhiBuffer *indexBuf, quint32 indexOffset, IndexFormat indexFormat) {
    if (!inPass)
        return;
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceSize> offsets;
    for (int i = 0; i < bindingCount; ++i) {
        buffers.push_back(static_cast<VulkanBuffer *>(bindings[i].first)->buffer);
        offsets.push_back(bindings[i].second);
    }
    if (bindingCount > 0)
        vk->df->vkCmdBindVertexBuffers(cb, static_cast<quint32>(startBinding), static_cast<quint32>(bindingCount),
                                       buffers.data(), offsets.data());
    if (indexBuf)
        vk->df->vkCmdBindIndexBuffer(cb, static_cast<VulkanBuffer *>(indexBuf)->buffer, indexOffset,
                                     indexFormat == IndexUInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

void VulkanCommandBuffer::setViewport(const QRhiViewport &viewport) {
    if (!inPass || !target)
        return;
    const auto [x, y, w, h] = viewport.viewport();
    // QRhi viewports have their origin at the bottom left.
    VkViewport vp = {
        x, static_cast<float>(target->size.height()) - (y + h), w, h, viewport.minDepth(), viewport.maxDepth()};
    vk->df->vkCmdSetViewport(cb, 0, 1, &vp);
    if (!graphics || !graphics->flags().testFlag(QRhiGraphicsPipeline::UsesScissor)) {
        VkRect2D scissor = {{static_cast<qint32>(vp.x), static_cast<qint32>(vp.y)},
                            {static_cast<quint32>(std::max(0.0f, w)), static_cast<quint32>(std::max(0.0f, h))}};
        vk->df->vkCmdSetScissor(cb, 0, 1, &scissor);
    }
}

void VulkanCommandBuffer::setScissor(const QRhiScissor &scissor) {
    if (!inPass || !target)
        return;
    const auto [x, y, w, h] = scissor.scissor();
    VkRect2D rect = {{x, target->size.height() - (y + h)},
                     {static_cast<quint32>(std::max(0, w)), static_cast<quint32>(std::max(0, h))}};
    vk->df->vkCmdSetScissor(cb, 0, 1, &rect);
}

void VulkanCommandBuffer::setBlendConstants(const QColor &c) {
    const float constants[4] = {static_cast<float>(c.redF()), static_cast<float>(c.greenF()),
                                static_cast<float>(c.blueF()), static_cast<float>(c.alphaF())};
    if (inPass)
        vk->df->vkCmdSetBlendConstants(cb, constants);
}

void VulkanCommandBuffer::setStencilRef(quint32 refValue) {
    if (inPass)
        vk->df->vkCmdSetStencilReference(cb, VK_STENCIL_FRONT_AND_BACK, refValue);
}

void VulkanCommandBuffer::draw(quint32 vertexCount, quint32 instanceCount, quint32 firstVertex, quint32 firstInstance) {
    if (!inPass || !graphics) {
        qWarning("QRhiCommandBuffer::draw: no pass or no pipeline");
        return;
    }
    vk->df->vkCmdDraw(cb, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandBuffer::drawIndexed(quint32 indexCount, quint32 instanceCount, quint32 firstIndex,
                                      qint32 vertexOffset, quint32 firstInstance) {
    if (!inPass || !graphics) {
        qWarning("QRhiCommandBuffer::drawIndexed: no pass or no pipeline");
        return;
    }
    vk->df->vkCmdDrawIndexed(cb, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VulkanCommandBuffer::beginComputePass(QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags) {
    if (!recording || inPass || inCompute) {
        qWarning("QRhiCommandBuffer::beginComputePass: not in a frame, or a pass is already in progress");
        return;
    }
    resourceUpdate(resourceUpdates);
    inCompute = true;
    compute = nullptr;
}

void VulkanCommandBuffer::endComputePass(QRhiResourceUpdateBatch *resourceUpdates) {
    if (!inCompute) {
        qWarning("QRhiCommandBuffer::endComputePass: no compute pass is in progress");
        return;
    }
    VkMemoryBarrier mb = {};
    mb.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vk->df->vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1,
                                 &mb, 0, nullptr, 0, nullptr);
    inCompute = false;
    compute = nullptr;
    resourceUpdate(resourceUpdates);
}

void VulkanCommandBuffer::setComputePipeline(QRhiComputePipeline *ps) {
    if (!inCompute)
        return;
    compute = static_cast<VulkanComputePipeline *>(ps);
    if (compute && compute->pipeline != VK_NULL_HANDLE)
        vk->df->vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, compute->pipeline);
}

void VulkanCommandBuffer::dispatch(int x, int y, int z) {
    if (!inCompute || !compute) {
        qWarning("QRhiCommandBuffer::dispatch: no compute pass or no pipeline");
        return;
    }
    vk->df->vkCmdDispatch(cb, static_cast<quint32>(x), static_cast<quint32>(y), static_cast<quint32>(z));
}

} // namespace

QRhiImplementation *createVulkanRhi(QRhiInitParams *params, QRhiNativeHandles *importDevice) {
    auto *p = static_cast<QRhiVulkanInitParams *>(params);
    if (!p || !p->inst) {
        qWarning("QRhi::create: the Vulkan backend needs a QRhiVulkanInitParams with an instance");
        return nullptr;
    }
    auto *backend = new VulkanRhi;
    backend->inst = p->inst;
    backend->requestedExtensions = p->deviceExtensions;
    if (importDevice)
        backend->handles = *static_cast<QRhiVulkanNativeHandles *>(importDevice);
    return backend;
}

#endif // QT_CONFIG(vulkan)
