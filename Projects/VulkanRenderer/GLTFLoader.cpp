#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "GLTFLoader.h"

VkDescriptorSetLayout LeoRenderer::descriptorSetLayoutImage = VK_NULL_HANDLE;
VkDescriptorSetLayout LeoRenderer::descriptorSetLayoutUBO = VK_NULL_HANDLE;
VkMemoryPropertyFlags LeoRenderer::memoryPropertyFlags = 0;
uint32_t LeoRenderer::descriptorBindingFlags = LeoRenderer::DescriptorBindingFlags::ImageBaseColor;

bool loadImageDataFunc(
    tinygltf::Image* image,
    const int imageIndex,
    std::string* error,
    std::string* warning,
    int req_width, int req_height,
    const unsigned char* bytes,
    int size, void* userData)
{
    if (image->uri.find_last_of('.') != std::string::npos)
    {
        if (image->uri.substr(image->uri.find_last_of('.') + 1) == "ktx") return true;
    }

    return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
}

bool loadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
    // This function will be used for samples that don't require images to be loaded
    return true;
}

void LeoRenderer::Texture::UpdateDescriptor()
{
    mDescriptor.sampler = mSampler;
    mDescriptor.imageView = mImageView;
    mDescriptor.imageLayout = mImageLayout;
}

void LeoRenderer::Texture::OnDestroy()
{
    if (mDevice)
    {
        vkDestroyImageView(mDevice->logicalDevice, mImageView, nullptr);
        vkDestroyImage(mDevice->logicalDevice, mImage, nullptr);
        vkFreeMemory(mDevice->logicalDevice, mDeviceMemory, nullptr);
        vkDestroySampler(mDevice->logicalDevice, mSampler, nullptr);
    }
}

void LeoRenderer::Texture::FromGLTFImage(
    tinygltf::Image &gltfImage,
    std::string path,
    vks::VulkanDevice *device,
    VkQueue copyQueue)
{
    mDevice = device;

    bool isKTX = false;
    if (gltfImage.uri.find_last_of('.') != std::string::npos)
    {
        if (gltfImage.uri.substr(gltfImage.uri.find_last_of('.') + 1) == "ktx") isKTX = true;
    }

    VkFormat format;
    if (!isKTX)     // Use stb
    {
        unsigned char* buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        bool deleteBuffer = false;
        if (gltfImage.component == 3) {
            // Most devices don't support RGB only on Vulkan so convert if necessary
            bufferSize = gltfImage.width * gltfImage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &gltfImage.image[0];
            for (size_t i = 0; i < gltfImage.width * gltfImage.height; ++i) {
                for (int32_t j = 0; j < 3; ++j) {
                    rgba[j] = rgb[j];
                }
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else {
            buffer = &gltfImage.image[0];
            bufferSize = gltfImage.image.size();
        }
        format = VK_FORMAT_R8G8B8A8_UNORM;

        VkFormatProperties formatProperties;

        mWidth = gltfImage.width;
        mHeight = gltfImage.height;
        mMipLevels = static_cast<uint32_t>(floor(log2(std::max(mWidth, mHeight))) + 1.0);

        vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);
        assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
        assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

        VkMemoryAllocateInfo memAllocInfo{};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memReqs{};

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        uint8_t* data;
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
        memcpy(data, buffer, bufferSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mMipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &mImage));
        vkGetImageMemoryRequirements(device->logicalDevice, mImage, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, mImage, mDeviceMemory, 0));

        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.levelCount = 1;
        subresourceRange.layerCount = 1;
        {
            // 管线同步屏障，拷贝前
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
        // 从StagingBuffer拷贝图片内容到mImage
        vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
        {
            // 管线同步屏障，拷贝后
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

        device->flushCommandBuffer(copyCmd, copyQueue, true);

        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

        // Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
        VkCommandBuffer blitCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        for (uint32_t i = 1; i < mMipLevels; i++) {
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
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        if (deleteBuffer) {
            delete[] buffer;
        }

        device->flushCommandBuffer(blitCmd, copyQueue, true);
    }
    else
    {
        std::string filename = path + "/" + gltfImage.uri;
        ktxTexture* ktxTex;
        ktxResult result = KTX_SUCCESS;
        if (!vks::tools::fileExists(filename))
        {
            vks::tools::exitFatal("Could not load texture from " + filename + "\n\nThe file may be part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
        }
        result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTex);
        assert(result == KTX_SUCCESS);

        mWidth = ktxTex->baseWidth;
        mHeight = ktxTex->baseHeight;
        mMipLevels = ktxTex->numLevels;

        ktx_uint8_t * ktxTexData = ktxTexture_GetData(ktxTex);
        ktx_size_t ktxTexSize = ktxTexture_GetSize(ktxTex);

        format = VK_FORMAT_R8G8B8A8_UNORM;

        // Get device properties for the requested texture format
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = vks::initializers::bufferCreateInfo();
        bufferCreateInfo.size = ktxTexSize;
        // This buffer is used as a transfer source for the buffer copy
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

        VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
        VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

        uint8_t* data;
        VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
        memcpy(data, ktxTexData, ktxTexSize);
        vkUnmapMemory(device->logicalDevice, stagingMemory);

        std::vector<VkBufferImageCopy> bufferCopyRegions;
        for (uint32_t i = 0; i < mMipLevels; i++)
        {
            ktx_size_t offset;
            assert(ktxTexture_GetImageOffset(ktxTex, i, 0, 0, &offset) == KTX_SUCCESS);
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
        VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.mipLevels = mMipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.extent = { mWidth, mHeight, 1 };
        imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &mImage));

        vkGetImageMemoryRequirements(device->logicalDevice, mImage, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, mImage, mDeviceMemory, 0));

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = mMipLevels;
        subresourceRange.layerCount = 1;

        vks::tools::setImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
        vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
        vks::tools::setImageLayout(copyCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
        device->flushCommandBuffer(copyCmd, copyQueue);
        mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
        vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

        ktxTexture_Destroy(ktxTex);
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxLod = (float)mMipLevels;
    samplerInfo.maxAnisotropy = 8.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &mSampler));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = mImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = mMipLevels;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &mImageView));

    mDescriptor.sampler = mSampler;
    mDescriptor.imageView = mImageView;
    mDescriptor.imageLayout = mImageLayout;
}


void LeoRenderer::Material::CreateDescSet(
    VkDescriptorPool descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    uint32_t descriptorBindingFlags)
{
    VkDescriptorSetAllocateInfo descSetAllocInfo{};
    descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAllocInfo.descriptorPool = descriptorPool;
    descSetAllocInfo.pSetLayouts = &descriptorSetLayout;
    descSetAllocInfo.descriptorSetCount = 1;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(mDevice->logicalDevice, &descSetAllocInfo, &mDescriptorSet));

    std::vector<VkDescriptorImageInfo> imageDescs{};
    std::vector<VkWriteDescriptorSet> writeDescSets{};
    if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
    {
        imageDescs.push_back(mBaseColorTexture->mDescriptor);

    }
}
