#include "VKFrameBuffer.hpp"

namespace LeoVK
{
    bool FrameBufferAttachment::HasDepth()
    {
        std::vector<VkFormat> formats ={
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        return std::find(formats.begin(), formats.end(), mFormat) != std::end(formats);
    }

    bool FrameBufferAttachment::HasStencil()
    {
        std::vector<VkFormat> formats ={
            VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        return std::find(formats.begin(), formats.end(), mFormat) != std::end(formats);
    }

    bool FrameBufferAttachment::IsDepthStencil()
    {
        return HasDepth() || HasStencil();
    }

    FrameBuffer::FrameBuffer(LeoVK::VulkanDevice *vulkanDevice)
    {
        assert(vulkanDevice);
        mpVulkanDevice = vulkanDevice;
    }

    FrameBuffer::~FrameBuffer()
    {
        assert(mpVulkanDevice);
        for (auto attachment : mAttachments)
        {
            vkDestroyImage(mpVulkanDevice->mLogicalDevice, attachment.mImage, nullptr);
            vkDestroyImageView(mpVulkanDevice->mLogicalDevice, attachment.mView, nullptr);
            vkFreeMemory(mpVulkanDevice->mLogicalDevice, attachment.mMemory, nullptr);
        }
        vkDestroySampler(mpVulkanDevice->mLogicalDevice, mSampler, nullptr);
        vkDestroyRenderPass(mpVulkanDevice->mLogicalDevice, mRenderPass, nullptr);
        vkDestroyFramebuffer(mpVulkanDevice->mLogicalDevice, mFrameBuffer, nullptr);
    }

    /**
	* Add a new attachment described by createInfo to the frameBuffer's attachment list
	*
	* @param createInfo Structure that specifies the framebuffer to be constructed
	*
	* @return Index of the new attachment
	*/
    uint32_t FrameBuffer::AddAttachment(AttachmentCreateInfo createInfo)
    {
        LeoVK::FrameBufferAttachment attachment {};
        attachment.mFormat = createInfo.format;

        VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

        // 选择一个aspect mask
        if (createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        if (createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            if (attachment.HasDepth()) aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (attachment.HasStencil()) aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        assert(aspectMask > 0);

        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = createInfo.format;
        imageCI.extent = {createInfo.width, createInfo.height, 1};
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = createInfo.layerCount;
        imageCI.samples = createInfo.imageSampleCount;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = createInfo.usage;

        VkMemoryAllocateInfo memAlloc = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Create Image
        VK_CHECK(vkCreateImage(mpVulkanDevice->mLogicalDevice, &imageCI, nullptr, &attachment.mImage))
        vkGetImageMemoryRequirements(mpVulkanDevice->mLogicalDevice, attachment.mImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &attachment.mMemory));
        VK_CHECK(vkBindImageMemory(mpVulkanDevice->mLogicalDevice, attachment.mImage, attachment.mMemory, 0));

        attachment.mSubresourceRange = {};
        attachment.mSubresourceRange.aspectMask = aspectMask;
        attachment.mSubresourceRange.levelCount = 1;
        attachment.mSubresourceRange.layerCount = createInfo.layerCount;

        VkImageViewCreateInfo imageView = LeoVK::Init::ImageViewCreateInfo();
        imageView.viewType = (createInfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        imageView.format = createInfo.format;
        imageView.subresourceRange = attachment.mSubresourceRange;
        //todo: workaround for depth+stencil attachments
        imageView.subresourceRange.aspectMask = (attachment.HasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
        imageView.image = attachment.mImage;
        VK_CHECK(vkCreateImageView(mpVulkanDevice->mLogicalDevice, &imageView, nullptr, &attachment.mView));

        // Fill attachment description
        attachment.mDescription = {};
        attachment.mDescription.samples = createInfo.imageSampleCount;
        attachment.mDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.mDescription.storeOp = (createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.mDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.mDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.mDescription.format = createInfo.format;
        attachment.mDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Final layout
        // If not, final layout depends on attachment type
        if (attachment.IsDepthStencil())
        {
            attachment.mDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        else
        {
            attachment.mDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        mAttachments.push_back(attachment);

        return static_cast<uint32_t>(mAttachments.size() - 1);
    }

    /**
	* Creates a default sampler for sampling from any of the framebuffer attachments
	* Applications are free to create their own samplers for different use cases
	*
	* @param magFilter Magnification filter for lookups
	* @param minFilter Minification filter for lookups
	* @param addressMode Addressing mode for the U,V and W coordinates
	*
	* @return VkResult for the sampler creation
	*/
    VkResult FrameBuffer::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode)
    {
        VkSamplerCreateInfo samplerInfo = LeoVK::Init::SamplerCreateInfo();
        samplerInfo.magFilter = magFilter;
        samplerInfo.minFilter = minFilter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = addressMode;
        samplerInfo.addressModeV = addressMode;
        samplerInfo.addressModeW = addressMode;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

        return vkCreateSampler(mpVulkanDevice->mLogicalDevice, &samplerInfo, nullptr, &mSampler);
    }

    /**
	* Creates a default render pass setup with one sub pass
	*
	* @return VK_SUCCESS if all resources have been created successfully
	*/
    VkResult FrameBuffer::CreateRenderPass()
    {
        std::vector<VkAttachmentDescription> attachmentDescriptions;
        for (auto& attachment : mAttachments)
        {
            attachmentDescriptions.push_back(attachment.mDescription);
        }

        // 收集attachment
        std::vector<VkAttachmentReference> colorReferences;
        VkAttachmentReference depthReference{};
        bool hasDepth = false;
        bool hasColor = false;

        uint32_t attachmentIndex = 0;

        for (auto& attachment : mAttachments)
        {
            if (attachment.IsDepthStencil())
            {
                assert(!hasDepth);      // 确保同时只有一个Depth
                depthReference.attachment = attachmentIndex;
                depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                hasDepth = true;
            }
            else
            {
                colorReferences.push_back( {attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } );
                hasColor = true;
            }
            attachmentIndex++;
        }
        // 默认的RenderPass只有一个subpass
        VkSubpassDescription subpassDesc {};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        if (hasColor)
        {
            subpassDesc.pColorAttachments = colorReferences.data();
            subpassDesc.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
        }
        if (hasDepth)
        {
            subpassDesc.pDepthStencilAttachment = &depthReference;
        }

        // subpass Dependencies用于attachment layout的转换
        std::array<VkSubpassDependency, 2> subpassDeps{};
        subpassDeps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDeps[0].dstSubpass = 0;
        subpassDeps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDeps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDeps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDeps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDeps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        subpassDeps[1].srcSubpass = 0;
        subpassDeps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        subpassDeps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDeps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDeps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDeps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDeps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Create render pass
        VkRenderPassCreateInfo renderPassCI = LeoVK::Init::RenderPassCreateInfo();
        renderPassCI.pAttachments = attachmentDescriptions.data();
        renderPassCI.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpassDesc;
        renderPassCI.dependencyCount = 2;
        renderPassCI.pDependencies = subpassDeps.data();
        VK_CHECK(vkCreateRenderPass(mpVulkanDevice->mLogicalDevice, &renderPassCI, nullptr, &mRenderPass));

        std::vector<VkImageView> attachmentViews;
        for (auto attachment : mAttachments)
        {
            attachmentViews.push_back(attachment.mView);
        }

        // Find. max number of layers across attachments
        uint32_t maxLayers = 0;
        for (auto attachment : mAttachments)
        {
            if (attachment.mSubresourceRange.layerCount > maxLayers)
            {
                maxLayers = attachment.mSubresourceRange.layerCount;
            }
        }

        VkFramebufferCreateInfo frameBufferCI = LeoVK::Init::FrameBufferCreateInfo();
        frameBufferCI.renderPass = mRenderPass;
        frameBufferCI.pAttachments = attachmentViews.data();
        frameBufferCI.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
        frameBufferCI.width = mWidth;
        frameBufferCI.height = mHeight;
        frameBufferCI.layers = maxLayers;
        VK_CHECK(vkCreateFramebuffer(mpVulkanDevice->mLogicalDevice, &frameBufferCI, nullptr, &mFrameBuffer));

        return VK_SUCCESS;
    }
}