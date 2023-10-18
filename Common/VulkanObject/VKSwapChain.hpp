#pragma once

#include "ProjectPCH.hpp"

#include "VKTools.hpp"

struct SwapChainBuffer
{
    VkImage     mImage;
    VkImageView mView;
};

class VulkanSwapChain
{
public:
    void InitSurface(void* platformHandle, void* platformWindow);
    void Connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    void Create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen =false);
    VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
    void Cleanup();

public:
    VkFormat                        mFormat;
    VkColorSpaceKHR                 mColorSpace;
    VkSwapchainKHR                  mSwapChain = VK_NULL_HANDLE;
    VkExtent2D                      mExtent;
    uint32_t                        mImageCount;
    uint32_t                        mQueueNodeIndex = UINT32_MAX;
    std::vector<VkImage>            mImages;
    std::vector<SwapChainBuffer>    mBuffers;

private:
    VkInstance          mInstance;
    VkDevice            mDevice;
    VkPhysicalDevice    mPhysicalDevice;
    VkSurfaceKHR        mSurface;

    // Function pointers
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR        fpGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR        fpGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR   fpGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkCreateSwapchainKHR                        fpCreateSwapchainKHR;
    PFN_vkDestroySwapchainKHR                       fpDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR                     fpGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR                       fpAcquireNextImageKHR;
    PFN_vkQueuePresentKHR                           fpQueuePresentKHR;
};