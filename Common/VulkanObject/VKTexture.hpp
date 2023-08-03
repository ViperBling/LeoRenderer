#pragma once

#include "ProjectPCH.hpp"

#include "VKBuffer.hpp"
#include "VKDevice.hpp"
#include "VKTools.hpp"

namespace LeoVK
{
    struct TextureSampler
    {
        VkFilter mMagFilter;
        VkFilter mMinFilter;
        VkSamplerAddressMode mAddressModeU;
        VkSamplerAddressMode mAddressModeV;
        VkSamplerAddressMode mAddressModeW;
    };

    class Texture
    {
    public:
        void Destroy();
        void UpdateDescriptor();
        ktxResult LoadKTXFile(std::string filename, ktxTexture** target);

    public:
        LeoVK::VulkanDevice*  mpDevice;
        VkImage               mImage = VK_NULL_HANDLE;
        VkImageLayout         mImageLayout;
        VkDeviceMemory        mDeviceMemory;
        VkImageView           mView;
        uint32_t              mWidth, mHeight;
        uint32_t              mMipLevels;
        uint32_t              mLayerCount;
        VkDescriptorImageInfo mDescriptor;
        VkSampler             mSampler;
    };

    class Texture2D : public Texture
    {
    public:
        void LoadFromBuffer(
            void*                   buffer,
            VkDeviceSize            bufferSize,
            VkFormat                format,
            uint32_t                texWidth,
            uint32_t                texHeight,
            LeoVK::VulkanDevice*    device,
            VkQueue                 copyQueue,
            VkFilter                filter          = VK_FILTER_LINEAR,
            VkImageUsageFlags       imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout           imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        void LoadFromFile(
            std::string         filename,
            VkFormat            format,
            LeoVK::VulkanDevice *device,
            VkQueue             copyQueue,
            VkImageUsageFlags   imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout       imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

    class Texture2DArray : public Texture
    {
    public:
        void LoadFromFile(
            std::string         filename,
            VkFormat            format,
            LeoVK::VulkanDevice *device,
            VkQueue             copyQueue,
            VkImageUsageFlags   imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout       imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };

    class TextureCube : public Texture
    {
    public:
        void LoadFromFile(
            std::string         filename,
            VkFormat            format,
            LeoVK::VulkanDevice *device,
            VkQueue             copyQueue,
            VkImageUsageFlags   imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout       imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    };
}