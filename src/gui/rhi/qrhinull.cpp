// MIT License - see LICENSE
// The Null backend of QRhi: it keeps the bytes of buffers and textures, clears render targets to the clear color, and
// draws nothing. It checks the arguments like the other backends, so applications can be tested without a GPU.
#include "qrhi_p.h"

#include <QtCore/qdebug.h>

#include <algorithm>
#include <cstring>
#include <map>

namespace {

class NullRhi;

class NullBuffer final : public QRhiBuffer {
public:
    NullBuffer(QRhiImplementation *rhi, Type type, UsageFlags usage, quint32 size)
        : QRhiBuffer(rhi, type, usage, size) {}
    ~NullBuffer() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Buffer; }
    void destroy() override { bytes.clear(); }
    bool create() override {
        if (size() == 0) {
            qWarning("QRhiBuffer: the size is 0");
            return false;
        }
        bytes = QByteArray(static_cast<qsizetype>(size()), '\0');
        return true;
    }
    NativeBuffer nativeBuffer() override { return NativeBuffer(); }

    QByteArray bytes;
};

class NullTexture final : public QRhiTexture {
public:
    NullTexture(QRhiImplementation *rhi, Format format, const QSize &size, int depth, int arraySize, int sampleCount,
                Flags flags)
        : QRhiTexture(rhi, format, size, depth, arraySize, sampleCount, flags) {}
    ~NullTexture() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Texture; }
    void destroy() override { levels.clear(); }
    bool create() override {
        if (pixelSize().isEmpty() || QRhiImplementation::bytesPerPixel(format()) == 0) {
            qWarning("QRhiTexture: the size is empty or the format unknown");
            return false;
        }
        levels.clear();
        const int levelCount = flags().testFlag(MipMapped) ? QRhi::mipLevelsForSize(pixelSize()) : 1;
        const int layerCount = flags().testFlag(CubeMap) ? 6 : std::max(1, arraySize());
        for (int layer = 0; layer < layerCount; ++layer) {
            for (int level = 0; level < levelCount; ++level) {
                const QSize s = QRhiImplementation::levelSize(pixelSize(), level);
                levels[key(layer, level)] = QByteArray(
                    static_cast<qsizetype>(s.width()) * s.height() * QRhiImplementation::bytesPerPixel(format()), '\0');
            }
        }
        return true;
    }
    NativeTexture nativeTexture() override { return NativeTexture(); }
    bool createFrom(NativeTexture) override { return false; }

    static int key(int layer, int level) { return layer * 64 + level; }
    QByteArray *level(int layer, int level) {
        const auto it = levels.find(key(layer, level));
        return it == levels.end() ? nullptr : &it->second;
    }

    std::map<int, QByteArray> levels;
};

class NullSampler final : public QRhiSampler {
public:
    NullSampler(QRhiImplementation *rhi, Filter mag, Filter min, Filter mip, AddressMode u, AddressMode v,
                AddressMode w)
        : QRhiSampler(rhi, mag, min, mip, u, v, w) {}
    ~NullSampler() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::Sampler; }
    void destroy() override {}
    bool create() override { return true; }
};

class NullRenderBuffer final : public QRhiRenderBuffer {
public:
    NullRenderBuffer(QRhiImplementation *rhi, Type type, const QSize &size, int samples)
        : QRhiRenderBuffer(rhi, type, size, samples) {}
    ~NullRenderBuffer() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::RenderBuffer; }
    void destroy() override {}
    bool create() override { return !pixelSize().isEmpty(); }
};

class NullRenderPassDescriptor final : public QRhiRenderPassDescriptor {
public:
    explicit NullRenderPassDescriptor(QRhiImplementation *rhi) : QRhiRenderPassDescriptor(rhi) {}
    ~NullRenderPassDescriptor() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::RenderPassDescriptor; }
    void destroy() override {}
    bool create() override { return true; }
    bool isCompatible(const QRhiRenderPassDescriptor *other) const override { return other != nullptr; }
};

class NullTextureRenderTarget final : public QRhiTextureRenderTarget {
public:
    NullTextureRenderTarget(QRhiImplementation *rhi, const QRhiTextureRenderTargetDescription &desc, Flags flags)
        : QRhiTextureRenderTarget(rhi, desc, flags) {}
    ~NullTextureRenderTarget() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::TextureRenderTarget; }
    void destroy() override { created = false; }
    QSize pixelSize() const override { return size; }
    float devicePixelRatio() const override { return 1.0f; }
    int sampleCount() const override { return samples; }
    QRhiRenderPassDescriptor *newCompatibleRenderPassDescriptor() override {
        return new NullRenderPassDescriptor(m_rhiImpl);
    }
    bool create() override {
        const QRhiTextureRenderTargetDescription desc = description();
        if (desc.colorAttachmentCount() == 0 && !desc.depthStencilBuffer() && !desc.depthTexture()) {
            qWarning("QRhiTextureRenderTarget: there are no attachments");
            return false;
        }
        if (!renderPassDescriptor()) {
            qWarning("QRhiTextureRenderTarget: no render pass descriptor was set");
            return false;
        }
        size = QSize();
        samples = 1;
        for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i) {
            const QRhiColorAttachment *att = desc.colorAttachmentAt(i);
            QSize s;
            if (att->texture()) {
                if (!att->texture()->flags().testFlag(QRhiTexture::RenderTarget)) {
                    qWarning("QRhiTextureRenderTarget: a color texture lacks the RenderTarget flag");
                    return false;
                }
                s = QRhiImplementation::levelSize(att->texture()->pixelSize(), att->level());
                samples = att->texture()->sampleCount();
            } else if (att->renderBuffer()) {
                s = att->renderBuffer()->pixelSize();
                samples = att->renderBuffer()->sampleCount();
            } else {
                qWarning("QRhiTextureRenderTarget: a color attachment has neither texture nor render buffer");
                return false;
            }
            if (size.isValid() && size != s) {
                qWarning("QRhiTextureRenderTarget: the attachments differ in size");
                return false;
            }
            size = s;
        }
        if (!size.isValid() && desc.depthTexture())
            size = desc.depthTexture()->pixelSize();
        if (!size.isValid() && desc.depthStencilBuffer())
            size = desc.depthStencilBuffer()->pixelSize();
        created = true;
        return true;
    }

    QSize size;
    int samples = 1;
    bool created = false;
};

class NullShaderResourceBindings final : public QRhiShaderResourceBindings {
public:
    explicit NullShaderResourceBindings(QRhiImplementation *rhi) : QRhiShaderResourceBindings(rhi) {}
    ~NullShaderResourceBindings() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::ShaderResourceBindings; }
    void destroy() override {}
    bool create() override {
        for (const QRhiShaderResourceBinding &b : m_bindings) {
            const bool needsBuffer = b.type() == QRhiShaderResourceBinding::UniformBuffer ||
                                     b.type() >= QRhiShaderResourceBinding::BufferLoad;
            if (needsBuffer && !b.buffer()) {
                qWarning("QRhiShaderResourceBindings: a buffer binding has no buffer");
                return false;
            }
            if (b.type() == QRhiShaderResourceBinding::SampledTexture && (!b.texture() || !b.sampler())) {
                qWarning("QRhiShaderResourceBindings: a sampled texture binding lacks the texture or the sampler");
                return false;
            }
        }
        return true;
    }
};

class NullGraphicsPipeline final : public QRhiGraphicsPipeline {
public:
    explicit NullGraphicsPipeline(QRhiImplementation *rhi) : QRhiGraphicsPipeline(rhi) {}
    ~NullGraphicsPipeline() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::GraphicsPipeline; }
    void destroy() override {}
    bool create() override {
        if (shaderStageCount() == 0 || !shaderResourceBindings() || !renderPassDescriptor()) {
            qWarning("QRhiGraphicsPipeline: shader stages, bindings and a render pass descriptor are needed");
            return false;
        }
        return true;
    }
};

class NullComputePipeline final : public QRhiComputePipeline {
public:
    explicit NullComputePipeline(QRhiImplementation *rhi) : QRhiComputePipeline(rhi) {}
    ~NullComputePipeline() override { destroy(); }
    QRhiResource::Type resourceType() const override { return QRhiResource::ComputePipeline; }
    void destroy() override {}
    bool create() override { return shaderResourceBindings() != nullptr && shaderStage().shader().isValid(); }
};

class NullCommandBuffer final : public QRhiCommandBuffer {
public:
    explicit NullCommandBuffer(QRhiImplementation *rhi) : QRhiCommandBuffer(rhi) {}
    ~NullCommandBuffer() override = default;
    QRhiResource::Type resourceType() const override { return QRhiResource::CommandBuffer; }
    void destroy() override {}

    void resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates) override;
    void beginPass(QRhiRenderTarget *rt, const QColor &colorClearValue,
                   const QRhiDepthStencilClearValue &depthStencilClearValue, QRhiResourceUpdateBatch *resourceUpdates,
                   BeginPassFlags flags) override;
    void endPass(QRhiResourceUpdateBatch *resourceUpdates) override;
    void setGraphicsPipeline(QRhiGraphicsPipeline *ps) override {
        if (!inPass)
            qWarning("QRhiCommandBuffer::setGraphicsPipeline: not in a pass");
        pipeline = ps;
    }
    void setShaderResources(QRhiShaderResourceBindings *srb, int, const std::pair<int, quint32> *) override {
        if (!pipeline)
            qWarning("QRhiCommandBuffer::setShaderResources: no pipeline is set");
        else if (srb && !srb->isLayoutCompatible(pipeline->shaderResourceBindings()))
            qWarning("QRhiCommandBuffer::setShaderResources: the bindings do not match the pipeline");
    }
    void setVertexInput(int, int, const VertexInput *, QRhiBuffer *, quint32, IndexFormat) override {}
    void setViewport(const QRhiViewport &) override {}
    void setScissor(const QRhiScissor &) override {}
    void setBlendConstants(const QColor &) override {}
    void setStencilRef(quint32) override {}
    void draw(quint32, quint32, quint32, quint32) override {
        if (!inPass || !pipeline)
            qWarning("QRhiCommandBuffer::draw: no pass or no pipeline");
    }
    void drawIndexed(quint32, quint32, quint32, qint32, quint32) override {
        if (!inPass || !pipeline)
            qWarning("QRhiCommandBuffer::drawIndexed: no pass or no pipeline");
    }
    void beginComputePass(QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags) override {
        runUpdates(resourceUpdates);
        inCompute = true;
    }
    void endComputePass(QRhiResourceUpdateBatch *resourceUpdates) override {
        inCompute = false;
        runUpdates(resourceUpdates);
    }
    void setComputePipeline(QRhiComputePipeline *) override {}
    void dispatch(int, int, int) override {
        if (!inCompute)
            qWarning("QRhiCommandBuffer::dispatch: not in a compute pass");
    }

    void runUpdates(QRhiResourceUpdateBatch *batch);

    QRhiGraphicsPipeline *pipeline = nullptr;
    bool inPass = false;
    bool inCompute = false;
};

class NullRhi final : public QRhiImplementation {
public:
    bool create(QRhi::Flags) override {
        cb = std::make_unique<NullCommandBuffer>(this);
        unregisterResource(cb.get()); // the command buffer belongs to the backend, not to the application
        return true;
    }
    void destroy() override { cb.reset(); }
    QRhi::Implementation backend() const override { return QRhi::Null; }
    QString driverInfo() const override { return QStringLiteral("Null"); }

    QRhiGraphicsPipeline *createGraphicsPipeline() override { return new NullGraphicsPipeline(this); }
    QRhiComputePipeline *createComputePipeline() override { return new NullComputePipeline(this); }
    QRhiShaderResourceBindings *createShaderResourceBindings() override { return new NullShaderResourceBindings(this); }
    QRhiBuffer *createBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size) override {
        return new NullBuffer(this, type, usage, size);
    }
    QRhiRenderBuffer *createRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize,
                                         int sampleCount) override {
        return new NullRenderBuffer(this, type, pixelSize, sampleCount);
    }
    QRhiTexture *createTexture(QRhiTexture::Format format, const QSize &pixelSize, int depth, int arraySize,
                               int sampleCount, QRhiTexture::Flags textureFlags) override {
        return new NullTexture(this, format, pixelSize, depth, arraySize, sampleCount, textureFlags);
    }
    QRhiSampler *createSampler(QRhiSampler::Filter mag, QRhiSampler::Filter min, QRhiSampler::Filter mip,
                               QRhiSampler::AddressMode u, QRhiSampler::AddressMode v,
                               QRhiSampler::AddressMode w) override {
        return new NullSampler(this, mag, min, mip, u, v, w);
    }
    QRhiTextureRenderTarget *createTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                       QRhiTextureRenderTarget::Flags targetFlags) override {
        return new NullTextureRenderTarget(this, desc, targetFlags);
    }

    QRhi::FrameOpResult beginOffscreenFrame(QRhiCommandBuffer **out, QRhi::BeginFrameFlags) override {
        if (inFrame) {
            qWarning("QRhi::beginOffscreenFrame: a frame is already in progress");
            return QRhi::FrameOpError;
        }
        inFrame = true;
        *out = cb.get();
        return QRhi::FrameOpSuccess;
    }
    QRhi::FrameOpResult endOffscreenFrame(QRhi::EndFrameFlags) override {
        if (!inFrame) {
            qWarning("QRhi::endOffscreenFrame: no frame is in progress");
            return QRhi::FrameOpError;
        }
        inFrame = false;
        return QRhi::FrameOpSuccess;
    }
    QRhi::FrameOpResult finish() override { return QRhi::FrameOpSuccess; }

    QList<int> supportedSampleCounts() const override { return {1, 4}; }
    int ubufAlignment() const override { return 256; }
    bool isYUpInFramebuffer() const override { return false; }
    bool isYUpInNDC() const override { return true; }
    bool isClipDepthZeroToOne() const override { return true; }
    QMatrix4x4 clipSpaceCorrMatrix() const override {
        QMatrix4x4 m;
        m(1, 1) = -1.0f;
        m(2, 2) = 0.5f;
        m(2, 3) = 0.5f;
        return m;
    }
    bool isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags) const override {
        return format != QRhiTexture::UnknownFormat;
    }
    bool isFeatureSupported(QRhi::Feature feature) const override {
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
        case QRhi::ReadBackNonUniformBuffer:
        case QRhi::ReadBackNonBaseMipLevel:
        case QRhi::TexelFetch:
        case QRhi::RenderToNonBaseMipLevel:
        case QRhi::IntAttributes:
        case QRhi::ScreenSpaceDerivatives:
        case QRhi::ReadBackAnyTextureFormat:
        case QRhi::ImageDataStride:
        case QRhi::TextureArrays:
            return true;
        default:
            return false;
        }
    }
    int resourceLimit(QRhi::ResourceLimit limit) const override {
        switch (limit) {
        case QRhi::TextureSizeMin:
            return 1;
        case QRhi::TextureSizeMax:
            return 16384;
        case QRhi::MaxColorAttachments:
            return 8;
        case QRhi::FramesInFlight:
        case QRhi::MaxAsyncReadbackFrames:
            return 1;
        case QRhi::MaxThreadGroupsPerDimension:
            return 65535;
        case QRhi::MaxThreadsPerThreadGroup:
        case QRhi::MaxThreadGroupX:
        case QRhi::MaxThreadGroupY:
            return 1024;
        case QRhi::MaxThreadGroupZ:
            return 64;
        case QRhi::TextureArraySizeMax:
            return 2048;
        case QRhi::MaxUniformBufferRange:
            return 65536;
        case QRhi::MaxVertexInputs:
        case QRhi::MaxVertexOutputs:
            return 16;
        }
        return 0;
    }
    const QRhiNativeHandles *nativeHandles() override { return nullptr; }
    bool isDeviceLost() const override { return false; }
    void releaseCachedResources() override {}
    int currentFrameSlot() const override { return 0; }

    void executeUpdates(QRhiCommandBuffer *, QRhiResourceUpdateBatch *batch) override;

    std::unique_ptr<NullCommandBuffer> cb;
    bool inFrame = false;
};

void NullRhi::executeUpdates(QRhiCommandBuffer *, QRhiResourceUpdateBatch *batch) {
    QRhiResourceUpdateBatchPrivate *d = batchPrivate(batch);
    for (const QRhiResourceUpdateBatchPrivate::BufferOp &op : d->bufferOps) {
        auto *buf = static_cast<NullBuffer *>(op.buf);
        if (buf->bytes.isEmpty()) {
            qWarning("QRhiResourceUpdateBatch: the buffer was not created");
            continue;
        }
        if (op.type == QRhiResourceUpdateBatchPrivate::BufferOp::Read) {
            op.result->data = buf->bytes.mid(op.offset, op.size);
            if (op.result->completed)
                op.result->completed();
        } else {
            std::memcpy(buf->bytes.data() + op.offset, op.data.constData(), op.size);
        }
    }
    for (const QRhiResourceUpdateBatchPrivate::TextureOp &op : d->textureOps) {
        switch (op.type) {
        case QRhiResourceUpdateBatchPrivate::TextureOp::Upload: {
            auto *tex = static_cast<NullTexture *>(op.dst);
            for (const QRhiTextureUploadEntry &entry : op.upload.entries()) {
                QByteArray *level = tex->level(entry.layer(), entry.level());
                if (!level) {
                    qWarning("QRhiResourceUpdateBatch::uploadTexture: no such layer or level");
                    continue;
                }
                const QSize levelSize = QRhiImplementation::levelSize(tex->pixelSize(), entry.level());
                const int bpp = QRhiImplementation::bytesPerPixel(tex->format());
                QSize size;
                const QByteArray pixels = pixelsForUpload(entry.description(), tex->format(), &size);
                const QPoint at = entry.description().destinationTopLeft();
                if (!size.isValid() || pixels.size() < static_cast<qsizetype>(size.width()) * size.height() * bpp ||
                    at.x() + size.width() > levelSize.width() || at.y() + size.height() > levelSize.height()) {
                    qWarning("QRhiResourceUpdateBatch::uploadTexture: the pixels do not fit");
                    continue;
                }
                for (int y = 0; y < size.height(); ++y)
                    std::memcpy(level->data() +
                                    (static_cast<std::ptrdiff_t>(at.y() + y) * levelSize.width() + at.x()) * bpp,
                                pixels.constData() + static_cast<std::ptrdiff_t>(y) * size.width() * bpp,
                                static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(bpp));
            }
            break;
        }
        case QRhiResourceUpdateBatchPrivate::TextureOp::Copy: {
            auto *dst = static_cast<NullTexture *>(op.dst);
            auto *src = static_cast<NullTexture *>(op.src);
            QByteArray *from = src->level(op.copy.sourceLayer(), op.copy.sourceLevel());
            QByteArray *to = dst->level(op.copy.destinationLayer(), op.copy.destinationLevel());
            if (!from || !to || src->format() != dst->format()) {
                qWarning("QRhiResourceUpdateBatch::copyTexture: bad layer, level or format");
                break;
            }
            const int bpp = QRhiImplementation::bytesPerPixel(src->format());
            const QSize fromSize = QRhiImplementation::levelSize(src->pixelSize(), op.copy.sourceLevel());
            const QSize toSize = QRhiImplementation::levelSize(dst->pixelSize(), op.copy.destinationLevel());
            const QSize size = op.copy.pixelSize().isValid() ? op.copy.pixelSize() : fromSize;
            const QPoint a = op.copy.sourceTopLeft(), b = op.copy.destinationTopLeft();
            if (a.x() + size.width() > fromSize.width() || a.y() + size.height() > fromSize.height() ||
                b.x() + size.width() > toSize.width() || b.y() + size.height() > toSize.height()) {
                qWarning("QRhiResourceUpdateBatch::copyTexture: the rectangle does not fit");
                break;
            }
            for (int y = 0; y < size.height(); ++y)
                std::memcpy(to->data() + (static_cast<std::ptrdiff_t>(b.y() + y) * toSize.width() + b.x()) * bpp,
                            from->constData() +
                                (static_cast<std::ptrdiff_t>(a.y() + y) * fromSize.width() + a.x()) * bpp,
                            static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(bpp));
            break;
        }
        case QRhiResourceUpdateBatchPrivate::TextureOp::Read: {
            auto *tex = static_cast<NullTexture *>(op.read.texture());
            QByteArray *level = tex->level(op.read.layer(), op.read.level());
            if (!level) {
                qWarning("QRhiResourceUpdateBatch::readBackTexture: no such layer or level");
                break;
            }
            op.result->format = tex->format();
            op.result->pixelSize = QRhiImplementation::levelSize(tex->pixelSize(), op.read.level());
            op.result->data = *level;
            if (op.result->completed)
                op.result->completed();
            break;
        }
        case QRhiResourceUpdateBatchPrivate::TextureOp::GenMips:
            break;
        }
    }
    d->clear();
}

void NullCommandBuffer::runUpdates(QRhiResourceUpdateBatch *batch) {
    if (!batch)
        return;
    m_rhiImpl->executeUpdates(this, batch);
    m_rhiImpl->releaseBatch(batch);
}

void NullCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates) {
    if (inPass)
        qWarning("QRhiCommandBuffer::resourceUpdate: inside a pass");
    runUpdates(resourceUpdates);
}

void NullCommandBuffer::beginPass(QRhiRenderTarget *rt, const QColor &colorClearValue,
                                  const QRhiDepthStencilClearValue &, QRhiResourceUpdateBatch *resourceUpdates,
                                  BeginPassFlags) {
    if (inPass) {
        qWarning("QRhiCommandBuffer::beginPass: a pass is already in progress");
        return;
    }
    inPass = true;
    pipeline = nullptr;
    runUpdates(resourceUpdates);
    auto *target = static_cast<NullTextureRenderTarget *>(rt);
    if (!target || !target->created) {
        qWarning("QRhiCommandBuffer::beginPass: the render target was not created");
        return;
    }
    if (target->flags().testFlag(QRhiTextureRenderTarget::PreserveColorContents))
        return;
    // The clear of the color attachments, which is what makes a Null pass testable.
    const QRhiTextureRenderTargetDescription desc = target->description();
    for (qsizetype i = 0; i < desc.colorAttachmentCount(); ++i) {
        const QRhiColorAttachment *att = desc.colorAttachmentAt(i);
        if (!att->texture())
            continue;
        auto *tex = static_cast<NullTexture *>(att->texture());
        QByteArray *level = tex->level(att->layer(), att->level());
        if (!level)
            continue;
        if (tex->format() == QRhiTexture::RGBA8 || tex->format() == QRhiTexture::BGRA8) {
            char px[4] = {static_cast<char>(colorClearValue.red()), static_cast<char>(colorClearValue.green()),
                          static_cast<char>(colorClearValue.blue()), static_cast<char>(colorClearValue.alpha())};
            if (tex->format() == QRhiTexture::BGRA8)
                std::swap(px[0], px[2]);
            for (qsizetype p = 0; p + 4 <= level->size(); p += 4)
                std::memcpy(level->data() + p, px, 4);
        }
    }
}

void NullCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates) {
    if (!inPass)
        qWarning("QRhiCommandBuffer::endPass: no pass is in progress");
    inPass = false;
    pipeline = nullptr;
    runUpdates(resourceUpdates);
}

} // namespace

QRhiImplementation *createNullRhi(QRhiInitParams *params) {
    (void)params;
    return new NullRhi;
}
