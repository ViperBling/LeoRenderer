#pragma once

#include "ProjectPCH.hpp"
#include "VKTools.hpp"

namespace LeoVK
{
    /**
	* @brief Encapsulates access to a Vulkan buffer backed up by device memory
	* @note To be filled by an external source like the VulkanDevice
	*/
    class Buffer
    {
    public:
        VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void UnMap();
        VkResult Bind(VkDeviceSize offset = 0);

        void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void CopyToBuffer(void* data, VkDeviceSize size);

        VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void Destroy();

    public:
        VkDevice                mDevice;
        VkBuffer                mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory          mMemory = VK_NULL_HANDLE;
        VkDescriptorBufferInfo  mDescriptor;
        VkDeviceSize            mSize = 0;
        VkDeviceSize            mAlignment = 0;
        void*                   mpMapped = nullptr;

        VkBufferUsageFlags      mUsageFlags;
        VkMemoryPropertyFlags   mMemPropFlags;
    };
}