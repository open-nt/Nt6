// MIT License - see LICENSE
//
// Tests for Vulkan in Nt GUI: QVulkanInstance, QVulkanFunctions and QVulkanDeviceFunctions, QVulkanWindow before it
// has a surface (the offscreen platform makes none), and a triangle traced with VK_KHR_ray_tracing_pipeline into an
// image and read back, on the first device that traces rays. Everything after the first check is skipped when the
// Vulkan loader is missing, and the ray tracing part when no device supports it; Mesa's lavapipe does, in software.
#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qvulkanfunctions.h>
#include <QtGui/qvulkaninstance.h>
#include <QtGui/qvulkanwindow.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Under AddressSanitizer: the Vulkan loader and drivers keep their dispatch tables, and lavapipe its LLVM JIT, to the
// end of the process (see test_opengl.cpp). These defaults apply to this test alone; the leaks of Nt are still reported.
#if defined(__SANITIZE_ADDRESS__)
// The sanitizer runtime looks these up by name; declared first for -Wmissing-declarations.
extern "C" const char *__asan_default_options();
extern "C" const char *__lsan_default_suppressions();
extern "C" const char *__asan_default_options() { return "fast_unwind_on_malloc=0"; }
extern "C" const char *__lsan_default_suppressions() {
    return "leak:libvulkan\nleak:libvulkan_lvp\nleak:libvulkan_intel\nleak:libvulkan_radeon\nleak:libnvidia-\nleak:libVkLayer\n"
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

namespace {

template <typename T> T make(VkStructureType type) {
    T t{};
    t.sType = type;
    return t;
}

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) { return (value + alignment - 1) & ~(alignment - 1); }

bool color(QRgb p, int r, int g, int b, int tolerance = 3) {
    const bool ok = std::abs(qRed(p) - r) <= tolerance && std::abs(qGreen(p) - g) <= tolerance &&
                    std::abs(qBlue(p) - b) <= tolerance;
    if (!ok)
        std::printf("  pixel is (%d, %d, %d), expected (%d, %d, %d)\n", qRed(p), qGreen(p), qBlue(p), r, g, b);
    return ok;
}

void testInfoVector() {
    QVulkanInfoVector<QVulkanExtension> extensions;
    extensions.append(QVulkanExtension{"VK_KHR_surface", 25});
    extensions.append(QVulkanExtension{"VK_EXT_debug_utils", 2});
    CHECK(extensions.contains("VK_KHR_surface"));
    CHECK(extensions.contains("VK_KHR_surface", 25));
    CHECK(!extensions.contains("VK_KHR_surface", 26));
    CHECK(!extensions.contains("VK_KHR_xcb_surface"));
    CHECK(extensions.contains("VK_EXT_debug_utils", 0));
    CHECK(!extensions.contains("VK_EXT_debug_utils", -1));
    CHECK(extensions.size() == 2);
    CHECK((extensions.at(0) == QVulkanExtension{"VK_KHR_surface", 25}));
    CHECK(extensions.at(0) != extensions.at(1));

    QVulkanInfoVector<QVulkanLayer> layers;
    QVulkanLayer layer;
    layer.name = "VK_LAYER_KHRONOS_validation";
    layer.version = 1;
    layer.specVersion = QVersionNumber(1, 3, 2);
    layer.description = "Khronos Validation Layer";
    layers.append(layer);
    CHECK(layers.contains("VK_LAYER_KHRONOS_validation"));
    CHECK(layers.contains("VK_LAYER_KHRONOS_validation", 1));
    CHECK(!layers.contains("VK_LAYER_KHRONOS_validation", 2));
    CHECK(layers.at(0) == layer);
    layer.version = 2;
    CHECK(layers.at(0) != layer);
}

void testInstance(QVulkanInstance &inst) {
    CHECK(inst.isValid());
    CHECK(inst.vkInstance() != VK_NULL_HANDLE);
    CHECK(inst.errorCode() == VK_SUCCESS);
    CHECK(inst.functions() != nullptr);
    CHECK(inst.apiVersion() >= QVersionNumber(1, 0));
    CHECK(inst.supportedApiVersion() >= inst.apiVersion());
    CHECK(inst.getInstanceProcAddr("vkEnumeratePhysicalDevices") != nullptr);
    CHECK(inst.getInstanceProcAddr("vkNoSuchFunction") == nullptr);
    CHECK(inst.extensions().contains("VK_KHR_surface")); // added for the platform
    CHECK(!inst.supportedExtensions().isEmpty());
    CHECK(inst.supportedExtensions().contains("VK_KHR_surface"));
    for (const QVulkanLayer &layer : inst.supportedLayers())
        CHECK(!layer.name.isEmpty());

    QVulkanFunctions *f = inst.functions();
    uint32_t count = 0;
    CHECK(f->vkEnumeratePhysicalDevices(inst.vkInstance(), &count, nullptr) == VK_SUCCESS);
    CHECK(count > 0);
    std::vector<VkPhysicalDevice> devices(count);
    CHECK(f->vkEnumeratePhysicalDevices(inst.vkInstance(), &count, devices.data()) == VK_SUCCESS);
    for (VkPhysicalDevice dev : devices) {
        VkPhysicalDeviceProperties props{};
        f->vkGetPhysicalDeviceProperties(dev, &props);
        CHECK(std::strlen(props.deviceName) > 0);
        CHECK(VK_API_VERSION_MAJOR(props.apiVersion) >= 1);
        std::printf("  device: %s (Vulkan %u.%u)\n", props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
                    VK_API_VERSION_MINOR(props.apiVersion));
    }

    // Debug output filters can be installed and cleared whether or not messages arrive.
    inst.installDebugOutputFilter([](QVulkanInstance::DebugMessageSeverityFlags, QVulkanInstance::DebugMessageTypeFlags,
                                     const void *) { return false; });
    inst.clearDebugOutputFilters();

    // Layers and extensions that do not exist are dropped, not fatal.
    QVulkanInstance other;
    other.setLayers({"VK_LAYER_NT_does_not_exist"});
    other.setExtensions({"VK_NT_no_such_extension"});
    other.setApiVersion(QVersionNumber(1, 0));
    other.setFlags(QVulkanInstance::NoDebugOutputRedirect);
    CHECK(other.create());
    CHECK(other.layers().isEmpty());
    CHECK(!other.extensions().contains("VK_NT_no_such_extension"));
    CHECK(!other.extensions().contains("VK_EXT_debug_utils"));
    CHECK(other.apiVersion() == QVersionNumber(1, 0));
    CHECK(other.flags() == QVulkanInstance::NoDebugOutputRedirect);
    other.destroy();
    CHECK(!other.isValid());
    CHECK(other.vkInstance() == VK_NULL_HANDLE);
    CHECK(other.functions() == nullptr);
    CHECK(other.create()); // again
    CHECK(other.isValid());

    // Adopting an existing VkInstance.
    QVulkanInstance adopted;
    adopted.setVkInstance(other.vkInstance());
    CHECK(adopted.create());
    CHECK(adopted.vkInstance() == other.vkInstance());
    CHECK(adopted.functions() != nullptr);
    uint32_t adoptedCount = 0;
    CHECK(adopted.functions()->vkEnumeratePhysicalDevices(adopted.vkInstance(), &adoptedCount, nullptr) == VK_SUCCESS);
    CHECK(adoptedCount == count);
    adopted.destroy(); // does not destroy other's instance
    CHECK(other.isValid());
    CHECK(other.functions()->vkEnumeratePhysicalDevices(other.vkInstance(), &adoptedCount, nullptr) == VK_SUCCESS);
}

// A device on the first physical device, a buffer in host-visible memory written and read through the device functions.
void testDevice(QVulkanInstance &inst) {
    QVulkanFunctions *f = inst.functions();
    uint32_t count = 1;
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    const VkResult enumerated = f->vkEnumeratePhysicalDevices(inst.vkInstance(), &count, &physDev);
    CHECK(enumerated == VK_SUCCESS || enumerated == VK_INCOMPLETE);
    CHECK(physDev != VK_NULL_HANDLE);

    uint32_t familyCount = 0;
    f->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &familyCount, nullptr);
    CHECK(familyCount > 0);
    const float priority = 1.0f;
    auto queueInfo = make<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    queueInfo.queueFamilyIndex = 0;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    auto devInfo = make<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    VkDevice dev = VK_NULL_HANDLE;
    CHECK(f->vkCreateDevice(physDev, &devInfo, nullptr, &dev) == VK_SUCCESS);
    if (!dev)
        return;
    QVulkanDeviceFunctions *df = inst.deviceFunctions(dev);
    CHECK(df != nullptr);
    CHECK(inst.deviceFunctions(dev) == df);

    VkPhysicalDeviceMemoryProperties memProps{};
    f->vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    auto bufInfo = make<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    bufInfo.size = 256;
    bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    CHECK(df->vkCreateBuffer(dev, &bufInfo, nullptr, &buffer) == VK_SUCCESS);
    VkMemoryRequirements req{};
    df->vkGetBufferMemoryRequirements(dev, buffer, &req);
    CHECK(req.size >= 256);
    uint32_t typeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((req.memoryTypeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & wanted) == wanted) {
            typeIndex = i;
            break;
        }
    }
    CHECK(typeIndex != UINT32_MAX);
    auto allocInfo = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    allocInfo.allocationSize = req.size;
    allocInfo.memoryTypeIndex = typeIndex;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    CHECK(df->vkAllocateMemory(dev, &allocInfo, nullptr, &memory) == VK_SUCCESS);
    CHECK(df->vkBindBufferMemory(dev, buffer, memory, 0) == VK_SUCCESS);
    void *mapped = nullptr;
    CHECK(df->vkMapMemory(dev, memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS);
    if (mapped) {
        auto *words = static_cast<uint32_t *>(mapped);
        for (uint32_t i = 0; i < 64; ++i)
            words[i] = i * 7u;
        df->vkUnmapMemory(dev, memory);
        mapped = nullptr;
        CHECK(df->vkMapMemory(dev, memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS);
        bool same = mapped != nullptr;
        for (uint32_t i = 0; same && i < 64; ++i)
            same = static_cast<uint32_t *>(mapped)[i] == i * 7u;
        CHECK(same);
        if (mapped)
            df->vkUnmapMemory(dev, memory);
    }
    df->vkDestroyBuffer(dev, buffer, nullptr);
    df->vkFreeMemory(dev, memory, nullptr);
    CHECK(df->vkDeviceWaitIdle(dev) == VK_SUCCESS);
    df->vkDestroyDevice(dev, nullptr);
    inst.resetDeviceFunctions(dev);
}

// QVulkanWindow can tell about the devices before it is shown; on the offscreen platform there is no surface, so
// showing it does not make it valid, and does not crash.
void testWindowWithoutSurface(QVulkanInstance &inst) {
    QVulkanWindow window;
    CHECK(window.surfaceType() == QSurface::VulkanSurface);
    CHECK(window.vulkanInstance() == nullptr);
    CHECK(window.availablePhysicalDevices().isEmpty()); // no instance yet: a warning, no crash
    window.setVulkanInstance(&inst);
    CHECK(window.vulkanInstance() == &inst);
    const QList<VkPhysicalDeviceProperties> devices = window.availablePhysicalDevices();
    CHECK(!devices.isEmpty());
    CHECK(window.supportedDeviceExtensions().contains("VK_KHR_swapchain") ||
          !window.supportedDeviceExtensions().isEmpty());
    CHECK(window.supportedSampleCounts().contains(1));
    CHECK(window.concurrentFrameCount() >= 1 &&
          window.concurrentFrameCount() <= QVulkanWindow::MAX_CONCURRENT_FRAME_COUNT);
    CHECK(!window.isValid());
    CHECK(window.device() == VK_NULL_HANDLE);
    CHECK(window.currentCommandBuffer() == VK_NULL_HANDLE);
    CHECK(window.swapChainImage(0) == VK_NULL_HANDLE);
    CHECK(window.grab().isNull());
    const QMatrix4x4 clip = window.clipCorrectionMatrix();
    CHECK(qFuzzyCompare(clip(1, 1), -1.0f) && qFuzzyCompare(clip(2, 2), 0.5f) && qFuzzyCompare(clip(2, 3), 0.5f));
    window.setSampleCount(4);
    CHECK(window.sampleCountFlagBits() == VK_SAMPLE_COUNT_4_BIT);
    window.setSampleCount(3);
    CHECK(window.sampleCountFlagBits() == VK_SAMPLE_COUNT_2_BIT);
    window.setFlags(QVulkanWindow::PersistentResources);
    CHECK(window.flags() == QVulkanWindow::PersistentResources);
    window.resize(64, 64);
    window.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    CHECK(!window.isValid()); // the offscreen platform makes no Vulkan surfaces
    window.hide();
}

// Ray tracing, headless: one triangle, traced into a 64x64 image that is read back.
class RayTracingTest {
public:
    explicit RayTracingTest(QVulkanInstance &inst) : m_inst(inst), m_f(inst.functions()) {}

    bool findDevice() {
        uint32_t count = 0;
        m_f->vkEnumeratePhysicalDevices(m_inst.vkInstance(), &count, nullptr);
        std::vector<VkPhysicalDevice> devices(count);
        m_f->vkEnumeratePhysicalDevices(m_inst.vkInstance(), &count, devices.data());
        for (VkPhysicalDevice dev : devices) {
            uint32_t extCount = 0;
            m_f->vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> exts(extCount);
            m_f->vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());
            int found = 0;
            for (const VkExtensionProperties &e : exts) {
                if (!std::strcmp(e.extensionName, "VK_KHR_acceleration_structure") ||
                    !std::strcmp(e.extensionName, "VK_KHR_ray_tracing_pipeline") ||
                    !std::strcmp(e.extensionName, "VK_KHR_deferred_host_operations"))
                    ++found;
            }
            VkPhysicalDeviceProperties props{};
            m_f->vkGetPhysicalDeviceProperties(dev, &props);
            if (found != 3 || VK_API_VERSION_MINOR(props.apiVersion) < 2)
                continue;
            auto rt = make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
            auto as = make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
            auto address = make<VkPhysicalDeviceBufferDeviceAddressFeatures>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
            as.pNext = &address;
            rt.pNext = &as;
            auto features = make<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            features.pNext = &rt;
            m_f->vkGetPhysicalDeviceFeatures2(dev, &features);
            if (rt.rayTracingPipeline && as.accelerationStructure && address.bufferDeviceAddress) {
                m_physDev = dev;
                std::printf("  ray tracing on: %s\n", props.deviceName);
                return true;
            }
        }
        return false;
    }

    void run() {
        createDevice();
        QVulkanDeviceFunctions *df = m_df;
        resolve("vkCreateAccelerationStructureKHR", &m_createAS);
        resolve("vkDestroyAccelerationStructureKHR", &m_destroyAS);
        resolve("vkGetAccelerationStructureBuildSizesKHR", &m_getBuildSizes);
        resolve("vkGetAccelerationStructureDeviceAddressKHR", &m_getASAddress);
        resolve("vkCmdBuildAccelerationStructuresKHR", &m_cmdBuildAS);
        resolve("vkCreateRayTracingPipelinesKHR", &m_createPipelines);
        resolve("vkGetRayTracingShaderGroupHandlesKHR", &m_getGroupHandles);
        resolve("vkCmdTraceRaysKHR", &m_cmdTraceRays);

        m_rtProps = make<VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR);
        m_asProps = make<VkPhysicalDeviceAccelerationStructurePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR);
        m_rtProps.pNext = &m_asProps;
        auto props = make<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
        props.pNext = &m_rtProps;
        m_f->vkGetPhysicalDeviceProperties2(m_physDev, &props);
        CHECK(m_rtProps.shaderGroupHandleSize > 0);

        // The triangle, the bottom level, the instance and the top level.
        const float vertices[] = {-1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        Buffer vertexBuffer = createBuffer(sizeof(vertices),
                                           VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           true);
        upload(vertexBuffer, vertices, sizeof(vertices));
        auto triangles =
            make<VkAccelerationStructureGeometryKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
        triangles.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        triangles.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        triangles.geometry.triangles = make<VkAccelerationStructureGeometryTrianglesDataKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR);
        triangles.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        triangles.geometry.triangles.vertexData.deviceAddress = vertexBuffer.address;
        triangles.geometry.triangles.vertexStride = 3 * sizeof(float);
        triangles.geometry.triangles.maxVertex = 2;
        triangles.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
        AccelerationStructure blas = build(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, triangles);

        VkAccelerationStructureInstanceKHR instance{};
        instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1.0f;
        instance.mask = 0xff;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = blas.address;
        Buffer instanceBuffer = createBuffer(sizeof(instance),
                                             VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                             true);
        upload(instanceBuffer, &instance, sizeof(instance));
        auto instances =
            make<VkAccelerationStructureGeometryKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
        instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        instances.geometry.instances = make<VkAccelerationStructureGeometryInstancesDataKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR);
        instances.geometry.instances.data.deviceAddress = instanceBuffer.address;
        AccelerationStructure tlas = build(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, instances);

        // The image the rays write, and the pipeline.
        const uint32_t width = 64, height = 64;
        auto imageInfo = make<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImage image = VK_NULL_HANDLE;
        CHECK(df->vkCreateImage(m_dev, &imageInfo, nullptr, &image) == VK_SUCCESS);
        VkMemoryRequirements req{};
        df->vkGetImageMemoryRequirements(m_dev, image, &req);
        auto allocInfo = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        allocInfo.allocationSize = req.size;
        allocInfo.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        CHECK(df->vkAllocateMemory(m_dev, &allocInfo, nullptr, &imageMemory) == VK_SUCCESS);
        df->vkBindImageMemory(m_dev, image, imageMemory, 0);
        auto viewInfo = make<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view = VK_NULL_HANDLE;
        CHECK(df->vkCreateImageView(m_dev, &viewInfo, nullptr, &view) == VK_SUCCESS);

        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr};
        auto layoutInfo = make<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        CHECK(df->vkCreateDescriptorSetLayout(m_dev, &layoutInfo, nullptr, &setLayout) == VK_SUCCESS);
        VkPushConstantRange pushConstants{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t)};
        auto plInfo = make<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        plInfo.setLayoutCount = 1;
        plInfo.pSetLayouts = &setLayout;
        plInfo.pushConstantRangeCount = 1;
        plInfo.pPushConstantRanges = &pushConstants;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        CHECK(df->vkCreatePipelineLayout(m_dev, &plInfo, nullptr, &pipelineLayout) == VK_SUCCESS);

        const VkShaderStageFlagBits stageBits[3] = {VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
                                                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
        const char *const files[3] = {":/shaders/rtriangle.rgen.spv", ":/shaders/rtriangle.rmiss.spv",
                                      ":/shaders/rtriangle.rchit.spv"};
        VkPipelineShaderStageCreateInfo stages[3];
        VkRayTracingShaderGroupCreateInfoKHR groups[3];
        for (int i = 0; i < 3; ++i) {
            stages[i] = make<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            stages[i].stage = stageBits[i];
            stages[i].module = loadShader(files[i]);
            stages[i].pName = "main";
            groups[i] =
                make<VkRayTracingShaderGroupCreateInfoKHR>(VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
            groups[i].type = i < 2 ? VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR
                                   : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            groups[i].generalShader = i < 2 ? static_cast<uint32_t>(i) : VK_SHADER_UNUSED_KHR;
            groups[i].closestHitShader = i == 2 ? 2 : VK_SHADER_UNUSED_KHR;
            groups[i].anyHitShader = VK_SHADER_UNUSED_KHR;
            groups[i].intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        auto pipeInfo = make<VkRayTracingPipelineCreateInfoKHR>(VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);
        pipeInfo.stageCount = 3;
        pipeInfo.pStages = stages;
        pipeInfo.groupCount = 3;
        pipeInfo.pGroups = groups;
        pipeInfo.maxPipelineRayRecursionDepth = 1;
        pipeInfo.layout = pipelineLayout;
        VkPipeline pipeline = VK_NULL_HANDLE;
        CHECK(m_createPipelines(m_dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline) == VK_SUCCESS);
        for (const VkPipelineShaderStageCreateInfo &stage : stages)
            df->vkDestroyShaderModule(m_dev, stage.module, nullptr);

        // The shader binding table.
        const uint32_t handleSize = m_rtProps.shaderGroupHandleSize;
        const VkDeviceSize handleStride = alignUp(handleSize, m_rtProps.shaderGroupHandleAlignment);
        const VkDeviceSize regionSize = alignUp(handleStride, m_rtProps.shaderGroupBaseAlignment);
        std::vector<uint8_t> handles(static_cast<std::size_t>(3 * handleSize));
        CHECK(m_getGroupHandles(m_dev, pipeline, 0, 3, handles.size(), handles.data()) == VK_SUCCESS);
        Buffer sbt = createBuffer(
            3 * regionSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            true);
        for (std::size_t group = 0; group < 3; ++group)
            uploadAt(sbt, group * regionSize, handles.data() + group * handleSize, handleSize);
        const VkStridedDeviceAddressRegionKHR rgen{sbt.address, regionSize, regionSize};
        const VkStridedDeviceAddressRegionKHR miss{sbt.address + regionSize, handleStride, regionSize};
        const VkStridedDeviceAddressRegionKHR hit{sbt.address + 2 * regionSize, handleStride, regionSize};
        const VkStridedDeviceAddressRegionKHR callable{};

        // The descriptors.
        VkDescriptorPoolSize poolSizes[2] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
                                             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
        auto poolInfo = make<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = poolSizes;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        CHECK(df->vkCreateDescriptorPool(m_dev, &poolInfo, nullptr, &pool) == VK_SUCCESS);
        auto setInfo = make<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        setInfo.descriptorPool = pool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        CHECK(df->vkAllocateDescriptorSets(m_dev, &setInfo, &set) == VK_SUCCESS);
        auto asWrite = make<VkWriteDescriptorSetAccelerationStructureKHR>(
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
        asWrite.accelerationStructureCount = 1;
        asWrite.pAccelerationStructures = &tlas.handle;
        VkDescriptorImageInfo imageDescriptor{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_GENERAL};
        VkWriteDescriptorSet writes[2] = {make<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET),
                                          make<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET)};
        writes[0].pNext = &asWrite;
        writes[0].dstSet = set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[1].dstSet = set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo = &imageDescriptor;
        df->vkUpdateDescriptorSets(m_dev, 2, writes, 0, nullptr);

        // Trace, copy the image into a host-visible buffer and read it.
        Buffer readback = createBuffer(width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, false);
        const uint32_t swapRedBlue = 0;
        submitAndWait([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_SHADER_WRITE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
            df->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &set, 0,
                                        nullptr);
            df->vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t),
                                   &swapRedBlue);
            m_cmdTraceRays(cmd, &rgen, &miss, &hit, &callable, width, height, 1);
            imageBarrier(cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {width, height, 1};
            df->vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1, &copy);
        });
        void *mapped = nullptr;
        CHECK(df->vkMapMemory(m_dev, readback.memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS);
        if (mapped) {
            const auto *pixels = static_cast<const uint8_t *>(mapped);
            auto at = [&](uint32_t x, uint32_t y) {
                const uint8_t *p = pixels + (y * width + x) * 4;
                return qRgb(p[0], p[1], p[2]);
            };
            // The corners miss the triangle and show the background; the middle hits it, with barycentric
            // coordinates as the color, so the channels sum to one.
            CHECK(color(at(0, 0), 26, 31, 51));
            CHECK(color(at(width - 1, 0), 26, 31, 51));
            CHECK(color(at(0, height - 1), 26, 31, 51));
            CHECK(color(at(width - 1, height - 1), 26, 31, 51));
            const QRgb middle = at(width / 2, height / 2);
            CHECK(middle != qRgb(26, 31, 51));
            CHECK(std::abs(qRed(middle) + qGreen(middle) + qBlue(middle) - 255) <= 3);
            // The triangle spans -1..1 at a distance of 2.5 from a camera with a 90 degree field of view, so it
            // fills the middle 40% of the image. Its bottom corners are red and green and its top is blue: near the
            // bottom left corner red dominates, and near the top blue does.
            const QRgb lowerLeft = at(25 * width / 64, 42 * height / 64);
            CHECK(qRed(lowerLeft) > qGreen(lowerLeft) && qRed(lowerLeft) > qBlue(lowerLeft));
            const QRgb top = at(width / 2, 21 * height / 64);
            CHECK(qBlue(top) > qRed(top) && qBlue(top) > qGreen(top));
            df->vkUnmapMemory(m_dev, readback.memory);
        }

        destroyBuffer(readback);
        df->vkDestroyDescriptorPool(m_dev, pool, nullptr);
        destroyBuffer(sbt);
        df->vkDestroyPipeline(m_dev, pipeline, nullptr);
        df->vkDestroyPipelineLayout(m_dev, pipelineLayout, nullptr);
        df->vkDestroyDescriptorSetLayout(m_dev, setLayout, nullptr);
        df->vkDestroyImageView(m_dev, view, nullptr);
        df->vkDestroyImage(m_dev, image, nullptr);
        df->vkFreeMemory(m_dev, imageMemory, nullptr);
        destroyAccelerationStructure(tlas);
        destroyBuffer(instanceBuffer);
        destroyAccelerationStructure(blas);
        destroyBuffer(vertexBuffer);
        df->vkDestroyCommandPool(m_dev, m_cmdPool, nullptr);
        df->vkDestroyDevice(m_dev, nullptr);
        m_inst.resetDeviceFunctions(m_dev);
    }

private:
    struct Buffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceAddress address = 0;
    };
    struct AccelerationStructure {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        Buffer buffer;
        VkDeviceAddress address = 0;
    };

    template <typename F> void resolve(const char *name, F *out) {
        *out = reinterpret_cast<F>(m_f->vkGetDeviceProcAddr(m_dev, name));
        CHECK(*out != nullptr);
    }

    void createDevice() {
        uint32_t familyCount = 0;
        m_f->vkGetPhysicalDeviceQueueFamilyProperties(m_physDev, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        m_f->vkGetPhysicalDeviceQueueFamilyProperties(m_physDev, &familyCount, families.data());
        m_queueFamily = 0;
        for (uint32_t i = 0; i < familyCount; ++i) {
            if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_queueFamily = i;
                break;
            }
        }
        m_f->vkGetPhysicalDeviceMemoryProperties(m_physDev, &m_memProps);
        auto address = make<VkPhysicalDeviceBufferDeviceAddressFeatures>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
        address.bufferDeviceAddress = VK_TRUE;
        auto as = make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
        as.accelerationStructure = VK_TRUE;
        as.pNext = &address;
        auto rt = make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
        rt.rayTracingPipeline = VK_TRUE;
        rt.pNext = &as;
        auto features = make<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        features.pNext = &rt;
        const char *extensions[] = {"VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline",
                                    "VK_KHR_deferred_host_operations"};
        const float priority = 1.0f;
        auto queueInfo = make<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        queueInfo.queueFamilyIndex = m_queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        auto devInfo = make<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        devInfo.pNext = &features;
        devInfo.queueCreateInfoCount = 1;
        devInfo.pQueueCreateInfos = &queueInfo;
        devInfo.enabledExtensionCount = 3;
        devInfo.ppEnabledExtensionNames = extensions;
        CHECK(m_f->vkCreateDevice(m_physDev, &devInfo, nullptr, &m_dev) == VK_SUCCESS);
        m_df = m_inst.deviceFunctions(m_dev);
        CHECK(m_df != nullptr);
        m_df->vkGetDeviceQueue(m_dev, m_queueFamily, 0, &m_queue);
        auto poolInfo = make<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        poolInfo.queueFamilyIndex = m_queueFamily;
        CHECK(m_df->vkCreateCommandPool(m_dev, &poolInfo, nullptr, &m_cmdPool) == VK_SUCCESS);
    }

    uint32_t memoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted) const {
        for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
            if ((typeBits & (1u << i)) && (m_memProps.memoryTypes[i].propertyFlags & wanted) == wanted)
                return i;
        }
        CHECK(!"no memory type");
        return 0;
    }

    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool hostVisible) {
        Buffer b;
        auto info = make<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        info.size = size;
        info.usage = usage;
        CHECK(m_df->vkCreateBuffer(m_dev, &info, nullptr, &b.buffer) == VK_SUCCESS);
        VkMemoryRequirements req{};
        m_df->vkGetBufferMemoryRequirements(m_dev, b.buffer, &req);
        auto flags = make<VkMemoryAllocateFlagsInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
        flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        auto alloc = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memoryType(req.memoryTypeBits, hostVisible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                                                           : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            alloc.pNext = &flags;
        CHECK(m_df->vkAllocateMemory(m_dev, &alloc, nullptr, &b.memory) == VK_SUCCESS);
        m_df->vkBindBufferMemory(m_dev, b.buffer, b.memory, 0);
        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            auto addressInfo = make<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
            addressInfo.buffer = b.buffer;
            b.address = m_df->vkGetBufferDeviceAddress(m_dev, &addressInfo);
            CHECK(b.address != 0);
        }
        return b;
    }

    void destroyBuffer(Buffer &b) {
        m_df->vkDestroyBuffer(m_dev, b.buffer, nullptr);
        m_df->vkFreeMemory(m_dev, b.memory, nullptr);
        b = Buffer();
    }

    void uploadAt(const Buffer &b, VkDeviceSize offset, const void *data, std::size_t size) {
        void *mapped = nullptr;
        CHECK(m_df->vkMapMemory(m_dev, b.memory, 0, VK_WHOLE_SIZE, 0, &mapped) == VK_SUCCESS);
        if (mapped) {
            std::memcpy(static_cast<uint8_t *>(mapped) + offset, data, size);
            m_df->vkUnmapMemory(m_dev, b.memory);
        }
    }

    void upload(const Buffer &b, const void *data, std::size_t size) { uploadAt(b, 0, data, size); }

    AccelerationStructure build(VkAccelerationStructureTypeKHR type,
                                const VkAccelerationStructureGeometryKHR &geometry) {
        auto info = make<VkAccelerationStructureBuildGeometryInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
        info.type = type;
        info.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        info.geometryCount = 1;
        info.pGeometries = &geometry;
        const uint32_t primitiveCount = 1;
        auto sizes = make<VkAccelerationStructureBuildSizesInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);
        m_getBuildSizes(m_dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &info, &primitiveCount, &sizes);
        CHECK(sizes.accelerationStructureSize > 0);

        AccelerationStructure as;
        as.buffer = createBuffer(
            sizes.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
        auto createInfo =
            make<VkAccelerationStructureCreateInfoKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
        createInfo.buffer = as.buffer.buffer;
        createInfo.size = sizes.accelerationStructureSize;
        createInfo.type = type;
        CHECK(m_createAS(m_dev, &createInfo, nullptr, &as.handle) == VK_SUCCESS);
        auto addressInfo = make<VkAccelerationStructureDeviceAddressInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
        addressInfo.accelerationStructure = as.handle;
        as.address = m_getASAddress(m_dev, &addressInfo);

        const VkDeviceSize scratchAlignment = m_asProps.minAccelerationStructureScratchOffsetAlignment;
        Buffer scratch =
            createBuffer(sizes.buildScratchSize + scratchAlignment,
                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, false);
        info.dstAccelerationStructure = as.handle;
        info.scratchData.deviceAddress = alignUp(scratch.address, scratchAlignment);
        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = primitiveCount;
        const VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;
        submitAndWait([&](VkCommandBuffer cmd) { m_cmdBuildAS(cmd, 1, &info, &ranges); });
        destroyBuffer(scratch);
        return as;
    }

    void destroyAccelerationStructure(AccelerationStructure &as) {
        m_destroyAS(m_dev, as.handle, nullptr);
        destroyBuffer(as.buffer);
    }

    VkShaderModule loadShader(const char *path) {
        QFile file(QString::fromLatin1(path));
        CHECK(file.open(QIODevice::ReadOnly));
        const QByteArray code = file.readAll();
        CHECK(code.size() > 0 && code.size() % 4 == 0);
        auto info = make<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        info.codeSize = static_cast<std::size_t>(code.size());
        info.pCode = reinterpret_cast<const uint32_t *>(code.constData());
        VkShaderModule module = VK_NULL_HANDLE;
        CHECK(m_df->vkCreateShaderModule(m_dev, &info, nullptr, &module) == VK_SUCCESS);
        return module;
    }

    void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess,
                      VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        auto barrier = make<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
        barrier.srcAccessMask = srcAccess;
        barrier.dstAccessMask = dstAccess;
        barrier.oldLayout = from;
        barrier.newLayout = to;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        m_df->vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    template <typename Record> void submitAndWait(Record record) {
        auto allocInfo = make<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        allocInfo.commandPool = m_cmdPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        CHECK(m_df->vkAllocateCommandBuffers(m_dev, &allocInfo, &cmd) == VK_SUCCESS);
        auto begin = make<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        CHECK(m_df->vkBeginCommandBuffer(cmd, &begin) == VK_SUCCESS);
        record(cmd);
        CHECK(m_df->vkEndCommandBuffer(cmd) == VK_SUCCESS);
        auto submit = make<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        CHECK(m_df->vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        CHECK(m_df->vkQueueWaitIdle(m_queue) == VK_SUCCESS);
        m_df->vkFreeCommandBuffers(m_dev, m_cmdPool, 1, &cmd);
    }

    QVulkanInstance &m_inst;
    QVulkanFunctions *m_f;
    VkPhysicalDevice m_physDev = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_df = nullptr;
    uint32_t m_queueFamily = 0;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_cmdPool = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties m_memProps{};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProps{};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR m_asProps{};
    PFN_vkCreateAccelerationStructureKHR m_createAS = nullptr;
    PFN_vkDestroyAccelerationStructureKHR m_destroyAS = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR m_getBuildSizes = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR m_getASAddress = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR m_cmdBuildAS = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR m_createPipelines = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR m_getGroupHandles = nullptr;
    PFN_vkCmdTraceRaysKHR m_cmdTraceRays = nullptr;
};

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testInfoVector();
#if defined(__SANITIZE_ADDRESS__)
    // The Vulkan loader unloads the drivers and layers with the last instance, and leaks in them would then be
    // reported from an unknown module, which no suppression matches; keep them loaded.
    ::setenv("VK_LOADER_DISABLE_DYNAMIC_LIBRARY_UNLOADING", "1", 1);
#endif

    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 2));
    if (!inst.create()) {
        CHECK(inst.errorCode() != VK_SUCCESS);
        CHECK(!inst.isValid());
        CHECK(inst.supportedApiVersion().isNull() || inst.errorCode() != VK_ERROR_INITIALIZATION_FAILED);
        std::puts("SKIP: no Vulkan");
        std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
        return g_failures ? 1 : 0;
    }
    testInstance(inst);
    testDevice(inst);
    testWindowWithoutSurface(inst);

    RayTracingTest rayTracing(inst);
    if (rayTracing.findDevice())
        rayTracing.run();
    else
        std::puts("SKIP: no device with ray tracing");

    inst.destroy();
    CHECK(!inst.isValid());
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
