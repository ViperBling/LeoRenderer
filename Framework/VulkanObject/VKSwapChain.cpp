#include "VKSwapChain.hpp"

void VulkanSwapChain::InitSurface(void *platformHandle, void *platformWindow)
{
    VkResult result = VK_SUCCESS;

    VkWin32SurfaceCreateInfoKHR surfaceCI {};
    surfaceCI.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceCI.hinstance = (HINSTANCE)platformHandle;
    surfaceCI.hwnd = (HWND)platformWindow;
    result = vkCreateWin32SurfaceKHR(mInstance, &surfaceCI, nullptr, &mSurface);

    if (result != VK_SUCCESS) LeoVK::VKTools::ExitFatal("Could not create surface!", result);

    // Get available queue family properties
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueCount, nullptr);
    assert(queueCount >= 1);
    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++)
    {
        fpGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++)
    {
        if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (graphicsQueueNodeIndex == UINT32_MAX)
            {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE)
            {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX)
    {
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queueCount; ++i)
        {
            if (supportsPresent[i] == VK_TRUE)
            {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
    {
        LeoVK::VKTools::ExitFatal("Could not find a graphics and/or presenting queue!", -1);
    }

    // todo : Add support for separate graphics and presenting queue
    if (graphicsQueueNodeIndex != presentQueueNodeIndex)
    {
        LeoVK::VKTools::ExitFatal("Separate graphics and presenting queues are not supported yet!", -1);
    }

    mQueueNodeIndex = graphicsQueueNodeIndex;

    // Get list of supported surface formats
    uint32_t formatCount;
    VK_CHECK(fpGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, nullptr));
    assert(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VK_CHECK(fpGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &formatCount, surfaceFormats.data()));

    // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
    // there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
    if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
    {
        mFormat = VK_FORMAT_B8G8R8A8_UNORM;
        mColorSpace = surfaceFormats[0].colorSpace;
    }
    else
    {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (auto&& surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                mFormat = surfaceFormat.format;
                mColorSpace = surfaceFormat.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM)
        {
            mFormat = surfaceFormats[0].format;
            mColorSpace = surfaceFormats[0].colorSpace;
        }
    }
}

void VulkanSwapChain::Connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
    this->mInstance = instance;
    this->mPhysicalDevice = physicalDevice;
    this->mDevice = device;
    GET_INSTANCE_PROC_ADDR(mInstance, GetPhysicalDeviceSurfaceSupportKHR);
    GET_INSTANCE_PROC_ADDR(mInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
    GET_INSTANCE_PROC_ADDR(mInstance, GetPhysicalDeviceSurfaceFormatsKHR);
    GET_INSTANCE_PROC_ADDR(mInstance, GetPhysicalDeviceSurfacePresentModesKHR);
    GET_DEVICE_PROC_ADDR(mDevice, CreateSwapchainKHR);
    GET_DEVICE_PROC_ADDR(mDevice, DestroySwapchainKHR);
    GET_DEVICE_PROC_ADDR(mDevice, GetSwapchainImagesKHR);
    GET_DEVICE_PROC_ADDR(mDevice, AcquireNextImageKHR);
    GET_DEVICE_PROC_ADDR(mDevice, QueuePresentKHR);
}

void VulkanSwapChain::Create(uint32_t *width, uint32_t *height, bool vsync, bool fullscreen)
{
    VkSwapchainKHR preSwapChain = mSwapChain;

    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfCaps;
    VK_CHECK(fpGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfCaps));

    // Get available present modes
    uint32_t presentModeCount;
    VK_CHECK(fpGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, nullptr));
    assert(presentModeCount > 0);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(fpGetPhysicalDeviceSurfacePresentModesKHR(mPhysicalDevice, mSurface, &presentModeCount, presentModes.data()));

    // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
    if (surfCaps.currentExtent.width == (uint32_t)-1)
    {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        mExtent.width = *width;
        mExtent.height = *height;
    }
    else
    {
        // If the surface size is defined, the swap chain size must match
        mExtent = surfCaps.currentExtent;
        *width = surfCaps.currentExtent.width;
        *height = surfCaps.currentExtent.height;
    }

    // Select a present mode for the swapchain

    // VK_PRESENT_MODE_FIFO_KHR即开启垂直同步
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR swapChainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!vsync)
    {
        for (size_t i = 0; i < presentModeCount; i++)
        {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                swapChainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if ((swapChainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
            {
                swapChainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desireNumOfSwapChainImages = surfCaps.minImageCount + 1;
    if ((surfCaps.maxImageCount > 0) && (desireNumOfSwapChainImages > surfCaps.maxImageCount))
    {
        desireNumOfSwapChainImages = surfCaps.maxImageCount;
    }

    // Find the transformation of the surface
    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        // We prefer a non-rotated transform
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        preTransform = surfCaps.currentTransform;
    }

    // Find a supported composite alpha format (not all devices support alpha opaque)
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto& compositeAlphaFlag : compositeAlphaFlags)
    {
        if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag)
        {
            compositeAlpha = compositeAlphaFlag;
            break;
        };
    }

    VkSwapchainCreateInfoKHR swapChainCI = {};
    swapChainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCI.pNext = nullptr;
    swapChainCI.surface = mSurface;
    swapChainCI.minImageCount = desireNumOfSwapChainImages;
    swapChainCI.imageFormat = mFormat;
    swapChainCI.imageColorSpace = mColorSpace;
    swapChainCI.imageExtent = { mExtent.width, mExtent.height };
    swapChainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapChainCI.imageArrayLayers = 1;
    swapChainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCI.queueFamilyIndexCount = 0;
    swapChainCI.pQueueFamilyIndices = nullptr;
    swapChainCI.presentMode = swapChainPresentMode;
    swapChainCI.oldSwapchain = preSwapChain;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
    swapChainCI.clipped = VK_TRUE;
    swapChainCI.compositeAlpha = compositeAlpha;

    // Set additional usage flag for blitting from the swapchain images if supported
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, mFormat, &formatProps);
    if ((formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT_KHR) ||
        (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT))
    {
        swapChainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    VK_CHECK(fpCreateSwapchainKHR(mDevice, &swapChainCI, nullptr, &mSwapChain));

    // If an existing swap chain is re-created, destroy the old swap chain
    // This also cleans up all the presentable images
    if (preSwapChain != VK_NULL_HANDLE)
    {
        for (uint32_t i = 0; i < mImageCount; i++)
        {
            vkDestroyImageView(mDevice, mBuffers[i].mView, nullptr);
        }
        fpDestroySwapchainKHR(mDevice, preSwapChain, nullptr);
    }
    VK_CHECK(fpGetSwapchainImagesKHR(mDevice, mSwapChain, &mImageCount, nullptr));

    // Get the swap chain images
    mImages.resize(mImageCount);
    VK_CHECK(fpGetSwapchainImagesKHR(mDevice, mSwapChain, &mImageCount, mImages.data()));

    // Get the swap chain buffers containing the image and imageview
    mBuffers.resize(mImageCount);
    for (uint32_t i = 0; i < mImageCount; i++)
    {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext = nullptr;
        colorAttachmentView.format = mFormat;
        colorAttachmentView.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A
        };
        colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags = 0;

        mBuffers[i].mImage = mImages[i];
        colorAttachmentView.image = mBuffers[i].mImage;

        VK_CHECK(vkCreateImageView(mDevice, &colorAttachmentView, nullptr, &mBuffers[i].mView));
    }
}

VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex)
{
    if (mSwapChain == VK_NULL_HANDLE)
    {
        // Probably acquireNextImage() is called just after cleanup() (e.g. window has been terminated on Android).
        return VK_ERROR_OUT_OF_DATE_KHR;
    }

    // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
    // With that we don't have to handle VK_NOT_READY
    return fpAcquireNextImageKHR(mDevice, mSwapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence)nullptr, imageIndex);
}

VkResult VulkanSwapChain::QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapChain;
    presentInfo.pImageIndices = &imageIndex;
    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (waitSemaphore != VK_NULL_HANDLE)
    {
        presentInfo.pWaitSemaphores = &waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }
    return fpQueuePresentKHR(queue, &presentInfo);
}

void VulkanSwapChain::Cleanup()
{
    if (mSwapChain != VK_NULL_HANDLE)
    {
        for (uint32_t i = 0; i < mImageCount; i++)
        {
            vkDestroyImageView(mDevice, mBuffers[i].mView, nullptr);
        }
    }
    if (mSurface != VK_NULL_HANDLE)
    {
        fpDestroySwapchainKHR(mDevice, mSwapChain, nullptr);
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    }
    mSurface = VK_NULL_HANDLE;
    mSwapChain = VK_NULL_HANDLE;
}
