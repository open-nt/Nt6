// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#if QT_CONFIG(vulkan) || defined(Q_QDOC)

#include <QtCore/qbytearraylist.h>
#include <QtGui/qvulkaninstance.h>
#include <rhi/qrhi.h>

class QWindow;

/*!
    \class QRhiVulkanInitParams
    \brief The QRhiVulkanInitParams struct is the parameters that QRhi::create() takes for the Vulkan backend.

    \code
    QVulkanInstance instance;
    instance.create();
    QRhiVulkanInitParams params;
    params.inst = &instance;
    std::unique_ptr<QRhi> rhi(QRhi::create(QRhi::Vulkan, &params));
    \endcode

    \differences
    The backend renders offscreen only for now: \c window is kept for the swap chains that follow, and is not used.
    \enddifferences

    \sa QRhi
*/
struct QRhiVulkanInitParams : public QRhiInitParams {
    /*! The Vulkan instance, which has to be created and outlive the QRhi. */
    QVulkanInstance *inst = nullptr;
    /*! The window that the backend should be able to present to; unused for now. */
    QWindow *window = nullptr;
    /*! Names of extra device extensions to enable; those that the device lacks are dropped with a warning. */
    QByteArrayList deviceExtensions;
};

/*!
    \class QRhiVulkanNativeHandles
    \brief The QRhiVulkanNativeHandles struct holds the Vulkan objects that a QRhi uses, or that are imported into it.

    \sa QRhi::nativeHandles()
*/
struct QRhiVulkanNativeHandles : public QRhiNativeHandles {
    /*! The physical device. */
    VkPhysicalDevice physDev = VK_NULL_HANDLE;
    /*! The logical device. */
    VkDevice dev = VK_NULL_HANDLE;
    /*! The index of the queue family of the graphics queue. */
    quint32 gfxQueueFamilyIdx = 0;
    /*! The index of the graphics queue in its family. */
    quint32 gfxQueueIdx = 0;
    /*! The graphics queue. */
    VkQueue gfxQueue = VK_NULL_HANDLE;
};

#endif // QT_CONFIG(vulkan)
