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

    \snippet vulkan/rtriangle.cpp window

    The window needs an instance before it is shown:

    \snippet vulkan/rtriangle.cpp main

    Each frame, the renderer records into QVulkanWindow::currentCommandBuffer() and ends with
    QVulkanWindow::frameReady(), which submits and presents; calling QWindow::requestUpdate() too renders the next
    frame, at the refresh rate of the display, or as fast as the device can when the window's QSurfaceFormat has a
    swap interval of 0. QVulkanWindow::grab() reads a frame back into a QImage.

    \section1 Ray Tracing

    The \c rtriangle example renders a triangle with the ray tracing pipeline of Vulkan, \c VK_KHR_ray_tracing_pipeline,
    which NVIDIA RTX, AMD RDNA 2 and Intel Arc GPUs have in hardware and Mesa's \c lavapipe driver has in software. The
    renderer asks for the extensions and their features before the device is created, in
    QVulkanWindowRenderer::preInitResources():

    \snippet vulkan/rtriangle.cpp features

    In QVulkanWindowRenderer::initResources() it resolves the functions of the extensions with
    QVulkanFunctions::vkGetDeviceProcAddr(), builds a bottom-level acceleration structure for the triangle and a
    top-level one for its instance, and creates the pipeline from the ray generation, miss and closest-hit shaders in
    \c examples/vulkan/shaders, compiled to SPIR-V with \c glslc and loaded from resources. Each frame it turns the
    instance, rebuilds the top level, traces one ray per pixel into a storage image and copies the image to the
    swapchain image:

    \snippet vulkan/rtriangle.cpp frame

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
