#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <array>

#include "Utilities/AssetLoader.hpp"
#include "VKMemoryAllocator.hpp"
#include "VKShaderReflection.hpp"

namespace LeoVK
{
    struct ImageUsage
    {
        using Value = uint32_t;

        enum Bits : Value
        {
            UNKNOWN                          = (Value)vk::ImageUsageFlagBits{ },
            TRANSFER_SOURCE                  = (Value)vk::ImageUsageFlagBits::eTransferSrc,
            TRANSFER_DESTINATION             = (Value)vk::ImageUsageFlagBits::eTransferDst,
            SHADER_READ                      = (Value)vk::ImageUsageFlagBits::eSampled,
            STORAGE                          = (Value)vk::ImageUsageFlagBits::eStorage,
            COLOR_ATTACHMENT                 = (Value)vk::ImageUsageFlagBits::eColorAttachment,
            DEPTH_SPENCIL_ATTACHMENT         = (Value)vk::ImageUsageFlagBits::eDepthStencilAttachment,
            INPUT_ATTACHMENT                 = (Value)vk::ImageUsageFlagBits::eInputAttachment,
            FRAGMENT_SHADING_RATE_ATTACHMENT = (Value)vk::ImageUsageFlagBits::eFragmentShadingRateAttachmentKHR,
        };
    };

    enum class ImageView
    {
        NATIVE = 0,
        DEPTH_ONLY,
        STENCIL_ONLY,
    };

    struct ImageOptions
    {
        using Value = uint32_t;

        enum Bits : Value
        {
            DEFAULT = 0,
            MIPMAPS = 1 << 0,
            CUBEMAP = 1 << 1,
        };
    };

    class Image
    {
    public:
        Image() = default;
        Image(uint32_t width, uint32_t height, Format format, ImageUsage::Value usage, MemoryUsage memoryUsage, ImageOptions::Value options);
        Image(vk::Image image, uint32_t width, uint32_t height, Format format);
        Image(Image&& other) noexcept;
        Image& operator=(Image&& other) noexcept;
        virtual ~Image();

        void Init(uint32_t width, uint32_t height, Format format, ImageUsage::Value usage, MemoryUsage memoryUsage, ImageOptions::Value options);

        vk::ImageView GetNativeView(ImageView view) const;
        vk::ImageView GetNativeView(ImageView view, uint32_t layer) const;
        uint32_t GetMipLevelWidth(uint32_t mipLevel) const;
        uint32_t GetMipLevelHeight(uint32_t mipLevel) const;

        vk::Image GetNativeImage() const { return mImage; }
        Format GetFormat() const { return mFormat; }
        uint32_t GetWidth() const { return mExtent.width; }
        uint32_t GetHeight() const { return mExtent.height; }
        uint32_t GetMipLevelCount() const { return mMipLevelCount; }
        uint32_t GetLayerCount() const { return mLayerCount; }

    private:
        void Destroy();
        void InitViews(const vk::Image& image, Format format);

    private:
        struct ImageViews
        {
            vk::ImageView NativeView;
            vk::ImageView DepthOnlyView;
            vk::ImageView StencilOnlyView;
        };

        vk::Image mImage;
        ImageViews mImageViews;
        std::vector<ImageViews> mCubeImageViews;
        vk::Extent2D mExtent = {0u, 0u};
        uint32_t mMipLevelCount = 1;
        uint32_t mLayerCount = 1;
        Format mFormat = Format::UNDEFINED;
        VmaAllocation mAllocation = VK_NULL_HANDLE;
    };

    using ImageReference = std::reference_wrapper<const Image>;

    vk::ImageAspectFlags ImageFormatToImageAspect(Format format);
    vk::ImageLayout ImageUsageToImageLayout(ImageUsage::Bits usage);
    vk::AccessFlags ImageUsageToAccessFlags(ImageUsage::Bits usage);
    vk::PipelineStageFlags ImageUsageToPipelineStage(ImageUsage::Bits usage);

    vk::ImageSubresourceLayers GetDefaultImageSubresourceLayers(const Image& image);
    vk::ImageSubresourceLayers GetDefaultImageSubresourceLayers(const Image& image, uint32_t mipLevel, uint32_t layer);
    vk::ImageSubresourceRange GetDefaultImageSubresourceRange(const Image& image);

    uint32_t CalculateImageMipLevelCount(ImageOptions::Value options, uint32_t width, uint32_t height);
    uint32_t CalculateImageLayerCount(ImageOptions::Value options);
}