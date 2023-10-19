#include "VKTexture.hpp"


#include <utility>

namespace LeoVK
{
    void Texture::Destroy()
    {
        vkDestroyImageView(mpDevice->mLogicalDevice, mView, nullptr);
        vkDestroyImage(mpDevice->mLogicalDevice, mImage, nullptr);
        if (mSampler)
        {
            vkDestroySampler(mpDevice->mLogicalDevice, mSampler, nullptr);
        }
        vkFreeMemory(mpDevice->mLogicalDevice, mDeviceMemory, nullptr);
    }

    void Texture::UpdateDescriptor()
    {
        mDescriptor.sampler = mSampler;
        mDescriptor.imageView = mView;
        mDescriptor.imageLayout = mImageLayout;
    }

    ktxResult Texture::LoadKTXFile(std::string filename, ktxTexture **target)
    {
        ktxResult res = KTX_SUCCESS;
        if (!LeoVK::VKTools::FileExists(filename))
        {
            LeoVK::VKTools::ExitFatal(
                "Could not load texture from " + filename +
                "\n\nThe file may be part of the additional asset pack."
                "\\n\\nRun \\\"download_assets.py\\\" in the repository root to download the latest version.", -1);
        }
        res = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, target);
        return res;
    }

    // ============================== Texture2D ============================== //

    void Texture2D::LoadFromBuffer(
        void *buffer,
        VkDeviceSize bufferSize,
        VkFormat format,
        uint32_t texWidth,
        uint32_t texHeight,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkFilter filter,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        assert(buffer);
        mpDevice = device;
        mWidth = texWidth;
        mHeight = texHeight;
        mMipLevels = 1;

        VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkBuffer stageBuffer;
        VkDeviceMemory stageMemory{};

        VkBufferCreateInfo bufferCI = LeoVK::Init::BufferCreateInfo();
        bufferCI.size = bufferSize;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCI, nullptr, &stageBuffer))

        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stageBuffer, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &stageMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stageBuffer, stageMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stageMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stageMemory);

        VkBufferImageCopy bufferCopyRegion{};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent = { texWidth, texHeight, 1 };
        bufferCopyRegion.bufferOffset = 0;

        // Create optimal tiled target image
        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = format;
        imageCI.mipLevels = mMipLevels;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.extent = { texWidth, texHeight, 1 };
        imageCI.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCI.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCI.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCI, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);

        memAI.allocationSize = memReqs.size;

        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = 1;

        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy the layers and mip levels from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stageBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion);

        // Change texture image layout to shader read after all faces have been copied
        mImageLayout = imageLayout;
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Clean up staging resources
        vkFreeMemory(mpDevice->mLogicalDevice, stageMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stageBuffer, nullptr);

        // Create sampler
        VkSamplerCreateInfo samplerCI = {};
        samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter = filter;
        samplerCI.minFilter = filter;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCI.mipLodBias = 0.0f;
        samplerCI.compareOp = VK_COMPARE_OP_NEVER;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = 0.0f;
        samplerCI.maxAnisotropy = 1.0f;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCI, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.pNext = nullptr;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }

    /**
	* Load a 2D texture including all mip levels
	*
	* @param filename File to load (supports .ktx)
	* @param format Vulkan format of the image data stored in the file
	* @param device Vulkan device to create the texture on
	* @param copyQueue Queue used for the texture staging copy commands (must support transfer)
	* @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
	* @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	*
	*/
    void Texture2D::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        ktxTexture* ktxTex;
        ktxResult result = LoadKTXFile(std::move(filename), &ktxTex);
        assert(result == KTX_SUCCESS);

        mpDevice = device;
        mWidth = ktxTex->baseWidth;
        mHeight = ktxTex->baseHeight;
        mMipLevels = ktxTex->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTex);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTex);
        
        VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = LeoVK::Init::BufferCreateInfo();
        bufferCreateInfo.size = ktxTextureSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);

        // Setup buffer copy regions for each mip level
        std::vector<VkBufferImageCopy> bufferCopyRegions;

        for (uint32_t i = 0; i < mMipLevels; i++)
        {
            ktx_size_t offset;
            KTX_error_code result = ktxTexture_GetImageOffset(ktxTex, i, 0, 0, &offset);
            assert(result == KTX_SUCCESS);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = std::max(1u, ktxTex->baseWidth >> i);
            bufferCopyRegion.imageExtent.height = std::max(1u, ktxTex->baseHeight >> i);
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = offset;

            bufferCopyRegions.push_back(bufferCopyRegion);
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = LeoVK::Init::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mMipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;

        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = 1;

        // Image barrier for optimal image (target)
        // Optimal image will be used as destination for the copy
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy mip levels from staging buffer
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data()
        );

        // Change texture image layout to shader read after all mip levels have been copied
        mImageLayout = imageLayout;
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Clean up staging resources
        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);
        
        
        ktxTexture_Destroy(ktxTex);

        // Create a default sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        // Max level-of-detail should match mip level count
        samplerCreateInfo.maxLod = (float)mMipLevels;
        // Only enable anisotropic filtering if enabled on the device
        samplerCreateInfo.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCreateInfo.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler));

        // Create image view
        // Textures are not directly accessed by the shaders and
        // are abstracted by image views containing additional
        // information and sub resource ranges
        VkImageViewCreateInfo viewCreateInfo = {};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        viewCreateInfo.subresourceRange.levelCount = mMipLevels;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }

    // ============================== TextureArray ============================== //
    void Texture2DArray::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        ktxTexture* ktxTex;
        ktxResult result = LoadKTXFile(std::move(filename), &ktxTex);
        assert(result == KTX_SUCCESS);

        mpDevice = device;
        mWidth = ktxTex->baseWidth;
        mHeight = ktxTex->baseHeight;
        mLayerCount = ktxTex->numLayers;
        mMipLevels = ktxTex->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTex);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTex);

        VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = LeoVK::Init::BufferCreateInfo();
        bufferCreateInfo.size = ktxTextureSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);

        // Setup buffer copy regions for each layer including all of its miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;

        for (uint32_t layer = 0; layer < mLayerCount; layer++)
        {
            for (uint32_t level = 0; level < mMipLevels; level++)
            {
                ktx_size_t offset;
                KTX_error_code result = ktxTexture_GetImageOffset(ktxTex, level, layer, 0, &offset);
                assert(result == KTX_SUCCESS);

                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = ktxTex->baseWidth >> level;
                bufferCopyRegion.imageExtent.height = ktxTex->baseHeight >> level;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = LeoVK::Init::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        imageCreateInfo.arrayLayers = mLayerCount;
        imageCreateInfo.mipLevels = mMipLevels;

        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = mLayerCount;

        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy the layers and mip levels from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture image layout to shader read after all faces have been copied
        mImageLayout = imageLayout;
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = LeoVK::Init::SamplerCreateInfo();
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCreateInfo.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)mMipLevels;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = LeoVK::Init::ImageViewCreateInfo();
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCreateInfo.subresourceRange.layerCount = mLayerCount;
        viewCreateInfo.subresourceRange.levelCount = mMipLevels;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        // Clean up staging resources
        ktxTexture_Destroy(ktxTex);
        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }

    // ============================== TextureCubeMap ============================== //
    void TextureCube::LoadFromFile(
        std::string filename,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout)
    {
        ktxTexture* ktxTex;
        ktxResult texRes = LoadKTXFile(std::move(filename), &ktxTex);
        assert(texRes == KTX_SUCCESS);

        mpDevice = device;
        mWidth = ktxTex->baseWidth;
        mHeight = ktxTex->baseHeight;
        mMipLevels = ktxTex->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTex);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTex);

        VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = LeoVK::Init::BufferCreateInfo();
        bufferCreateInfo.size = ktxTextureSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);

        // Setup buffer copy regions for each face including all of its mip levels
        std::vector<VkBufferImageCopy> bufferCopyRegions;

        for (uint32_t face = 0; face < 6; face++)
        {
            for (uint32_t level = 0; level < mMipLevels; level++)
            {
                ktx_size_t offset;
                KTX_error_code result = ktxTexture_GetImageOffset(ktxTex, level, 0, face, &offset);
                assert(result == KTX_SUCCESS);

                VkBufferImageCopy bufferCopyRegion = {};
                bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                bufferCopyRegion.imageSubresource.mipLevel = level;
                bufferCopyRegion.imageSubresource.baseArrayLayer = face;
                bufferCopyRegion.imageSubresource.layerCount = 1;
                bufferCopyRegion.imageExtent.width = ktxTex->baseWidth >> level;
                bufferCopyRegion.imageExtent.height = ktxTex->baseHeight >> level;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCreateInfo = LeoVK::Init::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mMipLevels;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        // Cube faces count as array layers in Vulkan
        imageCreateInfo.arrayLayers = 6;
        // This flag is required for cube map images
        imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);

        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = 6;

        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

        // Copy the cube map faces from the staging buffer to the optimal tiled image
        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

        // Change texture image layout to shader read after all faces have been copied
        mImageLayout = imageLayout;
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

        mpDevice->FlushCommandBuffer(copyCmd, copyQueue);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = LeoVK::Init::SamplerCreateInfo();
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
        samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCreateInfo.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = (float)mMipLevels;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCreateInfo, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCreateInfo = LeoVK::Init::ImageViewCreateInfo();
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCreateInfo.format = format;
        viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCreateInfo.subresourceRange.layerCount = 6;
        viewCreateInfo.subresourceRange.levelCount = mMipLevels;
        viewCreateInfo.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCreateInfo, nullptr, &mView));

        // Clean up staging resources
        ktxTexture_Destroy(ktxTex);
        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
    }
}