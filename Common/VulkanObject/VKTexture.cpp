#include "VKTexture.hpp"

namespace LeoVK
{
    void Texture::Create()
    {

    }

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

    void Texture::LoadTextureFromFile(
        ktxTexture* ktxTex,
        VkFormat format,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout,
        uint32_t layerCount)
    {
        mpDevice = device;
        mWidth = ktxTex->baseWidth;
        mHeight = ktxTex->baseHeight;
        mMipLevels = ktxTex->numLevels;

        ktx_uint8_t *ktxTextureData = ktxTexture_GetData(ktxTex);
        ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTex);

        VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        // Create a host-visible staging buffer that contains the raw image data
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCI = LeoVK::Init::BufferCreateInfo();
        bufferCI.size = ktxTextureSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCI, nullptr, &stagingBuffer));

        // Get memory requirements for the staging buffer (alignment, memory type bits)
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stagingBuffer, &memReqs);
        memAI.allocationSize = memReqs.size;
        // Get memory type index for a host visible buffer
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &stagingMemory));
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stagingBuffer, stagingMemory, 0));

        // Copy texture data into staging buffer
        uint8_t *data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
        memcpy(data, ktxTextureData, ktxTextureSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stagingMemory);

        // Setup buffer copy regions for each layer including all of its miplevels
        std::vector<VkBufferImageCopy> bufferCopyRegions;

        for (uint32_t layer = 0; layer < layerCount; layer++)
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
                bufferCopyRegion.imageSubresource.layerCount = layerCount;
                bufferCopyRegion.imageExtent.width = ktxTex->baseWidth >> level;
                bufferCopyRegion.imageExtent.height = ktxTex->baseHeight >> level;
                bufferCopyRegion.imageExtent.depth = 1;
                bufferCopyRegion.bufferOffset = offset;

                bufferCopyRegions.push_back(bufferCopyRegion);
            }
        }

        // Create optimal tiled target image
        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = format;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.extent = { mWidth, mHeight, 1 };
        imageCI.usage = imageUsageFlags;
        // Ensure that the TRANSFER_DST bit is set for staging
        if (!(imageCI.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        {
            imageCI.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        imageCI.arrayLayers = layerCount;
        imageCI.mipLevels = mMipLevels;

        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCI, nullptr, &mImage));

        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

        // Use a separate command buffer for texture loading
        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Image barrier for optimal image (target)
        // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = layerCount;

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
        VkSamplerCreateInfo samplerCI = LeoVK::Init::SamplerCreateInfo();
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = samplerCI.addressModeU;
        samplerCI.addressModeW = samplerCI.addressModeU;
        samplerCI.mipLodBias = 0.0f;
        samplerCI.maxAnisotropy = mpDevice->mEnabledFeatures.samplerAnisotropy ? mpDevice->mProperties.limits.maxSamplerAnisotropy : 1.0f;
        samplerCI.anisotropyEnable = mpDevice->mEnabledFeatures.samplerAnisotropy;
        samplerCI.compareOp = VK_COMPARE_OP_NEVER;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = (float)mMipLevels;
        samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerCI, nullptr, &mSampler));

        // Create image view
        VkImageViewCreateInfo viewCI = LeoVK::Init::ImageViewCreateInfo();
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewCI.format = format;
        viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        viewCI.subresourceRange.layerCount = layerCount;
        viewCI.subresourceRange.levelCount = mMipLevels;
        viewCI.image = mImage;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCI, nullptr, &mView));

        // Clean up staging resources
        ktxTexture_Destroy(ktxTex);
        vkFreeMemory(mpDevice->mLogicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stagingBuffer, nullptr);

        // Update descriptor image info member that can be used for setting up descriptor sets
        UpdateDescriptor();
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
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &mDeviceMemory));
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0));

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

    void Texture2D::LoadFromImage(
        tinygltf::Image& gltfImage,
        TextureSampler textureSampler,
        LeoVK::VulkanDevice *device,
        VkQueue copyQueue)
    {
        unsigned char* buffer;
        VkDeviceSize bufferSize;
        bool deleteBuffer = false;
        if (gltfImage.component == 3)
        {
            bufferSize = gltfImage.width * gltfImage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &gltfImage.image[0];
            for (size_t i = 0; i < gltfImage.width * gltfImage.height; ++i)
            {
                for (int32_t j = 0; j < 3; ++j) rgba[j] = rgb[j];
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else
        {
            buffer = &gltfImage.image[0];
            bufferSize = gltfImage.image.size();
        }

        assert(buffer);
        mWidth = gltfImage.width;
        mHeight = gltfImage.height;
        mMipLevels = static_cast<uint32_t>(floor(log2(std::max(mWidth, mHeight))) + 1.0);

        VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(mpDevice->mPhysicalDevice, format, &formatProps);
        assert(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLIT_SRC_BIT);
        assert(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_2_BLIT_DST_BIT);

        VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        VkBuffer stageBuffer;
        VkDeviceMemory stageMem;

        VkBufferCreateInfo bufferCI = LeoVK::Init::BufferCreateInfo();
        bufferCI.size = bufferSize;
        bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(mpDevice->mLogicalDevice, &bufferCI, nullptr, &stageBuffer))
        vkGetBufferMemoryRequirements(mpDevice->mLogicalDevice, stageBuffer, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &stageMem))
        VK_CHECK(vkBindBufferMemory(mpDevice->mLogicalDevice, stageBuffer, stageMem, 0))

        uint8_t* data;
        VK_CHECK(vkMapMemory(mpDevice->mLogicalDevice, stageMem, 0, memReqs.size, 0, (void**)&data))
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(mpDevice->mLogicalDevice, stageMem);

        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = format;
        imageCI.mipLevels = mMipLevels;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.extent = { mWidth, mHeight, 1 };
        imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCI, nullptr, &mImage))
        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mImage, &memReqs);
        memAI.allocationSize = memReqs.size;
        memAI.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAI, nullptr, &mDeviceMemory))
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mImage, mDeviceMemory, 0))

        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = mWidth;
        bufferCopyRegion.imageExtent.height = mHeight;
        bufferCopyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(copyCmd, stageBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        mpDevice->FlushCommandBuffer(copyCmd, copyQueue, true);
        vkFreeMemory(mpDevice->mLogicalDevice, stageMem, nullptr);
        vkDestroyBuffer(mpDevice->mLogicalDevice, stageBuffer, nullptr);

        // Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
        VkCommandBuffer blitCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        for (uint32_t i = 1; i < mMipLevels; i++)
        {
            VkImageBlit imageBlit{};

            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = int32_t(mWidth >> (i - 1));
            imageBlit.srcOffsets[1].y = int32_t(mHeight >> (i - 1));
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = int32_t(mWidth >> i);
            imageBlit.dstOffsets[1].y = int32_t(mHeight >> i);
            imageBlit.dstOffsets[1].z = 1;

            VkImageSubresourceRange mipSubRange = {};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = mImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
            vkCmdBlitImage(blitCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = mImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
        }

        subresourceRange.levelCount = mMipLevels;
        mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        mpDevice->FlushCommandBuffer(blitCmd, copyQueue, true);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = textureSampler.mMagFilter;
        samplerInfo.minFilter = textureSampler.mMinFilter;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = textureSampler.mAddressModeU;
        samplerInfo.addressModeV = textureSampler.mAddressModeV;
        samplerInfo.addressModeW = textureSampler.mAddressModeW;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.maxAnisotropy = 1.0;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxLod = (float)mMipLevels;
        samplerInfo.maxAnisotropy = 8.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerInfo, nullptr, &mSampler));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.layerCount = 1;
        viewInfo.subresourceRange.levelCount = mMipLevels;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewInfo, nullptr, &mView));

        UpdateDescriptor();

        if (deleteBuffer) delete[] buffer;
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
	* @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
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
        ktxResult result = LoadKTXFile(filename, &ktxTex);
        assert(result == KTX_SUCCESS);

        LoadTextureFromFile(
            ktxTex,
            format,
            device,
            copyQueue,
            imageUsageFlags,
            imageLayout,
            1
            );
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
        ktxResult result = LoadKTXFile(filename, &ktxTex);
        assert(result == KTX_SUCCESS);

        uint32_t layerCount = ktxTex->numLayers;

        LoadTextureFromFile(
            ktxTex,
            format,
            device,
            copyQueue,
            imageUsageFlags,
            imageLayout,
            layerCount
        );
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
        ktxResult result = LoadKTXFile(filename, &ktxTex);
        assert(result == KTX_SUCCESS);

        LoadTextureFromFile(
            ktxTex,
            format,
            device,
            copyQueue,
            imageUsageFlags,
            imageLayout,
            6
        );
    }
}