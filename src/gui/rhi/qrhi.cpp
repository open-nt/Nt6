// MIT License - see LICENSE
#include "qrhi_p.h"

#include <QtCore/qdebug.h>

#include <algorithm>

/*!
    \class QRhiInitParams
    \brief The QRhiInitParams struct is the base of the parameters that QRhi::create() takes for a backend.
*/

/*!
    \class QRhiNativeHandles
    \brief The QRhiNativeHandles struct is the base of the native objects of a backend, to import or to inspect.
*/

/*!
    \class QRhiNullInitParams
    \brief The QRhiNullInitParams struct is the parameters of the Null backend. There are none.
*/

/*!
    \class QRhiNullNativeHandles
    \brief The QRhiNullNativeHandles struct is the native handles of the Null backend. There are none.
*/

/*!
    \class QRhiViewport
    \brief The QRhiViewport class describes the part of a render target that a draw goes to, and its depth range.

    \sa QRhiCommandBuffer::setViewport()
*/

/*!
    \class QRhiScissor
    \brief The QRhiScissor class describes the rectangle of a render target that draws are cut to.

    \sa QRhiCommandBuffer::setScissor()
*/

/*!
    \class QRhiVertexInputBinding
    \brief The QRhiVertexInputBinding class describes a vertex buffer binding: how far apart the vertices are.

    \sa QRhiVertexInputLayout
*/

/*!
    \class QRhiVertexInputAttribute
    \brief The QRhiVertexInputAttribute class describes where in a binding the data of a shader input is.

    \sa QRhiVertexInputLayout
*/

/*!
    \class QRhiVertexInputLayout
    \brief The QRhiVertexInputLayout class describes the vertex buffers of a pipeline: the bindings and the attributes.

    \sa QRhiGraphicsPipeline
*/

/*!
    \class QRhiShaderStage
    \brief The QRhiShaderStage class pairs a QShader with the stage of the pipeline that it runs in.

    \sa QRhiGraphicsPipeline, QShader
*/

/*!
    \class QRhiResource
    \brief The QRhiResource class is the base of the objects that a QRhi makes: buffers, textures, pipelines and so on.

    A resource is made by a function of QRhi, set up with its setters, and then made native with \c create(). It
    belongs to the application, which deletes it before it deletes the QRhi. The destructor calls destroy(). A resource
    that is still in use by the GPU can be given to deleteLater(), which deletes it when no frame uses it any more.

    \differences
    A QRhi that is deleted while resources are alive destroys them and prints a warning, instead of crashing.
    \enddifferences

    \sa QRhi
*/

/*!
    \class QRhiBuffer
    \brief The QRhiBuffer class is a vertex, index, uniform or storage buffer.

    \code
    QRhiBuffer *vbuf = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices));
    vbuf->create();
    batch->uploadStaticBuffer(vbuf, vertices);
    \endcode

    \sa QRhi::newBuffer(), QRhiResourceUpdateBatch
*/

/*!
    \class QRhiTexture
    \brief The QRhiTexture class is a texture or an image: 1D, 2D, 3D, an array or a cube map, with optional mip levels.

    \sa QRhi::newTexture(), QRhiSampler
*/

/*!
    \class QRhiSampler
    \brief The QRhiSampler class says how a texture is filtered and addressed when a shader reads it.

    \sa QRhi::newSampler()
*/

/*!
    \class QRhiRenderBuffer
    \brief The QRhiRenderBuffer class is a depth-stencil or color buffer that is rendered to but not read by shaders.

    \sa QRhi::newRenderBuffer()
*/

/*!
    \class QRhiRenderPassDescriptor
    \brief The QRhiRenderPassDescriptor class describes the formats of the attachments that a render pass and its pipelines use.

    \sa QRhiTextureRenderTarget::newCompatibleRenderPassDescriptor()
*/

/*!
    \class QRhiColorAttachment
    \brief The QRhiColorAttachment class describes one color attachment of a texture render target.

    \sa QRhiTextureRenderTargetDescription
*/

/*!
    \class QRhiTextureRenderTargetDescription
    \brief The QRhiTextureRenderTargetDescription class lists the attachments of a texture render target.

    \sa QRhiTextureRenderTarget
*/

/*!
    \class QRhiRenderTarget
    \brief The QRhiRenderTarget class is the base of the places that a render pass draws to.

    \sa QRhiTextureRenderTarget
*/

/*!
    \class QRhiTextureRenderTarget
    \brief The QRhiTextureRenderTarget class is a render target made of textures and render buffers.

    \code
    QRhiTexture *tex = rhi->newTexture(QRhiTexture::RGBA8, QSize(256, 256), 1, QRhiTexture::RenderTarget);
    tex->create();
    QRhiTextureRenderTarget *rt = rhi->newTextureRenderTarget({tex});
    QRhiRenderPassDescriptor *rp = rt->newCompatibleRenderPassDescriptor();
    rp->create();
    rt->setRenderPassDescriptor(rp);
    rt->create();
    \endcode

    \sa QRhi::newTextureRenderTarget()
*/

/*!
    \class QRhiShaderResourceBinding
    \brief The QRhiShaderResourceBinding class describes one buffer, texture or image that shaders can reach.

    \sa QRhiShaderResourceBindings
*/

/*!
    \class QRhiShaderResourceBindings
    \brief The QRhiShaderResourceBindings class is the set of bindings that a draw or a dispatch uses.

    A set that is made for a pipeline can be replaced by any other set with the same layout when drawing.

    \sa QRhiShaderResourceBinding, QRhiGraphicsPipeline
*/

/*!
    \class QRhiGraphicsPipeline
    \brief The QRhiGraphicsPipeline class is the state of a draw: shaders, vertex input, rasterization, blending and tests.

    \sa QRhi::newGraphicsPipeline()
*/

/*!
    \class QRhiComputePipeline
    \brief The QRhiComputePipeline class is the state of a dispatch: a compute shader and its bindings.

    \sa QRhi::newComputePipeline()
*/

/*!
    \class QRhiTextureSubresourceUploadDescription
    \brief The QRhiTextureSubresourceUploadDescription class describes the pixels that go to one layer and level of a texture.

    \sa QRhiTextureUploadEntry
*/

/*!
    \class QRhiTextureUploadEntry
    \brief The QRhiTextureUploadEntry class names the layer and level that a QRhiTextureSubresourceUploadDescription goes to.

    \sa QRhiTextureUploadDescription
*/

/*!
    \class QRhiTextureUploadDescription
    \brief The QRhiTextureUploadDescription class lists what is uploaded to a texture.

    \sa QRhiResourceUpdateBatch::uploadTexture()
*/

/*!
    \class QRhiTextureCopyDescription
    \brief The QRhiTextureCopyDescription class describes the part of a texture that is copied to another.

    \sa QRhiResourceUpdateBatch::copyTexture()
*/

/*!
    \class QRhiReadbackDescription
    \brief The QRhiReadbackDescription class names the texture, layer and level that is read back.

    \sa QRhiResourceUpdateBatch::readBackTexture()
*/

/*!
    \class QRhiReadbackResult
    \brief The QRhiReadbackResult struct receives what a read back returns.

    The data is there when the function \c completed is called; the Null and Vulkan backends of Nt complete the read back
    when the operation runs, which is at the end of the pass or the frame.

    \sa QRhiResourceUpdateBatch::readBackTexture()
*/

/*!
    \class QRhiResourceUpdateBatch
    \brief The QRhiResourceUpdateBatch class collects uploads, copies and read backs, to run them at a safe point of a frame.

    Get a batch from QRhi::nextResourceUpdateBatch(), add to it, and give it to beginPass(), endPass() or
    QRhiCommandBuffer::resourceUpdate(), which run it and release it.

    \code
    QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
    batch->updateDynamicBuffer(ubuf, 0, 64, matrix.constData());
    cb->beginPass(rt, Qt::black, {1.0f, 0}, batch);
    \endcode

    \sa QRhi
*/

/*!
    \class QRhiDepthStencilClearValue
    \brief The QRhiDepthStencilClearValue struct holds the depth and stencil values that a pass clears to.

    \sa QRhiCommandBuffer::beginPass()
*/

/*!
    \class QRhiCommandBuffer
    \brief The QRhiCommandBuffer class records the passes and draws of a frame.

    \code
    QRhiCommandBuffer *cb = nullptr;
    rhi->beginOffscreenFrame(&cb);
    cb->beginPass(rt, Qt::black, {1.0f, 0});
    cb->setGraphicsPipeline(pipeline);
    cb->setViewport(QRhiViewport(0, 0, 256, 256));
    cb->setShaderResources();
    cb->draw(3);
    cb->endPass();
    rhi->endOffscreenFrame();
    \endcode

    \sa QRhi::beginOffscreenFrame()
*/

/*!
    \class QRhi
    \brief The QRhi class is the rendering hardware interface: buffers, textures, pipelines and passes over Vulkan.

    A QRhi is made for a backend with create(). The application makes resources with it, records passes in a frame, and
    deletes the resources and then the QRhi:

    \code
    QRhiNullInitParams params;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Null, &params));
    \endcode

    The Vulkan backend takes a QRhiVulkanInitParams with a QVulkanInstance. All backends share the API and the checks of
    the arguments, so the Null backend, which only keeps the bytes of buffers and textures, finds the errors of an
    application without a GPU.

    \differences
    This is a subset of Qt's QRhi: no swap chains yet (frames are offscreen), no timestamps, no tessellation or geometry
    pipelines, no multiview and no variable rate shading, and the shaders are \l QShader files made by \c ntsb.
    D3D11, D3D12 and Metal are not built. \c QRhiSwapChain and the OpenGL backend follow.
    \enddifferences

    \sa QRhiResource, QShader
*/

/*!
    \fn QRhiResource::Type QRhiResource::resourceType() const

    Returns the kind of the resource.
*/

/*!
    \fn void QRhiResource::destroy()

    Releases the native objects of the resource, which can then be made again with \c create(). Called by the destructor.
*/

/*!
    \fn QByteArray QRhiResource::name() const

    Returns the name of the resource, for debugging.
*/

/*!
    \fn quint64 QRhiResource::globalResourceId() const

    Returns a number that is different for every resource of the process.
*/

/*!
    \fn QRhi *QRhiResource::rhi() const

    Returns the QRhi that made the resource.
*/

/*!
    \fn bool QRhiBuffer::create()

    Makes the native buffer from the type, the usage and the size. Returns \c false if that fails.
*/

/*!
    \fn QRhiBuffer::NativeBuffer QRhiBuffer::nativeBuffer()

    Returns the native objects of the buffer.
*/

/*!
    \fn bool QRhiTexture::create()

    Makes the native texture. Returns \c false if that fails.
*/

/*!
    \fn QRhiTexture::NativeTexture QRhiTexture::nativeTexture()

    Returns the native object of the texture.
*/

/*!
    \fn bool QRhiTexture::createFrom(NativeTexture src)

    Makes the texture from the native object \a src, which the texture does not own.
*/

/*!
    \fn bool QRhiSampler::create()

    Makes the native sampler. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiRenderBuffer::create()

    Makes the native render buffer. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiRenderPassDescriptor::create()

    Makes the native render pass. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiRenderPassDescriptor::isCompatible(const QRhiRenderPassDescriptor *other) const

    Returns \c true if a pipeline made for this descriptor can be used in a pass of \a other.
*/

/*!
    \fn QSize QRhiRenderTarget::pixelSize() const

    Returns the size in pixels of the render target.
*/

/*!
    \fn float QRhiRenderTarget::devicePixelRatio() const

    Returns the ratio of device pixels to logical pixels of the render target: 1 for a texture.
*/

/*!
    \fn int QRhiRenderTarget::sampleCount() const

    Returns the number of samples for each pixel.
*/

/*!
    \fn QRhiRenderPassDescriptor *QRhiTextureRenderTarget::newCompatibleRenderPassDescriptor()

    Returns a new render pass descriptor that matches the attachments. The caller owns it, makes it with \c create() and gives it to setRenderPassDescriptor().
*/

/*!
    \fn bool QRhiTextureRenderTarget::create()

    Makes the native framebuffer from the description and the render pass descriptor. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiShaderResourceBindings::create()

    Makes the native binding layout and set. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiGraphicsPipeline::create()

    Makes the native pipeline. Needs the shader stages, the vertex input layout, the bindings and the render pass descriptor. Returns \c false if that fails.
*/

/*!
    \fn bool QRhiComputePipeline::create()

    Makes the native pipeline. Returns \c false if that fails.
*/

/*!
    \fn void QRhiCommandBuffer::resourceUpdate(QRhiResourceUpdateBatch *resourceUpdates)

    Runs the operations of \a resourceUpdates, outside of a pass, and releases the batch.
*/

/*!
    \fn void QRhiCommandBuffer::beginPass(QRhiRenderTarget *rt, const QColor &colorClearValue, const QRhiDepthStencilClearValue &depthStencilClearValue, QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags flags)

    Begins a render pass into \a rt, clearing the color to \a colorClearValue and the depth and stencil to \a depthStencilClearValue unless the target preserves them. The operations of \a resourceUpdates run first. \a flags has options.
*/

/*!
    \fn void QRhiCommandBuffer::endPass(QRhiResourceUpdateBatch *resourceUpdates)

    Ends the render pass, and then runs the operations of \a resourceUpdates.
*/

/*!
    \fn void QRhiCommandBuffer::setGraphicsPipeline(QRhiGraphicsPipeline *ps)

    Makes \a ps the pipeline of the next draws.
*/

/*!
    \fn void QRhiCommandBuffer::setShaderResources(QRhiShaderResourceBindings *srb, int dynamicOffsetCount, const std::pair<int, quint32> *dynamicOffsets)

    Makes \a srb the bindings of the next draws; \c nullptr means the bindings of the pipeline. \a dynamicOffsets has \a dynamicOffsetCount pairs of a binding and an offset; Nt's bindings have no dynamic offsets, so they are not used.
*/

/*!
    \fn void QRhiCommandBuffer::setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings, QRhiBuffer *indexBuf, quint32 indexOffset, IndexFormat indexFormat)

    Sets \a bindingCount vertex buffers from \a startBinding to those in the array \a bindings, and the index buffer \a indexBuf from \a indexOffset with the \a indexFormat.
*/

/*!
    \fn void QRhiCommandBuffer::setViewport(const QRhiViewport &viewport)

    Sets the viewport to \a viewport.
*/

/*!
    \fn void QRhiCommandBuffer::setScissor(const QRhiScissor &scissor)

    Sets the scissor rectangle to \a scissor. The pipeline must have the UsesScissor flag.
*/

/*!
    \fn void QRhiCommandBuffer::setBlendConstants(const QColor &c)

    Sets the blend constants to \a c. The pipeline must have the UsesBlendConstants flag.
*/

/*!
    \fn void QRhiCommandBuffer::setStencilRef(quint32 refValue)

    Sets the stencil reference value to \a refValue. The pipeline must have the UsesStencilRef flag.
*/

/*!
    \fn void QRhiCommandBuffer::draw(quint32 vertexCount, quint32 instanceCount, quint32 firstVertex, quint32 firstInstance)

    Draws \a vertexCount vertices of \a instanceCount instances, from \a firstVertex and \a firstInstance.
*/

/*!
    \fn void QRhiCommandBuffer::drawIndexed(quint32 indexCount, quint32 instanceCount, quint32 firstIndex, qint32 vertexOffset, quint32 firstInstance)

    Draws \a indexCount indexes of \a instanceCount instances, from \a firstIndex, adding \a vertexOffset to each index, from \a firstInstance.
*/

/*!
    \fn void QRhiCommandBuffer::beginComputePass(QRhiResourceUpdateBatch *resourceUpdates, BeginPassFlags flags)

    Begins a compute pass. The operations of \a resourceUpdates run first. \a flags has options.
*/

/*!
    \fn void QRhiCommandBuffer::endComputePass(QRhiResourceUpdateBatch *resourceUpdates)

    Ends the compute pass, and then runs the operations of \a resourceUpdates.
*/

/*!
    \fn void QRhiCommandBuffer::setComputePipeline(QRhiComputePipeline *ps)

    Makes \a ps the pipeline of the next dispatches.
*/

/*!
    \fn void QRhiCommandBuffer::dispatch(int x, int y, int z)

    Runs \a x by \a y by \a z thread groups.
*/

// ---------------------------------------------------------------------------------------------------------------------
// QRhiResource

QRhiResource::QRhiResource(QRhiImplementation *rhi) : m_rhiImpl(rhi), m_rhi(rhi->q), m_id(rhi->nextResourceId++) {
    rhi->registerResource(this);
}

/*!
    Destroys the resource. The native objects have to be released by the destructor of the class of the backend, which calls
    destroy().
*/
QRhiResource::~QRhiResource() {
    if (m_rhiImpl)
        m_rhiImpl->unregisterResource(this);
}

/*!
    Deletes the resource when the frames that may still use it are done: at the next call of QRhi::runCleanup(), which the
    end of a frame makes. Use it for a resource that was used in a frame that is in flight.
*/
void QRhiResource::deleteLater() {
    if (m_rhiImpl)
        m_rhiImpl->deleteLater(this);
    else
        delete this;
}

/*!
    Sets the name of the resource to \a name, which a backend gives to the native object for debugging tools if
    QRhi::EnableDebugMarkers is set.
*/
void QRhiResource::setName(const QByteArray &name) { m_name = name; }

// ---------------------------------------------------------------------------------------------------------------------
// QRhiShaderResourceBindings

bool QRhiShaderResourceBindings::isLayoutCompatible(const QRhiShaderResourceBindings *other) const {
    if (other == this)
        return true;
    if (!other || other->m_bindings.size() != m_bindings.size())
        return false;
    for (std::size_t i = 0; i < m_bindings.size(); ++i) {
        const QRhiShaderResourceBinding &a = m_bindings[i];
        const QRhiShaderResourceBinding &b = other->m_bindings[i];
        if (a.binding() != b.binding() || a.type() != b.type() || a.stage() != b.stage())
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// QRhiImplementation

QRhiImplementation::~QRhiImplementation() = default;

int QRhiImplementation::bytesPerPixel(QRhiTexture::Format format) {
    switch (format) {
    case QRhiTexture::RGBA8:
    case QRhiTexture::BGRA8:
    case QRhiTexture::RG16:
    case QRhiTexture::R32F:
    case QRhiTexture::RGB10A2:
    case QRhiTexture::D24:
    case QRhiTexture::D24S8:
    case QRhiTexture::D32F:
        return 4;
    case QRhiTexture::R8:
        return 1;
    case QRhiTexture::RG8:
    case QRhiTexture::R16:
    case QRhiTexture::R16F:
    case QRhiTexture::D16:
        return 2;
    case QRhiTexture::RGBA16F:
        return 8;
    case QRhiTexture::RGBA32F:
        return 16;
    case QRhiTexture::UnknownFormat:
        break;
    }
    return 0;
}

bool QRhiImplementation::isDepthFormat(QRhiTexture::Format format) {
    return format == QRhiTexture::D16 || format == QRhiTexture::D24 || format == QRhiTexture::D24S8 ||
           format == QRhiTexture::D32F;
}

QSize QRhiImplementation::levelSize(const QSize &base, int level) {
    return QSize(std::max(1, base.width() >> level), std::max(1, base.height() >> level));
}

// The pixels of an upload as bytes of the format of the texture; the size of the part is stored in *size.
QByteArray QRhiImplementation::pixelsForUpload(const QRhiTextureSubresourceUploadDescription &desc,
                                               QRhiTexture::Format format, QSize *size) {
    if (!desc.image().isNull()) {
        QImage image = desc.image();
        if (desc.sourceSize().isValid() || !desc.sourceTopLeft().isNull())
            image =
                image.copy(QRect(desc.sourceTopLeft(), desc.sourceSize().isValid() ? desc.sourceSize() : image.size()));
        if (format == QRhiTexture::R8) {
            image = image.convertToFormat(QImage::Format_Grayscale8);
        } else {
            image = image.convertToFormat(QImage::Format_RGBA8888);
        }
        *size = image.size();
        const int bpp = QRhiImplementation::bytesPerPixel(format);
        QByteArray out;
        out.reserve(static_cast<qsizetype>(image.width()) * image.height() * bpp);
        for (int y = 0; y < image.height(); ++y) {
            const uchar *line = image.constScanLine(y);
            for (int x = 0; x < image.width(); ++x) {
                const uchar *p = line + static_cast<std::ptrdiff_t>(x) * (format == QRhiTexture::R8 ? 1 : 4);
                if (format == QRhiTexture::BGRA8) {
                    out.append(static_cast<char>(p[2]));
                    out.append(static_cast<char>(p[1]));
                    out.append(static_cast<char>(p[0]));
                    out.append(static_cast<char>(p[3]));
                } else {
                    out.append(reinterpret_cast<const char *>(p), format == QRhiTexture::R8 ? 1 : 4);
                }
            }
        }
        return out;
    }
    *size = desc.sourceSize();
    return desc.data();
}

void QRhiImplementation::registerResource(QRhiResource *resource) { resources.push_back(resource); }

void QRhiImplementation::unregisterResource(QRhiResource *resource) {
    const auto it = std::find(resources.begin(), resources.end(), resource);
    if (it != resources.end())
        resources.erase(it);
    const auto pending = std::find(pendingDeletes.begin(), pendingDeletes.end(), resource);
    if (pending != pendingDeletes.end())
        pendingDeletes.erase(pending);
}

void QRhiImplementation::deleteLater(QRhiResource *resource) {
    if (std::find(pendingDeletes.begin(), pendingDeletes.end(), resource) == pendingDeletes.end())
        pendingDeletes.push_back(resource);
}

void QRhiImplementation::runCleanup() {
    std::vector<QRhiResource *> toDelete;
    toDelete.swap(pendingDeletes);
    for (QRhiResource *resource : toDelete)
        delete resource;
}

QRhiResourceUpdateBatch *QRhiImplementation::nextBatch() {
    for (std::size_t i = 0; i < batchPool.size(); ++i) {
        if (!batchInUse[i]) {
            batchInUse[i] = true;
            return batchPool[i].get();
        }
    }
    batchPool.emplace_back(new QRhiResourceUpdateBatch(this));
    batchInUse.push_back(true);
    batchPool.back()->d->poolIndex = static_cast<int>(batchPool.size()) - 1;
    return batchPool.back().get();
}

void QRhiImplementation::releaseBatch(QRhiResourceUpdateBatch *batch) {
    const int index = batch->d->poolIndex;
    if (index >= 0 && static_cast<std::size_t>(index) < batchPool.size()) {
        batch->d->clear();
        batchInUse[static_cast<std::size_t>(index)] = false;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// QRhiResourceUpdateBatch

QRhiResourceUpdateBatch::QRhiResourceUpdateBatch(QRhiImplementation *rhi) : d(new QRhiResourceUpdateBatchPrivate) {
    d->rhi = rhi;
}

/*!
    Destroys the batch. Batches are owned by the QRhi; use release() instead.
*/
QRhiResourceUpdateBatch::~QRhiResourceUpdateBatch() = default;

/*!
    Gives the batch back to the QRhi without running it. The batch must not be used afterwards.
*/
void QRhiResourceUpdateBatch::release() { d->rhi->releaseBatch(this); }

/*!
    Appends the operations of \a other to this batch, and releases \a other.
*/
void QRhiResourceUpdateBatch::merge(QRhiResourceUpdateBatch *other) {
    if (!other || other == this)
        return;
    d->bufferOps.insert(d->bufferOps.end(), other->d->bufferOps.begin(), other->d->bufferOps.end());
    d->textureOps.insert(d->textureOps.end(), other->d->textureOps.begin(), other->d->textureOps.end());
    other->release();
}

/*!
    Returns \c true: Nt's batches grow as needed, so the capacity is always fine.
*/
bool QRhiResourceUpdateBatch::hasOptimalCapacity() const { return true; }

/*!
    Records that \a size bytes of \a data go to the dynamic buffer \a buf at \a offset. The data is copied now.
*/
void QRhiResourceUpdateBatch::updateDynamicBuffer(QRhiBuffer *buf, quint32 offset, quint32 size, const void *data) {
    if (!buf || buf->type() != QRhiBuffer::Dynamic) {
        qWarning("QRhiResourceUpdateBatch::updateDynamicBuffer: the buffer is not dynamic");
        return;
    }
    if (offset + size > buf->size() || !data) {
        qWarning("QRhiResourceUpdateBatch::updateDynamicBuffer: the range is outside the buffer");
        return;
    }
    QRhiResourceUpdateBatchPrivate::BufferOp op;
    op.type = QRhiResourceUpdateBatchPrivate::BufferOp::DynamicUpdate;
    op.buf = buf;
    op.offset = offset;
    op.size = size;
    op.data = QByteArray(static_cast<const char *>(data), static_cast<qsizetype>(size));
    d->bufferOps.push_back(std::move(op));
}

/*!
    Records that \a size bytes of \a data go to the immutable or static buffer \a buf at \a offset. The data is copied now.
*/
void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, quint32 offset, quint32 size, const void *data) {
    if (!buf || buf->type() == QRhiBuffer::Dynamic) {
        qWarning("QRhiResourceUpdateBatch::uploadStaticBuffer: the buffer is dynamic");
        return;
    }
    if (offset + size > buf->size() || !data) {
        qWarning("QRhiResourceUpdateBatch::uploadStaticBuffer: the range is outside the buffer");
        return;
    }
    QRhiResourceUpdateBatchPrivate::BufferOp op;
    op.type = QRhiResourceUpdateBatchPrivate::BufferOp::StaticUpload;
    op.buf = buf;
    op.offset = offset;
    op.size = size;
    op.data = QByteArray(static_cast<const char *>(data), static_cast<qsizetype>(size));
    d->bufferOps.push_back(std::move(op));
}

/*!
    \overload

    Records that the whole of \a buf is set from \a data, which has to have the size of the buffer.
*/
void QRhiResourceUpdateBatch::uploadStaticBuffer(QRhiBuffer *buf, const void *data) {
    if (buf)
        uploadStaticBuffer(buf, 0, buf->size(), data);
}

/*!
    Records that the pixels of \a desc go to the texture \a tex.
*/
void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QRhiTextureUploadDescription &desc) {
    if (!tex || desc.entries().empty())
        return;
    QRhiResourceUpdateBatchPrivate::TextureOp op;
    op.type = QRhiResourceUpdateBatchPrivate::TextureOp::Upload;
    op.dst = tex;
    op.upload = desc;
    d->textureOps.push_back(std::move(op));
}

/*!
    \overload

    Records that the \a image goes to the first level of the first layer of \a tex.
*/
void QRhiResourceUpdateBatch::uploadTexture(QRhiTexture *tex, const QImage &image) {
    if (image.isNull()) {
        qWarning("QRhiResourceUpdateBatch::uploadTexture: the image is null");
        return;
    }
    uploadTexture(tex, QRhiTextureUploadDescription(
                           QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(image))));
}

/*!
    Records a copy of the part of \a src that \a desc describes to \a dst. Both need the right flags: \a src
    QRhiTexture::UsedAsTransferSource.
*/
void QRhiResourceUpdateBatch::copyTexture(QRhiTexture *dst, QRhiTexture *src, const QRhiTextureCopyDescription &desc) {
    if (!dst || !src)
        return;
    QRhiResourceUpdateBatchPrivate::TextureOp op;
    op.type = QRhiResourceUpdateBatchPrivate::TextureOp::Copy;
    op.dst = dst;
    op.src = src;
    op.copy = desc;
    d->textureOps.push_back(std::move(op));
}

/*!
    Records that the texture of \a rb is read back into \a result. The texture needs QRhiTexture::UsedAsTransferSource.
*/
void QRhiResourceUpdateBatch::readBackTexture(const QRhiReadbackDescription &rb, QRhiReadbackResult *result) {
    if (!rb.texture() || !result) {
        qWarning("QRhiResourceUpdateBatch::readBackTexture: no texture or no result");
        return;
    }
    QRhiResourceUpdateBatchPrivate::TextureOp op;
    op.type = QRhiResourceUpdateBatchPrivate::TextureOp::Read;
    op.read = rb;
    op.result = result;
    d->textureOps.push_back(std::move(op));
}

/*!
    Records that \a size bytes of the buffer \a buf from \a offset are read back into \a result.
*/
void QRhiResourceUpdateBatch::readBackBuffer(QRhiBuffer *buf, quint32 offset, quint32 size,
                                             QRhiReadbackResult *result) {
    if (!buf || !result || offset + size > buf->size()) {
        qWarning("QRhiResourceUpdateBatch::readBackBuffer: bad arguments");
        return;
    }
    QRhiResourceUpdateBatchPrivate::BufferOp op;
    op.type = QRhiResourceUpdateBatchPrivate::BufferOp::Read;
    op.buf = buf;
    op.offset = offset;
    op.size = size;
    op.result = result;
    d->bufferOps.push_back(std::move(op));
}

/*!
    Records that the mip levels of \a tex are made from the first one. The texture needs QRhiTexture::MipMapped and
    QRhiTexture::UsedWithGenerateMips.
*/
void QRhiResourceUpdateBatch::generateMips(QRhiTexture *tex) {
    if (!tex)
        return;
    QRhiResourceUpdateBatchPrivate::TextureOp op;
    op.type = QRhiResourceUpdateBatchPrivate::TextureOp::GenMips;
    op.dst = tex;
    d->textureOps.push_back(std::move(op));
}

// ---------------------------------------------------------------------------------------------------------------------
// QRhi

QRhi::QRhi() = default;

/*!
    Destroys the QRhi. The resources that are still alive are destroyed, with a warning: delete them first.
*/
QRhi::~QRhi() {
    if (!d)
        return;
    d->runCleanup();
    if (!d->resources.empty()) {
        qWarning("QRhi: %zu resources are still alive; destroying them", d->resources.size());
        std::vector<QRhiResource *> alive = d->resources;
        for (QRhiResource *resource : alive) {
            resource->destroy();
            resource->m_rhiImpl = nullptr; // the resource outlives the QRhi, and must not touch it any more
        }
        d->resources.clear();
    }
    d->batchPool.clear();
    d->destroy();
}

/*!
    Makes a QRhi for the backend \a impl with the parameters \a params, which are those of the backend, such as
    QRhiVulkanInitParams, and \a flags has options. \a importDevice, if not \c nullptr, gives native objects of the application to use. Returns
    \c nullptr if the backend is not available or fails.
*/
QRhi *QRhi::create(Implementation impl, QRhiInitParams *params, Flags flags, QRhiNativeHandles *importDevice) {
    std::unique_ptr<QRhiImplementation> backend;
    switch (impl) {
    case Null:
        backend.reset(createNullRhi(params));
        break;
    case Vulkan:
#if QT_CONFIG(vulkan)
        backend.reset(createVulkanRhi(params, importDevice));
#else
        (void)importDevice;
#endif
        break;
    default:
        break;
    }
    if (!backend)
        return nullptr;
    std::unique_ptr<QRhi> rhi(new QRhi);
    backend->q = rhi.get();
    backend->rhiFlags = flags;
    rhi->d = std::move(backend);
    if (!rhi->d->create(flags))
        return nullptr;
    return rhi.release();
}

/*!
    Returns the backend of this QRhi.
*/
QRhi::Implementation QRhi::backend() const { return d->backend(); }

/*!
    Returns the name of the backend.
*/
QString QRhi::backendName() const { return backendName(backend()); }

/*!
    Returns the name of the backend \a impl.
*/
QString QRhi::backendName(Implementation impl) {
    switch (impl) {
    case Null:
        return QStringLiteral("Null");
    case Vulkan:
        return QStringLiteral("Vulkan");
    case OpenGLES2:
        return QStringLiteral("OpenGL");
    case D3D11:
        return QStringLiteral("D3D11");
    case Metal:
        return QStringLiteral("Metal");
    case D3D12:
        return QStringLiteral("D3D12");
    }
    return QString();
}

/*!
    Returns a text about the device and the driver, such as "llvmpipe (LLVM 19)", for logs.
*/
QString QRhi::driverInfoString() const { return d->driverInfo(); }

/*!
    Returns a new graphics pipeline. Set its state and call \c create().
*/
QRhiGraphicsPipeline *QRhi::newGraphicsPipeline() { return d->createGraphicsPipeline(); }

/*!
    Returns a new compute pipeline. Set its state and call \c create().
*/
QRhiComputePipeline *QRhi::newComputePipeline() { return d->createComputePipeline(); }

/*!
    Returns a new, empty set of shader resource bindings. Set the bindings and call \c create().
*/
QRhiShaderResourceBindings *QRhi::newShaderResourceBindings() { return d->createShaderResourceBindings(); }

/*!
    Returns a new buffer of the \a type, for the \a usage, of \a size bytes. Call \c create() to make it native.
*/
QRhiBuffer *QRhi::newBuffer(QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size) {
    return d->createBuffer(type, usage, size);
}

/*!
    Returns a new render buffer of the \a type and \a pixelSize, with \a sampleCount samples. Call \c create() to make it
    native.
*/
QRhiRenderBuffer *QRhi::newRenderBuffer(QRhiRenderBuffer::Type type, const QSize &pixelSize, int sampleCount) {
    return d->createRenderBuffer(type, pixelSize, sampleCount);
}

/*!
    Returns a new 2D texture of the \a format and \a pixelSize, with \a sampleCount samples and the \a flags. Call
    \c create() to make it native.
*/
QRhiTexture *QRhi::newTexture(QRhiTexture::Format format, const QSize &pixelSize, int sampleCount,
                              QRhiTexture::Flags flags) {
    return d->createTexture(format, pixelSize, 0, 0, sampleCount, flags);
}

/*!
    \overload

    Returns a new 3D texture of the \a format, \a width, \a height and \a depth.
*/
QRhiTexture *QRhi::newTexture(QRhiTexture::Format format, int width, int height, int depth, int sampleCount,
                              QRhiTexture::Flags flags) {
    return d->createTexture(format, QSize(width, height), depth, 0, sampleCount, flags | QRhiTexture::ThreeDimensional);
}

/*!
    Returns a new array texture of \a arraySize layers of the \a format and \a pixelSize, with \a sampleCount samples and the
    \a flags.
*/
QRhiTexture *QRhi::newTextureArray(QRhiTexture::Format format, int arraySize, const QSize &pixelSize, int sampleCount,
                                   QRhiTexture::Flags flags) {
    return d->createTexture(format, pixelSize, 0, arraySize, sampleCount, flags | QRhiTexture::TextureArray);
}

/*!
    Returns a new sampler with the filters \a magFilter, \a minFilter and \a mipmapMode, and the address modes \a addressU,
    \a addressV and \a addressW. Call \c create() to make it native.
*/
QRhiSampler *QRhi::newSampler(QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter,
                              QRhiSampler::Filter mipmapMode, QRhiSampler::AddressMode addressU,
                              QRhiSampler::AddressMode addressV, QRhiSampler::AddressMode addressW) {
    return d->createSampler(magFilter, minFilter, mipmapMode, addressU, addressV, addressW);
}

/*!
    Returns a new render target with the attachments of \a desc and the \a flags. Make a render pass descriptor with
    newCompatibleRenderPassDescriptor(), set it, and call \c create().
*/
QRhiTextureRenderTarget *QRhi::newTextureRenderTarget(const QRhiTextureRenderTargetDescription &desc,
                                                      QRhiTextureRenderTarget::Flags flags) {
    return d->createTextureRenderTarget(desc, flags);
}

/*!
    Begins a frame that has no swap chain, and puts its command buffer in \a cb. \a flags has options, of which there are none
    yet. Record passes, and call endOffscreenFrame().
*/
QRhi::FrameOpResult QRhi::beginOffscreenFrame(QRhiCommandBuffer **cb, BeginFrameFlags flags) {
    return d->beginOffscreenFrame(cb, flags);
}

/*!
    Ends the offscreen frame and submits its commands. \a flags has options, of which there are none yet. The GPU work is
    done when the function returns, as it waits.
*/
QRhi::FrameOpResult QRhi::endOffscreenFrame(EndFrameFlags flags) {
    const FrameOpResult result = d->endOffscreenFrame(flags);
    d->runCleanup();
    return result;
}

/*!
    Waits until the GPU is idle.
*/
QRhi::FrameOpResult QRhi::finish() { return d->finish(); }

/*!
    Returns a batch to record resource updates in, outside of a frame too.
*/
QRhiResourceUpdateBatch *QRhi::nextResourceUpdateBatch() { return d->nextBatch(); }

/*!
    Returns the numbers of samples that textures and render buffers can have, from small to large.
*/
QList<int> QRhi::supportedSampleCounts() const { return d->supportedSampleCounts(); }

/*!
    Returns the alignment in bytes of the offsets in uniform buffers.
*/
int QRhi::ubufAlignment() const { return d->ubufAlignment(); }

/*!
    Returns \a v rounded up to a multiple of ubufAlignment().
*/
int QRhi::ubufAligned(int v) const {
    const int a = d->ubufAlignment();
    return a > 1 ? (v + a - 1) / a * a : v;
}

/*!
    Returns the number of mip levels of a texture of \a size, down to 1 by 1.
*/
int QRhi::mipLevelsForSize(const QSize &size) {
    int side = std::max(size.width(), size.height());
    int levels = 1;
    while (side > 1) {
        side >>= 1;
        ++levels;
    }
    return levels;
}

/*!
    Returns the size of the level \a mipLevel of a texture whose first level has the size \a baseLevelSize.
*/
QSize QRhi::sizeForMipLevel(int mipLevel, const QSize &baseLevelSize) {
    return QRhiImplementation::levelSize(baseLevelSize, mipLevel);
}

/*!
    Returns \c true if the first row of the framebuffer is at the bottom, as in OpenGL.
*/
bool QRhi::isYUpInFramebuffer() const { return d->isYUpInFramebuffer(); }

/*!
    Returns \c true if the y axis of normalized device coordinates points up, as in OpenGL.
*/
bool QRhi::isYUpInNDC() const { return d->isYUpInNDC(); }

/*!
    Returns \c true if the clip space depth goes from 0 to 1, not from -1 to 1.
*/
bool QRhi::isClipDepthZeroToOne() const { return d->isClipDepthZeroToOne(); }

/*!
    Returns the matrix to multiply a projection matrix made for OpenGL with, so that it works with this backend.
*/
QMatrix4x4 QRhi::clipSpaceCorrMatrix() const { return d->clipSpaceCorrMatrix(); }

/*!
    Returns \c true if textures of the \a format and \a flags can be made.
*/
bool QRhi::isTextureFormatSupported(QRhiTexture::Format format, QRhiTexture::Flags flags) const {
    return d->isTextureFormatSupported(format, flags);
}

/*!
    Returns \c true if the backend has the \a feature.
*/
bool QRhi::isFeatureSupported(QRhi::Feature feature) const { return d->isFeatureSupported(feature); }

/*!
    Returns the value of the \a limit.
*/
int QRhi::resourceLimit(ResourceLimit limit) const { return d->resourceLimit(limit); }

/*!
    Returns the native objects that the backend uses, such as the \c VkDevice of the Vulkan backend, as a
    QRhiVulkanNativeHandles; \c nullptr if there are none.
*/
const QRhiNativeHandles *QRhi::nativeHandles() { return d->nativeHandles(); }

/*!
    Returns \c true if the device was lost, for example because the driver reset.
*/
bool QRhi::isDeviceLost() const { return d->isDeviceLost(); }

/*!
    Releases caches that can be made again, such as pipeline caches.
*/
void QRhi::releaseCachedResources() { d->releaseCachedResources(); }

/*!
    Deletes the resources that were given to QRhiResource::deleteLater().
*/
void QRhi::runCleanup() { d->runCleanup(); }

/*!
    Returns the slot of the frame in flight that is being recorded: from 0 to resourceLimit(FramesInFlight) minus one.
*/
int QRhi::currentFrameSlot() const { return d->currentFrameSlot(); }

/*!
    Returns the number of resources that are alive.
*/
qsizetype QRhi::totalResourceCount() const { return static_cast<qsizetype>(d->resources.size()); }
