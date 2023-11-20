#include "VKContext.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <iostream>

#include <vulkan/vk_mem_alloc.h>
#include <glslang/Public/ShaderLang.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace LeoVK
{
    // Vulkan Surface
    struct WindowSurface
    {
        vk::SurfaceKHR Surface;
    };

    const WindowSurface& CreateVulkanSurface(GLFWwindow* window, const VulkanContext& context)
    {
        static WindowSurface result;
        (void)glfwCreateWindowSurface(context.GetInstance(), window, nullptr, (VkSurfaceKHR*)&result.Surface);
        return result;
    }

    bool CheckVulkanPresentationSupport(const vk::Instance& instance, const vk::PhysicalDevice& physicalDevice, uint32_t familyQueueIndex)
    {
        return glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, familyQueueIndex) == GLFW_TRUE;
    }

    // Vulkan Context
    static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) 
    {
        std::cerr << pCallbackData->pMessage << std::endl;
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            #if defined(WIN32)
            __debugbreak();
            #endif
        }
        return VK_FALSE;
    }

    constexpr vk::PhysicalDeviceType DeviceTypeMapping[] = {
        vk::PhysicalDeviceType::eCpu,
        vk::PhysicalDeviceType::eDiscreteGpu,
        vk::PhysicalDeviceType::eIntegratedGpu,
        vk::PhysicalDeviceType::eVirtualGpu,
        vk::PhysicalDeviceType::eOther,
    };

    void CheckRequestedLayers(const VulkanContextCreateOptions& options)
    {
        options.InfoCallback("enumerating requested layers:");

        auto layers = vk::enumerateInstanceLayerProperties();
        for (const char* layerName : options.Layers)
        {
            options.InfoCallback("- " + std::string(layerName));

            auto layerIt = std::find_if(layers.begin(), layers.end(),
                [layerName](const vk::LayerProperties& layer)
                {
                    return std::strcmp(layer.layerName.data(), layerName) == 0;
                });

            if (layerIt == layers.end())
            {
                options.ErrorCallback(("cannot enable requested layer: " + std::string(layerName)).c_str());
                return;
            }
        }
    }

    void CheckRequestedExtensions(const VulkanContextCreateOptions& options)
    {
        options.InfoCallback("enumerating requested extensions:");

        auto extensions = vk::enumerateInstanceExtensionProperties();
        for (const char* extensionName : options.Extensions)
        {
            options.InfoCallback("- " + std::string(extensionName));

            auto layerIt = std::find_if(extensions.begin(), extensions.end(),
                [extensionName](const vk::ExtensionProperties& extension)
                {
                    return std::strcmp(extension.extensionName.data(), extensionName) == 0;
                });

            if (layerIt == extensions.end())
            {
                options.ErrorCallback("cannot enable requested extension");
                return;
            }
        }
    }

    std::optional<uint32_t> DetermineQueueFamilyIndex(const vk::Instance& instance, const vk::PhysicalDevice device, const vk::SurfaceKHR& surface)
    {
        auto queueFamilyProperties = device.getQueueFamilyProperties();
        uint32_t index = 0;
        for (const auto& property : queueFamilyProperties)
        {
            if ((property.queueCount > 0) &&
                (device.getSurfaceSupportKHR(index, surface)) &&
                (CheckVulkanPresentationSupport(instance, device, index)) &&
                (property.queueFlags & vk::QueueFlagBits::eGraphics) &&
                (property.queueFlags & vk::QueueFlagBits::eCompute))
            {
                return index;
            }
            index++;
        }
        return { };
    }

    VulkanContext::VulkanContext(const VulkanContextCreateOptions &options)
    {
        vk::ApplicationInfo appInfo;
        appInfo.pApplicationName = options.ApplicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 1, 0);
        appInfo.pEngineName = options.EngineName;
        appInfo.engineVersion = VK_MAKE_VERSION(1, 1, 0);
        appInfo.apiVersion = VK_MAKE_VERSION(options.APIMajorVersion, options.APIMinorVersion, 0);
        
        auto extensions = options.Extensions;
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        vk::InstanceCreateInfo instanceCI {};
        instanceCI
            .setPApplicationInfo(&appInfo)
            .setPEnabledExtensionNames(extensions)
            .setPEnabledLayerNames(options.Layers);
        
        CheckRequestedExtensions(options);
        CheckRequestedLayers(options);

        this->mInstance = vk::createInstance(instanceCI);
        this->mAPIVersion = appInfo.apiVersion;
        options.InfoCallback("created vulkan instance");
    }

    VulkanContext::~VulkanContext()
    {
        this->mDevice.waitIdle();
        this->mVirtualFrames.Destroy();
        this->mDescriptorCache.Destroy();

        if ((bool)this->mCommandPool) this->mDevice.destroyCommandPool(this->mCommandPool);

        this->mSwapchainImages.clear();

        vmaDestroyAllocator(this->mAllocator);

        glslang::FinalizeProcess();

        if ((bool)this->mSwapchain) this->mDevice.destroySwapchainKHR(mSwapchain);
        if ((bool)this->mImageAvailableSemaphore) this->mDevice.destroySemaphore(this->mImageAvailableSemaphore);
        if ((bool)this->mRenderingFinishedSemaphore) this->mDevice.destroySemaphore(this->mRenderingFinishedSemaphore);
        if ((bool)this->mImmediateFence) this->mDevice.destroyFence(this->mImmediateFence);
        if ((bool)this->mDevice) this->mDevice.destroy();
        if ((bool)this->mDebugUtilsMessenger) this->mInstance.destroyDebugUtilsMessengerEXT(this->mDebugUtilsMessenger, nullptr, this->mDynamicLoader);
        if ((bool)this->mSurface) this->mInstance.destroySurfaceKHR(this->mSurface);
        if ((bool)this->mInstance) this->mInstance.destroy();
        this->mPresentImageCount = { };
        this->mQueueFamilyIndex = { };
        this->mAPIVersion = { };
    }

    const Image &VulkanContext::AcquireSwapchainImage(size_t index, ImageUsage::Bits usage)
    {
        this->mSwapchainImageUsages[index] = usage;
        return this->mSwapchainImages[index];
    }

    ImageUsage::Bits VulkanContext::GetSwapchainImageUsage(size_t index) const
    {
        return this->mSwapchainImageUsages[index];
    }

    void VulkanContext::InitializeContext(const WindowSurface &surface, const ContextInitializeOptions &options)
    {
        this->mSurface = surface.Surface;

        if (!this->mSurface)
        {
            options.ErrorCallback("cannot create vulkan surface");
            return;
        }
        options.InfoCallback("Enumerating physical devices:");

        // 枚举物理设备
        auto physicalDevices = this->mInstance.enumeratePhysicalDevices();
        for (const auto& pd : physicalDevices)
        {
            auto props = pd.getProperties();
            options.InfoCallback("- " + std::string(props.deviceName.data()) + " (type: " + std::to_string((int)props.deviceType) + ")");

            if (props.apiVersion < this->mAPIVersion)
            {
                options.InfoCallback(std::string(props.deviceName.data()) + ": skipping device as its Vulkan API version is less than required");
                options.InfoCallback("    " +
                    std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_MINOR(props.apiVersion)) + " < " +
                    std::to_string(VK_VERSION_MAJOR(this->mAPIVersion)) + "." + std::to_string(VK_VERSION_MINOR(this->mAPIVersion))
                );
                continue;
            }

            auto queueFamilyIndex = DetermineQueueFamilyIndex(this->mInstance, pd, this->mSurface);
            if (!queueFamilyIndex.has_value())
            {
                options.InfoCallback(std::string(props.deviceName.data()) + ": skipping device as it does not support required queue family");
                continue;
            }
            this->mPhysicalDevice = pd;
            this->mPhysicalDeviceProps = props;
            this->mQueueFamilyIndex = queueFamilyIndex.value();
            if (props.deviceType == DeviceTypeMapping[(size_t)options.PreferredDeviceType]) break;
        }

        if (!this->mPhysicalDevice)
        {
            options.ErrorCallback("Cannot find suitable physical device");
            return;
        }
        else
        {
            options.InfoCallback("Selected physical device: " + std::string(this->mPhysicalDeviceProps.deviceName.data()));
        }

        // Surface present info
        auto presentModes = this->mPhysicalDevice.getSurfacePresentModesKHR(this->mSurface);
        auto surfaceCapabilities = this->mPhysicalDevice.getSurfaceCapabilitiesKHR(this->mSurface);
        auto surfaceFormats = this->mPhysicalDevice.getSurfaceFormatsKHR(this->mSurface);

        // Find best present mode
        this->mPresentMode = vk::PresentModeKHR::eImmediate;
        if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end())
        {
            this->mPresentMode = vk::PresentModeKHR::eMailbox;
        }
        // Determine present image count
        this->mPresentImageCount = std::max(surfaceCapabilities.maxImageCount, 1u);
        // Find best surface format
        this->mSurfaceFormat = surfaceFormats.front();
        for (const auto & fmt : surfaceFormats)
        {
            if (fmt.format == vk::Format::eR8G8B8A8Unorm || fmt.format == vk::Format::eB8G8R8A8Unorm)
            {
                this->mSurfaceFormat = fmt;
                break;
            }
        }

        options.InfoCallback("Selected surface format: " + std::to_string((int)this->mSurfaceFormat.format));
        options.InfoCallback("Selected present mode: " + std::to_string((int)this->mPresentMode));
        options.InfoCallback("Selected present image count: " + std::to_string(this->mPresentImageCount));

        // Logical device and device queue
        vk::DeviceQueueCreateInfo queueCI;
        std::array queueProps = {1.0f};
        queueCI.setQueueFamilyIndex(this->mQueueFamilyIndex);
        queueCI.setQueuePriorities(queueProps);

        auto deviceExtensions = options.DeviceExtensions;
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        deviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

        vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descIndexingFeatures;
        descIndexingFeatures.descriptorBindingPartiallyBound                    = true;
        descIndexingFeatures.shaderInputAttachmentArrayDynamicIndexing          = true;
        descIndexingFeatures.shaderUniformTexelBufferArrayDynamicIndexing       = true;
        descIndexingFeatures.shaderStorageTexelBufferArrayDynamicIndexing       = true;
        descIndexingFeatures.shaderUniformBufferArrayNonUniformIndexing         = true;
        descIndexingFeatures.shaderSampledImageArrayNonUniformIndexing          = true;
        descIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing         = true;
        descIndexingFeatures.shaderStorageImageArrayNonUniformIndexing          = true;
        descIndexingFeatures.shaderInputAttachmentArrayNonUniformIndexing       = true;
        descIndexingFeatures.shaderUniformTexelBufferArrayNonUniformIndexing    = true;
        descIndexingFeatures.shaderStorageTexelBufferArrayNonUniformIndexing    = true;
        descIndexingFeatures.descriptorBindingUniformBufferUpdateAfterBind      = true;
        descIndexingFeatures.descriptorBindingSampledImageUpdateAfterBind       = true;
        descIndexingFeatures.descriptorBindingStorageImageUpdateAfterBind       = true;
        descIndexingFeatures.descriptorBindingStorageBufferUpdateAfterBind      = true;
        descIndexingFeatures.descriptorBindingUniformTexelBufferUpdateAfterBind = true;
        descIndexingFeatures.descriptorBindingStorageTexelBufferUpdateAfterBind = true;

        vk::PhysicalDeviceMultiviewFeatures multiviewFeatures;
        multiviewFeatures.multiview = true;
        multiviewFeatures.pNext = &descIndexingFeatures;

        vk::DeviceCreateInfo deviceCI;
        deviceCI
            .setQueueCreateInfos(queueCI)
            .setPEnabledExtensionNames(deviceExtensions)
            .setPNext(&multiviewFeatures);
        
        this->mDevice = this->mPhysicalDevice.createDevice(deviceCI);
        this->mDeviceQueue = this->mDevice.getQueue(this->mQueueFamilyIndex, 0);

        options.InfoCallback("Created logical device");

        this->mDynamicLoader.init(this->mInstance, this->mDevice);

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsCI;
        debugUtilsCI
            .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
            .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
            .setPfnUserCallback(ValidationLayerCallback);
        this->mDebugUtilsMessenger = this->mInstance.createDebugUtilsMessengerEXT(debugUtilsCI, nullptr, this->mDynamicLoader);

        VmaAllocatorCreateInfo allocatorCI = {};
        allocatorCI.physicalDevice = this->mPhysicalDevice;
        allocatorCI.device = this->mDevice;
        allocatorCI.instance = this->mInstance;
        allocatorCI.vulkanApiVersion = this->mAPIVersion;
        vmaCreateAllocator(&allocatorCI, &this->mAllocator);

        options.InfoCallback("Created VMA allocator");

        glslang::InitializeProcess();
        options.InfoCallback("Initialized glslang");

        this->RecreateSwapchain(surfaceCapabilities.maxImageExtent.width, surfaceCapabilities.maxImageExtent.height);
        options.InfoCallback("Created swapchain");

        this->mImageAvailableSemaphore = this->mDevice.createSemaphore(vk::SemaphoreCreateInfo());
        this->mRenderingFinishedSemaphore = this->mDevice.createSemaphore(vk::SemaphoreCreateInfo());
        this->mImmediateFence = this->mDevice.createFence(vk::FenceCreateInfo{ });

        vk::CommandPoolCreateInfo cmdPoolCI {};
        cmdPoolCI.setQueueFamilyIndex(this->mQueueFamilyIndex);
        cmdPoolCI.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient);
        this->mCommandPool = this->mDevice.createCommandPool(cmdPoolCI);

        vk::CommandBufferAllocateInfo cmdBufferAI {};
        cmdBufferAI.setCommandPool(this->mCommandPool);
        cmdBufferAI.setLevel(vk::CommandBufferLevel::ePrimary);
        cmdBufferAI.setCommandBufferCount(1);
        this->mImmediateCommandBuffer = CommandBuffer(this->mDevice.allocateCommandBuffers(cmdBufferAI).front());
        options.InfoCallback("Created command pool and command buffer");

        this->mDescriptorCache.Init();
        this->mVirtualFrames.Init(options.VirtualFrameCount, options.MaxStageBufferSize);
        options.InfoCallback("Vulkan context initialize finished.");
    }

    void VulkanContext::RecreateSwapchain(uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
        this->mDevice.waitIdle();

        auto surfaceCapbilities = this->mPhysicalDevice.getSurfaceCapabilitiesKHR(this->mSurface);
        this->mSurfaceExtent = vk::Extent2D(
            std::clamp(surfaceWidth, surfaceCapbilities.minImageExtent.width, surfaceCapbilities.maxImageExtent.width),
            std::clamp(surfaceHeight, surfaceCapbilities.minImageExtent.height, surfaceCapbilities.maxImageExtent.height)
        );

        if (this->mSurfaceExtent == vk::Extent2D(0, 0))
        {
            this->mSurfaceExtent = vk::Extent2D(1, 1);
            this->mRenderingEnabled = false;
            return;
        }

        this->mRenderingEnabled = true;

        vk::SwapchainCreateInfoKHR swapchainCI {};
        swapchainCI
            .setSurface(this->mSurface)
            .setMinImageCount(this->mPresentImageCount)
            .setImageFormat(this->mSurfaceFormat.format)
            .setImageColorSpace(this->mSurfaceFormat.colorSpace)
            .setImageExtent(this->mSurfaceExtent)
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
            .setImageSharingMode(vk::SharingMode::eExclusive)
            .setPreTransform(surfaceCapbilities.currentTransform)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(this->mPresentMode)
            .setClipped(true)
            .setOldSwapchain(this->mSwapchain);
        
        this->mSwapchain = this->mDevice.createSwapchainKHR(swapchainCI);

        if (swapchainCI.oldSwapchain)
        {
            this->mDevice.destroySwapchainKHR(swapchainCI.oldSwapchain);
        }

        auto swapChainImages = this->mDevice.getSwapchainImagesKHR(this->mSwapchain);
        this->mPresentImageCount = static_cast<uint32_t>(mSwapchainImages.size());
        this->mSwapchainImages.clear();
        this->mSwapchainImages.reserve(this->mPresentImageCount);
        this->mSwapchainImageUsages.assign(this->mPresentImageCount, ImageUsage::UNKNOWN);

        for (uint32_t i = 0; i < this->mPresentImageCount; i++)
        {
            this->mSwapchainImages.emplace_back(Image(
                swapChainImages[i],
                this->mSurfaceExtent.width,
                this->mSurfaceExtent.height,
                FromNative(this->mSurfaceFormat.format)
            ));
        }
    }

    void VulkanContext::StartFrame()
    {
        this->mVirtualFrames.StartFrame();
    }

    bool VulkanContext::IsFrameRunning() const
    {
        return this->mVirtualFrames.IsFrameRunning();
    }

    const Image &VulkanContext::AcquireCurrentSwapchainImage(ImageUsage::Bits usage)
    {
        return this->AcquireSwapchainImage(this->mVirtualFrames.GetPresentImageIndex(), usage);
    }

    CommandBuffer &VulkanContext::GetCurrentCommandBuffer()
    {
        return this->mVirtualFrames.GetCurrentFrame().Commands;
    }

    StageBuffer &VulkanContext::GetCurrentStageBuffer()
    {
        return this->mVirtualFrames.GetCurrentFrame().StagingBuffer;
    }

    void VulkanContext::SubmitCommandsImmediate(const CommandBuffer &commands)
    {
        vk::SubmitInfo submitInfo {};
        submitInfo.setCommandBuffers(commands.GetNativeCmdBuffer());
        this->GetGraphicsQueue().submit(submitInfo, this->mImmediateFence);
        auto waitResult = this->mDevice.waitForFences(this->mImmediateFence, false, UINT64_MAX);
        assert(waitResult == vk::Result::eSuccess);
        this->mDevice.resetFences(this->mImmediateFence);
    }

    CommandBuffer &VulkanContext::GetImmediateCommandBuffer()
    {
        return this->mImmediateCommandBuffer;
    }

    void VulkanContext::EndFrame()
    {
        this->mVirtualFrames.EndFrame();
    }

    static VulkanContext* CurrentVulkanContext = nullptr;

    VulkanContext &GetCurrentVulkanContext()
    {
        assert(CurrentVulkanContext != nullptr);
        return *CurrentVulkanContext;
    }

    void SetCurrentVulkanContext(VulkanContext &context)
    {
        CurrentVulkanContext = std::addressof(context);
    }
}