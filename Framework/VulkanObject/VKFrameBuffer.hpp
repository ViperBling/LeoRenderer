#pragma once

#include <algorithm>
#include <iterator>
#include <vector>
#include <array>
#include "Vulkan/vulkan.h"

#include "VKDevice.hpp"
#include "VKTools.hpp"

namespace LeoVK
{
    struct AttachmentCreateInfo
    {
        uint32_t width, height;
        uint32_t layerCount;
        VkFormat format;
        VkImageUsageFlags usage;
        VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
    };

    class FrameBufferAttachment
    {
    public:
        bool HasDepth();
        bool HasStencil();
        bool IsDepthStencil();

    public:
        VkImage mImage;
        VkDeviceMemory mMemory;
        VkImageView mView;
        VkFormat mFormat;
        VkImageSubresourceRange mSubresourceRange;
        VkAttachmentDescription mDescription;
    };

    class FrameBuffer
    {
    public:
        explicit FrameBuffer(LeoVK::VulkanDevice* vulkanDevice);
        virtual ~FrameBuffer();

        uint32_t AddAttachment(AttachmentCreateInfo createInfo);
        VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode);
        VkResult CreateRenderPass();

    public:
        uint32_t mWidth, mHeight;
        VkFramebuffer mFrameBuffer;
        VkRenderPass mRenderPass;
        VkSampler mSampler;
        std::vector<FrameBufferAttachment> mAttachments;

    private:
        LeoVK::VulkanDevice* mpVulkanDevice;
    };
}