#include "VKContext.hpp"

#include <algorithm>
#include <cstring>
#include <optional>
#include <iostream>

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
    }
    const Image &VulkanContext::AcquireSwapchainImage(size_t index, ImageUsage::Bits usage)
    {
        // TODO: insert return statement here
    }
    ImageUsage::Bits VulkanContext::GetSwapchainImageUsage(size_t index) const
    {
        return ImageUsage::Bits();
    }
    void VulkanContext::InitializeContext(const WindowSurface &surface, const ContextInitializeOptions &options)
    {
    }
    void VulkanContext::RecreateSwapchain(uint32_t surfaceWidth, uint32_t surfaceHeight)
    {
    }
    void VulkanContext::StartFrame()
    {
    }
    bool VulkanContext::IsFrameRunning() const
    {
        return false;
    }
    const Image &VulkanContext::AcquireCurrentSwapchainImage(ImageUsage::Bits usage)
    {
        // TODO: insert return statement here
    }
    CommandBuffer &VulkanContext::GetCurrentCommandBuffer()
    {
        // TODO: insert return statement here
    }
    StageBuffer &VulkanContext::GetCurrentStageBuffer()
    {
        // TODO: insert return statement here
    }
    void VulkanContext::SubmitCommandsImmediate(const CommandBuffer &commands)
    {
    }
    CommandBuffer &VulkanContext::GetImmediateCommandBuffer()
    {
        // TODO: insert return statement here
    }
    void VulkanContext::EndFrame()
    {
    }
    VulkanContext &GetCurrentVulkanContext()
    {
        // TODO: insert return statement here
    }
    void SetCurrentVulkanContext(VulkanContext &context)
    {
    }
}