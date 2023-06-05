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
    uint32_t descBindFlags)
{
    VkDescriptorSetAllocateInfo descSetAllocInfo{};
    descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAllocInfo.descriptorPool = descriptorPool;
    descSetAllocInfo.pSetLayouts = &descriptorSetLayout;
    descSetAllocInfo.descriptorSetCount = 1;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(mDevice->logicalDevice, &descSetAllocInfo, &mDescriptorSet));

    std::vector<VkDescriptorImageInfo> imageDescs{};
    std::vector<VkWriteDescriptorSet> writeDescSets{};
    if (descBindFlags & DescriptorBindingFlags::ImageBaseColor)
    {
        imageDescs.push_back(mBaseColorTexture->mDescriptor);
        VkWriteDescriptorSet writeDescSet{};
        writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescSet.descriptorCount = 1;
        writeDescSet.dstSet = mDescriptorSet;
        writeDescSet.dstBinding = static_cast<uint32_t>(writeDescSets.size());
        writeDescSet.pImageInfo = &mBaseColorTexture->mDescriptor;
        writeDescSets.push_back(writeDescSet);
    }
    if (mNormalTexture && descBindFlags & DescriptorBindingFlags::ImageNormalMap)
    {
        imageDescs.push_back(mNormalTexture->mDescriptor);
        VkWriteDescriptorSet writeDescSet{};
        writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeDescSet.descriptorCount = 1;
        writeDescSet.dstSet = mDescriptorSet;
        writeDescSet.dstBinding = static_cast<uint32_t>(writeDescSets.size());
        writeDescSet.pImageInfo = &mNormalTexture->mDescriptor;
        writeDescSets.push_back(writeDescSet);
    }
    vkUpdateDescriptorSets(mDevice->logicalDevice, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
}

void LeoRenderer::Primitive::SetDimensions(glm::vec3 min, glm::vec3 max)
{
    mDimensions.min = min;
    mDimensions.max = max;
    mDimensions.size = max - min;
    mDimensions.center = (min + max) / 2.0f;
    mDimensions.radius = glm::distance(min, max) / 2.0f;
}


LeoRenderer::Mesh::Mesh(vks::VulkanDevice *device, glm::mat4 matrix)
{
    mDevice = device;
    mUniformBlock.matrix = matrix;
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sizeof(mUniformBlock),
        &mUniformBuffer.buffer,
        &mUniformBuffer.memory,
        &mUniformBlock));
    VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, mUniformBuffer.memory, 0, sizeof(mUniformBlock), 0, &mUniformBuffer.mapped));
    mUniformBuffer.descriptor = {mUniformBuffer.buffer, 0, sizeof(mUniformBlock)};
}

LeoRenderer::Mesh::~Mesh()
{
    vkDestroyBuffer(mDevice->logicalDevice, mUniformBuffer.buffer, nullptr);
    vkFreeMemory(mDevice->logicalDevice, mUniformBuffer.memory, nullptr);
    for (auto primitive : mPrimitives) delete primitive;
}

// gltf Node
glm::mat4 LeoRenderer::Node::LocalMatrix()
{
    return glm::translate(glm::mat4(1.0f), mTranslation) * glm::mat4(mRotation) * glm::scale(glm::mat4(1.0f), mScale) * mMatrix;
}

glm::mat4 LeoRenderer::Node::GetMatrix()
{
    glm::mat4 localMat = LocalMatrix();
    LeoRenderer::Node* p = mParent;
    while (p)
    {
        localMat = p->LocalMatrix();
        p = p->mParent;
    }
    return localMat;
}

void LeoRenderer::Node::Update()
{
    if (mMesh)
    {
        glm::mat localMat = GetMatrix();
        if (mSkin)
        {
            mMesh->mUniformBlock.matrix = localMat;
            // Update joint
            glm::mat4 inverseTransform = glm::inverse(localMat);
            for (size_t i = 0; i < mSkin->mJoints.size(); i++)
            {
                LeoRenderer::Node* joinNode = mSkin->mJoints[i];
                glm::mat4 joinMat = joinNode->GetMatrix() * mSkin->mInverseBindMatrices[i];
                joinMat = inverseTransform * joinMat;
                mMesh->mUniformBlock.jointMatrix[i] = joinMat;
            }
            mMesh->mUniformBlock.jointCount = (float)mSkin->mJoints.size();
            memcpy(mMesh->mUniformBuffer.mapped, &mMesh->mUniformBlock, sizeof(mMesh->mUniformBlock));
        }
        else
        {
            memcpy(mMesh->mUniformBuffer.mapped, &localMat, sizeof(glm::mat4));
        }
    }
    for (auto& child : mChildren) child->Update();
}

LeoRenderer::Node::~Node()
{
    if (mMesh) delete mMesh;
    for (auto& child : mChildren) delete child;
}

/*
	glTF default vertex layout with easy Vulkan mapping functions
*/

VkVertexInputBindingDescription LeoRenderer::Vertex::mVertexInputBindingDescription;
std::vector<VkVertexInputAttributeDescription> LeoRenderer::Vertex::mVertexInputAttributeDescriptions;
VkPipelineVertexInputStateCreateInfo LeoRenderer::Vertex::mPipelineVertexInputStateCreateInfo;

VkVertexInputBindingDescription LeoRenderer::Vertex::InputBindingDescription(uint32_t binding)
{
    return VkVertexInputBindingDescription({binding, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX});
}

VkVertexInputAttributeDescription LeoRenderer::Vertex::InputAttributeDescription(
    uint32_t binding, uint32_t location, LeoRenderer::VertexComponent component)
{
    switch(component)
    {
        case VertexComponent::Position:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mPos))});
        case VertexComponent::Normal:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mNormal))});
        case VertexComponent::UV:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mUV))});
        case VertexComponent::Color:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mColor))});
        case VertexComponent::Tangent:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mTangent))});
        case VertexComponent::Joint0:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mJoint0))});
        case VertexComponent::Weight0:
            return VkVertexInputAttributeDescription(
                {location, binding, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex, mWeight0))});
        default:
            return VkVertexInputAttributeDescription({});
    }
}

std::vector<VkVertexInputAttributeDescription> LeoRenderer::Vertex::InputAttributeDescriptions(
    uint32_t binding, const std::vector<VertexComponent>& components)
{
    std::vector<VkVertexInputAttributeDescription> res;
    uint32_t location = 0;
    for (VertexComponent component : components)
    {
        res.push_back(Vertex::InputAttributeDescription(binding, location, component));
        location++;
    }
    return res;
}

VkPipelineVertexInputStateCreateInfo * LeoRenderer::Vertex::GetPipelineVertexInputState(const std::vector<VertexComponent>& components)
{
    mVertexInputBindingDescription = Vertex::InputBindingDescription(0);
    Vertex::mVertexInputAttributeDescriptions = Vertex::InputAttributeDescriptions(0, components);
    mPipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    mPipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
    mPipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = &Vertex::mVertexInputBindingDescription;
    mPipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(Vertex::mVertexInputAttributeDescriptions.size());
    mPipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = Vertex::mVertexInputAttributeDescriptions.data();
    return &mPipelineVertexInputStateCreateInfo;
}


LeoRenderer::Model::~Model()
{
    vkDestroyBuffer(m_pDevice->logicalDevice, mVertices.buffer, nullptr);
    vkFreeMemory(m_pDevice->logicalDevice, mVertices.memory, nullptr);
    vkDestroyBuffer(m_pDevice->logicalDevice, mIndices.buffer, nullptr);
    vkFreeMemory(m_pDevice->logicalDevice, mIndices.memory, nullptr);

    for (auto texture : mTextures) texture.OnDestroy();
    for (auto node : mNodes) delete node;
    for (auto skin : mSkins) delete skin;

    if (descriptorSetLayoutUBO != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_pDevice->logicalDevice, descriptorSetLayoutUBO, nullptr);
        descriptorSetLayoutUBO = VK_NULL_HANDLE;
    }
    if (descriptorSetLayoutImage != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_pDevice->logicalDevice, descriptorSetLayoutImage, nullptr);
        descriptorSetLayoutImage = VK_NULL_HANDLE;
    }
    vkDestroyDescriptorPool(m_pDevice->logicalDevice, mDescPool, nullptr);
    mEmptyTexture.OnDestroy();
}

void LeoRenderer::Model::LoadNode(
    LeoRenderer::Node *parent,
    const tinygltf::Node &node,
    uint32_t nodeIndex,
    const tinygltf::Model &model,
    std::vector<uint32_t> &indexBuffer,
    std::vector<Vertex> &vertexBuffer, float globalScale)
{
    auto newNode = new LeoRenderer::Node{};
    newNode->mIndex = nodeIndex;
    newNode->mParent = parent;
    newNode->mName = node.name;
    newNode->mSkinIndex = node.skin;
    newNode->mMatrix = glm::mat4(1.0f);

    // Generate local node matrix
    auto translation = glm::vec3(0.0f);
    if (node.translation.size() == 3)
    {
        translation = glm::make_vec3(node.translation.data());
        newNode->mTranslation = translation;
    }
    auto rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4)
    {
        glm::quat q = glm::make_quat(node.rotation.data());
        newNode->mRotation = glm::mat4(q);
    }
    auto scale = glm::vec3(1.0f);
    if (node.scale.size() == 3)
    {
        scale = glm::make_vec3(node.scale.data());
        newNode->mScale = scale;
    }
    if (node.matrix.size() == 16)
    {
        newNode->mMatrix = glm::make_mat4x4(node.matrix.data());
        if (globalScale != 1.0f)
        {
            newNode->mMatrix = glm::scale(newNode->mMatrix, glm::vec3(globalScale));
        }
    }

    if (!node.children.empty())
    {
        for (int i : node.children)
        {
            LoadNode(newNode, model.nodes[i], i, model, indexBuffer, vertexBuffer, globalScale);
        }
    }

    if (node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh* newMesh = new Mesh(m_pDevice, newNode->mMatrix);
        newMesh->mName = mesh.name;
        for (const auto & primitive : mesh.primitives)
        {
            if (primitive.indices < 0) {
                continue;
            }
            auto indexStart = static_cast<uint32_t>(indexBuffer.size());
            auto vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            glm::vec3 posMin{};
            glm::vec3 posMax{};
            bool hasSkin = false;
            // Vertices
            {
                const float *bufferPos = nullptr;
                const float *bufferNormals = nullptr;
                const float *bufferTexCoords = nullptr;
                const float* bufferColors = nullptr;
                const float *bufferTangents = nullptr;
                uint32_t numColorComponents;
                const uint16_t *bufferJoints = nullptr;
                const float *bufferWeights = nullptr;

                // Position attribute is required
                assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor &posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                const tinygltf::BufferView &posView = model.bufferViews[posAccessor.bufferView];
                bufferPos = reinterpret_cast<const float *>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
                posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

                if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView &normView = model.bufferViews[normAccessor.bufferView];
                    bufferNormals = reinterpret_cast<const float *>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
                }

                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoords = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                }

                if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                    const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
                    // Color buffer are either of type vec3 or vec4
                    numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
                    bufferColors = reinterpret_cast<const float*>(&(model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
                }

                if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                    const tinygltf::BufferView &tangentView = model.bufferViews[tangentAccessor.bufferView];
                    bufferTangents = reinterpret_cast<const float *>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
                }

                // Skinning
                // Joints
                if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                    const tinygltf::BufferView &jointView = model.bufferViews[jointAccessor.bufferView];
                    bufferJoints = reinterpret_cast<const uint16_t *>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
                }

                if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                    const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferWeights = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                }

                hasSkin = (bufferJoints && bufferWeights);

                vertexCount = static_cast<uint32_t>(posAccessor.count);

                for (size_t v = 0; v < posAccessor.count; v++)
                {
                    Vertex vert{};
                    vert.mPos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
                    vert.mNormal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : glm::vec3(0.0f)));
                    vert.mUV = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : glm::vec3(0.0f);
                    if (bufferColors) {
                        switch (numColorComponents)
                        {
                            case 3:
                                vert.mColor = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
                            case 4:
                                vert.mColor = glm::make_vec4(&bufferColors[v * 4]);
                        }
                    }
                    else {
                        vert.mColor = glm::vec4(1.0f);
                    }
                    vert.mTangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
                    vert.mJoint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
                    vert.mWeight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
                    vertexBuffer.push_back(vert);
                }
            }
            // Indices
            {
                const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

                indexCount = static_cast<uint32_t>(accessor.count);

                switch (accessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                        auto buf = new uint32_t[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        delete[] buf;
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        auto buf = new uint16_t[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        delete[] buf;
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        auto buf = new uint8_t[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        delete[] buf;
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                }
            }
            auto newPrimitive = new Primitive(indexStart, indexCount, primitive.material > -1 ? mMaterials[primitive.material] : mMaterials.back());
            newPrimitive->mFirstVertex = vertexStart;
            newPrimitive->mVertexCount = vertexCount;
            newPrimitive->SetDimensions(posMin, posMax);
            newMesh->mPrimitives.push_back(newPrimitive);
        }
        newNode->mMesh = newMesh;
    }
    if (parent)
    {
        parent->mChildren.push_back(newNode);
    }
    else
    {
        mNodes.push_back(newNode);
    }
    mLinearNodes.push_back(newNode);
}

void LeoRenderer::Model::LoadSkins(tinygltf::Model &gltfModel)
{
    for (tinygltf::Skin &source : gltfModel.skins)
    {
        Skin *newSkin = new Skin{};
        newSkin->mName = source.name;

        // Find skeleton root node
        if (source.skeleton > -1)
        {
            newSkin->mSkeletonRoot = NodeFromIndex(source.skeleton);
        }

        // Find joint nodes
        for (int jointIndex : source.joints)
        {
            Node* node = NodeFromIndex(jointIndex);
            if (node) newSkin->mJoints.push_back(NodeFromIndex(jointIndex));
        }

        // Get inverse bind matrices from buffer
        if (source.inverseBindMatrices > -1)
        {
            const tinygltf::Accessor &accessor = gltfModel.accessors[source.inverseBindMatrices];
            const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];
            newSkin->mInverseBindMatrices.resize(accessor.count);
            memcpy(newSkin->mInverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
        }
        mSkins.push_back(newSkin);
    }
}

void LeoRenderer::Model::LoadImages(tinygltf::Model &gltfModel, vks::VulkanDevice *device, VkQueue transferQueue)
{
    for (tinygltf::Image& image : gltfModel.images)
    {
        LeoRenderer::Texture tex;
        tex.FromGLTFImage(image, mPath, device, transferQueue);
        mTextures.push_back(tex);
    }
    CreateEmptyTexture(transferQueue);
}

void LeoRenderer::Model::LoadMaterials(tinygltf::Model &gltfModel)
{
    for (tinygltf::Material &mat : gltfModel.materials)
    {
        LeoRenderer::Material material(m_pDevice);

        if (mat.values.find("baseColorTexture") != mat.values.end())
        {
            material.mBaseColorTexture = GetTexture(gltfModel.textures[mat.values["baseColorTexture"].TextureIndex()].source);
        }
        // Metallic roughness workflow
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
        {
            material.mMetallicRoughnessTexture = GetTexture(gltfModel.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
        }
        if (mat.values.find("roughnessFactor") != mat.values.end())
        {
            material.mRoughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end())
        {
            material.mMetallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end())
        {
            material.mBaseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
        {
            material.mNormalTexture = GetTexture(gltfModel.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
        }
        else
        {
            material.mNormalTexture = &mEmptyTexture;
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
        {
            material.mEmissiveTexture = GetTexture(gltfModel.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
        {
            material.mOcclusionTexture = GetTexture(gltfModel.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
        }
        if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
        {
            tinygltf::Parameter param = mat.additionalValues["alphaMode"];

            if (param.string_value == "BLEND") material.mAlphaMode = Material::ALPHAMODE_BLEND;
            if (param.string_value == "MASK") material.mAlphaMode = Material::ALPHAMODE_MASK;

        }
        if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
        {
            material.mAlphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
        }

        mMaterials.push_back(material);
    }
    // Push a default material at the end of the list for meshes with no material assigned
    mMaterials.emplace_back(m_pDevice);
}

void LeoRenderer::Model::LoadAnimations(tinygltf::Model &gltfModel)
{
    for (tinygltf::Animation &anim : gltfModel.animations) {
        LeoRenderer::Animation animation{};
        animation.mName = anim.name;
        if (anim.name.empty())
        {
            animation.mName = std::to_string(mAnimations.size());
        }

        // Samplers
        for (auto &samp : anim.samplers)
        {
            LeoRenderer::AnimationSampler sampler{};

            if (samp.interpolation == "LINEAR") sampler.mInterpolation = AnimationSampler::InterpolationType::LINEAR;
            if (samp.interpolation == "STEP") sampler.mInterpolation = AnimationSampler::InterpolationType::STEP;
            if (samp.interpolation == "CUBICSPLINE") sampler.mInterpolation = AnimationSampler::InterpolationType::CUBICSPLINE;

            // Read sampler input time values
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[samp.input];
                const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                auto buf = new float[accessor.count];
                memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(float));
                for (size_t index = 0; index < accessor.count; index++)
                {
                    sampler.mInputs.push_back(buf[index]);
                }
                delete[] buf;

                for (auto input : sampler.mInputs)
                {
                    if (input < animation.mStart)
                    {
                        animation.mStart = input;
                    }
                    if (input > animation.mEnd)
                    {
                        animation.mEnd = input;
                    }
                }
            }

            // Read sampler output T/R/S values
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[samp.output];
                const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                switch (accessor.type)
                {
                    case TINYGLTF_TYPE_VEC3:
                    {
                        auto buf = new glm::vec3[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec3));
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            sampler.mOutputsVec4.emplace_back(buf[index], 0.0f);
                        }
                        delete[] buf;
                        break;
                    }
                    case TINYGLTF_TYPE_VEC4:
                    {
                        auto buf = new glm::vec4[accessor.count];
                        memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::vec4));
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            sampler.mOutputsVec4.emplace_back(buf[index]);
                        }
                        delete[] buf;
                        break;
                    }
                    default:
                    {
                        std::cout << "unknown type" << std::endl;
                        break;
                    }
                }
            }

            animation.mSamplers.push_back(sampler);
        }

        // Channels
        for (auto &source: anim.channels)
        {
            LeoRenderer::AnimationChannel channel{};
            if (source.target_path == "rotation")
            {
                channel.mPath = AnimationChannel::PathType::ROTATION;
            }
            if (source.target_path == "translation")
            {
                channel.mPath = AnimationChannel::PathType::TRANSLATION;
            }
            if (source.target_path == "scale")
            {
                channel.mPath = AnimationChannel::PathType::SCALE;
            }
            if (source.target_path == "weights")
            {
                std::cout << "weights not yet supported, skipping channel" << std::endl;
                continue;
            }
            channel.mSamplerIndex = source.sampler;
            channel.mNode = NodeFromIndex(source.target_node);

            if (!channel.mNode) continue;

            animation.mChannels.push_back(channel);
        }

        mAnimations.push_back(animation);
    }
}

void LeoRenderer::Model::LoadFromFile(
    std::string& filename,
    vks::VulkanDevice *device,
    VkQueue transferQueue,
    uint32_t fileLoadingFlags, float scale)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfContext;
    if (fileLoadingFlags & FileLoadingFlags::DontLoadImages)
        gltfContext.SetImageLoader(loadImageDataFuncEmpty, nullptr);
    else
        gltfContext.SetImageLoader(loadImageDataFunc, nullptr);

    size_t pos = filename.find_last_of('/');
    mPath = filename.substr(0, pos);

    std::string error, warning;

    m_pDevice = device;

    bool fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    if (fileLoaded)
    {
        if (!(fileLoadingFlags & FileLoadingFlags::DontLoadImages))
            LoadImages(gltfModel, device, transferQueue);
        LoadMaterials(gltfModel);

        const tinygltf::Scene &scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
        for (auto i : scene.nodes)
        {
            const tinygltf::Node node = gltfModel.nodes[i];
            LoadNode(nullptr, node, i, gltfModel, indexBuffer, vertexBuffer, scale);
        }
        if (!gltfModel.animations.empty())
        {
            LoadAnimations(gltfModel);
        }
        LoadSkins(gltfModel);

        for (auto node : mLinearNodes)
        {
            // Assign skins
            if (node->mSkinIndex > -1)
            {
                node->mSkin = mSkins[node->mSkinIndex];
            }
            // Initial pose
            if (node->mMesh)
            {
                node->Update();
            }
        }
    }
    else
    {
        // TODO: throw
        vks::tools::exitFatal("Could not load glTF file \"" + filename + "\": " + error, -1);
        return;
    }

    // Pre-Calculations for requested features
    if ((fileLoadingFlags & FileLoadingFlags::PreTransformVertices) ||
        (fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) ||
        (fileLoadingFlags & FileLoadingFlags::FlipY))
    {
        const bool preTransform = fileLoadingFlags & FileLoadingFlags::PreTransformVertices;
        const bool preMultiplyColor = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
        const bool flipY = fileLoadingFlags & FileLoadingFlags::FlipY;

        for (Node* node : mLinearNodes)
        {
            if (node->mMesh)
            {
                const glm::mat4 localMatrix = node->GetMatrix();
                for (Primitive* primitive : node->mMesh->mPrimitives)
                {
                    for (uint32_t i = 0; i < primitive->mVertexCount; i++)
                    {
                        Vertex& vertex = vertexBuffer[primitive->mFirstVertex + i];
                        // Pre-transform vertex positions by node-hierarchy
                        if (preTransform)
                        {
                            vertex.mPos = glm::vec3(localMatrix * glm::vec4(vertex.mPos, 1.0f));
                            vertex.mNormal = glm::normalize(glm::mat3(localMatrix) * vertex.mNormal);
                        }
                        // Flip Y-Axis of vertex positions
                        if (flipY)
                        {
                            vertex.mPos.y *= -1.0f;
                            vertex.mNormal.y *= -1.0f;
                        }
                        // Pre-Multiply vertex colors with material base color
                        if (preMultiplyColor)
                        {
                            vertex.mColor = primitive->mMaterial.mBaseColorFactor * vertex.mColor;
                        }
                    }
                }
            }
        }
    }

    for (auto& extension : gltfModel.extensionsUsed)
    {
        if (extension == "KHR_materials_pbrSpecularGlossiness")
        {
            std::cout << "Required extension: " << extension;
            m_bMetallicWorkFlow = false;
        }
    }

    size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
    size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
    mIndices.count = static_cast<uint32_t>(indexBuffer.size());
    mVertices.count = static_cast<uint32_t>(vertexBuffer.size());

    assert((vertexBufferSize > 0) && (indexBufferSize > 0));

    struct StagingBuffer
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } vertexStaging{}, indexStaging{};

    // Create staging buffers
    // Vertex data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBufferSize,
        &vertexStaging.buffer,
        &vertexStaging.memory,
        vertexBuffer.data()))

    // Index data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,
        &indexStaging.buffer,
        &indexStaging.memory,
        indexBuffer.data()));

    // Create device local buffers
    // Vertex buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        &mVertices.buffer,
        &mVertices.memory));
    // Index buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | memoryPropertyFlags,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        &mIndices.buffer,
        &mIndices.memory));

    // Copy from staging buffers
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion = {};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, mVertices.buffer, 1, &copyRegion);

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmd, indexStaging.buffer, mIndices.buffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
    vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

    GetSceneDimensions();

    // Setup descriptors
    uint32_t uboCount{ 0 };
    uint32_t imageCount{ 0 };
    for (auto node : mLinearNodes)
    {
        if (node->mMesh) uboCount++;
    }
    for (auto material : mMaterials)
    {
        if (material.mBaseColorTexture != nullptr) imageCount++;
    }

    std::vector<VkDescriptorPoolSize> poolSizes =
    {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount },
    };
    if (imageCount > 0)
    {
        if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
        }
        if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
        }
    }
    VkDescriptorPoolCreateInfo descriptorPoolCI{};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();
    descriptorPoolCI.maxSets = uboCount + imageCount;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolCI, nullptr, &mDescPool));

    // Descriptors for per-node uniform buffers
    {
        // Layout is global, so only create if it hasn't already been created before
        if (descriptorSetLayoutUBO == VK_NULL_HANDLE)
        {
            std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
            {
                vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            };
            VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
            descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
            descriptorLayoutCI.pBindings = setLayoutBindings.data();
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutUBO));
        }
        for (auto node : mNodes)
        {
            PrepareNodeDescriptor(node, descriptorSetLayoutUBO);
        }
    }

    // Descriptors for per-material images
    {
        // Layout is global, so only create if it hasn't already been created before
        if (descriptorSetLayoutImage == VK_NULL_HANDLE) {
            std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
            if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
                setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
            }
            if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
                setLayoutBindings.push_back(vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
            }
            VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
            descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
            descriptorLayoutCI.pBindings = setLayoutBindings.data();
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutImage));
        }
        for (auto& material : mMaterials) {
            if (material.mBaseColorTexture != nullptr) {
                material.CreateDescSet(mDescPool, LeoRenderer::descriptorSetLayoutImage, descriptorBindingFlags);
            }
        }
    }
}

void LeoRenderer::Model::BindBuffers(VkCommandBuffer commandBuffer)
{

}

void LeoRenderer::Model::DrawNode(
    LeoRenderer::Node *node,
    VkCommandBuffer commandBuffer,
    uint32_t renderFlags,
    VkPipelineLayout pipelineLayout,
    uint32_t bindImageSet)
{

}

void LeoRenderer::Model::Draw(
    VkCommandBuffer commandBuffer,
    uint32_t renderFlags,
    VkPipelineLayout pipelineLayout,
    uint32_t bindImageSet)
{

}

void LeoRenderer::Model::GetNodeDimensions(LeoRenderer::Node *node, glm::vec3 &min, glm::vec3 &max)
{

}

void LeoRenderer::Model::GetSceneDimensions()
{

}

void LeoRenderer::Model::UpdateAnimation(uint32_t index, float time)
{

}

LeoRenderer::Node *LeoRenderer::Model::FindNode(LeoRenderer::Node *parent, uint32_t index)
{
    return nullptr;
}

LeoRenderer::Node *LeoRenderer::Model::NodeFromIndex(uint32_t index)
{
    return nullptr;
}

void LeoRenderer::Model::PrepareNodeDescriptor(
    LeoRenderer::Node *node, VkDescriptorSetLayout descriptorSetLayout)
{

}

LeoRenderer::Texture *LeoRenderer::Model::GetTexture(uint32_t index)
{
    return nullptr;
}

void LeoRenderer::Model::CreateEmptyTexture(VkQueue transferQueue)
{

}
