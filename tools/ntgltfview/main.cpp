// MIT License - see LICENSE
//
// ntgltfview: shows a glTF 2.0 scene with hardware ray tracing (VK_KHR_ray_tracing_pipeline) in a QVulkanWindow, to
// check what Nt glTF loaded. Every triangle primitive of a mesh becomes a geometry of the mesh's bottom-level
// acceleration structure, every mesh instance of the default scene an instance of the top-level one. The camera
// orbits the scene's bounds, looks along it from inside, or is the file's own (--view), and the mouse and keyboard
// move it. The hit shaders read the triangle's vertices through buffer addresses and shade the metallic-roughness
// material with its five textures under a head light and a key light; masked materials are cut out in an any-hit
// shader. It needs a device with VK_KHR_acceleration_structure and VK_KHR_ray_tracing_pipeline; without one the
// window shows the background only, and says so.
//
//   ntgltfview Sponza.gltf --view inside --device 1
//   ntgltfview DamagedHelmet.glb --grab helmet.png --frames 120
//   ntgltfview GlassHurricaneCandleHolder.gltf --environment farm_road_2k.hdr --dlss quality
//
// With --raytrace on (the default) the key light casts shadows, smooth surfaces mirror what is around them, and glass
// (KHR_materials_transmission, _volume and _ior, and blended alpha) lets the rays through: they bend into a volume that
// has a thickness, and take its color. --environment gives the rays a panorama to find, a Radiance .hdr or an image,
// which is also the background. --dlss upscales the traced image with NVIDIA DLSS Super Resolution.
//
// Controls: drag to orbit or look around, wheel to zoom or change the speed; W A S D Q E (or the arrows) fly, with
// Shift faster; F switches between orbiting and flying; Space stops and starts the automatic motion; R resets;
// F11 or Alt+Return toggles full screen; T turns the ray traced shadows and reflections on and off; N steps through
// DLSS (off, quality, balanced, performance, DLAA); Escape quits. On a gamepad: the left stick flies, the right
// stick looks (or orbits), the triggers rise and sink (or zoom), A is faster, the shoulder buttons change the speed,
// Y switches modes, X resets, Start stops and starts the automatic motion, the left stick's click is T, the right
// stick's click is N and Select quits.
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QGamepad>
#include <QGamepadManager>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QString>
#include <QTimer>
#include <QVector3D>
#include <QVulkanDeviceFunctions>
#include <QVulkanFunctions>
#include <QVulkanInstance>
#include <QVulkanWindow>
#include <QWheelEvent>
#include <QtGltf>

#include "dlss.h"
#include "environment.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <set>
#include <vector>

namespace {

// A Vulkan structure with its sType set and the rest zero.
template <typename T> T make(VkStructureType type) {
    T t{};
    t.sType = type;
    return t;
}

VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) { return (value + alignment - 1) & ~(alignment - 1); }

[[noreturn]] void fail(const QString &message) {
    std::fprintf(stderr, "ntgltfview: %s\n", message.toUtf8().constData());
    std::exit(1);
}

void check(VkResult result, const char *what) {
    if (result != VK_SUCCESS)
        fail(QString("%1 failed: %2").arg(what).arg(static_cast<int>(result)));
}

template <typename F> void resolve(QVulkanFunctions *f, VkDevice dev, const char *name, F *out) {
    *out = reinterpret_cast<F>(f->vkGetDeviceProcAddr(dev, name));
    if (!*out)
        fail(QString("%1 is missing").arg(name));
}

// A buffer with its memory, and its device address when it was made with VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT.
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

// What the hit shaders need of one primitive; the layout matches the std430 struct in ntgltfview_common.glsl.
struct PrimitiveRecord {
    uint64_t positions = 0;
    uint64_t normals = 0;
    uint64_t indices = 0;
    uint64_t texCoords0 = 0;
    uint64_t texCoords1 = 0;
    uint64_t tangents = 0;
    float baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float emissive[4] = {0.0f, 0.0f, 0.0f, 1.0f};        // rgb times the strength; w: metallic
    float params[4] = {1.0f, 0.0f, 0.0f, 0.5f};          // roughness, double sided, alpha mode, alpha cutoff
    float textures[4] = {-1.0f, -1.0f, -1.0f, -1.0f};    // base, metallic-roughness, normal, occlusion
    float textures2[4] = {-1.0f, 0.0f, 1.0f, 1.0f};      // emissive, TEXCOORD_1 bits, normal scale, occlusion strength
    float transmission[4] = {0.0f, 1.5f, 0.0f, 1.0e30f}; // factor, index of refraction, thickness, attenuation distance
    float attenuation[4] = {1.0f, 1.0f, 1.0f, 0.0f};     // the color after that distance
    float textures3[4] = {-1.0f, -1.0f, 0.0f, 0.0f};     // transmission, thickness
    // KHR_texture_transform per texture slot, as two rows of a 2x3 matrix: (a, b, tx) and (c, d, ty).
    float uvTransform[14][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0},
                                {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0},
                                {1, 0, 0, 0}, {0, 1, 0, 0}, {1, 0, 0, 0}, {0, 1, 0, 0}};
};
static_assert(sizeof(PrimitiveRecord) == 400, "the record must match the shader's layout");

// The camera, as the ray generation and closest hit shaders' push constants: this frame's and the previous frame's,
// which gives the motion vectors. The layout matches ntgltfview_camera.glsl.
struct CameraConstants {
    float origin[3];
    float tanHalfFovY;
    float forward[3];
    float aspect;
    float right[3];
    uint32_t flags;
    float up[3];
    float depthNear;
    float prevOrigin[3];
    float prevTanHalfFovY;
    float prevForward[3];
    uint32_t swapRedBlue;
    float prevRight[3];
    float jitterX;
    float prevUp[3];
    float jitterY;
};
static_assert(sizeof(CameraConstants) == 128, "the push constants must match the shader's layout");

constexpr uint32_t kFlagTraceEffects = 2; // shadows and reflections
constexpr uint32_t kFlagEnvironment = 4;  // an environment image is bound
constexpr VkShaderStageFlags kCameraStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                             VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
constexpr uint32_t kGroupCount = 4; // shader groups: ray generation, two miss shaders, the hit group

// The pose of the camera in one frame.
struct CameraPose {
    QVector3D origin;
    QVector3D forward;
    QVector3D right;
    QVector3D up;
    float tanHalfFovY = 0.0f;
};

// An image with its memory and view.
struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    QSize size;
};

// Element `index` of the Halton sequence in `base`, in [0, 1): the sub-pixel jitter pattern DLSS is trained on.
float halton(uint32_t index, uint32_t base) {
    float f = 1.0f;
    float result = 0.0f;
    while (index > 0) {
        f /= static_cast<float>(base);
        result += f * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}

void store(float (&out)[3], const QVector3D &v) {
    out[0] = v.x();
    out[1] = v.y();
    out[2] = v.z();
}

// Where the camera starts: orbiting the scene, inside it looking along its longest side, or the file's own camera.
enum class View { Orbit, Inside, FileCamera };

// The camera as the window moves it and the renderer reads it; both run on the GUI thread.
struct Camera {
    enum class Mode { Orbit, Fly };
    Mode mode = Mode::Orbit;
    bool automatic = true; // orbiting, or panning slowly in fly mode, until the user takes over
    float fovY = 0.75f;
    // Orbit: around the scene's center.
    float azimuth = 0.0f;
    float elevation = 0.35f;
    float distanceFactor = 1.05f;
    // Fly: a position and a direction.
    QVector3D position;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float speed = 1.0f;                                          // units per second
    bool moving[6] = {false, false, false, false, false, false}; // forward, back, left, right, up, down
    bool fast = false;
    // Set by the renderer each frame, so that F can switch modes from where the camera is.
    QVector3D eye;
    QVector3D forward;
    // The gamepad whose sticks and triggers move the camera, if any.
    const QGamepad *pad = nullptr;

    static QVector3D direction(float yaw, float pitch) {
        return QVector3D(std::cos(pitch) * std::sin(yaw), std::sin(pitch), std::cos(pitch) * std::cos(yaw));
    }
    void lookAlong(const QVector3D &d) {
        yaw = std::atan2(d.x(), d.z());
        pitch = std::asin(std::clamp(d.y(), -1.0f, 1.0f));
    }
};

// What the user chose to see, shared by the window that changes it and the renderer that obeys it.
struct RenderOptions {
    bool rayTracing = true; // shadows and reflections, traced; off leaves the plain lit surfaces
    bool dlss = false;
    Dlss::Quality quality = Dlss::Quality::Quality;
    QString environment;            // a panorama for the background and the reflections, or empty
    QString dlssLibrary;            // where NVIDIA's libnvidia-ngx-dlss is
    bool dlssInstanceReady = false; // the instance was made with the extensions NGX needs
    // Set by the renderer for the window title.
    bool effectsSupported = true;
    bool dlssActive = false;
    QSize renderSize;
    QSize displaySize;
    std::function<void()> changed; // the renderer calls it when it has changed the options or what they gave

    void notify() const {
        if (changed)
            changed();
    }
};

// The triangle primitives of a mesh that can be traced.
bool traceable(const QtGltf::Primitive &p) { return p.mode == QtGltf::Primitive::Triangles && p.positions.size() >= 3; }

// The components of a list of vectors, one after another.
template <int Components, typename Vector> std::vector<float> flatten(const QList<Vector> &values) {
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(values.size() * Components));
    for (const Vector &v : values) {
        out.push_back(v.x());
        out.push_back(v.y());
        if constexpr (Components >= 3)
            out.push_back(v.z());
        if constexpr (Components >= 4)
            out.push_back(v.w());
    }
    return out;
}

} // namespace

class SceneRenderer : public QVulkanWindowRenderer {
public:
    SceneRenderer(QVulkanWindow *window, const QtGltf::Document &document, View view, Camera *camera,
                  RenderOptions *options, int frameLimit)
        : m_window(window), m_document(document), m_camera(camera), m_options(options), m_frameLimit(frameLimit) {
        measureScene();
        resetCamera(view);
    }

    // Puts the camera at the start of `view`.
    void resetCamera(View view) {
        Camera &c = *m_camera;
        c = Camera();
        c.speed = m_radius * 0.4f;
        const QVector3D extent = m_hi - m_lo;
        if (view == View::Inside) {
            // Inside at a fifth of the height and three tenths of the length from the center, looking along it.
            const QVector3D axis = extent.x() >= extent.z() ? QVector3D(1.0f, 0.0f, 0.0f) : QVector3D(0.0f, 0.0f, 1.0f);
            const float length = std::max(extent.x(), extent.z());
            c.mode = Camera::Mode::Fly;
            c.position = QVector3D(m_center.x(), m_lo.y() + extent.y() * 0.2f, m_center.z()) - axis * (length * 0.3f);
            c.lookAlong(axis);
        } else if (view == View::FileCamera && findFileCamera()) {
            c.mode = Camera::Mode::Fly;
            c.automatic = false;
            c.position = m_fileCameraTransform.map(QVector3D());
            c.lookAlong(m_fileCameraTransform.mapVector(QVector3D(0.0f, 0.0f, -1.0f)).normalized());
            c.fovY = m_fileCameraFovY;
        } else {
            if (view == View::FileCamera)
                std::puts("ntgltfview: the file has no camera; orbiting instead");
        }
        m_lastFrame = -1.0f;
        m_resetHistory = true;
    }

    // Before the device is made: ask for the ray tracing extensions and their features.
    void preInitResources() override {
        const QVulkanInfoVector<QVulkanExtension> extensions = m_window->supportedDeviceExtensions();
        m_supported = extensions.contains("VK_KHR_acceleration_structure") &&
                      extensions.contains("VK_KHR_ray_tracing_pipeline") &&
                      extensions.contains("VK_KHR_deferred_host_operations");
        if (!m_supported) {
            std::puts("ntgltfview: the device has no ray tracing extensions; showing the background only");
            return;
        }
        QByteArrayList wanted = {"VK_KHR_acceleration_structure", "VK_KHR_ray_tracing_pipeline",
                                 "VK_KHR_deferred_host_operations"};
        // NGX runs its networks through extensions of NVIDIA's driver (binary import, image view handles, push
        // descriptors); DLSS is possible only where the device has them all and the instance has its own.
        QByteArrayList ngxInstance;
        QByteArrayList ngxDevice;
        if (Dlss::builtIn() && m_options->dlssInstanceReady && Dlss::requiredExtensions(&ngxInstance, &ngxDevice)) {
            bool all = true;
            for (const QByteArray &name : ngxDevice)
                all = all && extensions.contains(name);
            if (all) {
                for (const QByteArray &name : ngxDevice) {
                    if (!wanted.contains(name))
                        wanted.append(name);
                }
                m_dlssPossible = true;
            }
        }
        m_window->setDeviceExtensions(wanted);
        m_window->setEnabledFeaturesModifier([this](VkPhysicalDeviceFeatures2 &features) {
            m_addressFeatures = make<VkPhysicalDeviceBufferDeviceAddressFeatures>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES);
            m_addressFeatures.bufferDeviceAddress = VK_TRUE;
            // The textures are an array indexed per hit.
            m_indexingFeatures = make<VkPhysicalDeviceDescriptorIndexingFeatures>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES);
            m_indexingFeatures.runtimeDescriptorArray = VK_TRUE;
            m_indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            m_indexingFeatures.pNext = &m_addressFeatures;
            m_asFeatures = make<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
            m_asFeatures.accelerationStructure = VK_TRUE;
            m_asFeatures.pNext = &m_indexingFeatures;
            m_rtFeatures = make<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
            m_rtFeatures.rayTracingPipeline = VK_TRUE;
            m_rtFeatures.pNext = &m_asFeatures;
            features.pNext = &m_rtFeatures;
        });
    }

    void initResources() override {
        m_clock.start();
        if (!m_supported)
            return;
        m_dev = m_window->device();
        QVulkanInstance *inst = m_window->vulkanInstance();
        QVulkanFunctions *f = inst->functions();
        m_df = inst->deviceFunctions(m_dev);
        f->vkGetPhysicalDeviceMemoryProperties(m_window->physicalDevice(), &m_memProps);

        resolve(f, m_dev, "vkCreateAccelerationStructureKHR", &m_createAS);
        resolve(f, m_dev, "vkDestroyAccelerationStructureKHR", &m_destroyAS);
        resolve(f, m_dev, "vkGetAccelerationStructureBuildSizesKHR", &m_getBuildSizes);
        resolve(f, m_dev, "vkGetAccelerationStructureDeviceAddressKHR", &m_getASAddress);
        resolve(f, m_dev, "vkCmdBuildAccelerationStructuresKHR", &m_cmdBuildAS);
        resolve(f, m_dev, "vkCreateRayTracingPipelinesKHR", &m_createPipelines);
        resolve(f, m_dev, "vkGetRayTracingShaderGroupHandlesKHR", &m_getGroupHandles);
        resolve(f, m_dev, "vkCmdTraceRaysKHR", &m_cmdTraceRays);

        m_rtProps = make<VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR);
        m_asProps = make<VkPhysicalDeviceAccelerationStructurePropertiesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR);
        m_rtProps.pNext = &m_asProps;
        auto props = make<VkPhysicalDeviceProperties2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2);
        props.pNext = &m_rtProps;
        f->vkGetPhysicalDeviceProperties2(m_window->physicalDevice(), &props);
        std::printf("ntgltfview: rendering on %s\n", props.properties.deviceName);
        // Shadows and reflections are traced from the closest hit shader, one ray deeper than the primary one.
        m_options->effectsSupported = m_rtProps.maxRayRecursionDepth >= 2;
        if (!m_options->effectsSupported)
            std::puts("ntgltfview: the device cannot trace from a hit shader; no shadows or reflections");
        if (m_options->dlss)
            startDlss();

        QElapsedTimer timer;
        timer.start();
        createGeometry();
        createTextures();
        createEnvironment();
        createTopLevel();
        createPipeline();
        createShaderBindingTable();
        createDescriptors();
        std::printf("ntgltfview: %lld meshes, %lld primitives, %lld triangles, %lld instances, %lld textures; built "
                    "in %lld ms\n",
                    static_cast<long long>(m_meshes.size()), static_cast<long long>(m_records.size()),
                    static_cast<long long>(m_triangles), static_cast<long long>(m_instanceCount),
                    static_cast<long long>(m_document.textures().size()), static_cast<long long>(timer.elapsed()));
    }

    void initSwapChainResources() override {
        if (!m_supported)
            return;
        // The rays write images of the render size: the swapchain's, or the one DLSS asks for, which it then
        // upscales to the swapchain's.
        const QSize display = m_window->swapChainImageSize();
        m_dlssOn = m_options->dlss && m_dlss.available();
        m_dlssQuality = m_options->quality;
        m_displaySize = display;
        m_renderSize = m_dlssOn ? m_dlss.renderSize(display, m_dlssQuality) : display;
        const VkImageUsageFlags sampled = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        createImage(VK_FORMAT_R8G8B8A8_UNORM, m_renderSize, sampled | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &m_color);
        createImage(VK_FORMAT_R32_SFLOAT, m_renderSize, sampled, &m_depth);
        createImage(VK_FORMAT_R16G16_SFLOAT, m_renderSize, sampled, &m_motion);
        if (m_dlssOn)
            createImage(VK_FORMAT_R8G8B8A8_UNORM, display, sampled | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &m_output);
        const VkFormat color = m_window->colorFormat();
        m_swapRedBlue = !m_dlssOn && (color == VK_FORMAT_B8G8R8A8_UNORM || color == VK_FORMAT_B8G8R8A8_SRGB) ? 1u : 0u;
        m_options->dlssActive = m_dlssOn;
        m_options->renderSize = m_renderSize;
        m_options->displaySize = display;
        m_resetHistory = true;

        // The descriptors that depend on the swapchain: the scene, the images and the primitive records.
        auto asWrite = make<VkWriteDescriptorSetAccelerationStructureKHR>(
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR);
        asWrite.accelerationStructureCount = 1;
        asWrite.pAccelerationStructures = &m_tlas.handle;
        VkDescriptorImageInfo imageInfos[3] = {{VK_NULL_HANDLE, m_color.view, VK_IMAGE_LAYOUT_GENERAL},
                                               {VK_NULL_HANDLE, m_depth.view, VK_IMAGE_LAYOUT_GENERAL},
                                               {VK_NULL_HANDLE, m_motion.view, VK_IMAGE_LAYOUT_GENERAL}};
        VkDescriptorBufferInfo recordsInfo{};
        recordsInfo.buffer = m_recordBuffer.buffer;
        recordsInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet writes[5];
        for (VkWriteDescriptorSet &w : writes) {
            w = make<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            w.dstSet = m_descriptorSet;
            w.descriptorCount = 1;
        }
        writes[0].pNext = &asWrite;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo = &imageInfos[0];
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo = &recordsInfo;
        writes[3].dstBinding = 4;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[3].pImageInfo = &imageInfos[1];
        writes[4].dstBinding = 5;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[4].pImageInfo = &imageInfos[2];
        m_df->vkUpdateDescriptorSets(m_dev, 5, writes, 0, nullptr);
    }

    void releaseSwapChainResources() override {
        if (!m_supported)
            return;
        m_dlss.release();
        destroyImage(m_color);
        destroyImage(m_depth);
        destroyImage(m_motion);
        destroyImage(m_output);
    }

    void releaseResources() override {
        if (!m_supported || !m_dev)
            return;
        m_dlss.shutdown();
        m_df->vkDestroyDescriptorPool(m_dev, m_descriptorPool, nullptr);
        m_df->vkDestroyPipeline(m_dev, m_pipeline, nullptr);
        m_df->vkDestroyPipelineLayout(m_dev, m_pipelineLayout, nullptr);
        m_df->vkDestroyDescriptorSetLayout(m_dev, m_descriptorSetLayout, nullptr);
        destroyBuffer(m_sbt);
        destroyAccelerationStructure(m_tlas);
        destroyBuffer(m_instanceBuffer);
        destroyBuffer(m_recordBuffer);
        for (TextureData &t : m_textures) {
            m_df->vkDestroySampler(m_dev, t.sampler, nullptr);
            m_df->vkDestroyImageView(m_dev, t.view, nullptr);
            m_df->vkDestroyImage(m_dev, t.image, nullptr);
            m_df->vkFreeMemory(m_dev, t.memory, nullptr);
        }
        m_textures.clear();
        m_df->vkDestroySampler(m_dev, m_environment.sampler, nullptr);
        m_df->vkDestroyImageView(m_dev, m_environment.view, nullptr);
        m_df->vkDestroyImage(m_dev, m_environment.image, nullptr);
        m_df->vkFreeMemory(m_dev, m_environment.memory, nullptr);
        m_environment = TextureData();
        for (MeshData &mesh : m_meshes) {
            destroyAccelerationStructure(mesh.blas);
            for (Buffer &b : mesh.buffers)
                destroyBuffer(b);
        }
        m_meshes.clear();
        m_dev = VK_NULL_HANDLE;
    }

    void startNextFrame() override {
        VkCommandBuffer cmd = m_window->currentCommandBuffer();
        if (m_supported && !m_records.empty())
            applyOptions(cmd);
        const QSize size = m_window->swapChainImageSize();
        const auto width = static_cast<uint32_t>(size.width());
        const auto height = static_cast<uint32_t>(size.height());
        VkImage target = m_window->swapChainImage(m_window->currentSwapChainImageIndex());
        advanceCamera();

        if (!m_supported || m_records.empty()) {
            VkClearValue clearValues[2]{};
            clearValues[0].color = {{0.1f, 0.14f, 0.24f, 1.0f}};
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
            const auto renderWidth = static_cast<uint32_t>(m_renderSize.width());
            const auto renderHeight = static_cast<uint32_t>(m_renderSize.height());
            const CameraConstants camera = cameraConstants(renderWidth, renderHeight);
            const VkImage guides[3] = {m_color.image, m_depth.image, m_motion.image};
            for (const VkImage image : guides)
                imageBarrier(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
                             VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
            m_df->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipeline);
            m_df->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_pipelineLayout, 0, 1,
                                          &m_descriptorSet, 0, nullptr);
            m_df->vkCmdPushConstants(cmd, m_pipelineLayout, kCameraStages, 0, sizeof(camera), &camera);
            m_cmdTraceRays(cmd, &m_rgenRegion, &m_missRegion, &m_hitRegion, &m_callableRegion, renderWidth,
                           renderHeight, 1);

            VkImage source = m_color.image;
            if (m_dlssOn && upscale(cmd, camera))
                source = m_output.image;
            VkImageLayout sourceLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkAccessFlags sourceAccess = VK_ACCESS_SHADER_WRITE_BIT;
            VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            if (source == m_output.image) {
                sourceAccess = VK_ACCESS_MEMORY_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            }
            imageBarrier(cmd, source, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sourceAccess,
                         VK_ACCESS_TRANSFER_READ_BIT, sourceStage, VK_PIPELINE_STAGE_TRANSFER_BIT);
            imageBarrier(cmd, target, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
            if (m_dlssOn) {
                // The upscaled image is RGBA; the blit converts it to the swapchain's format.
                VkImageBlit blit{};
                blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                blit.dstSubresource = blit.srcSubresource;
                blit.srcOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};
                blit.dstOffsets[1] = blit.srcOffsets[1];
                m_df->vkCmdBlitImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
            } else {
                VkImageCopy copy{};
                copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy.dstSubresource = copy.srcSubresource;
                copy.extent = {width, height, 1};
                m_df->vkCmdCopyImage(cmd, source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            }
            imageBarrier(cmd, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                         VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }

        m_window->frameReady();
        m_window->requestUpdate();
        countFrame();
    }

private:
    struct MeshData {
        int firstRecord = -1; // into m_records; -1 for a mesh with no traceable primitive
        std::vector<Buffer> buffers;
        AccelerationStructure blas;
    };

    struct TextureData {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    // Starts NGX on the device, once.
    void startDlss() {
        if (m_dlssTried)
            return;
        m_dlssTried = true;
        QString error;
        if (!m_dlssPossible) {
            error = Dlss::builtIn() ? QStringLiteral("the device or the instance lacks what NGX needs")
                                    : QStringLiteral("ntgltfview was built without the DLSS SDK (-DNT_DLSS_SDK_DIR)");
        } else if (m_dlss.init(m_window->vulkanInstance(), m_window->physicalDevice(), m_dev, m_options->dlssLibrary,
                               &error)) {
            std::puts("ntgltfview: DLSS Super Resolution available");
            return;
        }
        std::printf("ntgltfview: no DLSS: %s\n", error.toUtf8().constData());
    }

    // Makes the images again, for a change of DLSS or of the window's size.
    void rebuildTargets() {
        m_df->vkDeviceWaitIdle(m_dev);
        releaseSwapChainResources();
        initSwapChainResources();
        m_options->notify();
    }

    // Takes up what the user changed: DLSS on, off or at another quality means other image sizes. The caller has not
    // recorded anything for the frame yet.
    void applyOptions(VkCommandBuffer cmd) {
        if (m_options->dlss && !m_dlssTried)
            startDlss();
        if (m_options->dlss && !m_dlss.available())
            m_options->dlss = false;
        if (m_options->dlss != m_dlssOn || (m_options->dlss && m_options->quality != m_dlssQuality))
            rebuildTargets();
        if (m_dlssOn && !m_dlss.ready()) {
            if (m_dlss.create(cmd, m_renderSize, m_displaySize, m_dlssQuality)) {
                std::printf("ntgltfview: DLSS %s %dx%d -> %dx%d\n", Dlss::qualityName(m_dlssQuality),
                            m_renderSize.width(), m_renderSize.height(), m_displaySize.width(), m_displaySize.height());
            } else {
                m_options->dlss = false;
                rebuildTargets();
            }
        }
    }

    // Records the upscaling of the frame's color into m_output; false if DLSS could not.
    bool upscale(VkCommandBuffer cmd, const CameraConstants &camera) {
        const VkImage guides[3] = {m_color.image, m_depth.image, m_motion.image};
        for (const VkImage image : guides)
            imageBarrier(cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT,
                         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        imageBarrier(cmd, m_output.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
                     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        Dlss::Frame frame;
        const auto ref = [](const Image &i) { return Dlss::ImageRef{i.image, i.view, i.format, i.size}; };
        frame.color = ref(m_color);
        frame.depth = ref(m_depth);
        frame.motion = ref(m_motion);
        frame.output = ref(m_output);
        frame.jitterX = camera.jitterX;
        frame.jitterY = camera.jitterY;
        frame.reset = m_frameReset;
        return m_dlss.evaluate(cmd, frame);
    }

    void createImage(VkFormat format, const QSize &size, VkImageUsageFlags usage, Image *out) {
        auto info = make<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {static_cast<uint32_t>(std::max(1, size.width())),
                       static_cast<uint32_t>(std::max(1, size.height())), 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(m_df->vkCreateImage(m_dev, &info, nullptr, &out->image), "vkCreateImage");
        VkMemoryRequirements req{};
        m_df->vkGetImageMemoryRequirements(m_dev, out->image, &req);
        auto alloc = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(m_df->vkAllocateMemory(m_dev, &alloc, nullptr, &out->memory), "vkAllocateMemory");
        m_df->vkBindImageMemory(m_dev, out->image, out->memory, 0);
        auto view = make<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = out->image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(m_df->vkCreateImageView(m_dev, &view, nullptr, &out->view), "vkCreateImageView");
        out->format = format;
        out->size = size;
    }

    void destroyImage(Image &image) {
        if (image.view)
            m_df->vkDestroyImageView(m_dev, image.view, nullptr);
        if (image.image)
            m_df->vkDestroyImage(m_dev, image.image, nullptr);
        if (image.memory)
            m_df->vkFreeMemory(m_dev, image.memory, nullptr);
        image = Image();
    }

    // The world-space bounds of the default scene, from the primitives' bounds under the instance transforms.
    void measureScene() {
        QVector3D lo(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
        QVector3D hi = -lo;
        bool any = false;
        for (const QtGltf::Instance &instance : m_document.instances()) {
            const QtGltf::Node &node = m_document.nodes().at(instance.node);
            if (node.mesh < 0)
                continue;
            for (const QtGltf::Primitive &p : m_document.meshes().at(node.mesh).primitives) {
                if (!traceable(p))
                    continue;
                for (int corner = 0; corner < 8; ++corner) {
                    const QVector3D local((corner & 1) ? p.boundsMax.x() : p.boundsMin.x(),
                                          (corner & 2) ? p.boundsMax.y() : p.boundsMin.y(),
                                          (corner & 4) ? p.boundsMax.z() : p.boundsMin.z());
                    const QVector3D world = instance.transform.map(local);
                    lo = QVector3D(std::min(lo.x(), world.x()), std::min(lo.y(), world.y()),
                                   std::min(lo.z(), world.z()));
                    hi = QVector3D(std::max(hi.x(), world.x()), std::max(hi.y(), world.y()),
                                   std::max(hi.z(), world.z()));
                    any = true;
                }
            }
        }
        if (!any) {
            lo = QVector3D(-1.0f, -1.0f, -1.0f);
            hi = QVector3D(1.0f, 1.0f, 1.0f);
        }
        m_lo = lo;
        m_hi = hi;
        m_center = (lo + hi) * 0.5f;
        m_radius = std::max((hi - lo).length() * 0.5f, 1.0e-3f);
        std::printf("ntgltfview: bounds (%.2f, %.2f, %.2f) to (%.2f, %.2f, %.2f)\n", double(lo.x()), double(lo.y()),
                    double(lo.z()), double(hi.x()), double(hi.y()), double(hi.z()));
    }

    // The first camera of the default scene, with its world transform.
    bool findFileCamera() {
        for (const QtGltf::Instance &instance : m_document.instances()) {
            const QtGltf::Node &node = m_document.nodes().at(instance.node);
            if (node.camera < 0 || node.camera >= m_document.cameras().size())
                continue;
            const QtGltf::Camera &camera = m_document.cameras().at(node.camera);
            m_fileCameraTransform = instance.transform;
            m_fileCameraFovY = camera.type == QtGltf::Camera::Perspective ? camera.yfov : 0.75f;
            return true;
        }
        return false;
    }

    // Moves the camera by the time since the last frame: the automatic motion, the keys held down and the gamepad.
    void advanceCamera() {
        Camera &c = *m_camera;
        const float now = static_cast<float>(m_clock.elapsed()) * 0.001f;
        const float dt = m_lastFrame < 0.0f ? 0.0f : std::min(now - m_lastFrame, 0.1f);
        m_lastFrame = now;
        const bool padOn = c.pad && c.pad->isConnected();
        const float lookX = padOn ? static_cast<float>(c.pad->axisRightX()) : 0.0f;
        const float lookY = padOn ? static_cast<float>(c.pad->axisRightY()) : 0.0f;
        const float strafe = padOn ? static_cast<float>(c.pad->axisLeftX()) : 0.0f;
        const float ahead = padOn ? -static_cast<float>(c.pad->axisLeftY()) : 0.0f;
        const float rise = padOn ? static_cast<float>(c.pad->buttonR2() - c.pad->buttonL2()) : 0.0f;
        if (padOn && (lookX != 0.0f || lookY != 0.0f || strafe != 0.0f || ahead != 0.0f || rise != 0.0f))
            c.automatic = false;
        if (c.mode == Camera::Mode::Orbit) {
            if (c.automatic)
                c.azimuth += dt * 0.35f;
            c.azimuth -= lookX * dt * 2.0f;
            c.elevation = std::clamp(c.elevation + lookY * dt * 1.5f, -1.5f, 1.5f);
            c.distanceFactor = std::clamp(c.distanceFactor * std::pow(0.4f, rise * dt), 0.02f, 20.0f);
        } else {
            c.yaw -= lookX * dt * 2.0f;
            c.pitch = std::clamp(c.pitch - lookY * dt * 1.5f, -1.5f, 1.5f);
            if (c.automatic)
                c.yaw += dt * 0.25f * 0.45f * std::cos(now * 0.25f); // the slow pan of the inside view
            const QVector3D forward = Camera::direction(c.yaw, c.pitch);
            const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0.0f, 1.0f, 0.0f)).normalized();
            QVector3D step;
            if (c.moving[0])
                step += forward;
            if (c.moving[1])
                step -= forward;
            if (c.moving[2])
                step -= right;
            if (c.moving[3])
                step += right;
            if (c.moving[4])
                step += QVector3D(0.0f, 1.0f, 0.0f);
            if (c.moving[5])
                step -= QVector3D(0.0f, 1.0f, 0.0f);
            if (!step.isNull())
                c.position += step.normalized() * (c.speed * (c.fast ? 4.0f : 1.0f) * dt);
            // The sticks and triggers are analog, so the speed follows how far they are pushed.
            const QVector3D analog = forward * ahead + right * strafe + QVector3D(0.0f, 1.0f, 0.0f) * rise;
            c.position += analog * (c.speed * (c.fast ? 4.0f : 1.0f) * dt);
        }
    }

    CameraConstants cameraConstants(uint32_t width, uint32_t height) {
        Camera &c = *m_camera;
        CameraPose pose;
        if (c.mode == Camera::Mode::Orbit) {
            const float distance = m_radius / std::sin(c.fovY * 0.5f) * c.distanceFactor;
            pose.origin =
                m_center + distance * QVector3D(std::cos(c.elevation) * std::sin(c.azimuth), std::sin(c.elevation),
                                                std::cos(c.elevation) * std::cos(c.azimuth));
            pose.forward = (m_center - pose.origin).normalized();
        } else {
            pose.origin = c.position;
            pose.forward = Camera::direction(c.yaw, c.pitch);
        }
        c.eye = pose.origin;
        c.forward = pose.forward;
        pose.right = QVector3D::crossProduct(pose.forward, QVector3D(0.0f, 1.0f, 0.0f));
        if (pose.right.lengthSquared() < 1.0e-6f)
            pose.right = QVector3D::crossProduct(pose.forward, QVector3D(1.0f, 0.0f, 0.0f));
        pose.right.normalize();
        pose.up = QVector3D::crossProduct(pose.right, pose.forward);
        pose.tanHalfFovY = std::tan(c.fovY * 0.5f);

        // The motion vectors need where the camera was; with no history it was where it is.
        m_frameReset = m_resetHistory || !m_havePrevious;
        if (m_frameReset)
            m_previous = pose;
        m_resetHistory = false;
        m_havePrevious = true;

        CameraConstants constants{};
        store(constants.origin, pose.origin);
        store(constants.forward, pose.forward);
        store(constants.right, pose.right);
        store(constants.up, pose.up);
        store(constants.prevOrigin, m_previous.origin);
        store(constants.prevForward, m_previous.forward);
        store(constants.prevRight, m_previous.right);
        store(constants.prevUp, m_previous.up);
        constants.tanHalfFovY = pose.tanHalfFovY;
        constants.prevTanHalfFovY = m_previous.tanHalfFovY;
        constants.aspect = static_cast<float>(width) / static_cast<float>(height);
        constants.depthNear = std::max(1.0e-4f, m_radius * 1.0e-3f);
        constants.flags = (m_options->rayTracing && m_options->effectsSupported ? kFlagTraceEffects : 0u) |
                          (m_hasEnvironment ? kFlagEnvironment : 0u);
        constants.swapRedBlue = m_swapRedBlue;
        if (m_dlssOn) {
            // One sub-pixel offset per frame from a Halton sequence long enough to cover the output pixels that
            // each render pixel stands for.
            const float scale = static_cast<float>(m_displaySize.width()) / static_cast<float>(m_renderSize.width());
            const auto phases = static_cast<uint32_t>(std::max(8.0f, 8.0f * scale * scale));
            const uint32_t phase = (m_jitterIndex++ % phases) + 1u;
            constants.jitterX = halton(phase, 2) - 0.5f;
            constants.jitterY = halton(phase, 3) - 0.5f;
        }
        m_previous = pose;
        return constants;
    }

    uint32_t memoryType(uint32_t typeBits, VkMemoryPropertyFlags wanted) const {
        for (uint32_t i = 0; i < m_memProps.memoryTypeCount; ++i) {
            if ((typeBits & (1u << i)) && (m_memProps.memoryTypes[i].propertyFlags & wanted) == wanted)
                return i;
        }
        fail("no suitable memory type");
    }

    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) {
        Buffer b;
        auto info = make<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        info.size = std::max<VkDeviceSize>(size, 16);
        info.usage = usage;
        check(m_df->vkCreateBuffer(m_dev, &info, nullptr, &b.buffer), "vkCreateBuffer");
        VkMemoryRequirements req{};
        m_df->vkGetBufferMemoryRequirements(m_dev, b.buffer, &req);
        auto flags = make<VkMemoryAllocateFlagsInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO);
        flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        auto alloc = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memoryType(req.memoryTypeBits, properties);
        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
            alloc.pNext = &flags;
        check(m_df->vkAllocateMemory(m_dev, &alloc, nullptr, &b.memory), "vkAllocateMemory");
        m_df->vkBindBufferMemory(m_dev, b.buffer, b.memory, 0);
        if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            auto addressInfo = make<VkBufferDeviceAddressInfo>(VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO);
            addressInfo.buffer = b.buffer;
            b.address = m_df->vkGetBufferDeviceAddress(m_dev, &addressInfo);
        }
        return b;
    }

    // A host-visible buffer holding `data`, readable by the shaders and by acceleration structure builds.
    Buffer uploadBuffer(const void *data, std::size_t size) {
        Buffer b = createBuffer(size,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        upload(b, data, size);
        return b;
    }

    // Uploads a vertex attribute of a mesh and returns its address, or 0 for an empty list.
    template <int Components, typename Vector> uint64_t uploadAttribute(MeshData &mesh, const QList<Vector> &values) {
        if (values.isEmpty())
            return 0;
        const std::vector<float> data = flatten<Components>(values);
        mesh.buffers.push_back(uploadBuffer(data.data(), data.size() * sizeof(float)));
        return mesh.buffers.back().address;
    }

    void destroyBuffer(Buffer &b) {
        if (b.buffer)
            m_df->vkDestroyBuffer(m_dev, b.buffer, nullptr);
        if (b.memory)
            m_df->vkFreeMemory(m_dev, b.memory, nullptr);
        b = Buffer();
    }

    void upload(const Buffer &b, const void *data, std::size_t size) {
        void *mapped = nullptr;
        check(m_df->vkMapMemory(m_dev, b.memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory");
        std::memcpy(mapped, data, size);
        m_df->vkUnmapMemory(m_dev, b.memory);
    }

    AccelerationStructure createAccelerationStructure(VkAccelerationStructureTypeKHR type, VkDeviceSize size) {
        AccelerationStructure as;
        as.buffer = createBuffer(
            size, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        auto info =
            make<VkAccelerationStructureCreateInfoKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
        info.buffer = as.buffer.buffer;
        info.size = size;
        info.type = type;
        check(m_createAS(m_dev, &info, nullptr, &as.handle), "vkCreateAccelerationStructureKHR");
        auto addressInfo = make<VkAccelerationStructureDeviceAddressInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR);
        addressInfo.accelerationStructure = as.handle;
        as.address = m_getASAddress(m_dev, &addressInfo);
        return as;
    }

    void destroyAccelerationStructure(AccelerationStructure &as) {
        if (as.handle)
            m_destroyAS(m_dev, as.handle, nullptr);
        destroyBuffer(as.buffer);
        as = AccelerationStructure();
    }

    // Runs `record` in a command buffer and waits for it.
    template <typename Record> void submitAndWait(Record record) {
        auto allocInfo = make<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        allocInfo.commandPool = m_window->graphicsCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        check(m_df->vkAllocateCommandBuffers(m_dev, &allocInfo, &cmd), "vkAllocateCommandBuffers");
        auto begin = make<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        m_df->vkBeginCommandBuffer(cmd, &begin);
        record(cmd);
        m_df->vkEndCommandBuffer(cmd);
        auto submit = make<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        check(m_df->vkQueueSubmit(m_window->graphicsQueue(), 1, &submit, VK_NULL_HANDLE), "vkQueueSubmit");
        m_df->vkQueueWaitIdle(m_window->graphicsQueue());
        m_df->vkFreeCommandBuffers(m_dev, m_window->graphicsCommandPool(), 1, &cmd);
    }

    Buffer createScratch(VkDeviceSize size) {
        return createBuffer(size + m_asProps.minAccelerationStructureScratchOffsetAlignment,
                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    VkDeviceAddress scratchAddress(const Buffer &scratch) const {
        return alignUp(scratch.address, m_asProps.minAccelerationStructureScratchOffsetAlignment);
    }

    // Fills the material part of a record. A texture is referenced by its index; bit `slot` of the TEXCOORD bits
    // says it reads the second set.
    void applyMaterial(PrimitiveRecord &record, const QtGltf::Material &material) {
        record.baseColor[0] = material.baseColorFactor.x();
        record.baseColor[1] = material.baseColorFactor.y();
        record.baseColor[2] = material.baseColorFactor.z();
        record.baseColor[3] = material.baseColorFactor.w();
        const QVector3D emissive = material.emissiveFactor * material.emissiveStrength;
        record.emissive[0] = emissive.x();
        record.emissive[1] = emissive.y();
        record.emissive[2] = emissive.z();
        record.emissive[3] = material.metallicFactor;
        record.params[0] = material.roughnessFactor;
        record.params[1] = material.doubleSided ? 1.0f : 0.0f;
        record.params[2] = material.alphaMode == QtGltf::Material::Mask    ? 1.0f
                           : material.alphaMode == QtGltf::Material::Blend ? 2.0f
                                                                           : 0.0f;
        record.params[3] = material.alphaCutoff;
        record.textures2[2] = material.normalTexture.scale;
        record.textures2[3] = material.occlusionTexture.scale; // the strength of an occlusion texture
        record.transmission[0] = material.transmissionFactor;
        record.transmission[1] = material.ior;
        record.transmission[2] = material.thicknessFactor;
        record.transmission[3] = std::isfinite(material.attenuationDistance) ? material.attenuationDistance : 1.0e30f;
        record.attenuation[0] = material.attenuationColor.x();
        record.attenuation[1] = material.attenuationColor.y();
        record.attenuation[2] = material.attenuationColor.z();
        const QtGltf::TextureInfo *infos[7] = {&material.baseColorTexture, &material.metallicRoughnessTexture,
                                               &material.normalTexture,    &material.occlusionTexture,
                                               &material.emissiveTexture,  &material.transmissionTexture,
                                               &material.thicknessTexture};
        int uvBits = 0;
        for (int slot = 0; slot < 7; ++slot) {
            const QtGltf::TextureInfo &info = *infos[slot];
            if (!info.isValid() || info.index >= m_document.textures().size())
                continue;
            float *target = slot < 4    ? &record.textures[slot]
                            : slot == 4 ? &record.textures2[0]
                                        : &record.textures3[slot - 5];
            *target = static_cast<float>(info.index);
            // The scale, then the rotation, then the offset: u' = cos*sx*u + sin*sy*v + ox, v' = -sin*sx*u + cos*sy*v + oy.
            const float c = std::cos(info.uvRotation);
            const float sn = std::sin(info.uvRotation);
            float *row0 = record.uvTransform[slot * 2];
            float *row1 = record.uvTransform[slot * 2 + 1];
            row0[0] = c * info.uvScale.x();
            row0[1] = sn * info.uvScale.y();
            row0[2] = info.uvOffset.x();
            row1[0] = -sn * info.uvScale.x();
            row1[1] = c * info.uvScale.y();
            row1[2] = info.uvOffset.y();
            if (info.texCoord == 1)
                uvBits |= 1 << slot;
            // The maps that hold data rather than color are uploaded as linear images.
            if (slot == 1 || slot == 2 || slot == 3 || slot >= 5)
                m_dataTextures.insert(info.index);
        }
        record.textures2[1] = static_cast<float>(uvBits);
    }

    // Uploads every mesh's primitives, fills their records, and builds one bottom-level structure per mesh.
    void createGeometry() {
        const QList<QtGltf::Mesh> meshes = m_document.meshes();
        const QList<QtGltf::Material> materials = m_document.materials();
        m_meshes.resize(static_cast<std::size_t>(meshes.size()));
        for (qsizetype m = 0; m < meshes.size(); ++m) {
            MeshData &mesh = m_meshes[static_cast<std::size_t>(m)];
            std::vector<VkAccelerationStructureGeometryKHR> geometries;
            std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
            std::vector<uint32_t> counts;
            for (const QtGltf::Primitive &p : meshes.at(m).primitives) {
                if (!traceable(p))
                    continue;
                std::vector<uint32_t> indices(p.indices.begin(), p.indices.end());
                if (indices.empty()) {
                    indices.resize(static_cast<std::size_t>(p.positions.size()));
                    for (std::size_t i = 0; i < indices.size(); ++i)
                        indices[i] = static_cast<uint32_t>(i);
                }
                indices.resize(indices.size() - indices.size() % 3);
                if (indices.empty())
                    continue;
                if (mesh.firstRecord < 0)
                    mesh.firstRecord = static_cast<int>(m_records.size());

                PrimitiveRecord record;
                record.positions = uploadAttribute<3>(mesh, p.positions);
                mesh.buffers.push_back(uploadBuffer(indices.data(), indices.size() * sizeof(uint32_t)));
                record.indices = mesh.buffers.back().address;
                if (p.normals.size() == p.positions.size())
                    record.normals = uploadAttribute<3>(mesh, p.normals);
                if (p.texCoords0.size() == p.positions.size())
                    record.texCoords0 = uploadAttribute<2>(mesh, p.texCoords0);
                if (p.texCoords1.size() == p.positions.size())
                    record.texCoords1 = uploadAttribute<2>(mesh, p.texCoords1);
                if (p.tangents.size() == p.positions.size())
                    record.tangents = uploadAttribute<4>(mesh, p.tangents);
                if (p.material >= 0 && p.material < materials.size())
                    applyMaterial(record, materials.at(p.material));
                m_records.push_back(record);

                auto geometry =
                    make<VkAccelerationStructureGeometryKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
                geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                // An opaque primitive skips the any-hit shader; a masked or blended one goes through it.
                // Glass is not opaque to the shadow rays: the any-hit shader lets them through.
                geometry.flags =
                    record.params[2] == 0.0f && record.transmission[0] <= 0.0f ? VK_GEOMETRY_OPAQUE_BIT_KHR : 0;
                geometry.geometry.triangles = make<VkAccelerationStructureGeometryTrianglesDataKHR>(
                    VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR);
                geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                geometry.geometry.triangles.vertexData.deviceAddress = record.positions;
                geometry.geometry.triangles.vertexStride = 3 * sizeof(float);
                geometry.geometry.triangles.maxVertex = static_cast<uint32_t>(p.positions.size() - 1);
                geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                geometry.geometry.triangles.indexData.deviceAddress = record.indices;
                geometries.push_back(geometry);
                VkAccelerationStructureBuildRangeInfoKHR range{};
                range.primitiveCount = static_cast<uint32_t>(indices.size() / 3);
                ranges.push_back(range);
                counts.push_back(range.primitiveCount);
                m_triangles += static_cast<qsizetype>(range.primitiveCount);
            }
            if (geometries.empty()) {
                mesh.firstRecord = -1;
                continue;
            }

            auto build = make<VkAccelerationStructureBuildGeometryInfoKHR>(
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
            build.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            build.geometryCount = static_cast<uint32_t>(geometries.size());
            build.pGeometries = geometries.data();
            auto sizes = make<VkAccelerationStructureBuildSizesInfoKHR>(
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);
            m_getBuildSizes(m_dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, counts.data(), &sizes);
            mesh.blas = createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                                                    sizes.accelerationStructureSize);
            Buffer scratch = createScratch(sizes.buildScratchSize);
            build.dstAccelerationStructure = mesh.blas.handle;
            build.scratchData.deviceAddress = scratchAddress(scratch);
            const VkAccelerationStructureBuildRangeInfoKHR *rangePointer = ranges.data();
            submitAndWait([&](VkCommandBuffer cmd) { m_cmdBuildAS(cmd, 1, &build, &rangePointer); });
            destroyBuffer(scratch);
        }
        if (m_records.empty())
            std::puts("ntgltfview: the scene has no triangles");
        m_recordBuffer = createBuffer(std::max<std::size_t>(m_records.size(), 1) * sizeof(PrimitiveRecord),
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!m_records.empty())
            upload(m_recordBuffer, m_records.data(), m_records.size() * sizeof(PrimitiveRecord));
    }

    // Uploads every texture of the document with its sampler: color maps as sRGB images, data maps as linear ones.
    // A scene without textures gets one white texel, so that the descriptor array is never empty.
    void createTextures() {
        const QList<QtGltf::Texture> textures = m_document.textures();
        const QList<QtGltf::Image> images = m_document.images();
        const QList<QtGltf::Sampler> samplers = m_document.samplers();
        const qsizetype count = std::max<qsizetype>(textures.size(), 1);
        for (qsizetype i = 0; i < count; ++i) {
            QImage image;
            QtGltf::Sampler sampler;
            if (i < textures.size()) {
                const QtGltf::Texture &texture = textures.at(i);
                if (texture.source >= 0 && texture.source < images.size())
                    image = images.at(texture.source).toImage();
                if (texture.sampler >= 0 && texture.sampler < samplers.size())
                    sampler = samplers.at(texture.sampler);
                if (image.isNull())
                    std::printf("ntgltfview: texture %lld could not be decoded\n", static_cast<long long>(i));
            }
            if (image.isNull()) {
                image = QImage(1, 1, QImage::Format_ARGB32);
                image.fill(Qt::white);
            }
            const bool linear = m_dataTextures.count(static_cast<int>(i)) != 0;
            m_textures.push_back(createTexture(image.convertToFormat(QImage::Format_ARGB32), sampler, linear));
        }
    }

    TextureData createTexture(const QImage &image, const QtGltf::Sampler &sampler, bool linear) {
        const auto width = static_cast<uint32_t>(image.width());
        const auto height = static_cast<uint32_t>(image.height());
        // The pixels go through a staging buffer, row by row in case the image has padding.
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        std::vector<uint8_t> pixels(rowBytes * height);
        for (uint32_t y = 0; y < height; ++y)
            std::memcpy(pixels.data() + y * rowBytes, image.constScanLine(static_cast<int>(y)), rowBytes);
        const auto filter = [](QtGltf::Sampler::Filter f) {
            return f == QtGltf::Sampler::Nearest || f == QtGltf::Sampler::NearestMipmapNearest ||
                           f == QtGltf::Sampler::NearestMipmapLinear
                       ? VK_FILTER_NEAREST
                       : VK_FILTER_LINEAR;
        };
        const auto wrap = [](QtGltf::Sampler::Wrap w) {
            return w == QtGltf::Sampler::ClampToEdge      ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                   : w == QtGltf::Sampler::MirroredRepeat ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                                                          : VK_SAMPLER_ADDRESS_MODE_REPEAT;
        };
        // QImage::Format_ARGB32 is B, G, R, A in memory.
        return makeTexture(width, height, linear ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B8G8R8A8_SRGB, pixels,
                           filter(sampler.magFilter), filter(sampler.minFilter), wrap(sampler.wrapS),
                           wrap(sampler.wrapT));
    }

    // The panorama of --environment, or one white pixel for the shaders that bind it anyway.
    void createEnvironment() {
        std::vector<uint16_t> pixels(4, Environment::toHalf(1.0f));
        int width = 1;
        int height = 1;
        QString error;
        m_hasEnvironment = false;
        if (!m_options->environment.isEmpty()) {
            if (Environment::load(m_options->environment, &width, &height, &pixels, &error)) {
                m_hasEnvironment = true;
                std::printf("ntgltfview: environment %s (%dx%d)\n", m_options->environment.toUtf8().constData(), width,
                            height);
            } else {
                std::printf("ntgltfview: no environment: %s\n", error.toUtf8().constData());
                pixels.assign(4, Environment::toHalf(1.0f));
                width = height = 1;
            }
        }
        std::vector<uint8_t> bytes(pixels.size() * sizeof(uint16_t));
        std::memcpy(bytes.data(), pixels.data(), bytes.size());
        // The panorama wraps around its seam and stops at the poles.
        m_environment = makeTexture(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                    VK_FORMAT_R16G16B16A16_SFLOAT, bytes, VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                                    VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    }

    // An image with the given pixels, for the shaders to sample at the base level.
    TextureData makeTexture(uint32_t width, uint32_t height, VkFormat format, const std::vector<uint8_t> &pixels,
                            VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode wrapU,
                            VkSamplerAddressMode wrapV) {
        TextureData t;
        auto info = make<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {width, height, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(m_df->vkCreateImage(m_dev, &info, nullptr, &t.image), "vkCreateImage");
        VkMemoryRequirements req{};
        m_df->vkGetImageMemoryRequirements(m_dev, t.image, &req);
        auto alloc = make<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        check(m_df->vkAllocateMemory(m_dev, &alloc, nullptr, &t.memory), "vkAllocateMemory");
        m_df->vkBindImageMemory(m_dev, t.image, t.memory, 0);

        Buffer staging = createBuffer(pixels.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        upload(staging, pixels.data(), pixels.size());
        submitAndWait([&](VkCommandBuffer cmd) {
            imageBarrier(cmd, t.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {width, height, 1};
            m_df->vkCmdCopyBufferToImage(cmd, staging.buffer, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            imageBarrier(cmd, t.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
        });
        destroyBuffer(staging);

        auto view = make<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = t.image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(m_df->vkCreateImageView(m_dev, &view, nullptr, &t.view), "vkCreateImageView");

        auto samplerInfo = make<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
        samplerInfo.magFilter = magFilter;
        samplerInfo.minFilter = minFilter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU = wrapU;
        samplerInfo.addressModeV = wrapV;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.maxLod = 0.0f;
        check(m_df->vkCreateSampler(m_dev, &samplerInfo, nullptr, &t.sampler), "vkCreateSampler");
        return t;
    }

    // One top-level instance per mesh instance of the default scene, built once: the scene does not move.
    void createTopLevel() {
        std::vector<VkAccelerationStructureInstanceKHR> instances;
        for (const QtGltf::Instance &instance : m_document.instances()) {
            const QtGltf::Node &node = m_document.nodes().at(instance.node);
            if (node.mesh < 0 || node.mesh >= static_cast<int>(m_meshes.size()))
                continue;
            const MeshData &mesh = m_meshes[static_cast<std::size_t>(node.mesh)];
            if (mesh.firstRecord < 0)
                continue;
            VkAccelerationStructureInstanceKHR vk{};
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 4; ++column)
                    vk.transform.matrix[row][column] = instance.transform(row, column);
            vk.instanceCustomIndex = static_cast<uint32_t>(mesh.firstRecord) & 0xffffffu;
            vk.mask = 0xff;
            vk.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            vk.accelerationStructureReference = mesh.blas.address;
            instances.push_back(vk);
        }
        m_instanceCount = static_cast<qsizetype>(instances.size());
        if (instances.empty()) {
            m_records.clear();
            return;
        }
        m_instanceBuffer = createBuffer(instances.size() * sizeof(VkAccelerationStructureInstanceKHR),
                                        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        upload(m_instanceBuffer, instances.data(), instances.size() * sizeof(VkAccelerationStructureInstanceKHR));

        auto geometry = make<VkAccelerationStructureGeometryKHR>(VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR);
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances = make<VkAccelerationStructureGeometryInstancesDataKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR);
        geometry.geometry.instances.data.deviceAddress = m_instanceBuffer.address;
        auto build = make<VkAccelerationStructureBuildGeometryInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR);
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount = 1;
        build.pGeometries = &geometry;
        const auto instanceCount = static_cast<uint32_t>(instances.size());
        auto sizes = make<VkAccelerationStructureBuildSizesInfoKHR>(
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR);
        m_getBuildSizes(m_dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &instanceCount, &sizes);
        m_tlas =
            createAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, sizes.accelerationStructureSize);
        Buffer scratch = createScratch(sizes.buildScratchSize);
        build.dstAccelerationStructure = m_tlas.handle;
        build.scratchData.deviceAddress = scratchAddress(scratch);
        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = instanceCount;
        const VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;
        submitAndWait([&](VkCommandBuffer cmd) { m_cmdBuildAS(cmd, 1, &build, &ranges); });
        destroyBuffer(scratch);
    }

    VkShaderModule loadShader(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            fail(QString("cannot open %1").arg(path));
        const QByteArray code = file.readAll();
        auto info = make<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        info.codeSize = static_cast<std::size_t>(code.size());
        info.pCode = reinterpret_cast<const uint32_t *>(code.constData());
        VkShaderModule module = VK_NULL_HANDLE;
        check(m_df->vkCreateShaderModule(m_dev, &info, nullptr, &module), "vkCreateShaderModule");
        return module;
    }

    // The pipeline: ray generation, miss, and a hit group of closest hit and any hit, in three shader groups.
    void createPipeline() {
        VkDescriptorSetLayoutBinding bindings[7]{};
        bindings[0].binding = 0;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags = kCameraStages;
        bindings[1].binding = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        const VkShaderStageFlags hitStages = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        bindings[2].binding = 2;
        bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[2].descriptorCount = 1;
        bindings[2].stageFlags = hitStages;
        bindings[3].binding = 3;
        bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[3].descriptorCount = static_cast<uint32_t>(m_textures.size());
        bindings[3].stageFlags = hitStages;
        auto layoutInfo = make<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        bindings[4].binding = 4;
        bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[4].descriptorCount = 1;
        bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        bindings[5] = bindings[4];
        bindings[5].binding = 5;
        bindings[6].binding = 6;
        bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[6].descriptorCount = 1;
        bindings[6].stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        layoutInfo.bindingCount = 7;
        layoutInfo.pBindings = bindings;
        check(m_df->vkCreateDescriptorSetLayout(m_dev, &layoutInfo, nullptr, &m_descriptorSetLayout),
              "vkCreateDescriptorSetLayout");

        VkPushConstantRange pushConstants{kCameraStages, 0, sizeof(CameraConstants)};
        auto pipelineLayoutInfo = make<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstants;
        check(m_df->vkCreatePipelineLayout(m_dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout),
              "vkCreatePipelineLayout");

        // Ray generation, two miss shaders (the sky, and a ray to the light that nothing blocked) and a hit group of
        // closest hit and any hit, in four shader groups.
        const VkShaderStageFlagBits stageBits[5] = {VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_SHADER_STAGE_MISS_BIT_KHR,
                                                    VK_SHADER_STAGE_MISS_BIT_KHR, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                                                    VK_SHADER_STAGE_ANY_HIT_BIT_KHR};
        const char *const files[5] = {":/shaders/ntgltfview.rgen.spv", ":/shaders/ntgltfview.rmiss.spv",
                                      ":/shaders/ntgltfview_shadow.rmiss.spv", ":/shaders/ntgltfview.rchit.spv",
                                      ":/shaders/ntgltfview.rahit.spv"};
        VkPipelineShaderStageCreateInfo stages[5];
        for (int i = 0; i < 5; ++i) {
            stages[i] = make<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
            stages[i].stage = stageBits[i];
            stages[i].module = loadShader(QString::fromLatin1(files[i]));
            stages[i].pName = "main";
        }
        VkRayTracingShaderGroupCreateInfoKHR groups[kGroupCount];
        for (VkRayTracingShaderGroupCreateInfoKHR &group : groups) {
            group =
                make<VkRayTracingShaderGroupCreateInfoKHR>(VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);
            group.generalShader = VK_SHADER_UNUSED_KHR;
            group.closestHitShader = VK_SHADER_UNUSED_KHR;
            group.anyHitShader = VK_SHADER_UNUSED_KHR;
            group.intersectionShader = VK_SHADER_UNUSED_KHR;
        }
        for (uint32_t i = 0; i < 3; ++i) {
            groups[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            groups[i].generalShader = i;
        }
        groups[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        groups[3].closestHitShader = 3;
        groups[3].anyHitShader = 4;

        auto info = make<VkRayTracingPipelineCreateInfoKHR>(VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR);
        info.stageCount = 5;
        info.pStages = stages;
        info.groupCount = kGroupCount;
        info.pGroups = groups;
        // The hit shader traces the shadow ray, so two; one where the device allows no more.
        info.maxPipelineRayRecursionDepth = m_options->effectsSupported ? 2 : 1;
        info.layout = m_pipelineLayout;
        check(m_createPipelines(m_dev, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &info, nullptr, &m_pipeline),
              "vkCreateRayTracingPipelinesKHR");
        for (const VkPipelineShaderStageCreateInfo &stage : stages)
            m_df->vkDestroyShaderModule(m_dev, stage.module, nullptr);
    }

    // The shader binding table: the handle of each group, one aligned record each; the two miss shaders are
    // neighbours, and the shadow ray picks the second by its miss index.
    void createShaderBindingTable() {
        const uint32_t handleSize = m_rtProps.shaderGroupHandleSize;
        const VkDeviceSize handleStride = alignUp(handleSize, m_rtProps.shaderGroupHandleAlignment);
        const VkDeviceSize regionSize = alignUp(handleStride, m_rtProps.shaderGroupBaseAlignment);
        const VkDeviceSize missSize = alignUp(2 * handleStride, m_rtProps.shaderGroupBaseAlignment);
        std::vector<uint8_t> handles(static_cast<std::size_t>(kGroupCount * handleSize));
        check(m_getGroupHandles(m_dev, m_pipeline, 0, kGroupCount, handles.size(), handles.data()),
              "vkGetRayTracingShaderGroupHandlesKHR");
        m_sbt = createBuffer(regionSize + missSize + regionSize,
                             VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void *mapped = nullptr;
        check(m_df->vkMapMemory(m_dev, m_sbt.memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory");
        auto *base = static_cast<uint8_t *>(mapped);
        std::memset(base, 0, static_cast<std::size_t>(regionSize + missSize + regionSize));
        std::memcpy(base, handles.data(), handleSize);                                              // ray generation
        std::memcpy(base + regionSize, handles.data() + handleSize, handleSize);                    // miss: sky
        std::memcpy(base + regionSize + handleStride, handles.data() + 2 * handleSize, handleSize); // miss: shadow
        std::memcpy(base + regionSize + missSize, handles.data() + 3 * handleSize, handleSize);     // hit group
        m_df->vkUnmapMemory(m_dev, m_sbt.memory);
        m_rgenRegion = {m_sbt.address, regionSize, regionSize};
        m_missRegion = {m_sbt.address + regionSize, handleStride, missSize};
        m_hitRegion = {m_sbt.address + regionSize + missSize, handleStride, regionSize};
        m_callableRegion = {};
    }

    void createDescriptors() {
        VkDescriptorPoolSize sizes[4] = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(m_textures.size()) + 1}};
        auto poolInfo = make<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 4;
        poolInfo.pPoolSizes = sizes;
        check(m_df->vkCreateDescriptorPool(m_dev, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");
        auto allocInfo = make<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;
        check(m_df->vkAllocateDescriptorSets(m_dev, &allocInfo, &m_descriptorSet), "vkAllocateDescriptorSets");

        // The textures do not change with the swapchain, so they are written once.
        std::vector<VkDescriptorImageInfo> infos;
        infos.reserve(m_textures.size());
        for (const TextureData &t : m_textures)
            infos.push_back({t.sampler, t.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        auto write = make<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        write.dstSet = m_descriptorSet;
        write.dstBinding = 3;
        write.descriptorCount = static_cast<uint32_t>(infos.size());
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = infos.data();
        m_df->vkUpdateDescriptorSets(m_dev, 1, &write, 0, nullptr);
        const VkDescriptorImageInfo environmentInfo{m_environment.sampler, m_environment.view,
                                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        write.dstBinding = 6;
        write.descriptorCount = 1;
        write.pImageInfo = &environmentInfo;
        m_df->vkUpdateDescriptorSets(m_dev, 1, &write, 0, nullptr);
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

    void countFrame() {
        ++m_frames;
        ++m_framesThisSecond;
        if (m_fpsClock.isValid() && m_fpsClock.elapsed() >= 1000) {
            std::printf("%d frames/s\n", m_framesThisSecond);
            m_framesThisSecond = 0;
            m_fpsClock.restart();
        } else if (!m_fpsClock.isValid()) {
            m_fpsClock.start();
        }
        if (m_frameLimit > 0 && m_frames >= m_frameLimit)
            QCoreApplication::quit();
    }

    QVulkanWindow *m_window;
    QtGltf::Document m_document;
    Camera *m_camera;
    RenderOptions *m_options;
    int m_frameLimit;
    bool m_supported = false;
    QVector3D m_lo;
    QVector3D m_hi;
    QVector3D m_center;
    float m_radius = 1.0f;
    QMatrix4x4 m_fileCameraTransform;
    float m_fileCameraFovY = 0.75f;
    float m_lastFrame = -1.0f;
    VkDevice m_dev = VK_NULL_HANDLE;
    QVulkanDeviceFunctions *m_df = nullptr;
    VkPhysicalDeviceMemoryProperties m_memProps{};
    VkPhysicalDeviceBufferDeviceAddressFeatures m_addressFeatures{};
    VkPhysicalDeviceDescriptorIndexingFeatures m_indexingFeatures{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR m_asFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR m_rtFeatures{};
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

    std::vector<MeshData> m_meshes;
    std::vector<TextureData> m_textures;
    TextureData m_environment;
    bool m_hasEnvironment = false;
    std::set<int> m_dataTextures; // the textures that hold data, not color
    std::vector<PrimitiveRecord> m_records;
    qsizetype m_triangles = 0;
    qsizetype m_instanceCount = 0;
    Buffer m_recordBuffer;
    Buffer m_instanceBuffer;
    AccelerationStructure m_tlas;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    Buffer m_sbt;
    VkStridedDeviceAddressRegionKHR m_rgenRegion{};
    VkStridedDeviceAddressRegionKHR m_missRegion{};
    VkStridedDeviceAddressRegionKHR m_hitRegion{};
    VkStridedDeviceAddressRegionKHR m_callableRegion{};
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    Image m_color;  // what the rays write, at the render size
    Image m_depth;  // the first hit's depth, for DLSS
    Image m_motion; // and its motion vector
    Image m_output; // DLSS's result, at the window's size
    QSize m_renderSize;
    QSize m_displaySize;
    uint32_t m_swapRedBlue = 0;
    Dlss::Session m_dlss;
    bool m_dlssPossible = false; // the device has the extensions NGX needs
    bool m_dlssTried = false;
    bool m_dlssOn = false; // the images are made for DLSS
    Dlss::Quality m_dlssQuality = Dlss::Quality::Quality;
    CameraPose m_previous;
    bool m_havePrevious = false;
    bool m_resetHistory = true; // the camera jumped, or the images are new: the previous frame does not count
    bool m_frameReset = false;
    uint32_t m_jitterIndex = 0;

    QElapsedTimer m_clock;
    QElapsedTimer m_fpsClock;
    int m_frames = 0;
    int m_framesThisSecond = 0;
};

// The window owns the camera and moves it with the mouse and keyboard; the renderer reads it each frame.
class ViewerWindow : public QVulkanWindow {
public:
    ViewerWindow(const QtGltf::Document &document, View view, int frameLimit, const RenderOptions &options,
                 const QString &name)
        : m_document(document), m_view(view), m_frameLimit(frameLimit), m_options(options), m_name(name) {
        m_options.changed = [this] { updateTitle(); };
        setUpGamepad();
        updateTitle();
    }
    QVulkanWindowRenderer *createRenderer() override {
        m_renderer = new SceneRenderer(this, m_document, m_view, &m_camera, &m_options, m_frameLimit);
        return m_renderer;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        m_lastMouse = event->position();
        m_camera.automatic = false;
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (!(event->buttons() & Qt::LeftButton))
            return;
        const QPointF delta = event->position() - m_lastMouse;
        m_lastMouse = event->position();
        const float dx = static_cast<float>(delta.x());
        const float dy = static_cast<float>(delta.y());
        if (m_camera.mode == Camera::Mode::Orbit) {
            m_camera.azimuth -= dx * 0.005f;
            m_camera.elevation = std::clamp(m_camera.elevation + dy * 0.005f, -1.5f, 1.5f);
        } else {
            m_camera.yaw -= dx * 0.003f;
            m_camera.pitch = std::clamp(m_camera.pitch - dy * 0.003f, -1.5f, 1.5f);
        }
    }

    void wheelEvent(QWheelEvent *event) override {
        const float steps = static_cast<float>(event->angleDelta().y()) / 120.0f;
        if (m_camera.mode == Camera::Mode::Orbit)
            m_camera.distanceFactor = std::clamp(m_camera.distanceFactor * std::pow(0.9f, steps), 0.02f, 20.0f);
        else
            m_camera.speed *= std::pow(1.25f, steps);
        m_camera.automatic = false;
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->isAutoRepeat())
            return;
        switch (event->key()) {
        case Qt::Key_Escape:
            QCoreApplication::quit();
            break;
        case Qt::Key_F11:
            toggleFullScreen();
            break;
        case Qt::Key_T:
            toggleRayTracing();
            break;
        case Qt::Key_N:
            cycleDlss();
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (event->modifiers() & Qt::AltModifier)
                toggleFullScreen();
            break;
        case Qt::Key_Space:
            m_camera.automatic = !m_camera.automatic;
            break;
        case Qt::Key_R:
            if (m_renderer)
                m_renderer->resetCamera(m_view);
            break;
        case Qt::Key_F:
            toggleMode();
            break;
        case Qt::Key_Shift:
            m_camera.fast = true;
            break;
        default:
            setMoving(event->key(), true);
            break;
        }
    }

    void keyReleaseEvent(QKeyEvent *event) override {
        if (event->isAutoRepeat())
            return;
        if (event->key() == Qt::Key_Shift)
            m_camera.fast = false;
        else
            setMoving(event->key(), false);
    }

private:
    // Moving means flying: leave the orbit from where the camera is.
    void startFlying() {
        if (m_camera.mode != Camera::Mode::Orbit)
            return;
        m_camera.mode = Camera::Mode::Fly;
        m_camera.position = m_camera.eye;
        m_camera.lookAlong(m_camera.forward);
    }

    // The title says what is on: the ray traced effects, and DLSS with the sizes it works between.
    void updateTitle() {
        QString title =
            QStringLiteral("ntgltfview - %1 [ray tracing %2")
                .arg(m_name)
                .arg(m_options.rayTracing && m_options.effectsSupported ? QStringLiteral("on") : QStringLiteral("off"));
        if (m_options.dlssActive)
            title += QStringLiteral(", DLSS %1 %2x%3 -> %4x%5")
                         .arg(QString::fromLatin1(Dlss::qualityName(m_options.quality)))
                         .arg(m_options.renderSize.width())
                         .arg(m_options.renderSize.height())
                         .arg(m_options.displaySize.width())
                         .arg(m_options.displaySize.height());
        setTitle(title + QLatin1Char(']'));
    }

    void toggleRayTracing() {
        if (!m_options.effectsSupported) {
            std::puts("ntgltfview: this device cannot trace shadows and reflections");
            return;
        }
        m_options.rayTracing = !m_options.rayTracing;
        std::printf("ntgltfview: ray traced shadows and reflections %s\n", m_options.rayTracing ? "on" : "off");
        updateTitle();
    }

    // DLSS off, then quality, balanced, performance and DLAA, and off again.
    void cycleDlss() {
        if (!Dlss::builtIn()) {
            std::puts("ntgltfview: built without the DLSS SDK (-DNT_DLSS_SDK_DIR)");
            return;
        }
        if (!m_options.dlss) {
            m_options.dlss = true;
            m_options.quality = Dlss::Quality::Quality;
        } else if (m_options.quality == Dlss::Quality::Quality) {
            m_options.quality = Dlss::Quality::Balanced;
        } else if (m_options.quality == Dlss::Quality::Balanced) {
            m_options.quality = Dlss::Quality::Performance;
        } else if (m_options.quality == Dlss::Quality::Performance) {
            m_options.quality = Dlss::Quality::Dlaa;
        } else {
            m_options.dlss = false;
        }
        std::printf("ntgltfview: DLSS %s\n", m_options.dlss ? Dlss::qualityName(m_options.quality) : "off");
    }

    void toggleFullScreen() {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    }

    // Switches modes from where the camera is now.
    void toggleMode() {
        if (m_camera.mode == Camera::Mode::Orbit)
            startFlying();
        else
            m_camera.mode = Camera::Mode::Orbit;
        m_camera.automatic = false;
    }

    // The pad with id 0 (plugging one in later works too): the sticks and triggers are read every frame, the
    // buttons act when pressed.
    void setUpGamepad() {
        QGamepadManager::instance()->setDeadZone(0.15);
        m_pad = new QGamepad(0, this);
        m_camera.pad = m_pad;
        connect(m_pad, &QGamepad::buttonAChanged, this, [this](bool down) { m_camera.fast = down; });
        connect(m_pad, &QGamepad::buttonYChanged, this, [this](bool down) {
            if (down)
                toggleMode();
        });
        connect(m_pad, &QGamepad::buttonXChanged, this, [this](bool down) {
            if (down && m_renderer)
                m_renderer->resetCamera(m_view);
        });
        connect(m_pad, &QGamepad::buttonStartChanged, this, [this](bool down) {
            if (down)
                m_camera.automatic = !m_camera.automatic;
        });
        connect(m_pad, &QGamepad::buttonL3Changed, this, [this](bool down) {
            if (down)
                toggleRayTracing();
        });
        connect(m_pad, &QGamepad::buttonR3Changed, this, [this](bool down) {
            if (down)
                cycleDlss();
        });
        connect(m_pad, &QGamepad::buttonSelectChanged, this, [](bool down) {
            if (down)
                QCoreApplication::quit();
        });
        connect(m_pad, &QGamepad::buttonR1Changed, this, [this](bool down) {
            if (down)
                m_camera.speed *= 1.25f;
        });
        connect(m_pad, &QGamepad::buttonL1Changed, this, [this](bool down) {
            if (down)
                m_camera.speed /= 1.25f;
        });
    }

    void setMoving(int key, bool down) {
        int index = -1;
        switch (key) {
        case Qt::Key_W:
        case Qt::Key_Up:
            index = 0;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            index = 1;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            index = 2;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            index = 3;
            break;
        case Qt::Key_E:
        case Qt::Key_PageUp:
            index = 4;
            break;
        case Qt::Key_Q:
        case Qt::Key_PageDown:
            index = 5;
            break;
        default:
            return;
        }
        if (down)
            startFlying();
        m_camera.moving[index] = down;
        if (down)
            m_camera.automatic = false;
    }

    QtGltf::Document m_document;
    View m_view;
    int m_frameLimit;
    RenderOptions m_options;
    QString m_name;
    Camera m_camera;
    SceneRenderer *m_renderer = nullptr;
    QGamepad *m_pad = nullptr;
    QPointF m_lastMouse;
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Shows a glTF 2.0 scene with hardware ray tracing.\n\n"
                       "Drag to orbit or look around, wheel to zoom or change the speed; W A S D Q E (or the arrows) "
                       "fly, with Shift faster; F switches between orbiting and flying; Space stops and starts the "
                       "automatic motion; R resets; F11 or Alt+Return toggles full screen; Escape quits.\n\n"
                       "Gamepad: left stick flies, right stick looks or orbits, triggers rise and sink (or zoom), "
                       "A faster, shoulder buttons change the speed, Y switches modes, X resets, Start stops and "
                       "starts the automatic motion, Select quits."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("The .gltf or .glb file."));
    const QCommandLineOption deviceOption(QStringLiteral("device"), QStringLiteral("Use the physical device INDEX."),
                                          QStringLiteral("INDEX"));
    const QCommandLineOption devicesOption(QStringLiteral("devices"), QStringLiteral("List the physical devices."));
    const QCommandLineOption framesOption(QStringLiteral("frames"), QStringLiteral("Quit after N frames."),
                                          QStringLiteral("N"));
    const QCommandLineOption grabOption(
        QStringLiteral("grab"), QStringLiteral("Save the frame after one second to FILE."), QStringLiteral("FILE"));
    const QCommandLineOption sizeOption(
        QStringLiteral("size"), QStringLiteral("The window size, WxH (default 1280x800)."), QStringLiteral("WxH"));
    const QCommandLineOption viewOption(
        QStringLiteral("view"),
        QStringLiteral("The camera: orbit (default), inside (looking along the scene) or camera (the file's own)."),
        QStringLiteral("MODE"), QStringLiteral("orbit"));
    const QCommandLineOption noVsyncOption(QStringLiteral("no-vsync"), QStringLiteral("Render as fast as possible."));
    const QCommandLineOption validateOption(QStringLiteral("validate"),
                                            QStringLiteral("Enable the Vulkan validation layer."));
    const QCommandLineOption rayTraceOption(
        QStringLiteral("raytrace"),
        QStringLiteral("Trace shadows and reflections: on (default) or off, for the plain lit surfaces."),
        QStringLiteral("on|off"), QStringLiteral("on"));
    const QCommandLineOption dlssOption(
        QStringLiteral("dlss"),
        QStringLiteral("Upscale with NVIDIA DLSS Super Resolution (an RTX GPU, and a build with -DNT_DLSS_SDK_DIR): "
                       "quality, balanced, performance, dlaa or off (default)."),
        QStringLiteral("MODE"), QStringLiteral("off"));
    const QCommandLineOption environmentOption(
        QStringLiteral("environment"),
        QStringLiteral("A panorama (a Radiance .hdr or an image, equirectangular) for the background and the "
                       "reflections."),
        QStringLiteral("FILE"));
    const QCommandLineOption dlssPathOption(QStringLiteral("dlss-path"),
                                            QStringLiteral("The directory with NVIDIA's libnvidia-ngx-dlss.so."),
                                            QStringLiteral("DIR"));
    parser.addOptions({deviceOption, devicesOption, framesOption, grabOption, sizeOption, viewOption, noVsyncOption,
                       validateOption, rayTraceOption, dlssOption, dlssPathOption, environmentOption});
    parser.process(app);

    RenderOptions options;
    const QString rayTrace = parser.value(rayTraceOption);
    if (rayTrace != QStringLiteral("on") && rayTrace != QStringLiteral("off"))
        fail(QStringLiteral("--raytrace is on or off, not %1").arg(rayTrace));
    options.rayTracing = rayTrace == QStringLiteral("on");
    const QString dlssMode = parser.value(dlssOption);
    if (dlssMode == QStringLiteral("quality"))
        options.quality = Dlss::Quality::Quality;
    else if (dlssMode == QStringLiteral("balanced"))
        options.quality = Dlss::Quality::Balanced;
    else if (dlssMode == QStringLiteral("performance"))
        options.quality = Dlss::Quality::Performance;
    else if (dlssMode == QStringLiteral("dlaa"))
        options.quality = Dlss::Quality::Dlaa;
    else if (dlssMode != QStringLiteral("off"))
        fail(QStringLiteral("unknown DLSS mode %1; use off, quality, balanced, performance or dlaa").arg(dlssMode));
    options.dlss = dlssMode != QStringLiteral("off");
    options.environment = parser.value(environmentOption);
    options.dlssLibrary =
        parser.isSet(dlssPathOption) ? parser.value(dlssPathOption) : QStringLiteral(NT_DLSS_LIBRARY_DIR);

    QVulkanInstance inst;
    inst.setApiVersion(QVersionNumber(1, 2));
    if (parser.isSet(validateOption))
        inst.setLayers({"VK_LAYER_KHRONOS_validation"});
    // NGX wants instance extensions of its own, which only an instance made with them has.
    QByteArrayList ngxInstance;
    QByteArrayList ngxDevice;
    if (Dlss::builtIn() && Dlss::requiredExtensions(&ngxInstance, &ngxDevice)) {
        const QVulkanInfoVector<QVulkanExtension> supported = inst.supportedExtensions();
        bool all = true;
        for (const QByteArray &name : ngxInstance)
            all = all && supported.contains(name);
        if (all) {
            inst.setExtensions(ngxInstance);
            options.dlssInstanceReady = true;
        }
    }
    if (!inst.create()) {
        std::fprintf(stderr, "ntgltfview: no Vulkan (error %d)\n", static_cast<int>(inst.errorCode()));
        return 1;
    }

    if (parser.isSet(devicesOption)) {
        QVulkanWindow probe;
        probe.setVulkanInstance(&inst);
        int index = 0;
        for (const VkPhysicalDeviceProperties &props : probe.availablePhysicalDevices())
            std::printf("%d: %s (Vulkan %u.%u)\n", index++, props.deviceName, VK_API_VERSION_MAJOR(props.apiVersion),
                        VK_API_VERSION_MINOR(props.apiVersion));
        return 0;
    }
    if (parser.positionalArguments().size() != 1)
        parser.showHelp(1);

    QtGltf::Document document;
    if (!document.load(parser.positionalArguments().first())) {
        std::fprintf(stderr, "ntgltfview: %s\n", document.errorString().toUtf8().constData());
        return 1;
    }

    const QString viewName = parser.value(viewOption);
    View view = View::Orbit;
    if (viewName == QStringLiteral("inside"))
        view = View::Inside;
    else if (viewName == QStringLiteral("camera"))
        view = View::FileCamera;
    else if (viewName != QStringLiteral("orbit"))
        fail(QStringLiteral("unknown view %1; use orbit, inside or camera").arg(viewName));
    ViewerWindow window(document, view, parser.value(framesOption).toInt(), options,
                        parser.positionalArguments().first());
    window.setVulkanInstance(&inst);
    if (parser.isSet(deviceOption)) {
        window.setPhysicalDeviceIndex(parser.value(deviceOption).toInt());
    } else if (options.dlss) {
        // DLSS runs on NVIDIA's GPUs: take the first one.
        const QList<VkPhysicalDeviceProperties> devices = window.availablePhysicalDevices();
        for (int i = 0; i < devices.size(); ++i) {
            if (devices.at(i).vendorID == 0x10de) {
                window.setPhysicalDeviceIndex(i);
                break;
            }
        }
    }
    QSurfaceFormat format;
    format.setSwapInterval(parser.isSet(noVsyncOption) ? 0 : 1);
    window.setFormat(format);
    QSize size(1280, 800);
    if (parser.isSet(sizeOption)) {
        const QStringList parts = parser.value(sizeOption).split(QLatin1Char('x'));
        if (parts.size() == 2 && parts[0].toInt() > 0 && parts[1].toInt() > 0)
            size = QSize(parts[0].toInt(), parts[1].toInt());
    }
    window.resize(size);
    window.show();

    if (parser.isSet(grabOption)) {
        const QString path = parser.value(grabOption);
        QTimer::singleShot(1000, &window, [&window, path] {
            const QImage image = window.grab();
            if (image.isNull() || !image.save(path))
                std::fprintf(stderr, "ntgltfview: could not save %s\n", path.toUtf8().constData());
            else
                std::printf("ntgltfview: saved %s (%dx%d)\n", path.toUtf8().constData(), image.width(), image.height());
        });
    }
    return app.exec();
}
