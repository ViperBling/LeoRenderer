#include "VulkanRenderer.hpp"

void VulkanRenderer::GenerateBRDFLUT()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
    const int32_t dim = 512;

    //  Image
    VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &mTextures.mLUTBRDF.mImage));
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mDevice, mTextures.mLUTBRDF.mImage, &memReqs);
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &mTextures.mLUTBRDF.mDeviceMemory));
    VK_CHECK(vkBindImageMemory(mDevice, mTextures.mLUTBRDF.mImage, mTextures.mLUTBRDF.mDeviceMemory, 0));

    // View
    VkImageViewCreateInfo imageViewCI = LeoVK::Init::ImageViewCreateInfo();
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = format;
    imageViewCI.subresourceRange = {};
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.image = mTextures.mLUTBRDF.mImage;
    VK_CHECK(vkCreateImageView(mDevice, &imageViewCI, nullptr, &mTextures.mLUTBRDF.mView));

    // Sampler
    VkSamplerCreateInfo samplerCI = LeoVK::Init::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(mDevice, &samplerCI, nullptr, &mTextures.mLUTBRDF.mSampler));

    
}

void VulkanRenderer::GenerateCubeMaps()
{

}