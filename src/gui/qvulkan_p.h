// MIT License - see LICENSE
#pragma once

// The Vulkan loader, opened at run time: Nt GUI does not link with libvulkan, so programs that never create a Vulkan
// instance run where there is none.

#include <QtCore/qglobal.h>

#if QT_CONFIG(vulkan)

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include <cstdint>
#include <type_traits>

namespace QtPrivate::Vulkan {

// The global functions of the loader: what an instance is made with.
struct Loader {
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_vkCreateInstance createInstance = nullptr;
    PFN_vkEnumerateInstanceLayerProperties enumerateInstanceLayerProperties = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties enumerateInstanceExtensionProperties = nullptr;
    PFN_vkEnumerateInstanceVersion enumerateInstanceVersion = nullptr; // null with a Vulkan 1.0 loader
};

// The loader library (libvulkan.so.1, vulkan-1.dll), opened the first time it is asked for; null when there is none.
const Loader *loader();

// Vulkan handles as integers, which is how the platform layer stores a VkSurfaceKHR: non-dispatchable handles are
// pointers on 64-bit targets and 64-bit integers elsewhere.
template <typename Handle> quint64 handleToU64(Handle handle) {
    if constexpr (std::is_pointer_v<Handle>)
        return static_cast<quint64>(reinterpret_cast<std::uintptr_t>(handle));
    else
        return static_cast<quint64>(handle);
}

template <typename Handle> Handle handleFromU64(quint64 value) {
    if constexpr (std::is_pointer_v<Handle>)
        return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(value));
    else
        return static_cast<Handle>(value);
}

} // namespace QtPrivate::Vulkan

#endif // QT_CONFIG(vulkan)
