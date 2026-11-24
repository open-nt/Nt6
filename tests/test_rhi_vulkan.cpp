// MIT License - see LICENSE
//
// Tests for the Vulkan backend of QRhi: a triangle with a vertex buffer, a dynamic uniform buffer and a sampled texture is
// drawn into a texture and read back; a compute shader fills a storage buffer. Skipped where there is no Vulkan loader
// or device (Mesa's lavapipe works, in software).
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qvulkaninstance.h>
#include <rhi/qrhi.h>
#include <rhi/qrhivulkan.h>

#include <cstdio>
#include <cstring>
#include <memory>

// Under AddressSanitizer: the Vulkan loader and drivers keep their tables to the end of the process (see test_vulkan.cpp).
#if defined(__SANITIZE_ADDRESS__)
extern "C" const char *__asan_default_options();
extern "C" const char *__lsan_default_suppressions();
extern "C" const char *__asan_default_options() { return "fast_unwind_on_malloc=0"; }
extern "C" const char *__lsan_default_suppressions() {
    return "leak:libvulkan\nleak:libvulkan_lvp\nleak:libvulkan_intel\nleak:libvulkan_radeon\nleak:libnvidia-\nleak:"
           "libVkLayer\n"
           "leak:libgallium\nleak:libLLVM\nleak:libdrm\nleak:libdbus-1\nleak:libxcb\nleak:libX11\n";
}
#endif

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static QShader loadShader(const char *path, QShader::Stage stage) {
    QFile file(QString::fromLatin1(path));
    QShader shader;
    if (!file.open(QIODevice::ReadOnly))
        return shader;
    shader.setStage(stage);
    shader.setShader(QShaderKey(QShader::SpirvShader, QShaderVersion(100)), QShaderCode(file.readAll(), "main"));
    return shader;
}

static void testDraw(QRhi *rhi) {
    const QSize size(64, 64);
    QRhiTexture *color =
        rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    CHECK(color->create());
    QRhiTextureRenderTarget *target =
        rhi->newTextureRenderTarget(QRhiTextureRenderTargetDescription(QRhiColorAttachment(color)));
    QRhiRenderPassDescriptor *pass = target->newCompatibleRenderPassDescriptor();
    CHECK(pass->create());
    target->setRenderPassDescriptor(pass);
    CHECK(target->create() && target->pixelSize() == size);

    // x, y, r, g, b: a triangle in the middle of the target, green
    const float vertices[] = {-0.5f, -0.5f, 0, 1, 0, 0.5f, -0.5f, 0, 1, 0, 0.0f, 0.5f, 0, 1, 0};
    QRhiBuffer *vbuf = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertices));
    CHECK(vbuf->create());
    QRhiBuffer *ubuf = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    CHECK(ubuf->create());

    QImage white(1, 1, QImage::Format_RGBA8888);
    white.fill(QColor(255, 255, 255, 255));
    QRhiTexture *tex = rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1));
    CHECK(tex->create());
    QRhiSampler *sampler = rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    CHECK(sampler->create());
    QRhiShaderResourceBindings *srb = rhi->newShaderResourceBindings();
    srb->setBindings(
        {QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, ubuf),
         QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, tex, sampler)});
    CHECK(srb->create());

    QShader vs = loadShader(":/shaders/rhi_tri.vert.spv", QShader::VertexStage);
    QShader fs = loadShader(":/shaders/rhi_tri.frag.spv", QShader::FragmentStage);
    CHECK(vs.isValid() && fs.isValid());
    QRhiGraphicsPipeline *pipeline = rhi->newGraphicsPipeline();
    pipeline->setShaderStages(
        {QRhiShaderStage(QRhiShaderStage::Vertex, vs), QRhiShaderStage(QRhiShaderStage::Fragment, fs)});
    QRhiVertexInputLayout layout;
    layout.setBindings({QRhiVertexInputBinding(5 * sizeof(float))});
    layout.setAttributes({QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
                          QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, 2 * sizeof(float))});
    pipeline->setVertexInputLayout(layout);
    pipeline->setShaderResourceBindings(srb);
    pipeline->setRenderPassDescriptor(pass);
    CHECK(pipeline->create());

    QRhiResourceUpdateBatch *updates = rhi->nextResourceUpdateBatch();
    updates->uploadStaticBuffer(vbuf, vertices);
    updates->uploadTexture(tex, white);
    const float tint[4] = {1.0f, 0.5f, 1.0f, 1.0f};
    updates->updateDynamicBuffer(ubuf, 0, 16, tint);

    QRhiReadbackResult result;
    QRhiCommandBuffer *cb = nullptr;
    CHECK(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    cb->beginPass(target, QColor(0, 0, 255, 255), QRhiDepthStencilClearValue(), updates);
    cb->setGraphicsPipeline(pipeline);
    cb->setViewport(QRhiViewport(0, 0, float(size.width()), float(size.height())));
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput input(vbuf, 0);
    cb->setVertexInput(0, 1, &input);
    cb->draw(3);
    QRhiResourceUpdateBatch *readback = rhi->nextResourceUpdateBatch();
    readback->readBackTexture(QRhiReadbackDescription(color), &result);
    cb->endPass(readback);
    CHECK(rhi->endOffscreenFrame() == QRhi::FrameOpSuccess);

    CHECK(result.pixelSize == size && result.data.size() == 64 * 64 * 4);
    if (result.data.size() == 64 * 64 * 4) {
        const auto pixel = [&](int x, int y) {
            const uchar *p = reinterpret_cast<const uchar *>(result.data.constData()) + (y * 64 + x) * 4;
            return QColor(p[0], p[1], p[2], p[3]);
        };
        const QColor inside = pixel(32, 40); // in the triangle: green times the tint, times the white texel
        CHECK(inside.red() == 0 && inside.green() >= 126 && inside.green() <= 129 && inside.blue() == 0 &&
              inside.alpha() == 255);
        const QColor outside = pixel(2, 2); // the clear color
        CHECK(outside == QColor(0, 0, 255, 255));
    }

    delete pipeline;
    delete srb;
    delete sampler;
    delete tex;
    delete ubuf;
    delete vbuf;
    delete target;
    delete pass;
    delete color;
}

static void testCompute(QRhi *rhi) {
    if (!rhi->isFeatureSupported(QRhi::Compute))
        return;
    QRhiBuffer *buffer = rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 64 * sizeof(quint32));
    CHECK(buffer->create());
    QRhiShaderResourceBindings *srb = rhi->newShaderResourceBindings();
    srb->setBindings({QRhiShaderResourceBinding::bufferStore(0, QRhiShaderResourceBinding::ComputeStage, buffer)});
    CHECK(srb->create());
    QRhiComputePipeline *pipeline = rhi->newComputePipeline();
    pipeline->setShaderStage(
        QRhiShaderStage(QRhiShaderStage::Compute, loadShader(":/shaders/rhi_fill.comp.spv", QShader::ComputeStage)));
    pipeline->setShaderResourceBindings(srb);
    CHECK(pipeline->create());

    QRhiReadbackResult result;
    QRhiCommandBuffer *cb = nullptr;
    CHECK(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    cb->beginComputePass();
    cb->setComputePipeline(pipeline);
    cb->setShaderResources();
    cb->dispatch(8, 1, 1);
    QRhiResourceUpdateBatch *readback = rhi->nextResourceUpdateBatch();
    readback->readBackBuffer(buffer, 0, 64 * sizeof(quint32), &result);
    cb->endComputePass(readback);
    CHECK(rhi->endOffscreenFrame() == QRhi::FrameOpSuccess);
    CHECK(result.data.size() == 64 * 4);
    if (result.data.size() == 64 * 4) {
        quint32 values[64];
        std::memcpy(values, result.data.constData(), sizeof(values));
        CHECK(values[0] == 0 && values[1] == 3 && values[63] == 189);
    }
    delete pipeline;
    delete srb;
    delete buffer;
}

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QVulkanInstance instance;
    if (!instance.create()) {
        std::printf("rhi vulkan: skipped, no Vulkan instance (error %d)\n", int(instance.errorCode()));
        return 0;
    }
    QRhiVulkanInitParams params;
    params.inst = &instance;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Vulkan, &params));
    if (!rhi) {
        std::printf("rhi vulkan: skipped, no usable Vulkan device\n");
        return 0;
    }
    std::printf("rhi vulkan: %s\n", qPrintable(rhi->driverInfoString()));
    CHECK(rhi->backend() == QRhi::Vulkan && !rhi->isYUpInFramebuffer() && rhi->isClipDepthZeroToOne());
    CHECK(rhi->nativeHandles() &&
          static_cast<const QRhiVulkanNativeHandles *>(rhi->nativeHandles())->dev != VK_NULL_HANDLE);
    CHECK(rhi->resourceLimit(QRhi::TextureSizeMax) >= 1024);
    testDraw(rhi.get());
    testCompute(rhi.get());
    CHECK(rhi->totalResourceCount() == 0);
    rhi.reset();
    if (g_failures == 0)
        std::printf("rhi vulkan: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
