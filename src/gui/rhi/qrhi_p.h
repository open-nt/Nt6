// MIT License - see LICENSE
#pragma once

#include <rhi/qrhi.h>

#include <memory>
#include <vector>

// What a resource update batch holds until a backend executes it.
class QRhiResourceUpdateBatchPrivate {
public:
    struct BufferOp {
        enum Type { DynamicUpdate, StaticUpload, Read };
        Type type = DynamicUpdate;
        QRhiBuffer *buf = nullptr;
        quint32 offset = 0;
        quint32 size = 0;
        QByteArray data; // for the updates and uploads
        QRhiReadbackResult *result = nullptr;
    };
    struct TextureOp {
        enum Type { Upload, Copy, Read, GenMips };
        Type type = Upload;
        QRhiTexture *dst = nullptr;
        QRhiTexture *src = nullptr; // for a copy
        QRhiTextureUploadDescription upload;
        QRhiTextureCopyDescription copy;
        QRhiReadbackDescription read;
        QRhiReadbackResult *result = nullptr;
    };

    QRhiImplementation *rhi = nullptr;
    int poolIndex = -1;
    std::vector<BufferOp> bufferOps;
    std::vector<TextureOp> textureOps;

    void clear() {
        bufferOps.clear();
        textureOps.clear();
    }
    bool isEmpty() const { return bufferOps.empty() && textureOps.empty(); }
};

class QRhiImplementation {
public:
    virtual ~QRhiImplementation();

    virtual bool create(QRhi::Flags flags) = 0;
    virtual void destroy() = 0;

    virtual QRhi::Implementation backend() const = 0;
    virtual QString driverInfo() const = 0;

    virtual QRhiGraphicsPipeline *createGraphicsPipeline() = 0;
    virtual QRhiComputePipeline *createComputePipeline() = 0;
    virtual QRhiShaderResourceBindings *createShaderResourceBindings() = 0;
    virtual QRhiBuffer *createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size) = 0;
    virtual QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                                 int sampleCount) = 0;
    virtual QRhiTexture *createTexture(QRhiTexture::Format format, const QSize &pixelSize, int depth, int arraySize,
                                       int sampleCount, QRhiTexture::Flags flags) = 0;
    virtual QRhiSampler *createSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                                       QRhiSampler::Filter mipmapMode, QRhiSampler::AddressMode u,
                                       QRhiSampler::AddressMode v, QRhiSampler::AddressMode w) = 0;
    virtual QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                               QRhiTextureRenderTarget::Flags flags) = 0;

    virtual QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **cb, QRhi::BeginFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags flags) = 0;
    virtual QRhi::FrameOpResult finish() = 0;

    virtual QList<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
    virtual bool isYUpInNDC() const = 0;
    virtual bool isClipDepthZeroToOne() const = 0;
    virtual QMatrix4x4 clipSpaceCorrMatrix() const = 0;
    virtual bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const = 0;
    virtual bool isFeatureSupported(QRhi::Feature feature) const = 0;
    virtual int resourceLimit(QRhi::ResourceLimit limit) const = 0;
    virtual const QRhiNativeHandles *nativeHandles() = 0;
    virtual bool isDeviceLost() const = 0;
    virtual void releaseCachedResources() = 0;
    virtual int currentFrameSlot() const = 0;

    // Runs the operations of the batch outside of a pass; called by the command buffer of a backend.
    virtual void executeUpdates(QRhiCommandBuffer *cb, QRhiResourceUpdateBatch *batch) = 0;

    // ---- shared ----
    static int bytesPerPixel(QRhiTexture::Format format);
    static bool isDepthFormat(QRhiTexture::Format format);
    static QByteArray pixelsForUpload(const QRhiTextureSubresourceUploadDescription &desc, QRhiTexture::Format format,
                                      QSize *size);
    static QSize levelSize(const QSize &base, int level);
    static QRhiResourceUpdateBatchPrivate *batchPrivate(QRhiResourceUpdateBatch *batch) { return batch->d.get(); }

    void registerResource(QRhiResource *resource);
    void unregisterResource(QRhiResource *resource);
    void deleteLater(QRhiResource *resource);
    void runCleanup();
    QRhiResourceUpdateBatch *nextBatch();
    void releaseBatch(QRhiResourceUpdateBatch *batch);

    QRhi *q = nullptr;
    QRhi::Flags rhiFlags;
    quint64 nextResourceId = 1;
    std::vector<QRhiResource *> resources;
    std::vector<QRhiResource *> pendingDeletes;
    std::vector<std::unique_ptr<QRhiResourceUpdateBatch>> batchPool;
    std::vector<bool> batchInUse;
    bool inDestructor = false;
};

QRhiImplementation *createNullRhi(QRhiInitParams *params);
#if QT_CONFIG(vulkan)
QRhiImplementation *createVulkanRhi(QRhiInitParams *params, QRhiNativeHandles *importDevice);
#endif
