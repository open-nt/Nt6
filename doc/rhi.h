// MIT License - see LICENSE
//
// Overview page about QRhi in Nt for the NDoc documentation. This file holds only a documentation comment and is not
// part of the library.

/*!
    \page nt-rhi.html
    \title Using Nt QRhi
    \brief How to draw and compute on the GPU with QRhi, over Vulkan, and how to make shaders for it.

    QRhi is the rendering hardware interface of Nt GUI: buffers, textures, pipelines and render passes that look the same
    on every backend. The application makes resources, records a frame, and reads the result back. The Null backend, which
    keeps the bytes of its buffers and textures and clears render targets, runs the same program without a GPU; the Vulkan
    backend runs it on a Vulkan device, Mesa's lavapipe included.

    \section1 Shaders

    QRhi reads QShader objects. \c ntsb compiles GLSL with \c glslc (from shaderc, in the Vulkan SDK or the \c glslc package)
    and packs the SPIR-V into a \c .nsb file:

    \code
    ntsb -o color.vert.nsb color.vert
    ntsb -o color.frag.nsb color.frag
    \endcode

    \code
    QShader vertex = QShader::fromSerialized(readFile(":/shaders/color.vert.nsb"));
    \endcode

    \section1 A Frame

    Make a QRhi, a texture to draw into, and a render target with its render pass descriptor. A pipeline is made for the
    render pass descriptor and a set of shader resource bindings; the vertex data goes in a buffer:

    \code
    QVulkanInstance instance;
    instance.create();
    QRhiVulkanInitParams params;
    params.inst = &instance;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Vulkan, &params));

    QRhiTexture *color = rhi->newTexture(QRhiTexture::RGBA8, QSize(256, 256), 1,
                                         QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    color->create();
    QRhiTextureRenderTarget *target = rhi->newTextureRenderTarget(QRhiTextureRenderTargetDescription(QRhiColorAttachment(color)));
    QRhiRenderPassDescriptor *pass = target->newCompatibleRenderPassDescriptor();
    pass->create();
    target->setRenderPassDescriptor(pass);
    target->create();

    QRhiCommandBuffer *cb = nullptr;
    rhi->beginOffscreenFrame(&cb);
    cb->beginPass(target, Qt::black, {1.0f, 0}, updates);
    cb->setGraphicsPipeline(pipeline);
    cb->setViewport(QRhiViewport(0, 0, 256, 256));
    cb->setShaderResources();
    const QRhiCommandBuffer::VertexInput input(vertexBuffer, 0);
    cb->setVertexInput(0, 1, &input);
    cb->draw(3);
    cb->endPass(readBack);
    rhi->endOffscreenFrame();
    \endcode

    Uploads, copies and read backs go in a QRhiResourceUpdateBatch that is given to \c beginPass() or \c endPass(). A
    projection matrix made for OpenGL is multiplied with QRhi::clipSpaceCorrMatrix() to work on every backend.

    \section1 What Nt Has

    The frames are offscreen and synchronous, and only the Null and Vulkan backends exist. Swap chains for windows, the
    OpenGL backend and ray tracing follow. Each class lists its differences from Qt; see QRhi.
*/
