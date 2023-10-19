#pragma once

#include "ProjectPCH.hpp"
#include "VKBuffer.hpp"
#include "VKTools.hpp"

namespace LeoVK
{
    class VulkanDevice
    {
    public:
        explicit VulkanDevice(VkPhysicalDevice physicalDevice);
        virtual ~VulkanDevice();

        uint32_t        GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags memProps, VkBool32 *memTypeFound = nullptr) const;
        uint32_t        GetQueueFamilyIndex(VkQueueFlags queueFlags) const;
        VkResult        CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, void *pNextChain, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
        VkResult        CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);
        VkResult        CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, LeoVK::Buffer *buffer, VkDeviceSize size, void *data = nullptr);
        void            CopyBuffer(LeoVK::Buffer *src, LeoVK::Buffer *dst, VkQueue queue, VkBufferCopy *copyRegion = nullptr);
        VkCommandPool   CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
        VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin = false);
        void            BeginCommandBuffer(VkCommandBuffer commandBuffer);
        void            FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
        void            FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);
        bool            ExtensionSupported(std::string extension);
        VkFormat        GetSupportedDepthFormat(bool checkSamplingSupport);

    public:
        /** @brief Physical device representation */
        VkPhysicalDevice mPhysicalDevice;
        /** @brief Logical device representation (application's view of the device) */
        VkDevice mLogicalDevice;
        /** @brief Properties of the physical device including limits that the application can check against */
        VkPhysicalDeviceProperties mProperties;
        /** @brief Features of the physical device that an application can use to check if a feature is supported */
        VkPhysicalDeviceFeatures mFeatures;
        /** @brief Features that have been enabled for use on the physical device */
        VkPhysicalDeviceFeatures mEnabledFeatures;
        /** @brief Memory types and heaps of the physical device */
        VkPhysicalDeviceMemoryProperties mMemoryProperties;
        /** @brief Queue family properties of the physical device */
        std::vector<VkQueueFamilyProperties> mQueueFamilyProperties;
        /** @brief List of extensions supported by the device */
        std::vector<std::string> mSupportedExtensions;
        /** @brief Default command pool for the graphics queue family index */
        VkCommandPool mCommandPool = VK_NULL_HANDLE;
        /** @brief Set to true when the debug marker extension is detected */
        bool mbEnableDebugMarkers = false;
        /** @brief Contains queue family indices */
        struct
        {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } mQueueFamilyIndices;
        operator VkDevice() const
        {
            return mLogicalDevice;
        };
    };
}