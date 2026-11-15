// MIT License - see LICENSE
//
// Overview page about Vulkan in Nt for the NDoc documentation. This file holds only a documentation comment and is
// not part of the library.

/*!
    \page nt-vulkan.html
    \title Using Nt Vulkan
    \brief How to create a Vulkan instance, render into a window with QVulkanWindow, and trace rays with
    VK_KHR_ray_tracing_pipeline.

    Nt GUI has the Vulkan classes of Qt GUI: QVulkanInstance wraps a \c VkInstance, QVulkanFunctions and
    QVulkanDeviceFunctions call the core Vulkan 1.0 to 1.3 functions of an instance and a device, and QVulkanWindow
    is a window with a swapchain that a QVulkanWindowRenderer draws into. They are part of \c Nt::Gui:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(viewer main.cpp)
    target_link_libraries(viewer PRIVATE Nt::Gui)
    \endcode

    Nt does not link with the Vulkan library. It opens the loader (\c libvulkan.so.1) when the first QVulkanInstance
    is created, so programs that never use Vulkan run where there is none, and QVulkanInstance::create() returns
    \c false instead of failing to start. Building Nt needs the Vulkan headers (\c vulkan/vulkan.h, from
    \c libvulkan-dev or \c vulkan-headers); without them the classes are left out, and \c QT_CONFIG(vulkan) is false in
    programs built against that Nt.

    \section1 An Instance

    Set the API version, the layers and the extensions wanted, then create the instance. What is not supported is
    dropped with a warning, so a program can ask for the validation layer and run where it is not installed:

    \code
    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 2));
    inst.setLayers({"VK_LAYER_KHRONOS_validation"});
    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());
    \endcode

    The functions of the instance are reached through QVulkanInstance::functions(), those of a device through
    QVulkanInstance::deviceFunctions(); functions of extensions through QVulkanInstance::getInstanceProcAddr() and
    QVulkanFunctions::vkGetDeviceProcAddr(). The warnings and errors of the validation layer are printed with
    qWarning() unless QVulkanInstance::NoDebugOutputRedirect is set.

    \section1 A Window

    QVulkanWindow picks a physical device and a queue that presents to the window, creates the device, the swapchain,
    a depth-stencil buffer, a default render pass and the synchronization of each frame, and recreates the swapchain
    when the window is resized. The program draws through a QVulkanWindowRenderer that
    QVulkanWindow::createRenderer() returns:

    \code
    class Window : public QVulkanWindow {
    public:
        explicit Window(int frameLimit) : m_frameLimit(frameLimit) {}
        QVulkanWindowRenderer *createRenderer() override { return new RayTracer(this, m_frameLimit); }

    private:
        int m_frameLimit;
    };
    \endcode

    The window needs an instance before it is shown:

    \code
    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 2)); // buffer device addresses and SPIR-V 1.4 are core from 1.2
    if (validate)
        inst.setLayers({"VK_LAYER_KHRONOS_validation"});
    if (!inst.create()) {
        std::fprintf(stderr, "rtriangle: no Vulkan (error %d)\n", static_cast<int>(inst.errorCode()));
        return 1;
    }

    Window window(frameLimit);
    window.setVulkanInstance(&inst);
    \endcode

    Each frame, the renderer records into QVulkanWindow::currentCommandBuffer() and ends with
    QVulkanWindow::frameReady(), which submits and presents; calling QWindow::requestUpdate() too renders the next
    frame, at the refresh rate of the display, or as fast as the device can when the window's QSurfaceFormat has a
    swap interval of 0. QVulkanWindow::grab() reads a frame back into a QImage.

    \section1 Ray Tracing

    The \c rtriangle example renders a triangle with the ray tracing pipeline of Vulkan, \c VK_KHR_ray_tracing_pipeline,
    which NVIDIA RTX, AMD RDNA 2 and Intel Arc GPUs have in hardware and Mesa's \c lavapipe driver has in software. The
    renderer asks for the extensions and their features before the device is created, in
    QVulkanWindowRenderer::preInitResources():

    \code
    // Before the device is made: ask for the ray tracing extensions and their features.
    void preInitResources() override {
        const QVulkanInfoVector<QVulkanExtension> extensions = m_window->supportedDeviceExtensions();
        m_supported = extensions.contains("VK_KHR_acceleration_structure") &&
                      extensions.contains("VK_KHR_ray_tracing_pipeline") &&
                      extensions.contains("VK_KHR_deferred_host_operations");
        if (!m_supported) {
            std::puts("rtriangle: the device has no ray tracing extensions; showing the background only");
            return;
        }
        m_window->setDeviceExtensions(
            {"VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline", "VK_KHR_deferred_host_operations"});
        // The features of the extensions live in their own structures, chained to VkPhysicalDeviceFeatures2; the
        // structures must live until the device is created, so they are members.
        m_window->setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2 &features) {
            m_addressFeatures = make<VkPhysicalDeviceBufferDeviceAddressFeatures>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
            m_addressFeatures.bufferDeviceAddress = VK_TRUE;
            m_asFeatures = make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
            m_asFeatures.accelerationStructure = VK_TRUE;
            m_asFeatures.pNext = &m_addressFeatures;
            m_rtFeatures = make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
            m_rtFeatures.rayTracingPipeline = VK_TRUE;
            m_rtFeatures.pNext = &m_asFeatures;
            features.pNext = &m_rtFeatures;
        });
    }
    \endcode

    In QVulkanWindowRenderer::initResources() it resolves the functions of the extensions with
    QVulkanFunctions::vkGetDeviceProcAddr(), builds a bottom-level acceleration structure for the triangle and a
    top-level one for its instance, and creates the pipeline from the ray generation, miss and closest-hit shaders in
    \c vulkan/shaders of the \l{Nt Examples}{examples repository}, compiled to SPIR-V with \c glslc and loaded from resources. Each frame it turns the
    instance, rebuilds the top level, traces one ray per pixel into a storage image and copies the image to the
    swapchain image:

    \code
    void startNextFrame() override {
        VkCommandBuffer cmd = m_window->currentCommandBuffer();
        const QSize size = m_window->swapChainImageSize();
        const auto width = static_cast<uint32_t>(size.width());
        const auto height = static_cast<uint32_t>(size.height());
        VkImage target = m_window->swapChainImage(m_window->currentSwapChainImageIndex());

        if (!m_supported) {
            // No ray tracing: the default render pass clears the window to the background color.
            VkClearValue clearValues[2]{};
            clearValues[0].color = {{0.1f, 0.12f, 0.2f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};
            auto pass = make<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
            pass.renderPass = m_window->defaultRenderPass();
            pass.framebuffer = m_window->currentFramebuffer();
            pass.renderArea.extent = {width, height};
            pass.clearValueCount = 2;
            pass.pClearValues = clearValues;
            m_df->vkCmdBeginRenderPass(cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
            m_df->vkCmdEndRenderPass(cmd);
        } else {
            // The triangle turns: write its transform into this frame's instance buffer and rebuild the top level.
            updateInstance(m_window->currentFrame());
            buildTopLevel(cmd, m_window->currentFrame());

            // Trace one ray per pixel into the storage image.
            imageBarrier(cmd, m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            m_df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
            m_df->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 1,
                                          &m_descriptorSet, 0, nullptr);
            m_df->vkCmdPushConstants(cmd, m_pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t),
                                     &m_swapRedBlue);
            m_cmdTraceRays(cmd, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, width, height, 1);

            // Copy it to the swapchain image, which must end in the present layout.
            imageBarrier(cmd, m_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);
            imageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkImageCopy copy{};
            copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.dstSubresource = copy.srcSubresource;
            copy.extent = {width, height, 1};
            m_df->vkCmdCopyImage(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            imageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }

        m_window->frameReady();
        m_window->requestUpdate(); // render continuously, at the display's rate with vsync
        countFrame();
    }
    \endcode

    Run \c{rtriangle --devices} to list the devices and \c{rtriangle --device N} to pick one, \c{--no-vsync} to
    measure, and \c{--validate} to enable the validation layer. Where the chosen device has no ray tracing the window
    shows the background only.

    \section1 What Is Not There Yet

    Only the \c xcb platform presents; the \c offscreen platform creates no surfaces, so QVulkanWindow is not valid
    there, while QVulkanInstance serves headless rendering and compute, which is how the tests trace rays in CI with
    lavapipe. There is no QRhi, no Qt Quick and no Qt Quick 3D yet; the ray tracing classes of a 3D scene module
    will be built on these Vulkan classes. See the \e{Differences from Qt} section of each class for the rest.

    \sa QVulkanInstance, QVulkanWindow, QVulkanWindowRenderer, QVulkanFunctions, QVulkanDeviceFunctions,
        {Using Nt GUI}, {Using Nt OpenGL}
*/
