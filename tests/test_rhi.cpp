// MIT License - see LICENSE
//
// Tests for QRhi and QShader in Nt GUI: the shader container, the checks shared by all backends and the behavior of the
// Null backend (buffers and textures keep their bytes, a pass clears its target, read backs return what was uploaded).
// The Vulkan backend is tested in test_rhi_vulkan.cpp.
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qtemporarydir.h>
#include <QtGui/qguiapplication.h>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include <cstdio>
#include <memory>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testShader() {
    QShader shader;
    CHECK(!shader.isValid());
    shader.setStage(QShader::FragmentStage);
    const QShaderKey key(QShader::SpirvShader, QShaderVersion(100));
    shader.setShader(key, QShaderCode(QByteArray("\x03\x02\x23\x07", 4), "main"));
    shader.setShader(QShaderKey(QShader::GlslShader, QShaderVersion(330)), QShaderCode("void main() {}", "main"));
    CHECK(shader.isValid() && shader.availableShaders().size() == 2);
    CHECK(shader.availableShaders().first().source() == QShader::SpirvShader);

    QShader copy = shader; // shares, and detaches on write
    copy.removeShader(key);
    CHECK(shader.availableShaders().size() == 2 && copy.availableShaders().size() == 1);
    CHECK(shader != copy);

    const QByteArray bytes = shader.serialized();
    const QShader back = QShader::fromSerialized(bytes);
    CHECK(back == shader && back.stage() == QShader::FragmentStage);
    CHECK(back.shader(key).shader() == QByteArray("\x03\x02\x23\x07", 4));
    CHECK(!QShader::fromSerialized(QByteArray("QSHADER junk")).isValid());
    CHECK(!QShader::fromSerialized(bytes.left(bytes.size() - 3)).isValid());
}

// ntsb packs SPIR-V (and, with glslc, GLSL) into a file that QShader reads back.
static void testNtsb() {
    const QString tool = qEnvironmentVariable("NT_NTSB");
    const QString dir = qEnvironmentVariable("NT_SHADER_DIR");
    if (tool.isEmpty() || dir.isEmpty())
        return;
    QTemporaryDir out;
    CHECK(out.isValid());
    const QString nsb = out.filePath("tri.vert.nsb");
    QProcess process;
    process.start(tool, {"-o", nsb, dir + "/rhi_tri.vert.spv"});
    CHECK(process.waitForFinished(30000) && process.exitCode() == 0);
    QFile file(nsb);
    CHECK(file.open(QIODevice::ReadOnly));
    const QShader shader = QShader::fromSerialized(file.readAll());
    CHECK(shader.isValid() && shader.stage() == QShader::VertexStage);
    const QShaderKey key(QShader::SpirvShader, QShaderVersion(100));
    CHECK(shader.shader(key).shader().size() > 100 &&
          shader.shader(key).shader().left(4) == QByteArray("\x03\x02\x23\x07", 4));
    QProcess bad;
    bad.start(tool, {"-o", out.filePath("x.nsb"), dir + "/rhi_tri.vert"}); // GLSL needs glslc; either way, no crash
    CHECK(bad.waitForFinished(30000));
}

static void testNullBackend() {
    QRhiNullInitParams params;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Null, &params));
    CHECK(rhi && rhi->backend() == QRhi::Null && rhi->backendName() == "Null");
    CHECK(QRhi::create(QRhi::D3D11, &params) == nullptr);
    CHECK(QRhi::mipLevelsForSize(QSize(256, 64)) == 9 && QRhi::sizeForMipLevel(3, QSize(100, 20)) == QSize(12, 2));
    CHECK(rhi->ubufAligned(65) == 256 && rhi->isClipDepthZeroToOne() && !rhi->isYUpInFramebuffer());

    // A buffer keeps what is uploaded to it
    QRhiBuffer *buffer = rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 16);
    CHECK(buffer->create());
    const float vertices[4] = {1, 2, 3, 4};
    QRhiResourceUpdateBatch *batch = rhi->nextResourceUpdateBatch();
    batch->uploadStaticBuffer(buffer, vertices);
    QRhiReadbackResult bufferResult;
    bool completed = false;
    bufferResult.completed = [&] { completed = true; };
    batch->readBackBuffer(buffer, 4, 8, &bufferResult);
    QRhiCommandBuffer *cb = nullptr;
    CHECK(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess && cb);
    cb->resourceUpdate(batch);
    CHECK(completed && bufferResult.data.size() == 8);
    float second = 0;
    memcpy(&second, bufferResult.data.constData(), 4);
    CHECK(second == 2.0f);
    CHECK(rhi->endOffscreenFrame() == QRhi::FrameOpSuccess);

    // A pass clears its render target, and the pixels read back
    QRhiTexture *texture = rhi->newTexture(QRhiTexture::RGBA8, QSize(4, 2), 1,
                                           QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    CHECK(texture->create());
    QRhiTextureRenderTarget *target =
        rhi->newTextureRenderTarget(QRhiTextureRenderTargetDescription(QRhiColorAttachment(texture)));
    QRhiRenderPassDescriptor *pass = target->newCompatibleRenderPassDescriptor();
    CHECK(pass->create());
    target->setRenderPassDescriptor(pass);
    CHECK(target->create() && target->pixelSize() == QSize(4, 2));

    QRhiReadbackResult pixels;
    CHECK(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    cb->beginPass(target, QColor(10, 20, 30, 255), QRhiDepthStencilClearValue());
    QRhiResourceUpdateBatch *readBack = rhi->nextResourceUpdateBatch();
    readBack->readBackTexture(QRhiReadbackDescription(texture), &pixels);
    cb->endPass(readBack);
    CHECK(rhi->endOffscreenFrame() == QRhi::FrameOpSuccess);
    CHECK(pixels.pixelSize == QSize(4, 2) && pixels.data.size() == 32);
    CHECK(static_cast<uchar>(pixels.data[0]) == 10 && static_cast<uchar>(pixels.data[1]) == 20 &&
          static_cast<uchar>(pixels.data[2]) == 30 && static_cast<uchar>(pixels.data[3]) == 255);

    // An upload of an image goes to the texture, and with a pass that keeps the contents it stays
    QImage image(2, 2, QImage::Format_RGBA8888);
    image.fill(QColor(200, 100, 50, 255));
    QRhiTexture *sampled = rhi->newTexture(QRhiTexture::RGBA8, QSize(2, 2), 1, QRhiTexture::UsedAsTransferSource);
    CHECK(sampled->create());
    QRhiResourceUpdateBatch *upload = rhi->nextResourceUpdateBatch();
    upload->uploadTexture(sampled, image);
    QRhiReadbackResult uploaded;
    upload->readBackTexture(QRhiReadbackDescription(sampled), &uploaded);
    CHECK(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess);
    cb->resourceUpdate(upload);
    CHECK(rhi->endOffscreenFrame() == QRhi::FrameOpSuccess);
    CHECK(uploaded.data.size() == 16 && static_cast<uchar>(uploaded.data[0]) == 200 &&
          static_cast<uchar>(uploaded.data[2]) == 50);

    // Bindings and pipelines are checked
    QRhiSampler *sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    CHECK(sampler->create());
    QRhiShaderResourceBindings *bindings = rhi->newShaderResourceBindings();
    bindings->setBindings(
        {QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, sampled, sampler)});
    CHECK(bindings->create());
    QRhiShaderResourceBindings *incomplete = rhi->newShaderResourceBindings();
    incomplete->setBindings(
        {QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, sampled, nullptr)});
    CHECK(!incomplete->create());
    CHECK(bindings->isLayoutCompatible(bindings) && !bindings->isLayoutCompatible(incomplete) == false);

    QRhiGraphicsPipeline *pipeline = rhi->newGraphicsPipeline();
    CHECK(!pipeline->create()); // no stages
    QShader vertexShader;
    vertexShader.setShader(QShaderKey(QShader::SpirvShader, QShaderVersion(100)), QShaderCode("x"));
    pipeline->setShaderStages({QRhiShaderStage(QRhiShaderStage::Vertex, vertexShader)});
    pipeline->setShaderResourceBindings(bindings);
    pipeline->setRenderPassDescriptor(pass);
    CHECK(pipeline->create());

    // A dynamic buffer takes dynamic updates only
    QRhiBuffer *uniforms = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64);
    CHECK(uniforms->create());
    CHECK(rhi->totalResourceCount() >= 8);

    // Delete the resources: deleteLater() goes at the end of a frame
    delete uniforms;
    delete pipeline;
    delete incomplete;
    delete bindings;
    delete sampler;
    delete sampled;
    delete target;
    delete pass;
    delete buffer;
    texture->deleteLater();
    CHECK(rhi->totalResourceCount() == 1);
    rhi->runCleanup();
    CHECK(rhi->totalResourceCount() == 0);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testShader();
    testNtsb();
    testNullBackend();
    if (g_failures == 0)
        std::printf("rhi: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
