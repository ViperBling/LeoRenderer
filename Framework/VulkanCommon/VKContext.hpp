#pragma once

#include <vulkan/vulkan.hpp>
#include <vector>
#include <functional>

#include "VKVirtualFrame.hpp"
#include "VKDescriptorCache.hpp"
#include "VKImage.hpp"
#include "VKCommandBuffer.hpp"

namespace LeoVK
{
    struct WindowSurface;

    inline void DefaultVulkanContextCallback(const std::string&) {}

    struct VulkanContextCreateOptions
    {
        int APIMajorVersion = 1;
        int APIMinorVersion = 3;
        std::function<void(const std::string&)> ErrorCallback = DefaultVulkanContextCallback;
        std::function<void(const std::string&)> InfoCallback = DefaultVulkanContextCallback;
        std::vector<const char*> Extensions;
        std::vector<const char*> Layers;
        const char* ApplicationName = "LeoRenderer";
        const char* EngineName = "LeoRenderer";
    };

    enum class DeviceType
    {
        CPU = 0,
        DISCRETE_GPU,
        INTEGRATED_GPU,
        VIRTUAL_GPU,
        OTHER,
    };

    struct ContextInitializeOptions
    {
        DeviceType PreferredDeviceType = DeviceType::DISCRETE_GPU;
        std::function<void(const std::string&)> ErrorCallback = DefaultVulkanContextCallback;
        std::function<void(const std::string&)> InfoCallback = DefaultVulkanContextCallback;
        std::vector<const char*> DeviceExtensions;
        size_t VirtualFrameCount = 3;
        size_t MaxStageBufferSize = 64 * 1024 * 1024;
    };

    class VulkanContext
    {
    public:
        VulkanContext(const VulkanContextCreateOptions& options);
        ~VulkanContext();
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&& other) = delete;
        VulkanContext& operator=(VulkanContext&& other) = delete;

        const vk::Instance& GetInstance() const { return this->mInstance; }
        const vk::SurfaceKHR& GetSurface() const { return this->mSurface; }
        const Format GetSurfaceFormat() const { return FromNative(this->mSurfaceFormat.format); }
        const vk::Extent2D& GetSurfaceExtent() const { return this->mSurfaceExtent; }
        const vk::PhysicalDevice& GetPhysicalDevice() const { return this->mPhysicalDevice; }
        const vk::Device& GetDevice() const { return this->mDevice; }
        const vk::Queue& GetPresentQueue() const { return this->mDeviceQueue; }
        const vk::Queue& GetGraphicsQueue() const { return this->mDeviceQueue; }
        const vk::Semaphore& GetRenderingFinishedSemaphore() const { return this->mRenderingFinishedSemaphore; }
        const vk::Semaphore& GetImageAvailableSemaphore() const { return this->mImageAvailableSemaphore; }
        const vk::SwapchainKHR& GetSwapchain() const { return this->mSwapchain; }
        const vk::CommandPool& GetCommandPool() const { return this->mCommandPool; }
        DescriptorCache& GetDescriptorCache() { return this->mDescriptorCache; }
        uint32_t GetQueueFamilyIndex() const { return this->mQueueFamilyIndex; }
        uint32_t GetPresentImageCount() const { return this->mPresentImageCount; }
        uint32_t GetAPIVersion() const { return this->mAPIVersion; }
        const VmaAllocator& GetAllocator() const { return this->mAllocator; }
        bool IsRenderingEnabled() const { return this->mRenderingEnabled; }
        size_t GetVirtualFrameCount() const { return this->mVirtualFrames.GetFrameCount(); }

        const Image& AcquireSwapchainImage(size_t index, ImageUsage::Bits usage);
        ImageUsage::Bits GetSwapchainImageUsage(size_t index) const;
        void InitializeContext(const WindowSurface& surface, const ContextInitializeOptions& options);
        void RecreateSwapchain(uint32_t surfaceWidth, uint32_t surfaceHeight);
        void StartFrame();
        bool IsFrameRunning() const;
        const Image& AcquireCurrentSwapchainImage(ImageUsage::Bits usage);
        CommandBuffer& GetCurrentCommandBuffer();
        StageBuffer& GetCurrentStageBuffer();
        void SubmitCommandsImmediate(const CommandBuffer& commands);
        CommandBuffer& GetImmediateCommandBuffer();
        void EndFrame();

    private:
        vk::Instance                    mInstance;
        vk::SurfaceKHR                  mSurface;
        vk::SurfaceFormatKHR            mSurfaceFormat;
        vk::PresentModeKHR              mPresentMode = { };
        vk::Extent2D                    mSurfaceExtent;
        uint32_t                        mPresentImageCount = { };
        vk::PhysicalDevice              mPhysicalDevice;
        vk::PhysicalDeviceProperties    mPhysicalDeviceProps;
        vk::Device                      mDevice;
        vk::Queue                       mDeviceQueue;
        vk::Semaphore                   mImageAvailableSemaphore;
        vk::Semaphore                   mRenderingFinishedSemaphore;
        vk::Fence                       mImmediateFence;
        vk::CommandPool                 mCommandPool;
        CommandBuffer                   mImmediateCommandBuffer{ { } };
        vk::SwapchainKHR                mSwapchain;
        vk::DebugUtilsMessengerEXT      mDebugUtilsMessenger;
        vk::DispatchLoaderDynamic       mDynamicLoader;
        VmaAllocator                    mAllocator = { };
        std::vector<Image>              mSwapchainImages;
        std::vector<ImageUsage::Bits>   mSwapchainImageUsages;
        VirtualFrameProvider            mVirtualFrames;
        DescriptorCache                 mDescriptorCache;
        uint32_t                        mQueueFamilyIndex = { };
        uint32_t                        mAPIVersion = { };
        bool                            mRenderingEnabled = true;
    };

    VulkanContext& GetCurrentVulkanContext();
    void SetCurrentVulkanContext(VulkanContext& context);
}