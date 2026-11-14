// MIT License - see LICENSE
#include <QtGui/qvulkanfunctions.h>

// Generated from the Vulkan registry (vk.xml) by vkgen.py; do not edit.

#if QT_CONFIG(vulkan)

namespace {

template <typename Proc, typename Object, typename F>
void resolve(Proc getProcAddr, Object object, const char *name, F *out) {
    *out = reinterpret_cast<F>(getProcAddr(object, name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

} // namespace

/*!
    \class QVulkanFunctions
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanFunctions class provides cross-platform access to the instance level core Vulkan 1.0 to 1.3 API.

    Each function calls the Vulkan function of the same name through the function pointer that the loader
    returned for the instance. Get the object with QVulkanInstance::functions(); it is owned by the instance.
    Functions of a Vulkan version newer than the instance's API version are null and must not be called.

    Extension functions are not here: resolve them with QVulkanInstance::getInstanceProcAddr() or
    vkGetDeviceProcAddr(), as the \c rtriangle example does for the ray tracing extensions.

    \differences
    The functions are declared in the header and defined out of line, so each call goes through one more
    jump than in Qt, where they are inline.
    \enddifferences

    \sa QVulkanInstance, QVulkanDeviceFunctions
*/

struct QVulkanFunctions::Private {
    PFN_vkDestroyInstance DestroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties GetPhysicalDeviceFormatProperties = nullptr;
    PFN_vkGetPhysicalDeviceImageFormatProperties GetPhysicalDeviceImageFormatProperties = nullptr;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
    PFN_vkCreateDevice CreateDevice = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties = nullptr;
    PFN_vkEnumerateDeviceLayerProperties EnumerateDeviceLayerProperties = nullptr;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties GetPhysicalDeviceSparseImageFormatProperties = nullptr;
    PFN_vkEnumeratePhysicalDeviceGroups EnumeratePhysicalDeviceGroups = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceFormatProperties2 GetPhysicalDeviceFormatProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceImageFormatProperties2 GetPhysicalDeviceImageFormatProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties2 GetPhysicalDeviceQueueFamilyProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties2 GetPhysicalDeviceMemoryProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 GetPhysicalDeviceSparseImageFormatProperties2 = nullptr;
    PFN_vkGetPhysicalDeviceExternalBufferProperties GetPhysicalDeviceExternalBufferProperties = nullptr;
    PFN_vkGetPhysicalDeviceExternalFenceProperties GetPhysicalDeviceExternalFenceProperties = nullptr;
    PFN_vkGetPhysicalDeviceExternalSemaphoreProperties GetPhysicalDeviceExternalSemaphoreProperties = nullptr;
    PFN_vkGetPhysicalDeviceToolProperties GetPhysicalDeviceToolProperties = nullptr;
};

// Resolves the instance-level functions of `instance` through `getProcAddr`. Functions of a Vulkan version newer than
// the instance's are left null.
QVulkanFunctions::QVulkanFunctions(PFN_vkGetInstanceProcAddr getProcAddr, VkInstance instance) : m_d(new Private) {
    resolve(getProcAddr, instance, "vkDestroyInstance", &m_d->DestroyInstance);
    resolve(getProcAddr, instance, "vkEnumeratePhysicalDevices", &m_d->EnumeratePhysicalDevices);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceFeatures", &m_d->GetPhysicalDeviceFeatures);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceFormatProperties", &m_d->GetPhysicalDeviceFormatProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceImageFormatProperties",
            &m_d->GetPhysicalDeviceImageFormatProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceProperties", &m_d->GetPhysicalDeviceProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceQueueFamilyProperties",
            &m_d->GetPhysicalDeviceQueueFamilyProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceMemoryProperties", &m_d->GetPhysicalDeviceMemoryProperties);
    resolve(getProcAddr, instance, "vkGetDeviceProcAddr", &m_d->GetDeviceProcAddr);
    resolve(getProcAddr, instance, "vkCreateDevice", &m_d->CreateDevice);
    resolve(getProcAddr, instance, "vkEnumerateDeviceExtensionProperties", &m_d->EnumerateDeviceExtensionProperties);
    resolve(getProcAddr, instance, "vkEnumerateDeviceLayerProperties", &m_d->EnumerateDeviceLayerProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceSparseImageFormatProperties",
            &m_d->GetPhysicalDeviceSparseImageFormatProperties);
    resolve(getProcAddr, instance, "vkEnumeratePhysicalDeviceGroups", &m_d->EnumeratePhysicalDeviceGroups);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceFeatures2", &m_d->GetPhysicalDeviceFeatures2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceProperties2", &m_d->GetPhysicalDeviceProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceFormatProperties2", &m_d->GetPhysicalDeviceFormatProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceImageFormatProperties2",
            &m_d->GetPhysicalDeviceImageFormatProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceQueueFamilyProperties2",
            &m_d->GetPhysicalDeviceQueueFamilyProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceMemoryProperties2", &m_d->GetPhysicalDeviceMemoryProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceSparseImageFormatProperties2",
            &m_d->GetPhysicalDeviceSparseImageFormatProperties2);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceExternalBufferProperties",
            &m_d->GetPhysicalDeviceExternalBufferProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceExternalFenceProperties",
            &m_d->GetPhysicalDeviceExternalFenceProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceExternalSemaphoreProperties",
            &m_d->GetPhysicalDeviceExternalSemaphoreProperties);
    resolve(getProcAddr, instance, "vkGetPhysicalDeviceToolProperties", &m_d->GetPhysicalDeviceToolProperties);
}

/*!
    Destroys the functions object. The instance itself is not touched.
*/
QVulkanFunctions::~QVulkanFunctions() = default;

/*!
    Calls the Vulkan function \c vkDestroyInstance with \a instance and \a pAllocator.
*/
void QVulkanFunctions::vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyInstance(instance, pAllocator);
}

/*!
    Calls the Vulkan function \c vkEnumeratePhysicalDevices with \a instance, \a pPhysicalDeviceCount and \a pPhysicalDevices.
*/
VkResult QVulkanFunctions::vkEnumeratePhysicalDevices(VkInstance instance, uint32_t *pPhysicalDeviceCount,
                                                      VkPhysicalDevice *pPhysicalDevices) {
    return m_d->EnumeratePhysicalDevices(instance, pPhysicalDeviceCount, pPhysicalDevices);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceFeatures with \a physicalDevice and \a pFeatures.
*/
void QVulkanFunctions::vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice,
                                                   VkPhysicalDeviceFeatures *pFeatures) {
    m_d->GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceFormatProperties with \a physicalDevice, \a format and \a pFormatProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format,
                                                           VkFormatProperties *pFormatProperties) {
    m_d->GetPhysicalDeviceFormatProperties(physicalDevice, format, pFormatProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceImageFormatProperties with \a physicalDevice, \a format, \a type, \a tiling, \a usage, \a flags and \a pImageFormatProperties.
*/
VkResult QVulkanFunctions::vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format,
                                                                    VkImageType type, VkImageTiling tiling,
                                                                    VkImageUsageFlags usage, VkImageCreateFlags flags,
                                                                    VkImageFormatProperties *pImageFormatProperties) {
    return m_d->GetPhysicalDeviceImageFormatProperties(physicalDevice, format, type, tiling, usage, flags,
                                                       pImageFormatProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceProperties with \a physicalDevice and \a pProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice,
                                                     VkPhysicalDeviceProperties *pProperties) {
    m_d->GetPhysicalDeviceProperties(physicalDevice, pProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceQueueFamilyProperties with \a physicalDevice, \a pQueueFamilyPropertyCount and \a pQueueFamilyProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice,
                                                                uint32_t *pQueueFamilyPropertyCount,
                                                                VkQueueFamilyProperties *pQueueFamilyProperties) {
    m_d->GetPhysicalDeviceQueueFamilyProperties(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceMemoryProperties with \a physicalDevice and \a pMemoryProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice,
                                                           VkPhysicalDeviceMemoryProperties *pMemoryProperties) {
    m_d->GetPhysicalDeviceMemoryProperties(physicalDevice, pMemoryProperties);
}

/*!
    Calls the Vulkan function \c vkGetDeviceProcAddr with \a device and \a pName.
*/
PFN_vkVoidFunction QVulkanFunctions::vkGetDeviceProcAddr(VkDevice device, const char *pName) {
    return m_d->GetDeviceProcAddr(device, pName);
}

/*!
    Calls the Vulkan function \c vkCreateDevice with \a physicalDevice, \a pCreateInfo, \a pAllocator and \a pDevice.
*/
VkResult QVulkanFunctions::vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
                                          const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    return m_d->CreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

/*!
    Calls the Vulkan function \c vkEnumerateDeviceExtensionProperties with \a physicalDevice, \a pLayerName, \a pPropertyCount and \a pProperties.
*/
VkResult QVulkanFunctions::vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice, const char *pLayerName,
                                                                uint32_t *pPropertyCount,
                                                                VkExtensionProperties *pProperties) {
    return m_d->EnumerateDeviceExtensionProperties(physicalDevice, pLayerName, pPropertyCount, pProperties);
}

/*!
    Calls the Vulkan function \c vkEnumerateDeviceLayerProperties with \a physicalDevice, \a pPropertyCount and \a pProperties.
*/
VkResult QVulkanFunctions::vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount,
                                                            VkLayerProperties *pProperties) {
    return m_d->EnumerateDeviceLayerProperties(physicalDevice, pPropertyCount, pProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceSparseImageFormatProperties with \a physicalDevice, \a format, \a type, \a samples, \a usage, \a tiling, \a pPropertyCount and \a pProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format,
                                                                      VkImageType type, VkSampleCountFlagBits samples,
                                                                      VkImageUsageFlags usage, VkImageTiling tiling,
                                                                      uint32_t *pPropertyCount,
                                                                      VkSparseImageFormatProperties *pProperties) {
    m_d->GetPhysicalDeviceSparseImageFormatProperties(physicalDevice, format, type, samples, usage, tiling,
                                                      pPropertyCount, pProperties);
}

/*!
    Calls the Vulkan function \c vkEnumeratePhysicalDeviceGroups with \a instance, \a pPhysicalDeviceGroupCount and \a pPhysicalDeviceGroupProperties.
*/
VkResult
QVulkanFunctions::vkEnumeratePhysicalDeviceGroups(VkInstance instance, uint32_t *pPhysicalDeviceGroupCount,
                                                  VkPhysicalDeviceGroupProperties *pPhysicalDeviceGroupProperties) {
    return m_d->EnumeratePhysicalDeviceGroups(instance, pPhysicalDeviceGroupCount, pPhysicalDeviceGroupProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceFeatures2 with \a physicalDevice and \a pFeatures.
*/
void QVulkanFunctions::vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice,
                                                    VkPhysicalDeviceFeatures2 *pFeatures) {
    m_d->GetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceProperties2 with \a physicalDevice and \a pProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice,
                                                      VkPhysicalDeviceProperties2 *pProperties) {
    m_d->GetPhysicalDeviceProperties2(physicalDevice, pProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceFormatProperties2 with \a physicalDevice, \a format and \a pFormatProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format,
                                                            VkFormatProperties2 *pFormatProperties) {
    m_d->GetPhysicalDeviceFormatProperties2(physicalDevice, format, pFormatProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceImageFormatProperties2 with \a physicalDevice, \a pImageFormatInfo and \a pImageFormatProperties.
*/
VkResult
QVulkanFunctions::vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice,
                                                            const VkPhysicalDeviceImageFormatInfo2 *pImageFormatInfo,
                                                            VkImageFormatProperties2 *pImageFormatProperties) {
    return m_d->GetPhysicalDeviceImageFormatProperties2(physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceQueueFamilyProperties2 with \a physicalDevice, \a pQueueFamilyPropertyCount and \a pQueueFamilyProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice,
                                                                 uint32_t *pQueueFamilyPropertyCount,
                                                                 VkQueueFamilyProperties2 *pQueueFamilyProperties) {
    m_d->GetPhysicalDeviceQueueFamilyProperties2(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceMemoryProperties2 with \a physicalDevice and \a pMemoryProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice,
                                                            VkPhysicalDeviceMemoryProperties2 *pMemoryProperties) {
    m_d->GetPhysicalDeviceMemoryProperties2(physicalDevice, pMemoryProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceSparseImageFormatProperties2 with \a physicalDevice, \a pFormatInfo, \a pPropertyCount and \a pProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceSparseImageFormatProperties2(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2 *pFormatInfo,
    uint32_t *pPropertyCount, VkSparseImageFormatProperties2 *pProperties) {
    m_d->GetPhysicalDeviceSparseImageFormatProperties2(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceExternalBufferProperties with \a physicalDevice, \a pExternalBufferInfo and \a pExternalBufferProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceExternalBufferProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo *pExternalBufferInfo,
    VkExternalBufferProperties *pExternalBufferProperties) {
    m_d->GetPhysicalDeviceExternalBufferProperties(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceExternalFenceProperties with \a physicalDevice, \a pExternalFenceInfo and \a pExternalFenceProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceExternalFenceProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo *pExternalFenceInfo,
    VkExternalFenceProperties *pExternalFenceProperties) {
    m_d->GetPhysicalDeviceExternalFenceProperties(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceExternalSemaphoreProperties with \a physicalDevice, \a pExternalSemaphoreInfo and \a pExternalSemaphoreProperties.
*/
void QVulkanFunctions::vkGetPhysicalDeviceExternalSemaphoreProperties(
    VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo *pExternalSemaphoreInfo,
    VkExternalSemaphoreProperties *pExternalSemaphoreProperties) {
    m_d->GetPhysicalDeviceExternalSemaphoreProperties(physicalDevice, pExternalSemaphoreInfo,
                                                      pExternalSemaphoreProperties);
}

/*!
    Calls the Vulkan function \c vkGetPhysicalDeviceToolProperties with \a physicalDevice, \a pToolCount and \a pToolProperties.
*/
VkResult QVulkanFunctions::vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t *pToolCount,
                                                             VkPhysicalDeviceToolProperties *pToolProperties) {
    return m_d->GetPhysicalDeviceToolProperties(physicalDevice, pToolCount, pToolProperties);
}

/*!
    \class QVulkanDeviceFunctions
    \inmodule NtGui
    \since 6.0
    \brief The QVulkanDeviceFunctions class provides cross-platform access to the device level core Vulkan 1.0 to 1.3 API.

    Each function calls the Vulkan function of the same name through the function pointer that
    \c vkGetDeviceProcAddr returned for the device, which skips the loader's dispatch. Get the object with
    QVulkanInstance::deviceFunctions(); it is owned by the instance and lives until
    QVulkanInstance::resetDeviceFunctions() is called for the device, or the instance is destroyed.
    Functions of a Vulkan version newer than the device's are null and must not be called.

    \differences
    The functions are defined out of line, as in QVulkanFunctions.
    \enddifferences

    \sa QVulkanInstance, QVulkanFunctions
*/

struct QVulkanDeviceFunctions::Private {
    PFN_vkDestroyDevice DestroyDevice = nullptr;
    PFN_vkGetDeviceQueue GetDeviceQueue = nullptr;
    PFN_vkQueueSubmit QueueSubmit = nullptr;
    PFN_vkQueueWaitIdle QueueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle DeviceWaitIdle = nullptr;
    PFN_vkAllocateMemory AllocateMemory = nullptr;
    PFN_vkFreeMemory FreeMemory = nullptr;
    PFN_vkMapMemory MapMemory = nullptr;
    PFN_vkUnmapMemory UnmapMemory = nullptr;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges = nullptr;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges = nullptr;
    PFN_vkGetDeviceMemoryCommitment GetDeviceMemoryCommitment = nullptr;
    PFN_vkBindBufferMemory BindBufferMemory = nullptr;
    PFN_vkBindImageMemory BindImageMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements = nullptr;
    PFN_vkGetImageSparseMemoryRequirements GetImageSparseMemoryRequirements = nullptr;
    PFN_vkQueueBindSparse QueueBindSparse = nullptr;
    PFN_vkCreateFence CreateFence = nullptr;
    PFN_vkDestroyFence DestroyFence = nullptr;
    PFN_vkResetFences ResetFences = nullptr;
    PFN_vkGetFenceStatus GetFenceStatus = nullptr;
    PFN_vkWaitForFences WaitForFences = nullptr;
    PFN_vkCreateSemaphore CreateSemaphore = nullptr;
    PFN_vkDestroySemaphore DestroySemaphore = nullptr;
    PFN_vkCreateQueryPool CreateQueryPool = nullptr;
    PFN_vkDestroyQueryPool DestroyQueryPool = nullptr;
    PFN_vkGetQueryPoolResults GetQueryPoolResults = nullptr;
    PFN_vkCreateBuffer CreateBuffer = nullptr;
    PFN_vkDestroyBuffer DestroyBuffer = nullptr;
    PFN_vkCreateImage CreateImage = nullptr;
    PFN_vkDestroyImage DestroyImage = nullptr;
    PFN_vkGetImageSubresourceLayout GetImageSubresourceLayout = nullptr;
    PFN_vkCreateImageView CreateImageView = nullptr;
    PFN_vkDestroyImageView DestroyImageView = nullptr;
    PFN_vkCreateCommandPool CreateCommandPool = nullptr;
    PFN_vkDestroyCommandPool DestroyCommandPool = nullptr;
    PFN_vkResetCommandPool ResetCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers FreeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer BeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer EndCommandBuffer = nullptr;
    PFN_vkResetCommandBuffer ResetCommandBuffer = nullptr;
    PFN_vkCmdCopyBuffer CmdCopyBuffer = nullptr;
    PFN_vkCmdCopyImage CmdCopyImage = nullptr;
    PFN_vkCmdCopyBufferToImage CmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer = nullptr;
    PFN_vkCmdUpdateBuffer CmdUpdateBuffer = nullptr;
    PFN_vkCmdFillBuffer CmdFillBuffer = nullptr;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
    PFN_vkCmdBeginQuery CmdBeginQuery = nullptr;
    PFN_vkCmdEndQuery CmdEndQuery = nullptr;
    PFN_vkCmdResetQueryPool CmdResetQueryPool = nullptr;
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp = nullptr;
    PFN_vkCmdCopyQueryPoolResults CmdCopyQueryPoolResults = nullptr;
    PFN_vkCmdExecuteCommands CmdExecuteCommands = nullptr;
    PFN_vkCreateEvent CreateEvent = nullptr;
    PFN_vkDestroyEvent DestroyEvent = nullptr;
    PFN_vkGetEventStatus GetEventStatus = nullptr;
    PFN_vkSetEvent SetEvent = nullptr;
    PFN_vkResetEvent ResetEvent = nullptr;
    PFN_vkCreateBufferView CreateBufferView = nullptr;
    PFN_vkDestroyBufferView DestroyBufferView = nullptr;
    PFN_vkCreateShaderModule CreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule DestroyShaderModule = nullptr;
    PFN_vkCreatePipelineCache CreatePipelineCache = nullptr;
    PFN_vkDestroyPipelineCache DestroyPipelineCache = nullptr;
    PFN_vkGetPipelineCacheData GetPipelineCacheData = nullptr;
    PFN_vkMergePipelineCaches MergePipelineCaches = nullptr;
    PFN_vkCreateComputePipelines CreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline DestroyPipeline = nullptr;
    PFN_vkCreatePipelineLayout CreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout DestroyPipelineLayout = nullptr;
    PFN_vkCreateSampler CreateSampler = nullptr;
    PFN_vkDestroySampler DestroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout CreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout DestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool CreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool DestroyDescriptorPool = nullptr;
    PFN_vkResetDescriptorPool ResetDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets AllocateDescriptorSets = nullptr;
    PFN_vkFreeDescriptorSets FreeDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets UpdateDescriptorSets = nullptr;
    PFN_vkCmdBindPipeline CmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets CmdBindDescriptorSets = nullptr;
    PFN_vkCmdClearColorImage CmdClearColorImage = nullptr;
    PFN_vkCmdDispatch CmdDispatch = nullptr;
    PFN_vkCmdDispatchIndirect CmdDispatchIndirect = nullptr;
    PFN_vkCmdSetEvent CmdSetEvent = nullptr;
    PFN_vkCmdResetEvent CmdResetEvent = nullptr;
    PFN_vkCmdWaitEvents CmdWaitEvents = nullptr;
    PFN_vkCmdPushConstants CmdPushConstants = nullptr;
    PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines = nullptr;
    PFN_vkCreateFramebuffer CreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer DestroyFramebuffer = nullptr;
    PFN_vkCreateRenderPass CreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass DestroyRenderPass = nullptr;
    PFN_vkGetRenderAreaGranularity GetRenderAreaGranularity = nullptr;
    PFN_vkCmdSetViewport CmdSetViewport = nullptr;
    PFN_vkCmdSetScissor CmdSetScissor = nullptr;
    PFN_vkCmdSetLineWidth CmdSetLineWidth = nullptr;
    PFN_vkCmdSetDepthBias CmdSetDepthBias = nullptr;
    PFN_vkCmdSetBlendConstants CmdSetBlendConstants = nullptr;
    PFN_vkCmdSetDepthBounds CmdSetDepthBounds = nullptr;
    PFN_vkCmdSetStencilCompareMask CmdSetStencilCompareMask = nullptr;
    PFN_vkCmdSetStencilWriteMask CmdSetStencilWriteMask = nullptr;
    PFN_vkCmdSetStencilReference CmdSetStencilReference = nullptr;
    PFN_vkCmdBindIndexBuffer CmdBindIndexBuffer = nullptr;
    PFN_vkCmdBindVertexBuffers CmdBindVertexBuffers = nullptr;
    PFN_vkCmdDraw CmdDraw = nullptr;
    PFN_vkCmdDrawIndexed CmdDrawIndexed = nullptr;
    PFN_vkCmdDrawIndirect CmdDrawIndirect = nullptr;
    PFN_vkCmdDrawIndexedIndirect CmdDrawIndexedIndirect = nullptr;
    PFN_vkCmdBlitImage CmdBlitImage = nullptr;
    PFN_vkCmdClearDepthStencilImage CmdClearDepthStencilImage = nullptr;
    PFN_vkCmdClearAttachments CmdClearAttachments = nullptr;
    PFN_vkCmdResolveImage CmdResolveImage = nullptr;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass = nullptr;
    PFN_vkCmdNextSubpass CmdNextSubpass = nullptr;
    PFN_vkCmdEndRenderPass CmdEndRenderPass = nullptr;
    PFN_vkBindBufferMemory2 BindBufferMemory2 = nullptr;
    PFN_vkBindImageMemory2 BindImageMemory2 = nullptr;
    PFN_vkGetDeviceGroupPeerMemoryFeatures GetDeviceGroupPeerMemoryFeatures = nullptr;
    PFN_vkCmdSetDeviceMask CmdSetDeviceMask = nullptr;
    PFN_vkGetImageMemoryRequirements2 GetImageMemoryRequirements2 = nullptr;
    PFN_vkGetBufferMemoryRequirements2 GetBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageSparseMemoryRequirements2 GetImageSparseMemoryRequirements2 = nullptr;
    PFN_vkTrimCommandPool TrimCommandPool = nullptr;
    PFN_vkGetDeviceQueue2 GetDeviceQueue2 = nullptr;
    PFN_vkCmdDispatchBase CmdDispatchBase = nullptr;
    PFN_vkCreateDescriptorUpdateTemplate CreateDescriptorUpdateTemplate = nullptr;
    PFN_vkDestroyDescriptorUpdateTemplate DestroyDescriptorUpdateTemplate = nullptr;
    PFN_vkUpdateDescriptorSetWithTemplate UpdateDescriptorSetWithTemplate = nullptr;
    PFN_vkGetDescriptorSetLayoutSupport GetDescriptorSetLayoutSupport = nullptr;
    PFN_vkCreateSamplerYcbcrConversion CreateSamplerYcbcrConversion = nullptr;
    PFN_vkDestroySamplerYcbcrConversion DestroySamplerYcbcrConversion = nullptr;
    PFN_vkResetQueryPool ResetQueryPool = nullptr;
    PFN_vkGetSemaphoreCounterValue GetSemaphoreCounterValue = nullptr;
    PFN_vkWaitSemaphores WaitSemaphores = nullptr;
    PFN_vkSignalSemaphore SignalSemaphore = nullptr;
    PFN_vkGetBufferDeviceAddress GetBufferDeviceAddress = nullptr;
    PFN_vkGetBufferOpaqueCaptureAddress GetBufferOpaqueCaptureAddress = nullptr;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddress GetDeviceMemoryOpaqueCaptureAddress = nullptr;
    PFN_vkCmdDrawIndirectCount CmdDrawIndirectCount = nullptr;
    PFN_vkCmdDrawIndexedIndirectCount CmdDrawIndexedIndirectCount = nullptr;
    PFN_vkCreateRenderPass2 CreateRenderPass2 = nullptr;
    PFN_vkCmdBeginRenderPass2 CmdBeginRenderPass2 = nullptr;
    PFN_vkCmdNextSubpass2 CmdNextSubpass2 = nullptr;
    PFN_vkCmdEndRenderPass2 CmdEndRenderPass2 = nullptr;
    PFN_vkCreatePrivateDataSlot CreatePrivateDataSlot = nullptr;
    PFN_vkDestroyPrivateDataSlot DestroyPrivateDataSlot = nullptr;
    PFN_vkSetPrivateData SetPrivateData = nullptr;
    PFN_vkGetPrivateData GetPrivateData = nullptr;
    PFN_vkCmdPipelineBarrier2 CmdPipelineBarrier2 = nullptr;
    PFN_vkCmdWriteTimestamp2 CmdWriteTimestamp2 = nullptr;
    PFN_vkQueueSubmit2 QueueSubmit2 = nullptr;
    PFN_vkCmdCopyBuffer2 CmdCopyBuffer2 = nullptr;
    PFN_vkCmdCopyImage2 CmdCopyImage2 = nullptr;
    PFN_vkCmdCopyBufferToImage2 CmdCopyBufferToImage2 = nullptr;
    PFN_vkCmdCopyImageToBuffer2 CmdCopyImageToBuffer2 = nullptr;
    PFN_vkGetDeviceBufferMemoryRequirements GetDeviceBufferMemoryRequirements = nullptr;
    PFN_vkGetDeviceImageMemoryRequirements GetDeviceImageMemoryRequirements = nullptr;
    PFN_vkGetDeviceImageSparseMemoryRequirements GetDeviceImageSparseMemoryRequirements = nullptr;
    PFN_vkCmdSetEvent2 CmdSetEvent2 = nullptr;
    PFN_vkCmdResetEvent2 CmdResetEvent2 = nullptr;
    PFN_vkCmdWaitEvents2 CmdWaitEvents2 = nullptr;
    PFN_vkCmdBlitImage2 CmdBlitImage2 = nullptr;
    PFN_vkCmdResolveImage2 CmdResolveImage2 = nullptr;
    PFN_vkCmdBeginRendering CmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering CmdEndRendering = nullptr;
    PFN_vkCmdSetCullMode CmdSetCullMode = nullptr;
    PFN_vkCmdSetFrontFace CmdSetFrontFace = nullptr;
    PFN_vkCmdSetPrimitiveTopology CmdSetPrimitiveTopology = nullptr;
    PFN_vkCmdSetViewportWithCount CmdSetViewportWithCount = nullptr;
    PFN_vkCmdSetScissorWithCount CmdSetScissorWithCount = nullptr;
    PFN_vkCmdBindVertexBuffers2 CmdBindVertexBuffers2 = nullptr;
    PFN_vkCmdSetDepthTestEnable CmdSetDepthTestEnable = nullptr;
    PFN_vkCmdSetDepthWriteEnable CmdSetDepthWriteEnable = nullptr;
    PFN_vkCmdSetDepthCompareOp CmdSetDepthCompareOp = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnable CmdSetDepthBoundsTestEnable = nullptr;
    PFN_vkCmdSetStencilTestEnable CmdSetStencilTestEnable = nullptr;
    PFN_vkCmdSetStencilOp CmdSetStencilOp = nullptr;
    PFN_vkCmdSetRasterizerDiscardEnable CmdSetRasterizerDiscardEnable = nullptr;
    PFN_vkCmdSetDepthBiasEnable CmdSetDepthBiasEnable = nullptr;
    PFN_vkCmdSetPrimitiveRestartEnable CmdSetPrimitiveRestartEnable = nullptr;
};

// Resolves the device-level functions of `device` through `getProcAddr`. Functions of a Vulkan version newer than
// the device's are left null.
QVulkanDeviceFunctions::QVulkanDeviceFunctions(PFN_vkGetDeviceProcAddr getProcAddr, VkDevice device)
    : m_d(new Private) {
    resolve(getProcAddr, device, "vkDestroyDevice", &m_d->DestroyDevice);
    resolve(getProcAddr, device, "vkGetDeviceQueue", &m_d->GetDeviceQueue);
    resolve(getProcAddr, device, "vkQueueSubmit", &m_d->QueueSubmit);
    resolve(getProcAddr, device, "vkQueueWaitIdle", &m_d->QueueWaitIdle);
    resolve(getProcAddr, device, "vkDeviceWaitIdle", &m_d->DeviceWaitIdle);
    resolve(getProcAddr, device, "vkAllocateMemory", &m_d->AllocateMemory);
    resolve(getProcAddr, device, "vkFreeMemory", &m_d->FreeMemory);
    resolve(getProcAddr, device, "vkMapMemory", &m_d->MapMemory);
    resolve(getProcAddr, device, "vkUnmapMemory", &m_d->UnmapMemory);
    resolve(getProcAddr, device, "vkFlushMappedMemoryRanges", &m_d->FlushMappedMemoryRanges);
    resolve(getProcAddr, device, "vkInvalidateMappedMemoryRanges", &m_d->InvalidateMappedMemoryRanges);
    resolve(getProcAddr, device, "vkGetDeviceMemoryCommitment", &m_d->GetDeviceMemoryCommitment);
    resolve(getProcAddr, device, "vkBindBufferMemory", &m_d->BindBufferMemory);
    resolve(getProcAddr, device, "vkBindImageMemory", &m_d->BindImageMemory);
    resolve(getProcAddr, device, "vkGetBufferMemoryRequirements", &m_d->GetBufferMemoryRequirements);
    resolve(getProcAddr, device, "vkGetImageMemoryRequirements", &m_d->GetImageMemoryRequirements);
    resolve(getProcAddr, device, "vkGetImageSparseMemoryRequirements", &m_d->GetImageSparseMemoryRequirements);
    resolve(getProcAddr, device, "vkQueueBindSparse", &m_d->QueueBindSparse);
    resolve(getProcAddr, device, "vkCreateFence", &m_d->CreateFence);
    resolve(getProcAddr, device, "vkDestroyFence", &m_d->DestroyFence);
    resolve(getProcAddr, device, "vkResetFences", &m_d->ResetFences);
    resolve(getProcAddr, device, "vkGetFenceStatus", &m_d->GetFenceStatus);
    resolve(getProcAddr, device, "vkWaitForFences", &m_d->WaitForFences);
    resolve(getProcAddr, device, "vkCreateSemaphore", &m_d->CreateSemaphore);
    resolve(getProcAddr, device, "vkDestroySemaphore", &m_d->DestroySemaphore);
    resolve(getProcAddr, device, "vkCreateQueryPool", &m_d->CreateQueryPool);
    resolve(getProcAddr, device, "vkDestroyQueryPool", &m_d->DestroyQueryPool);
    resolve(getProcAddr, device, "vkGetQueryPoolResults", &m_d->GetQueryPoolResults);
    resolve(getProcAddr, device, "vkCreateBuffer", &m_d->CreateBuffer);
    resolve(getProcAddr, device, "vkDestroyBuffer", &m_d->DestroyBuffer);
    resolve(getProcAddr, device, "vkCreateImage", &m_d->CreateImage);
    resolve(getProcAddr, device, "vkDestroyImage", &m_d->DestroyImage);
    resolve(getProcAddr, device, "vkGetImageSubresourceLayout", &m_d->GetImageSubresourceLayout);
    resolve(getProcAddr, device, "vkCreateImageView", &m_d->CreateImageView);
    resolve(getProcAddr, device, "vkDestroyImageView", &m_d->DestroyImageView);
    resolve(getProcAddr, device, "vkCreateCommandPool", &m_d->CreateCommandPool);
    resolve(getProcAddr, device, "vkDestroyCommandPool", &m_d->DestroyCommandPool);
    resolve(getProcAddr, device, "vkResetCommandPool", &m_d->ResetCommandPool);
    resolve(getProcAddr, device, "vkAllocateCommandBuffers", &m_d->AllocateCommandBuffers);
    resolve(getProcAddr, device, "vkFreeCommandBuffers", &m_d->FreeCommandBuffers);
    resolve(getProcAddr, device, "vkBeginCommandBuffer", &m_d->BeginCommandBuffer);
    resolve(getProcAddr, device, "vkEndCommandBuffer", &m_d->EndCommandBuffer);
    resolve(getProcAddr, device, "vkResetCommandBuffer", &m_d->ResetCommandBuffer);
    resolve(getProcAddr, device, "vkCmdCopyBuffer", &m_d->CmdCopyBuffer);
    resolve(getProcAddr, device, "vkCmdCopyImage", &m_d->CmdCopyImage);
    resolve(getProcAddr, device, "vkCmdCopyBufferToImage", &m_d->CmdCopyBufferToImage);
    resolve(getProcAddr, device, "vkCmdCopyImageToBuffer", &m_d->CmdCopyImageToBuffer);
    resolve(getProcAddr, device, "vkCmdUpdateBuffer", &m_d->CmdUpdateBuffer);
    resolve(getProcAddr, device, "vkCmdFillBuffer", &m_d->CmdFillBuffer);
    resolve(getProcAddr, device, "vkCmdPipelineBarrier", &m_d->CmdPipelineBarrier);
    resolve(getProcAddr, device, "vkCmdBeginQuery", &m_d->CmdBeginQuery);
    resolve(getProcAddr, device, "vkCmdEndQuery", &m_d->CmdEndQuery);
    resolve(getProcAddr, device, "vkCmdResetQueryPool", &m_d->CmdResetQueryPool);
    resolve(getProcAddr, device, "vkCmdWriteTimestamp", &m_d->CmdWriteTimestamp);
    resolve(getProcAddr, device, "vkCmdCopyQueryPoolResults", &m_d->CmdCopyQueryPoolResults);
    resolve(getProcAddr, device, "vkCmdExecuteCommands", &m_d->CmdExecuteCommands);
    resolve(getProcAddr, device, "vkCreateEvent", &m_d->CreateEvent);
    resolve(getProcAddr, device, "vkDestroyEvent", &m_d->DestroyEvent);
    resolve(getProcAddr, device, "vkGetEventStatus", &m_d->GetEventStatus);
    resolve(getProcAddr, device, "vkSetEvent", &m_d->SetEvent);
    resolve(getProcAddr, device, "vkResetEvent", &m_d->ResetEvent);
    resolve(getProcAddr, device, "vkCreateBufferView", &m_d->CreateBufferView);
    resolve(getProcAddr, device, "vkDestroyBufferView", &m_d->DestroyBufferView);
    resolve(getProcAddr, device, "vkCreateShaderModule", &m_d->CreateShaderModule);
    resolve(getProcAddr, device, "vkDestroyShaderModule", &m_d->DestroyShaderModule);
    resolve(getProcAddr, device, "vkCreatePipelineCache", &m_d->CreatePipelineCache);
    resolve(getProcAddr, device, "vkDestroyPipelineCache", &m_d->DestroyPipelineCache);
    resolve(getProcAddr, device, "vkGetPipelineCacheData", &m_d->GetPipelineCacheData);
    resolve(getProcAddr, device, "vkMergePipelineCaches", &m_d->MergePipelineCaches);
    resolve(getProcAddr, device, "vkCreateComputePipelines", &m_d->CreateComputePipelines);
    resolve(getProcAddr, device, "vkDestroyPipeline", &m_d->DestroyPipeline);
    resolve(getProcAddr, device, "vkCreatePipelineLayout", &m_d->CreatePipelineLayout);
    resolve(getProcAddr, device, "vkDestroyPipelineLayout", &m_d->DestroyPipelineLayout);
    resolve(getProcAddr, device, "vkCreateSampler", &m_d->CreateSampler);
    resolve(getProcAddr, device, "vkDestroySampler", &m_d->DestroySampler);
    resolve(getProcAddr, device, "vkCreateDescriptorSetLayout", &m_d->CreateDescriptorSetLayout);
    resolve(getProcAddr, device, "vkDestroyDescriptorSetLayout", &m_d->DestroyDescriptorSetLayout);
    resolve(getProcAddr, device, "vkCreateDescriptorPool", &m_d->CreateDescriptorPool);
    resolve(getProcAddr, device, "vkDestroyDescriptorPool", &m_d->DestroyDescriptorPool);
    resolve(getProcAddr, device, "vkResetDescriptorPool", &m_d->ResetDescriptorPool);
    resolve(getProcAddr, device, "vkAllocateDescriptorSets", &m_d->AllocateDescriptorSets);
    resolve(getProcAddr, device, "vkFreeDescriptorSets", &m_d->FreeDescriptorSets);
    resolve(getProcAddr, device, "vkUpdateDescriptorSets", &m_d->UpdateDescriptorSets);
    resolve(getProcAddr, device, "vkCmdBindPipeline", &m_d->CmdBindPipeline);
    resolve(getProcAddr, device, "vkCmdBindDescriptorSets", &m_d->CmdBindDescriptorSets);
    resolve(getProcAddr, device, "vkCmdClearColorImage", &m_d->CmdClearColorImage);
    resolve(getProcAddr, device, "vkCmdDispatch", &m_d->CmdDispatch);
    resolve(getProcAddr, device, "vkCmdDispatchIndirect", &m_d->CmdDispatchIndirect);
    resolve(getProcAddr, device, "vkCmdSetEvent", &m_d->CmdSetEvent);
    resolve(getProcAddr, device, "vkCmdResetEvent", &m_d->CmdResetEvent);
    resolve(getProcAddr, device, "vkCmdWaitEvents", &m_d->CmdWaitEvents);
    resolve(getProcAddr, device, "vkCmdPushConstants", &m_d->CmdPushConstants);
    resolve(getProcAddr, device, "vkCreateGraphicsPipelines", &m_d->CreateGraphicsPipelines);
    resolve(getProcAddr, device, "vkCreateFramebuffer", &m_d->CreateFramebuffer);
    resolve(getProcAddr, device, "vkDestroyFramebuffer", &m_d->DestroyFramebuffer);
    resolve(getProcAddr, device, "vkCreateRenderPass", &m_d->CreateRenderPass);
    resolve(getProcAddr, device, "vkDestroyRenderPass", &m_d->DestroyRenderPass);
    resolve(getProcAddr, device, "vkGetRenderAreaGranularity", &m_d->GetRenderAreaGranularity);
    resolve(getProcAddr, device, "vkCmdSetViewport", &m_d->CmdSetViewport);
    resolve(getProcAddr, device, "vkCmdSetScissor", &m_d->CmdSetScissor);
    resolve(getProcAddr, device, "vkCmdSetLineWidth", &m_d->CmdSetLineWidth);
    resolve(getProcAddr, device, "vkCmdSetDepthBias", &m_d->CmdSetDepthBias);
    resolve(getProcAddr, device, "vkCmdSetBlendConstants", &m_d->CmdSetBlendConstants);
    resolve(getProcAddr, device, "vkCmdSetDepthBounds", &m_d->CmdSetDepthBounds);
    resolve(getProcAddr, device, "vkCmdSetStencilCompareMask", &m_d->CmdSetStencilCompareMask);
    resolve(getProcAddr, device, "vkCmdSetStencilWriteMask", &m_d->CmdSetStencilWriteMask);
    resolve(getProcAddr, device, "vkCmdSetStencilReference", &m_d->CmdSetStencilReference);
    resolve(getProcAddr, device, "vkCmdBindIndexBuffer", &m_d->CmdBindIndexBuffer);
    resolve(getProcAddr, device, "vkCmdBindVertexBuffers", &m_d->CmdBindVertexBuffers);
    resolve(getProcAddr, device, "vkCmdDraw", &m_d->CmdDraw);
    resolve(getProcAddr, device, "vkCmdDrawIndexed", &m_d->CmdDrawIndexed);
    resolve(getProcAddr, device, "vkCmdDrawIndirect", &m_d->CmdDrawIndirect);
    resolve(getProcAddr, device, "vkCmdDrawIndexedIndirect", &m_d->CmdDrawIndexedIndirect);
    resolve(getProcAddr, device, "vkCmdBlitImage", &m_d->CmdBlitImage);
    resolve(getProcAddr, device, "vkCmdClearDepthStencilImage", &m_d->CmdClearDepthStencilImage);
    resolve(getProcAddr, device, "vkCmdClearAttachments", &m_d->CmdClearAttachments);
    resolve(getProcAddr, device, "vkCmdResolveImage", &m_d->CmdResolveImage);
    resolve(getProcAddr, device, "vkCmdBeginRenderPass", &m_d->CmdBeginRenderPass);
    resolve(getProcAddr, device, "vkCmdNextSubpass", &m_d->CmdNextSubpass);
    resolve(getProcAddr, device, "vkCmdEndRenderPass", &m_d->CmdEndRenderPass);
    resolve(getProcAddr, device, "vkBindBufferMemory2", &m_d->BindBufferMemory2);
    resolve(getProcAddr, device, "vkBindImageMemory2", &m_d->BindImageMemory2);
    resolve(getProcAddr, device, "vkGetDeviceGroupPeerMemoryFeatures", &m_d->GetDeviceGroupPeerMemoryFeatures);
    resolve(getProcAddr, device, "vkCmdSetDeviceMask", &m_d->CmdSetDeviceMask);
    resolve(getProcAddr, device, "vkGetImageMemoryRequirements2", &m_d->GetImageMemoryRequirements2);
    resolve(getProcAddr, device, "vkGetBufferMemoryRequirements2", &m_d->GetBufferMemoryRequirements2);
    resolve(getProcAddr, device, "vkGetImageSparseMemoryRequirements2", &m_d->GetImageSparseMemoryRequirements2);
    resolve(getProcAddr, device, "vkTrimCommandPool", &m_d->TrimCommandPool);
    resolve(getProcAddr, device, "vkGetDeviceQueue2", &m_d->GetDeviceQueue2);
    resolve(getProcAddr, device, "vkCmdDispatchBase", &m_d->CmdDispatchBase);
    resolve(getProcAddr, device, "vkCreateDescriptorUpdateTemplate", &m_d->CreateDescriptorUpdateTemplate);
    resolve(getProcAddr, device, "vkDestroyDescriptorUpdateTemplate", &m_d->DestroyDescriptorUpdateTemplate);
    resolve(getProcAddr, device, "vkUpdateDescriptorSetWithTemplate", &m_d->UpdateDescriptorSetWithTemplate);
    resolve(getProcAddr, device, "vkGetDescriptorSetLayoutSupport", &m_d->GetDescriptorSetLayoutSupport);
    resolve(getProcAddr, device, "vkCreateSamplerYcbcrConversion", &m_d->CreateSamplerYcbcrConversion);
    resolve(getProcAddr, device, "vkDestroySamplerYcbcrConversion", &m_d->DestroySamplerYcbcrConversion);
    resolve(getProcAddr, device, "vkResetQueryPool", &m_d->ResetQueryPool);
    resolve(getProcAddr, device, "vkGetSemaphoreCounterValue", &m_d->GetSemaphoreCounterValue);
    resolve(getProcAddr, device, "vkWaitSemaphores", &m_d->WaitSemaphores);
    resolve(getProcAddr, device, "vkSignalSemaphore", &m_d->SignalSemaphore);
    resolve(getProcAddr, device, "vkGetBufferDeviceAddress", &m_d->GetBufferDeviceAddress);
    resolve(getProcAddr, device, "vkGetBufferOpaqueCaptureAddress", &m_d->GetBufferOpaqueCaptureAddress);
    resolve(getProcAddr, device, "vkGetDeviceMemoryOpaqueCaptureAddress", &m_d->GetDeviceMemoryOpaqueCaptureAddress);
    resolve(getProcAddr, device, "vkCmdDrawIndirectCount", &m_d->CmdDrawIndirectCount);
    resolve(getProcAddr, device, "vkCmdDrawIndexedIndirectCount", &m_d->CmdDrawIndexedIndirectCount);
    resolve(getProcAddr, device, "vkCreateRenderPass2", &m_d->CreateRenderPass2);
    resolve(getProcAddr, device, "vkCmdBeginRenderPass2", &m_d->CmdBeginRenderPass2);
    resolve(getProcAddr, device, "vkCmdNextSubpass2", &m_d->CmdNextSubpass2);
    resolve(getProcAddr, device, "vkCmdEndRenderPass2", &m_d->CmdEndRenderPass2);
    resolve(getProcAddr, device, "vkCreatePrivateDataSlot", &m_d->CreatePrivateDataSlot);
    resolve(getProcAddr, device, "vkDestroyPrivateDataSlot", &m_d->DestroyPrivateDataSlot);
    resolve(getProcAddr, device, "vkSetPrivateData", &m_d->SetPrivateData);
    resolve(getProcAddr, device, "vkGetPrivateData", &m_d->GetPrivateData);
    resolve(getProcAddr, device, "vkCmdPipelineBarrier2", &m_d->CmdPipelineBarrier2);
    resolve(getProcAddr, device, "vkCmdWriteTimestamp2", &m_d->CmdWriteTimestamp2);
    resolve(getProcAddr, device, "vkQueueSubmit2", &m_d->QueueSubmit2);
    resolve(getProcAddr, device, "vkCmdCopyBuffer2", &m_d->CmdCopyBuffer2);
    resolve(getProcAddr, device, "vkCmdCopyImage2", &m_d->CmdCopyImage2);
    resolve(getProcAddr, device, "vkCmdCopyBufferToImage2", &m_d->CmdCopyBufferToImage2);
    resolve(getProcAddr, device, "vkCmdCopyImageToBuffer2", &m_d->CmdCopyImageToBuffer2);
    resolve(getProcAddr, device, "vkGetDeviceBufferMemoryRequirements", &m_d->GetDeviceBufferMemoryRequirements);
    resolve(getProcAddr, device, "vkGetDeviceImageMemoryRequirements", &m_d->GetDeviceImageMemoryRequirements);
    resolve(getProcAddr, device, "vkGetDeviceImageSparseMemoryRequirements",
            &m_d->GetDeviceImageSparseMemoryRequirements);
    resolve(getProcAddr, device, "vkCmdSetEvent2", &m_d->CmdSetEvent2);
    resolve(getProcAddr, device, "vkCmdResetEvent2", &m_d->CmdResetEvent2);
    resolve(getProcAddr, device, "vkCmdWaitEvents2", &m_d->CmdWaitEvents2);
    resolve(getProcAddr, device, "vkCmdBlitImage2", &m_d->CmdBlitImage2);
    resolve(getProcAddr, device, "vkCmdResolveImage2", &m_d->CmdResolveImage2);
    resolve(getProcAddr, device, "vkCmdBeginRendering", &m_d->CmdBeginRendering);
    resolve(getProcAddr, device, "vkCmdEndRendering", &m_d->CmdEndRendering);
    resolve(getProcAddr, device, "vkCmdSetCullMode", &m_d->CmdSetCullMode);
    resolve(getProcAddr, device, "vkCmdSetFrontFace", &m_d->CmdSetFrontFace);
    resolve(getProcAddr, device, "vkCmdSetPrimitiveTopology", &m_d->CmdSetPrimitiveTopology);
    resolve(getProcAddr, device, "vkCmdSetViewportWithCount", &m_d->CmdSetViewportWithCount);
    resolve(getProcAddr, device, "vkCmdSetScissorWithCount", &m_d->CmdSetScissorWithCount);
    resolve(getProcAddr, device, "vkCmdBindVertexBuffers2", &m_d->CmdBindVertexBuffers2);
    resolve(getProcAddr, device, "vkCmdSetDepthTestEnable", &m_d->CmdSetDepthTestEnable);
    resolve(getProcAddr, device, "vkCmdSetDepthWriteEnable", &m_d->CmdSetDepthWriteEnable);
    resolve(getProcAddr, device, "vkCmdSetDepthCompareOp", &m_d->CmdSetDepthCompareOp);
    resolve(getProcAddr, device, "vkCmdSetDepthBoundsTestEnable", &m_d->CmdSetDepthBoundsTestEnable);
    resolve(getProcAddr, device, "vkCmdSetStencilTestEnable", &m_d->CmdSetStencilTestEnable);
    resolve(getProcAddr, device, "vkCmdSetStencilOp", &m_d->CmdSetStencilOp);
    resolve(getProcAddr, device, "vkCmdSetRasterizerDiscardEnable", &m_d->CmdSetRasterizerDiscardEnable);
    resolve(getProcAddr, device, "vkCmdSetDepthBiasEnable", &m_d->CmdSetDepthBiasEnable);
    resolve(getProcAddr, device, "vkCmdSetPrimitiveRestartEnable", &m_d->CmdSetPrimitiveRestartEnable);
}

/*!
    Destroys the functions object. The device itself is not touched.
*/
QVulkanDeviceFunctions::~QVulkanDeviceFunctions() = default;

/*!
    Calls the Vulkan function \c vkDestroyDevice with \a device and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyDevice(VkDevice device, const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyDevice(device, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetDeviceQueue with \a device, \a queueFamilyIndex, \a queueIndex and \a pQueue.
*/
void QVulkanDeviceFunctions::vkGetDeviceQueue(VkDevice device, uint32_t queueFamilyIndex, uint32_t queueIndex,
                                              VkQueue *pQueue) {
    m_d->GetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

/*!
    Calls the Vulkan function \c vkQueueSubmit with \a queue, \a submitCount, \a pSubmits and \a fence.
*/
VkResult QVulkanDeviceFunctions::vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo *pSubmits,
                                               VkFence fence) {
    return m_d->QueueSubmit(queue, submitCount, pSubmits, fence);
}

/*!
    Calls the Vulkan function \c vkQueueWaitIdle with \a queue.
*/
VkResult QVulkanDeviceFunctions::vkQueueWaitIdle(VkQueue queue) { return m_d->QueueWaitIdle(queue); }

/*!
    Calls the Vulkan function \c vkDeviceWaitIdle with \a device.
*/
VkResult QVulkanDeviceFunctions::vkDeviceWaitIdle(VkDevice device) { return m_d->DeviceWaitIdle(device); }

/*!
    Calls the Vulkan function \c vkAllocateMemory with \a device, \a pAllocateInfo, \a pAllocator and \a pMemory.
*/
VkResult QVulkanDeviceFunctions::vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
                                                  const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory) {
    return m_d->AllocateMemory(device, pAllocateInfo, pAllocator, pMemory);
}

/*!
    Calls the Vulkan function \c vkFreeMemory with \a device, \a memory and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkFreeMemory(VkDevice device, VkDeviceMemory memory,
                                          const VkAllocationCallbacks *pAllocator) {
    m_d->FreeMemory(device, memory, pAllocator);
}

/*!
    Calls the Vulkan function \c vkMapMemory with \a device, \a memory, \a offset, \a size, \a flags and \a ppData.
*/
VkResult QVulkanDeviceFunctions::vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset,
                                             VkDeviceSize size, VkMemoryMapFlags flags, void **ppData) {
    return m_d->MapMemory(device, memory, offset, size, flags, ppData);
}

/*!
    Calls the Vulkan function \c vkUnmapMemory with \a device and \a memory.
*/
void QVulkanDeviceFunctions::vkUnmapMemory(VkDevice device, VkDeviceMemory memory) { m_d->UnmapMemory(device, memory); }

/*!
    Calls the Vulkan function \c vkFlushMappedMemoryRanges with \a device, \a memoryRangeCount and \a pMemoryRanges.
*/
VkResult QVulkanDeviceFunctions::vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
                                                           const VkMappedMemoryRange *pMemoryRanges) {
    return m_d->FlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

/*!
    Calls the Vulkan function \c vkInvalidateMappedMemoryRanges with \a device, \a memoryRangeCount and \a pMemoryRanges.
*/
VkResult QVulkanDeviceFunctions::vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount,
                                                                const VkMappedMemoryRange *pMemoryRanges) {
    return m_d->InvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}

/*!
    Calls the Vulkan function \c vkGetDeviceMemoryCommitment with \a device, \a memory and \a pCommittedMemoryInBytes.
*/
void QVulkanDeviceFunctions::vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory,
                                                         VkDeviceSize *pCommittedMemoryInBytes) {
    m_d->GetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}

/*!
    Calls the Vulkan function \c vkBindBufferMemory with \a device, \a buffer, \a memory and \a memoryOffset.
*/
VkResult QVulkanDeviceFunctions::vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                                    VkDeviceSize memoryOffset) {
    return m_d->BindBufferMemory(device, buffer, memory, memoryOffset);
}

/*!
    Calls the Vulkan function \c vkBindImageMemory with \a device, \a image, \a memory and \a memoryOffset.
*/
VkResult QVulkanDeviceFunctions::vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory,
                                                   VkDeviceSize memoryOffset) {
    return m_d->BindImageMemory(device, image, memory, memoryOffset);
}

/*!
    Calls the Vulkan function \c vkGetBufferMemoryRequirements with \a device, \a buffer and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer,
                                                           VkMemoryRequirements *pMemoryRequirements) {
    m_d->GetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetImageMemoryRequirements with \a device, \a image and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetImageMemoryRequirements(VkDevice device, VkImage image,
                                                          VkMemoryRequirements *pMemoryRequirements) {
    m_d->GetImageMemoryRequirements(device, image, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetImageSparseMemoryRequirements with \a device, \a image, \a pSparseMemoryRequirementCount and \a pSparseMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetImageSparseMemoryRequirements(
    VkDevice device, VkImage image, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements *pSparseMemoryRequirements) {
    m_d->GetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkQueueBindSparse with \a queue, \a bindInfoCount, \a pBindInfo and \a fence.
*/
VkResult QVulkanDeviceFunctions::vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount,
                                                   const VkBindSparseInfo *pBindInfo, VkFence fence) {
    return m_d->QueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}

/*!
    Calls the Vulkan function \c vkCreateFence with \a device, \a pCreateInfo, \a pAllocator and \a pFence.
*/
VkResult QVulkanDeviceFunctions::vkCreateFence(VkDevice device, const VkFenceCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator, VkFence *pFence) {
    return m_d->CreateFence(device, pCreateInfo, pAllocator, pFence);
}

/*!
    Calls the Vulkan function \c vkDestroyFence with \a device, \a fence and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyFence(device, fence, pAllocator);
}

/*!
    Calls the Vulkan function \c vkResetFences with \a device, \a fenceCount and \a pFences.
*/
VkResult QVulkanDeviceFunctions::vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences) {
    return m_d->ResetFences(device, fenceCount, pFences);
}

/*!
    Calls the Vulkan function \c vkGetFenceStatus with \a device and \a fence.
*/
VkResult QVulkanDeviceFunctions::vkGetFenceStatus(VkDevice device, VkFence fence) {
    return m_d->GetFenceStatus(device, fence);
}

/*!
    Calls the Vulkan function \c vkWaitForFences with \a device, \a fenceCount, \a pFences, \a waitAll and \a timeout.
*/
VkResult QVulkanDeviceFunctions::vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence *pFences,
                                                 VkBool32 waitAll, uint64_t timeout) {
    return m_d->WaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

/*!
    Calls the Vulkan function \c vkCreateSemaphore with \a device, \a pCreateInfo, \a pAllocator and \a pSemaphore.
*/
VkResult QVulkanDeviceFunctions::vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator, VkSemaphore *pSemaphore) {
    return m_d->CreateSemaphore(device, pCreateInfo, pAllocator, pSemaphore);
}

/*!
    Calls the Vulkan function \c vkDestroySemaphore with \a device, \a semaphore and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroySemaphore(VkDevice device, VkSemaphore semaphore,
                                                const VkAllocationCallbacks *pAllocator) {
    m_d->DestroySemaphore(device, semaphore, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateQueryPool with \a device, \a pCreateInfo, \a pAllocator and \a pQueryPool.
*/
VkResult QVulkanDeviceFunctions::vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator, VkQueryPool *pQueryPool) {
    return m_d->CreateQueryPool(device, pCreateInfo, pAllocator, pQueryPool);
}

/*!
    Calls the Vulkan function \c vkDestroyQueryPool with \a device, \a queryPool and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool,
                                                const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyQueryPool(device, queryPool, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetQueryPoolResults with \a device, \a queryPool, \a firstQuery, \a queryCount, \a dataSize, \a pData, \a stride and \a flags.
*/
VkResult QVulkanDeviceFunctions::vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery,
                                                       uint32_t queryCount, size_t dataSize, void *pData,
                                                       VkDeviceSize stride, VkQueryResultFlags flags) {
    return m_d->GetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}

/*!
    Calls the Vulkan function \c vkCreateBuffer with \a device, \a pCreateInfo, \a pAllocator and \a pBuffer.
*/
VkResult QVulkanDeviceFunctions::vkCreateBuffer(VkDevice device, const VkBufferCreateInfo *pCreateInfo,
                                                const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer) {
    return m_d->CreateBuffer(device, pCreateInfo, pAllocator, pBuffer);
}

/*!
    Calls the Vulkan function \c vkDestroyBuffer with \a device, \a buffer and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyBuffer(VkDevice device, VkBuffer buffer,
                                             const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyBuffer(device, buffer, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateImage with \a device, \a pCreateInfo, \a pAllocator and \a pImage.
*/
VkResult QVulkanDeviceFunctions::vkCreateImage(VkDevice device, const VkImageCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator, VkImage *pImage) {
    return m_d->CreateImage(device, pCreateInfo, pAllocator, pImage);
}

/*!
    Calls the Vulkan function \c vkDestroyImage with \a device, \a image and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyImage(device, image, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetImageSubresourceLayout with \a device, \a image, \a pSubresource and \a pLayout.
*/
void QVulkanDeviceFunctions::vkGetImageSubresourceLayout(VkDevice device, VkImage image,
                                                         const VkImageSubresource *pSubresource,
                                                         VkSubresourceLayout *pLayout) {
    m_d->GetImageSubresourceLayout(device, image, pSubresource, pLayout);
}

/*!
    Calls the Vulkan function \c vkCreateImageView with \a device, \a pCreateInfo, \a pAllocator and \a pView.
*/
VkResult QVulkanDeviceFunctions::vkCreateImageView(VkDevice device, const VkImageViewCreateInfo *pCreateInfo,
                                                   const VkAllocationCallbacks *pAllocator, VkImageView *pView) {
    return m_d->CreateImageView(device, pCreateInfo, pAllocator, pView);
}

/*!
    Calls the Vulkan function \c vkDestroyImageView with \a device, \a imageView and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyImageView(VkDevice device, VkImageView imageView,
                                                const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyImageView(device, imageView, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateCommandPool with \a device, \a pCreateInfo, \a pAllocator and \a pCommandPool.
*/
VkResult QVulkanDeviceFunctions::vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkCommandPool *pCommandPool) {
    return m_d->CreateCommandPool(device, pCreateInfo, pAllocator, pCommandPool);
}

/*!
    Calls the Vulkan function \c vkDestroyCommandPool with \a device, \a commandPool and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyCommandPool(VkDevice device, VkCommandPool commandPool,
                                                  const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyCommandPool(device, commandPool, pAllocator);
}

/*!
    Calls the Vulkan function \c vkResetCommandPool with \a device, \a commandPool and \a flags.
*/
VkResult QVulkanDeviceFunctions::vkResetCommandPool(VkDevice device, VkCommandPool commandPool,
                                                    VkCommandPoolResetFlags flags) {
    return m_d->ResetCommandPool(device, commandPool, flags);
}

/*!
    Calls the Vulkan function \c vkAllocateCommandBuffers with \a device, \a pAllocateInfo and \a pCommandBuffers.
*/
VkResult QVulkanDeviceFunctions::vkAllocateCommandBuffers(VkDevice device,
                                                          const VkCommandBufferAllocateInfo *pAllocateInfo,
                                                          VkCommandBuffer *pCommandBuffers) {
    return m_d->AllocateCommandBuffers(device, pAllocateInfo, pCommandBuffers);
}

/*!
    Calls the Vulkan function \c vkFreeCommandBuffers with \a device, \a commandPool, \a commandBufferCount and \a pCommandBuffers.
*/
void QVulkanDeviceFunctions::vkFreeCommandBuffers(VkDevice device, VkCommandPool commandPool,
                                                  uint32_t commandBufferCount, const VkCommandBuffer *pCommandBuffers) {
    m_d->FreeCommandBuffers(device, commandPool, commandBufferCount, pCommandBuffers);
}

/*!
    Calls the Vulkan function \c vkBeginCommandBuffer with \a commandBuffer and \a pBeginInfo.
*/
VkResult QVulkanDeviceFunctions::vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                      const VkCommandBufferBeginInfo *pBeginInfo) {
    return m_d->BeginCommandBuffer(commandBuffer, pBeginInfo);
}

/*!
    Calls the Vulkan function \c vkEndCommandBuffer with \a commandBuffer.
*/
VkResult QVulkanDeviceFunctions::vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    return m_d->EndCommandBuffer(commandBuffer);
}

/*!
    Calls the Vulkan function \c vkResetCommandBuffer with \a commandBuffer and \a flags.
*/
VkResult QVulkanDeviceFunctions::vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    return m_d->ResetCommandBuffer(commandBuffer, flags);
}

/*!
    Calls the Vulkan function \c vkCmdCopyBuffer with \a commandBuffer, \a srcBuffer, \a dstBuffer, \a regionCount and \a pRegions.
*/
void QVulkanDeviceFunctions::vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer,
                                             uint32_t regionCount, const VkBufferCopy *pRegions) {
    m_d->CmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}

/*!
    Calls the Vulkan function \c vkCmdCopyImage with \a commandBuffer, \a srcImage, \a srcImageLayout, \a dstImage, \a dstImageLayout, \a regionCount and \a pRegions.
*/
void QVulkanDeviceFunctions::vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                            VkImageLayout srcImageLayout, VkImage dstImage,
                                            VkImageLayout dstImageLayout, uint32_t regionCount,
                                            const VkImageCopy *pRegions) {
    m_d->CmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

/*!
    Calls the Vulkan function \c vkCmdCopyBufferToImage with \a commandBuffer, \a srcBuffer, \a dstImage, \a dstImageLayout, \a regionCount and \a pRegions.
*/
void QVulkanDeviceFunctions::vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage,
                                                    VkImageLayout dstImageLayout, uint32_t regionCount,
                                                    const VkBufferImageCopy *pRegions) {
    m_d->CmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}

/*!
    Calls the Vulkan function \c vkCmdCopyImageToBuffer with \a commandBuffer, \a srcImage, \a srcImageLayout, \a dstBuffer, \a regionCount and \a pRegions.
*/
void QVulkanDeviceFunctions::vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage,
                                                    VkImageLayout srcImageLayout, VkBuffer dstBuffer,
                                                    uint32_t regionCount, const VkBufferImageCopy *pRegions) {
    m_d->CmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}

/*!
    Calls the Vulkan function \c vkCmdUpdateBuffer with \a commandBuffer, \a dstBuffer, \a dstOffset, \a dataSize and \a pData.
*/
void QVulkanDeviceFunctions::vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer,
                                               VkDeviceSize dstOffset, VkDeviceSize dataSize, const void *pData) {
    m_d->CmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}

/*!
    Calls the Vulkan function \c vkCmdFillBuffer with \a commandBuffer, \a dstBuffer, \a dstOffset, \a size and \a data.
*/
void QVulkanDeviceFunctions::vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset,
                                             VkDeviceSize size, uint32_t data) {
    m_d->CmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}

/*!
    Calls the Vulkan function \c vkCmdPipelineBarrier with \a commandBuffer, \a srcStageMask, \a dstStageMask, \a dependencyFlags, \a memoryBarrierCount, \a pMemoryBarriers, \a bufferMemoryBarrierCount, \a pBufferMemoryBarriers, \a imageMemoryBarrierCount and \a pImageMemoryBarriers.
*/
void QVulkanDeviceFunctions::vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
                                                  VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
                                                  uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                                  uint32_t bufferMemoryBarrierCount,
                                                  const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                                  uint32_t imageMemoryBarrierCount,
                                                  const VkImageMemoryBarrier *pImageMemoryBarriers) {
    m_d->CmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount,
                            pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount,
                            pImageMemoryBarriers);
}

/*!
    Calls the Vulkan function \c vkCmdBeginQuery with \a commandBuffer, \a queryPool, \a query and \a flags.
*/
void QVulkanDeviceFunctions::vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query,
                                             VkQueryControlFlags flags) {
    m_d->CmdBeginQuery(commandBuffer, queryPool, query, flags);
}

/*!
    Calls the Vulkan function \c vkCmdEndQuery with \a commandBuffer, \a queryPool and \a query.
*/
void QVulkanDeviceFunctions::vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    m_d->CmdEndQuery(commandBuffer, queryPool, query);
}

/*!
    Calls the Vulkan function \c vkCmdResetQueryPool with \a commandBuffer, \a queryPool, \a firstQuery and \a queryCount.
*/
void QVulkanDeviceFunctions::vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                                 uint32_t firstQuery, uint32_t queryCount) {
    m_d->CmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}

/*!
    Calls the Vulkan function \c vkCmdWriteTimestamp with \a commandBuffer, \a pipelineStage, \a queryPool and \a query.
*/
void QVulkanDeviceFunctions::vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage,
                                                 VkQueryPool queryPool, uint32_t query) {
    m_d->CmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}

/*!
    Calls the Vulkan function \c vkCmdCopyQueryPoolResults with \a commandBuffer, \a queryPool, \a firstQuery, \a queryCount, \a dstBuffer, \a dstOffset, \a stride and \a flags.
*/
void QVulkanDeviceFunctions::vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool,
                                                       uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer,
                                                       VkDeviceSize dstOffset, VkDeviceSize stride,
                                                       VkQueryResultFlags flags) {
    m_d->CmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}

/*!
    Calls the Vulkan function \c vkCmdExecuteCommands with \a commandBuffer, \a commandBufferCount and \a pCommandBuffers.
*/
void QVulkanDeviceFunctions::vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount,
                                                  const VkCommandBuffer *pCommandBuffers) {
    m_d->CmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}

/*!
    Calls the Vulkan function \c vkCreateEvent with \a device, \a pCreateInfo, \a pAllocator and \a pEvent.
*/
VkResult QVulkanDeviceFunctions::vkCreateEvent(VkDevice device, const VkEventCreateInfo *pCreateInfo,
                                               const VkAllocationCallbacks *pAllocator, VkEvent *pEvent) {
    return m_d->CreateEvent(device, pCreateInfo, pAllocator, pEvent);
}

/*!
    Calls the Vulkan function \c vkDestroyEvent with \a device, \a event and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyEvent(device, event, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetEventStatus with \a device and \a event.
*/
VkResult QVulkanDeviceFunctions::vkGetEventStatus(VkDevice device, VkEvent event) {
    return m_d->GetEventStatus(device, event);
}

/*!
    Calls the Vulkan function \c vkSetEvent with \a device and \a event.
*/
VkResult QVulkanDeviceFunctions::vkSetEvent(VkDevice device, VkEvent event) { return m_d->SetEvent(device, event); }

/*!
    Calls the Vulkan function \c vkResetEvent with \a device and \a event.
*/
VkResult QVulkanDeviceFunctions::vkResetEvent(VkDevice device, VkEvent event) { return m_d->ResetEvent(device, event); }

/*!
    Calls the Vulkan function \c vkCreateBufferView with \a device, \a pCreateInfo, \a pAllocator and \a pView.
*/
VkResult QVulkanDeviceFunctions::vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator, VkBufferView *pView) {
    return m_d->CreateBufferView(device, pCreateInfo, pAllocator, pView);
}

/*!
    Calls the Vulkan function \c vkDestroyBufferView with \a device, \a bufferView and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyBufferView(VkDevice device, VkBufferView bufferView,
                                                 const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyBufferView(device, bufferView, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateShaderModule with \a device, \a pCreateInfo, \a pAllocator and \a pShaderModule.
*/
VkResult QVulkanDeviceFunctions::vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
                                                      const VkAllocationCallbacks *pAllocator,
                                                      VkShaderModule *pShaderModule) {
    return m_d->CreateShaderModule(device, pCreateInfo, pAllocator, pShaderModule);
}

/*!
    Calls the Vulkan function \c vkDestroyShaderModule with \a device, \a shaderModule and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule,
                                                   const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyShaderModule(device, shaderModule, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreatePipelineCache with \a device, \a pCreateInfo, \a pAllocator and \a pPipelineCache.
*/
VkResult QVulkanDeviceFunctions::vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo *pCreateInfo,
                                                       const VkAllocationCallbacks *pAllocator,
                                                       VkPipelineCache *pPipelineCache) {
    return m_d->CreatePipelineCache(device, pCreateInfo, pAllocator, pPipelineCache);
}

/*!
    Calls the Vulkan function \c vkDestroyPipelineCache with \a device, \a pipelineCache and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache,
                                                    const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyPipelineCache(device, pipelineCache, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetPipelineCacheData with \a device, \a pipelineCache, \a pDataSize and \a pData.
*/
VkResult QVulkanDeviceFunctions::vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache,
                                                        size_t *pDataSize, void *pData) {
    return m_d->GetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}

/*!
    Calls the Vulkan function \c vkMergePipelineCaches with \a device, \a dstCache, \a srcCacheCount and \a pSrcCaches.
*/
VkResult QVulkanDeviceFunctions::vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache,
                                                       uint32_t srcCacheCount, const VkPipelineCache *pSrcCaches) {
    return m_d->MergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}

/*!
    Calls the Vulkan function \c vkCreateComputePipelines with \a device, \a pipelineCache, \a createInfoCount, \a pCreateInfos, \a pAllocator and \a pPipelines.
*/
VkResult QVulkanDeviceFunctions::vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                          uint32_t createInfoCount,
                                                          const VkComputePipelineCreateInfo *pCreateInfos,
                                                          const VkAllocationCallbacks *pAllocator,
                                                          VkPipeline *pPipelines) {
    return m_d->CreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

/*!
    Calls the Vulkan function \c vkDestroyPipeline with \a device, \a pipeline and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyPipeline(VkDevice device, VkPipeline pipeline,
                                               const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyPipeline(device, pipeline, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreatePipelineLayout with \a device, \a pCreateInfo, \a pAllocator and \a pPipelineLayout.
*/
VkResult QVulkanDeviceFunctions::vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
                                                        const VkAllocationCallbacks *pAllocator,
                                                        VkPipelineLayout *pPipelineLayout) {
    return m_d->CreatePipelineLayout(device, pCreateInfo, pAllocator, pPipelineLayout);
}

/*!
    Calls the Vulkan function \c vkDestroyPipelineLayout with \a device, \a pipelineLayout and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout,
                                                     const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyPipelineLayout(device, pipelineLayout, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateSampler with \a device, \a pCreateInfo, \a pAllocator and \a pSampler.
*/
VkResult QVulkanDeviceFunctions::vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *pCreateInfo,
                                                 const VkAllocationCallbacks *pAllocator, VkSampler *pSampler) {
    return m_d->CreateSampler(device, pCreateInfo, pAllocator, pSampler);
}

/*!
    Calls the Vulkan function \c vkDestroySampler with \a device, \a sampler and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroySampler(VkDevice device, VkSampler sampler,
                                              const VkAllocationCallbacks *pAllocator) {
    m_d->DestroySampler(device, sampler, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateDescriptorSetLayout with \a device, \a pCreateInfo, \a pAllocator and \a pSetLayout.
*/
VkResult QVulkanDeviceFunctions::vkCreateDescriptorSetLayout(VkDevice device,
                                                             const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                             const VkAllocationCallbacks *pAllocator,
                                                             VkDescriptorSetLayout *pSetLayout) {
    return m_d->CreateDescriptorSetLayout(device, pCreateInfo, pAllocator, pSetLayout);
}

/*!
    Calls the Vulkan function \c vkDestroyDescriptorSetLayout with \a device, \a descriptorSetLayout and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                                          const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyDescriptorSetLayout(device, descriptorSetLayout, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateDescriptorPool with \a device, \a pCreateInfo, \a pAllocator and \a pDescriptorPool.
*/
VkResult QVulkanDeviceFunctions::vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo,
                                                        const VkAllocationCallbacks *pAllocator,
                                                        VkDescriptorPool *pDescriptorPool) {
    return m_d->CreateDescriptorPool(device, pCreateInfo, pAllocator, pDescriptorPool);
}

/*!
    Calls the Vulkan function \c vkDestroyDescriptorPool with \a device, \a descriptorPool and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                                     const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyDescriptorPool(device, descriptorPool, pAllocator);
}

/*!
    Calls the Vulkan function \c vkResetDescriptorPool with \a device, \a descriptorPool and \a flags.
*/
VkResult QVulkanDeviceFunctions::vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool,
                                                       VkDescriptorPoolResetFlags flags) {
    return m_d->ResetDescriptorPool(device, descriptorPool, flags);
}

/*!
    Calls the Vulkan function \c vkAllocateDescriptorSets with \a device, \a pAllocateInfo and \a pDescriptorSets.
*/
VkResult QVulkanDeviceFunctions::vkAllocateDescriptorSets(VkDevice device,
                                                          const VkDescriptorSetAllocateInfo *pAllocateInfo,
                                                          VkDescriptorSet *pDescriptorSets) {
    return m_d->AllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}

/*!
    Calls the Vulkan function \c vkFreeDescriptorSets with \a device, \a descriptorPool, \a descriptorSetCount and \a pDescriptorSets.
*/
VkResult QVulkanDeviceFunctions::vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool,
                                                      uint32_t descriptorSetCount,
                                                      const VkDescriptorSet *pDescriptorSets) {
    return m_d->FreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}

/*!
    Calls the Vulkan function \c vkUpdateDescriptorSets with \a device, \a descriptorWriteCount, \a pDescriptorWrites, \a descriptorCopyCount and \a pDescriptorCopies.
*/
void QVulkanDeviceFunctions::vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount,
                                                    const VkWriteDescriptorSet *pDescriptorWrites,
                                                    uint32_t descriptorCopyCount,
                                                    const VkCopyDescriptorSet *pDescriptorCopies) {
    m_d->UpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}

/*!
    Calls the Vulkan function \c vkCmdBindPipeline with \a commandBuffer, \a pipelineBindPoint and \a pipeline.
*/
void QVulkanDeviceFunctions::vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
                                               VkPipeline pipeline) {
    m_d->CmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}

/*!
    Calls the Vulkan function \c vkCmdBindDescriptorSets with \a commandBuffer, \a pipelineBindPoint, \a layout, \a firstSet, \a descriptorSetCount, \a pDescriptorSets, \a dynamicOffsetCount and \a pDynamicOffsets.
*/
void QVulkanDeviceFunctions::vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                                                     VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout,
                                                     uint32_t firstSet, uint32_t descriptorSetCount,
                                                     const VkDescriptorSet *pDescriptorSets,
                                                     uint32_t dynamicOffsetCount, const uint32_t *pDynamicOffsets) {
    m_d->CmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets,
                               dynamicOffsetCount, pDynamicOffsets);
}

/*!
    Calls the Vulkan function \c vkCmdClearColorImage with \a commandBuffer, \a image, \a imageLayout, \a pColor, \a rangeCount and \a pRanges.
*/
void QVulkanDeviceFunctions::vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image,
                                                  VkImageLayout imageLayout, const VkClearColorValue *pColor,
                                                  uint32_t rangeCount, const VkImageSubresourceRange *pRanges) {
    m_d->CmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}

/*!
    Calls the Vulkan function \c vkCmdDispatch with \a commandBuffer, \a groupCountX, \a groupCountY and \a groupCountZ.
*/
void QVulkanDeviceFunctions::vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY,
                                           uint32_t groupCountZ) {
    m_d->CmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}

/*!
    Calls the Vulkan function \c vkCmdDispatchIndirect with \a commandBuffer, \a buffer and \a offset.
*/
void QVulkanDeviceFunctions::vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                   VkDeviceSize offset) {
    m_d->CmdDispatchIndirect(commandBuffer, buffer, offset);
}

/*!
    Calls the Vulkan function \c vkCmdSetEvent with \a commandBuffer, \a event and \a stageMask.
*/
void QVulkanDeviceFunctions::vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event,
                                           VkPipelineStageFlags stageMask) {
    m_d->CmdSetEvent(commandBuffer, event, stageMask);
}

/*!
    Calls the Vulkan function \c vkCmdResetEvent with \a commandBuffer, \a event and \a stageMask.
*/
void QVulkanDeviceFunctions::vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event,
                                             VkPipelineStageFlags stageMask) {
    m_d->CmdResetEvent(commandBuffer, event, stageMask);
}

/*!
    Calls the Vulkan function \c vkCmdWaitEvents with \a commandBuffer, \a eventCount, \a pEvents, \a srcStageMask, \a dstStageMask, \a memoryBarrierCount, \a pMemoryBarriers, \a bufferMemoryBarrierCount, \a pBufferMemoryBarriers, \a imageMemoryBarrierCount and \a pImageMemoryBarriers.
*/
void QVulkanDeviceFunctions::vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent *pEvents,
                                             VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                             uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers,
                                             uint32_t bufferMemoryBarrierCount,
                                             const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                                             uint32_t imageMemoryBarrierCount,
                                             const VkImageMemoryBarrier *pImageMemoryBarriers) {
    m_d->CmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount,
                       pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount,
                       pImageMemoryBarriers);
}

/*!
    Calls the Vulkan function \c vkCmdPushConstants with \a commandBuffer, \a layout, \a stageFlags, \a offset, \a size and \a pValues.
*/
void QVulkanDeviceFunctions::vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout,
                                                VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size,
                                                const void *pValues) {
    m_d->CmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}

/*!
    Calls the Vulkan function \c vkCreateGraphicsPipelines with \a device, \a pipelineCache, \a createInfoCount, \a pCreateInfos, \a pAllocator and \a pPipelines.
*/
VkResult QVulkanDeviceFunctions::vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache,
                                                           uint32_t createInfoCount,
                                                           const VkGraphicsPipelineCreateInfo *pCreateInfos,
                                                           const VkAllocationCallbacks *pAllocator,
                                                           VkPipeline *pPipelines) {
    return m_d->CreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

/*!
    Calls the Vulkan function \c vkCreateFramebuffer with \a device, \a pCreateInfo, \a pAllocator and \a pFramebuffer.
*/
VkResult QVulkanDeviceFunctions::vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkFramebuffer *pFramebuffer) {
    return m_d->CreateFramebuffer(device, pCreateInfo, pAllocator, pFramebuffer);
}

/*!
    Calls the Vulkan function \c vkDestroyFramebuffer with \a device, \a framebuffer and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer,
                                                  const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyFramebuffer(device, framebuffer, pAllocator);
}

/*!
    Calls the Vulkan function \c vkCreateRenderPass with \a device, \a pCreateInfo, \a pAllocator and \a pRenderPass.
*/
VkResult QVulkanDeviceFunctions::vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo *pCreateInfo,
                                                    const VkAllocationCallbacks *pAllocator,
                                                    VkRenderPass *pRenderPass) {
    return m_d->CreateRenderPass(device, pCreateInfo, pAllocator, pRenderPass);
}

/*!
    Calls the Vulkan function \c vkDestroyRenderPass with \a device, \a renderPass and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass,
                                                 const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyRenderPass(device, renderPass, pAllocator);
}

/*!
    Calls the Vulkan function \c vkGetRenderAreaGranularity with \a device, \a renderPass and \a pGranularity.
*/
void QVulkanDeviceFunctions::vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass,
                                                        VkExtent2D *pGranularity) {
    m_d->GetRenderAreaGranularity(device, renderPass, pGranularity);
}

/*!
    Calls the Vulkan function \c vkCmdSetViewport with \a commandBuffer, \a firstViewport, \a viewportCount and \a pViewports.
*/
void QVulkanDeviceFunctions::vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport,
                                              uint32_t viewportCount, const VkViewport *pViewports) {
    m_d->CmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}

/*!
    Calls the Vulkan function \c vkCmdSetScissor with \a commandBuffer, \a firstScissor, \a scissorCount and \a pScissors.
*/
void QVulkanDeviceFunctions::vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor,
                                             uint32_t scissorCount, const VkRect2D *pScissors) {
    m_d->CmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}

/*!
    Calls the Vulkan function \c vkCmdSetLineWidth with \a commandBuffer and \a lineWidth.
*/
void QVulkanDeviceFunctions::vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    m_d->CmdSetLineWidth(commandBuffer, lineWidth);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthBias with \a commandBuffer, \a depthBiasConstantFactor, \a depthBiasClamp and \a depthBiasSlopeFactor.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor,
                                               float depthBiasClamp, float depthBiasSlopeFactor) {
    m_d->CmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}

/*!
    Calls the Vulkan function \c vkCmdSetBlendConstants with \a commandBuffer and \a blendConstants.
*/
void QVulkanDeviceFunctions::vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float *blendConstants) {
    m_d->CmdSetBlendConstants(commandBuffer, blendConstants);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthBounds with \a commandBuffer, \a minDepthBounds and \a maxDepthBounds.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds,
                                                 float maxDepthBounds) {
    m_d->CmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}

/*!
    Calls the Vulkan function \c vkCmdSetStencilCompareMask with \a commandBuffer, \a faceMask and \a compareMask.
*/
void QVulkanDeviceFunctions::vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                                        uint32_t compareMask) {
    m_d->CmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}

/*!
    Calls the Vulkan function \c vkCmdSetStencilWriteMask with \a commandBuffer, \a faceMask and \a writeMask.
*/
void QVulkanDeviceFunctions::vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                                      uint32_t writeMask) {
    m_d->CmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}

/*!
    Calls the Vulkan function \c vkCmdSetStencilReference with \a commandBuffer, \a faceMask and \a reference.
*/
void QVulkanDeviceFunctions::vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                                      uint32_t reference) {
    m_d->CmdSetStencilReference(commandBuffer, faceMask, reference);
}

/*!
    Calls the Vulkan function \c vkCmdBindIndexBuffer with \a commandBuffer, \a buffer, \a offset and \a indexType.
*/
void QVulkanDeviceFunctions::vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                                  VkIndexType indexType) {
    m_d->CmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}

/*!
    Calls the Vulkan function \c vkCmdBindVertexBuffers with \a commandBuffer, \a firstBinding, \a bindingCount, \a pBuffers and \a pOffsets.
*/
void QVulkanDeviceFunctions::vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                                    uint32_t bindingCount, const VkBuffer *pBuffers,
                                                    const VkDeviceSize *pOffsets) {
    m_d->CmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}

/*!
    Calls the Vulkan function \c vkCmdDraw with \a commandBuffer, \a vertexCount, \a instanceCount, \a firstVertex and \a firstInstance.
*/
void QVulkanDeviceFunctions::vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount,
                                       uint32_t firstVertex, uint32_t firstInstance) {
    m_d->CmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

/*!
    Calls the Vulkan function \c vkCmdDrawIndexed with \a commandBuffer, \a indexCount, \a instanceCount, \a firstIndex, \a vertexOffset and \a firstInstance.
*/
void QVulkanDeviceFunctions::vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount,
                                              uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                                              uint32_t firstInstance) {
    m_d->CmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

/*!
    Calls the Vulkan function \c vkCmdDrawIndirect with \a commandBuffer, \a buffer, \a offset, \a drawCount and \a stride.
*/
void QVulkanDeviceFunctions::vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                               uint32_t drawCount, uint32_t stride) {
    m_d->CmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

/*!
    Calls the Vulkan function \c vkCmdDrawIndexedIndirect with \a commandBuffer, \a buffer, \a offset, \a drawCount and \a stride.
*/
void QVulkanDeviceFunctions::vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                      VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    m_d->CmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}

/*!
    Calls the Vulkan function \c vkCmdBlitImage with \a commandBuffer, \a srcImage, \a srcImageLayout, \a dstImage, \a dstImageLayout, \a regionCount, \a pRegions and \a filter.
*/
void QVulkanDeviceFunctions::vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                            VkImageLayout srcImageLayout, VkImage dstImage,
                                            VkImageLayout dstImageLayout, uint32_t regionCount,
                                            const VkImageBlit *pRegions, VkFilter filter) {
    m_d->CmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}

/*!
    Calls the Vulkan function \c vkCmdClearDepthStencilImage with \a commandBuffer, \a image, \a imageLayout, \a pDepthStencil, \a rangeCount and \a pRanges.
*/
void QVulkanDeviceFunctions::vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image,
                                                         VkImageLayout imageLayout,
                                                         const VkClearDepthStencilValue *pDepthStencil,
                                                         uint32_t rangeCount, const VkImageSubresourceRange *pRanges) {
    m_d->CmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}

/*!
    Calls the Vulkan function \c vkCmdClearAttachments with \a commandBuffer, \a attachmentCount, \a pAttachments, \a rectCount and \a pRects.
*/
void QVulkanDeviceFunctions::vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount,
                                                   const VkClearAttachment *pAttachments, uint32_t rectCount,
                                                   const VkClearRect *pRects) {
    m_d->CmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}

/*!
    Calls the Vulkan function \c vkCmdResolveImage with \a commandBuffer, \a srcImage, \a srcImageLayout, \a dstImage, \a dstImageLayout, \a regionCount and \a pRegions.
*/
void QVulkanDeviceFunctions::vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                                               VkImageLayout srcImageLayout, VkImage dstImage,
                                               VkImageLayout dstImageLayout, uint32_t regionCount,
                                               const VkImageResolve *pRegions) {
    m_d->CmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}

/*!
    Calls the Vulkan function \c vkCmdBeginRenderPass with \a commandBuffer, \a pRenderPassBegin and \a contents.
*/
void QVulkanDeviceFunctions::vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                                                  const VkRenderPassBeginInfo *pRenderPassBegin,
                                                  VkSubpassContents contents) {
    m_d->CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}

/*!
    Calls the Vulkan function \c vkCmdNextSubpass with \a commandBuffer and \a contents.
*/
void QVulkanDeviceFunctions::vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    m_d->CmdNextSubpass(commandBuffer, contents);
}

/*!
    Calls the Vulkan function \c vkCmdEndRenderPass with \a commandBuffer.
*/
void QVulkanDeviceFunctions::vkCmdEndRenderPass(VkCommandBuffer commandBuffer) { m_d->CmdEndRenderPass(commandBuffer); }

/*!
    Calls the Vulkan function \c vkBindBufferMemory2 with \a device, \a bindInfoCount and \a pBindInfos.
*/
VkResult QVulkanDeviceFunctions::vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount,
                                                     const VkBindBufferMemoryInfo *pBindInfos) {
    return m_d->BindBufferMemory2(device, bindInfoCount, pBindInfos);
}

/*!
    Calls the Vulkan function \c vkBindImageMemory2 with \a device, \a bindInfoCount and \a pBindInfos.
*/
VkResult QVulkanDeviceFunctions::vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount,
                                                    const VkBindImageMemoryInfo *pBindInfos) {
    return m_d->BindImageMemory2(device, bindInfoCount, pBindInfos);
}

/*!
    Calls the Vulkan function \c vkGetDeviceGroupPeerMemoryFeatures with \a device, \a heapIndex, \a localDeviceIndex, \a remoteDeviceIndex and \a pPeerMemoryFeatures.
*/
void QVulkanDeviceFunctions::vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex,
                                                                uint32_t localDeviceIndex, uint32_t remoteDeviceIndex,
                                                                VkPeerMemoryFeatureFlags *pPeerMemoryFeatures) {
    m_d->GetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}

/*!
    Calls the Vulkan function \c vkCmdSetDeviceMask with \a commandBuffer and \a deviceMask.
*/
void QVulkanDeviceFunctions::vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    m_d->CmdSetDeviceMask(commandBuffer, deviceMask);
}

/*!
    Calls the Vulkan function \c vkGetImageMemoryRequirements2 with \a device, \a pInfo and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2 *pInfo,
                                                           VkMemoryRequirements2 *pMemoryRequirements) {
    m_d->GetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetBufferMemoryRequirements2 with \a device, \a pInfo and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetBufferMemoryRequirements2(VkDevice device,
                                                            const VkBufferMemoryRequirementsInfo2 *pInfo,
                                                            VkMemoryRequirements2 *pMemoryRequirements) {
    m_d->GetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetImageSparseMemoryRequirements2 with \a device, \a pInfo, \a pSparseMemoryRequirementCount and \a pSparseMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetImageSparseMemoryRequirements2(
    VkDevice device, const VkImageSparseMemoryRequirementsInfo2 *pInfo, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) {
    m_d->GetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkTrimCommandPool with \a device, \a commandPool and \a flags.
*/
void QVulkanDeviceFunctions::vkTrimCommandPool(VkDevice device, VkCommandPool commandPool,
                                               VkCommandPoolTrimFlags flags) {
    m_d->TrimCommandPool(device, commandPool, flags);
}

/*!
    Calls the Vulkan function \c vkGetDeviceQueue2 with \a device, \a pQueueInfo and \a pQueue.
*/
void QVulkanDeviceFunctions::vkGetDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2 *pQueueInfo, VkQueue *pQueue) {
    m_d->GetDeviceQueue2(device, pQueueInfo, pQueue);
}

/*!
    Calls the Vulkan function \c vkCmdDispatchBase with \a commandBuffer, \a baseGroupX, \a baseGroupY, \a baseGroupZ, \a groupCountX, \a groupCountY and \a groupCountZ.
*/
void QVulkanDeviceFunctions::vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY,
                                               uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY,
                                               uint32_t groupCountZ) {
    m_d->CmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}

/*!
    Calls the Vulkan function \c vkCreateDescriptorUpdateTemplate with \a device, \a pCreateInfo, \a pAllocator and \a pDescriptorUpdateTemplate.
*/
VkResult QVulkanDeviceFunctions::vkCreateDescriptorUpdateTemplate(
    VkDevice device, const VkDescriptorUpdateTemplateCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator,
    VkDescriptorUpdateTemplate *pDescriptorUpdateTemplate) {
    return m_d->CreateDescriptorUpdateTemplate(device, pCreateInfo, pAllocator, pDescriptorUpdateTemplate);
}

/*!
    Calls the Vulkan function \c vkDestroyDescriptorUpdateTemplate with \a device, \a descriptorUpdateTemplate and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyDescriptorUpdateTemplate(VkDevice device,
                                                               VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                               const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, pAllocator);
}

/*!
    Calls the Vulkan function \c vkUpdateDescriptorSetWithTemplate with \a device, \a descriptorSet, \a descriptorUpdateTemplate and \a pData.
*/
void QVulkanDeviceFunctions::vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet,
                                                               VkDescriptorUpdateTemplate descriptorUpdateTemplate,
                                                               const void *pData) {
    m_d->UpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
}

/*!
    Calls the Vulkan function \c vkGetDescriptorSetLayoutSupport with \a device, \a pCreateInfo and \a pSupport.
*/
void QVulkanDeviceFunctions::vkGetDescriptorSetLayoutSupport(VkDevice device,
                                                             const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
                                                             VkDescriptorSetLayoutSupport *pSupport) {
    m_d->GetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}

/*!
    Calls the Vulkan function \c vkCreateSamplerYcbcrConversion with \a device, \a pCreateInfo, \a pAllocator and \a pYcbcrConversion.
*/
VkResult QVulkanDeviceFunctions::vkCreateSamplerYcbcrConversion(VkDevice device,
                                                                const VkSamplerYcbcrConversionCreateInfo *pCreateInfo,
                                                                const VkAllocationCallbacks *pAllocator,
                                                                VkSamplerYcbcrConversion *pYcbcrConversion) {
    return m_d->CreateSamplerYcbcrConversion(device, pCreateInfo, pAllocator, pYcbcrConversion);
}

/*!
    Calls the Vulkan function \c vkDestroySamplerYcbcrConversion with \a device, \a ycbcrConversion and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion,
                                                             const VkAllocationCallbacks *pAllocator) {
    m_d->DestroySamplerYcbcrConversion(device, ycbcrConversion, pAllocator);
}

/*!
    Calls the Vulkan function \c vkResetQueryPool with \a device, \a queryPool, \a firstQuery and \a queryCount.
*/
void QVulkanDeviceFunctions::vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery,
                                              uint32_t queryCount) {
    m_d->ResetQueryPool(device, queryPool, firstQuery, queryCount);
}

/*!
    Calls the Vulkan function \c vkGetSemaphoreCounterValue with \a device, \a semaphore and \a pValue.
*/
VkResult QVulkanDeviceFunctions::vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t *pValue) {
    return m_d->GetSemaphoreCounterValue(device, semaphore, pValue);
}

/*!
    Calls the Vulkan function \c vkWaitSemaphores with \a device, \a pWaitInfo and \a timeout.
*/
VkResult QVulkanDeviceFunctions::vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo *pWaitInfo,
                                                  uint64_t timeout) {
    return m_d->WaitSemaphores(device, pWaitInfo, timeout);
}

/*!
    Calls the Vulkan function \c vkSignalSemaphore with \a device and \a pSignalInfo.
*/
VkResult QVulkanDeviceFunctions::vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo *pSignalInfo) {
    return m_d->SignalSemaphore(device, pSignalInfo);
}

/*!
    Calls the Vulkan function \c vkGetBufferDeviceAddress with \a device and \a pInfo.
*/
VkDeviceAddress QVulkanDeviceFunctions::vkGetBufferDeviceAddress(VkDevice device,
                                                                 const VkBufferDeviceAddressInfo *pInfo) {
    return m_d->GetBufferDeviceAddress(device, pInfo);
}

/*!
    Calls the Vulkan function \c vkGetBufferOpaqueCaptureAddress with \a device and \a pInfo.
*/
uint64_t QVulkanDeviceFunctions::vkGetBufferOpaqueCaptureAddress(VkDevice device,
                                                                 const VkBufferDeviceAddressInfo *pInfo) {
    return m_d->GetBufferOpaqueCaptureAddress(device, pInfo);
}

/*!
    Calls the Vulkan function \c vkGetDeviceMemoryOpaqueCaptureAddress with \a device and \a pInfo.
*/
uint64_t
QVulkanDeviceFunctions::vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device,
                                                              const VkDeviceMemoryOpaqueCaptureAddressInfo *pInfo) {
    return m_d->GetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
}

/*!
    Calls the Vulkan function \c vkCmdDrawIndirectCount with \a commandBuffer, \a buffer, \a offset, \a countBuffer, \a countBufferOffset, \a maxDrawCount and \a stride.
*/
void QVulkanDeviceFunctions::vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset,
                                                    VkBuffer countBuffer, VkDeviceSize countBufferOffset,
                                                    uint32_t maxDrawCount, uint32_t stride) {
    m_d->CmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}

/*!
    Calls the Vulkan function \c vkCmdDrawIndexedIndirectCount with \a commandBuffer, \a buffer, \a offset, \a countBuffer, \a countBufferOffset, \a maxDrawCount and \a stride.
*/
void QVulkanDeviceFunctions::vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer,
                                                           VkDeviceSize offset, VkBuffer countBuffer,
                                                           VkDeviceSize countBufferOffset, uint32_t maxDrawCount,
                                                           uint32_t stride) {
    m_d->CmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount,
                                     stride);
}

/*!
    Calls the Vulkan function \c vkCreateRenderPass2 with \a device, \a pCreateInfo, \a pAllocator and \a pRenderPass.
*/
VkResult QVulkanDeviceFunctions::vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2 *pCreateInfo,
                                                     const VkAllocationCallbacks *pAllocator,
                                                     VkRenderPass *pRenderPass) {
    return m_d->CreateRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
}

/*!
    Calls the Vulkan function \c vkCmdBeginRenderPass2 with \a commandBuffer, \a pRenderPassBegin and \a pSubpassBeginInfo.
*/
void QVulkanDeviceFunctions::vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer,
                                                   const VkRenderPassBeginInfo *pRenderPassBegin,
                                                   const VkSubpassBeginInfo *pSubpassBeginInfo) {
    m_d->CmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}

/*!
    Calls the Vulkan function \c vkCmdNextSubpass2 with \a commandBuffer, \a pSubpassBeginInfo and \a pSubpassEndInfo.
*/
void QVulkanDeviceFunctions::vkCmdNextSubpass2(VkCommandBuffer commandBuffer,
                                               const VkSubpassBeginInfo *pSubpassBeginInfo,
                                               const VkSubpassEndInfo *pSubpassEndInfo) {
    m_d->CmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}

/*!
    Calls the Vulkan function \c vkCmdEndRenderPass2 with \a commandBuffer and \a pSubpassEndInfo.
*/
void QVulkanDeviceFunctions::vkCmdEndRenderPass2(VkCommandBuffer commandBuffer,
                                                 const VkSubpassEndInfo *pSubpassEndInfo) {
    m_d->CmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
}

/*!
    Calls the Vulkan function \c vkCreatePrivateDataSlot with \a device, \a pCreateInfo, \a pAllocator and \a pPrivateDataSlot.
*/
VkResult QVulkanDeviceFunctions::vkCreatePrivateDataSlot(VkDevice device,
                                                         const VkPrivateDataSlotCreateInfo *pCreateInfo,
                                                         const VkAllocationCallbacks *pAllocator,
                                                         VkPrivateDataSlot *pPrivateDataSlot) {
    return m_d->CreatePrivateDataSlot(device, pCreateInfo, pAllocator, pPrivateDataSlot);
}

/*!
    Calls the Vulkan function \c vkDestroyPrivateDataSlot with \a device, \a privateDataSlot and \a pAllocator.
*/
void QVulkanDeviceFunctions::vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot,
                                                      const VkAllocationCallbacks *pAllocator) {
    m_d->DestroyPrivateDataSlot(device, privateDataSlot, pAllocator);
}

/*!
    Calls the Vulkan function \c vkSetPrivateData with \a device, \a objectType, \a objectHandle, \a privateDataSlot and \a data.
*/
VkResult QVulkanDeviceFunctions::vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle,
                                                  VkPrivateDataSlot privateDataSlot, uint64_t data) {
    return m_d->SetPrivateData(device, objectType, objectHandle, privateDataSlot, data);
}

/*!
    Calls the Vulkan function \c vkGetPrivateData with \a device, \a objectType, \a objectHandle, \a privateDataSlot and \a pData.
*/
void QVulkanDeviceFunctions::vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle,
                                              VkPrivateDataSlot privateDataSlot, uint64_t *pData) {
    m_d->GetPrivateData(device, objectType, objectHandle, privateDataSlot, pData);
}

/*!
    Calls the Vulkan function \c vkCmdPipelineBarrier2 with \a commandBuffer and \a pDependencyInfo.
*/
void QVulkanDeviceFunctions::vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer,
                                                   const VkDependencyInfo *pDependencyInfo) {
    m_d->CmdPipelineBarrier2(commandBuffer, pDependencyInfo);
}

/*!
    Calls the Vulkan function \c vkCmdWriteTimestamp2 with \a commandBuffer, \a stage, \a queryPool and \a query.
*/
void QVulkanDeviceFunctions::vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage,
                                                  VkQueryPool queryPool, uint32_t query) {
    m_d->CmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
}

/*!
    Calls the Vulkan function \c vkQueueSubmit2 with \a queue, \a submitCount, \a pSubmits and \a fence.
*/
VkResult QVulkanDeviceFunctions::vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2 *pSubmits,
                                                VkFence fence) {
    return m_d->QueueSubmit2(queue, submitCount, pSubmits, fence);
}

/*!
    Calls the Vulkan function \c vkCmdCopyBuffer2 with \a commandBuffer and \a pCopyBufferInfo.
*/
void QVulkanDeviceFunctions::vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2 *pCopyBufferInfo) {
    m_d->CmdCopyBuffer2(commandBuffer, pCopyBufferInfo);
}

/*!
    Calls the Vulkan function \c vkCmdCopyImage2 with \a commandBuffer and \a pCopyImageInfo.
*/
void QVulkanDeviceFunctions::vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2 *pCopyImageInfo) {
    m_d->CmdCopyImage2(commandBuffer, pCopyImageInfo);
}

/*!
    Calls the Vulkan function \c vkCmdCopyBufferToImage2 with \a commandBuffer and \a pCopyBufferToImageInfo.
*/
void QVulkanDeviceFunctions::vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer,
                                                     const VkCopyBufferToImageInfo2 *pCopyBufferToImageInfo) {
    m_d->CmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
}

/*!
    Calls the Vulkan function \c vkCmdCopyImageToBuffer2 with \a commandBuffer and \a pCopyImageToBufferInfo.
*/
void QVulkanDeviceFunctions::vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer,
                                                     const VkCopyImageToBufferInfo2 *pCopyImageToBufferInfo) {
    m_d->CmdCopyImageToBuffer2(commandBuffer, pCopyImageToBufferInfo);
}

/*!
    Calls the Vulkan function \c vkGetDeviceBufferMemoryRequirements with \a device, \a pInfo and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetDeviceBufferMemoryRequirements(VkDevice device,
                                                                 const VkDeviceBufferMemoryRequirements *pInfo,
                                                                 VkMemoryRequirements2 *pMemoryRequirements) {
    m_d->GetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetDeviceImageMemoryRequirements with \a device, \a pInfo and \a pMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetDeviceImageMemoryRequirements(VkDevice device,
                                                                const VkDeviceImageMemoryRequirements *pInfo,
                                                                VkMemoryRequirements2 *pMemoryRequirements) {
    m_d->GetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkGetDeviceImageSparseMemoryRequirements with \a device, \a pInfo, \a pSparseMemoryRequirementCount and \a pSparseMemoryRequirements.
*/
void QVulkanDeviceFunctions::vkGetDeviceImageSparseMemoryRequirements(
    VkDevice device, const VkDeviceImageMemoryRequirements *pInfo, uint32_t *pSparseMemoryRequirementCount,
    VkSparseImageMemoryRequirements2 *pSparseMemoryRequirements) {
    m_d->GetDeviceImageSparseMemoryRequirements(device, pInfo, pSparseMemoryRequirementCount,
                                                pSparseMemoryRequirements);
}

/*!
    Calls the Vulkan function \c vkCmdSetEvent2 with \a commandBuffer, \a event and \a pDependencyInfo.
*/
void QVulkanDeviceFunctions::vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event,
                                            const VkDependencyInfo *pDependencyInfo) {
    m_d->CmdSetEvent2(commandBuffer, event, pDependencyInfo);
}

/*!
    Calls the Vulkan function \c vkCmdResetEvent2 with \a commandBuffer, \a event and \a stageMask.
*/
void QVulkanDeviceFunctions::vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event,
                                              VkPipelineStageFlags2 stageMask) {
    m_d->CmdResetEvent2(commandBuffer, event, stageMask);
}

/*!
    Calls the Vulkan function \c vkCmdWaitEvents2 with \a commandBuffer, \a eventCount, \a pEvents and \a pDependencyInfos.
*/
void QVulkanDeviceFunctions::vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount,
                                              const VkEvent *pEvents, const VkDependencyInfo *pDependencyInfos) {
    m_d->CmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
}

/*!
    Calls the Vulkan function \c vkCmdBlitImage2 with \a commandBuffer and \a pBlitImageInfo.
*/
void QVulkanDeviceFunctions::vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2 *pBlitImageInfo) {
    m_d->CmdBlitImage2(commandBuffer, pBlitImageInfo);
}

/*!
    Calls the Vulkan function \c vkCmdResolveImage2 with \a commandBuffer and \a pResolveImageInfo.
*/
void QVulkanDeviceFunctions::vkCmdResolveImage2(VkCommandBuffer commandBuffer,
                                                const VkResolveImageInfo2 *pResolveImageInfo) {
    m_d->CmdResolveImage2(commandBuffer, pResolveImageInfo);
}

/*!
    Calls the Vulkan function \c vkCmdBeginRendering with \a commandBuffer and \a pRenderingInfo.
*/
void QVulkanDeviceFunctions::vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo *pRenderingInfo) {
    m_d->CmdBeginRendering(commandBuffer, pRenderingInfo);
}

/*!
    Calls the Vulkan function \c vkCmdEndRendering with \a commandBuffer.
*/
void QVulkanDeviceFunctions::vkCmdEndRendering(VkCommandBuffer commandBuffer) { m_d->CmdEndRendering(commandBuffer); }

/*!
    Calls the Vulkan function \c vkCmdSetCullMode with \a commandBuffer and \a cullMode.
*/
void QVulkanDeviceFunctions::vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    m_d->CmdSetCullMode(commandBuffer, cullMode);
}

/*!
    Calls the Vulkan function \c vkCmdSetFrontFace with \a commandBuffer and \a frontFace.
*/
void QVulkanDeviceFunctions::vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    m_d->CmdSetFrontFace(commandBuffer, frontFace);
}

/*!
    Calls the Vulkan function \c vkCmdSetPrimitiveTopology with \a commandBuffer and \a primitiveTopology.
*/
void QVulkanDeviceFunctions::vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer,
                                                       VkPrimitiveTopology primitiveTopology) {
    m_d->CmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
}

/*!
    Calls the Vulkan function \c vkCmdSetViewportWithCount with \a commandBuffer, \a viewportCount and \a pViewports.
*/
void QVulkanDeviceFunctions::vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount,
                                                       const VkViewport *pViewports) {
    m_d->CmdSetViewportWithCount(commandBuffer, viewportCount, pViewports);
}

/*!
    Calls the Vulkan function \c vkCmdSetScissorWithCount with \a commandBuffer, \a scissorCount and \a pScissors.
*/
void QVulkanDeviceFunctions::vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount,
                                                      const VkRect2D *pScissors) {
    m_d->CmdSetScissorWithCount(commandBuffer, scissorCount, pScissors);
}

/*!
    Calls the Vulkan function \c vkCmdBindVertexBuffers2 with \a commandBuffer, \a firstBinding, \a bindingCount, \a pBuffers, \a pOffsets, \a pSizes and \a pStrides.
*/
void QVulkanDeviceFunctions::vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding,
                                                     uint32_t bindingCount, const VkBuffer *pBuffers,
                                                     const VkDeviceSize *pOffsets, const VkDeviceSize *pSizes,
                                                     const VkDeviceSize *pStrides) {
    m_d->CmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthTestEnable with \a commandBuffer and \a depthTestEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    m_d->CmdSetDepthTestEnable(commandBuffer, depthTestEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthWriteEnable with \a commandBuffer and \a depthWriteEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    m_d->CmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthCompareOp with \a commandBuffer and \a depthCompareOp.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    m_d->CmdSetDepthCompareOp(commandBuffer, depthCompareOp);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthBoundsTestEnable with \a commandBuffer and \a depthBoundsTestEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer,
                                                           VkBool32 depthBoundsTestEnable) {
    m_d->CmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetStencilTestEnable with \a commandBuffer and \a stencilTestEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    m_d->CmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetStencilOp with \a commandBuffer, \a faceMask, \a failOp, \a passOp, \a depthFailOp and \a compareOp.
*/
void QVulkanDeviceFunctions::vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask,
                                               VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp,
                                               VkCompareOp compareOp) {
    m_d->CmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}

/*!
    Calls the Vulkan function \c vkCmdSetRasterizerDiscardEnable with \a commandBuffer and \a rasterizerDiscardEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer,
                                                             VkBool32 rasterizerDiscardEnable) {
    m_d->CmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetDepthBiasEnable with \a commandBuffer and \a depthBiasEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    m_d->CmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
}

/*!
    Calls the Vulkan function \c vkCmdSetPrimitiveRestartEnable with \a commandBuffer and \a primitiveRestartEnable.
*/
void QVulkanDeviceFunctions::vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer,
                                                            VkBool32 primitiveRestartEnable) {
    m_d->CmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
}

#endif // QT_CONFIG(vulkan)
