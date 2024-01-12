#include <cassert>

#include "VKImage.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    Image::Image(uint32_t width, uint32_t height, Format format, ImageUsage::Value usage, MemoryUsage memoryUsage, ImageOptions::Value options)
    {
        this->Init(width, height, format, usage, memoryUsage, options);
    }

    Image::Image(vk::Image image, uint32_t width, uint32_t height, Format format)
    {
        this->mExtent = vk::Extent2D{ width, height };
        this->mAllocation = {};
        this->InitViews(image, format);
    }

    Image::Image(Image &&other) noexcept
    {
        this->mImage = other.mImage;
        this->mImageViews = other.mImageViews;
        this->mCubeImageViews = std::move(other.mCubeImageViews);
        this->mExtent = other.mExtent;
        this->mFormat = other.mFormat;
        this->mAllocation = other.mAllocation;
        this->mMipLevelCount = other.mMipLevelCount;
        this->mLayerCount = other.mLayerCount;

        other.mImage = vk::Image();
        other.mImageViews = { };
        other.mCubeImageViews.clear();
        other.mExtent = vk::Extent2D{ 0u, 0u };
        other.mFormat = Format::UNDEFINED;
        other.mAllocation = {};
        other.mMipLevelCount = 1;
        other.mLayerCount = 1;
    }

    Image &Image::operator=(Image &&other) noexcept
    {
        this->Destroy();

        this->mImage = other.mImage;
        this->mImageViews = other.mImageViews;
        this->mCubeImageViews = std::move(other.mCubeImageViews);
        this->mExtent = other.mExtent;
        this->mFormat = other.mFormat;
        this->mAllocation = other.mAllocation;
        this->mMipLevelCount = other.mMipLevelCount;
        this->mLayerCount = other.mLayerCount;

        other.mImage = vk::Image();
        other.mImageViews = { };
        other.mCubeImageViews.clear();
        other.mExtent = vk::Extent2D{ 0u, 0u };
        other.mFormat = Format::UNDEFINED;
        other.mAllocation = {};
        other.mMipLevelCount = 1;
        other.mLayerCount = 1;

        return *this;
    }

    Image::~Image()
    {
        this->Destroy();
    }

    void Image::Init(uint32_t width, uint32_t height, Format format, ImageUsage::Value usage, MemoryUsage memoryUsage, ImageOptions::Value options)
    {
        this->mMipLevelCount = CalculateImageMipLevelCount(options, width, height);
        this->mLayerCount = CalculateImageLayerCount(options);

        vk::ImageCreateInfo imageCI {};
        imageCI.setImageType(vk::ImageType::e2D);
        imageCI.setFormat(ToNative(format));
        imageCI.setExtent(vk::Extent3D{ width, height, 1 });
        imageCI.setSamples(vk::SampleCountFlagBits::e1);
        imageCI.setMipLevels(this->GetMipLevelCount());
        imageCI.setArrayLayers(this->GetLayerCount());
        imageCI.setTiling(vk::ImageTiling::eOptimal);
        imageCI.setUsage(static_cast<vk::ImageUsageFlags>(usage));
        imageCI.setSharingMode(vk::SharingMode::eExclusive);
        imageCI.setInitialLayout(vk::ImageLayout::eUndefined);

        if (options & ImageOptions::CUBEMAP)
        {
            imageCI.setFlags(vk::ImageCreateFlagBits::eCubeCompatible);
        }
        this->mExtent = vk::Extent2D{ width, height };
        this->mAllocation = AllocateImage(imageCI, memoryUsage, &this->mImage);
        this->InitViews(this->mImage, format);
    }

    vk::ImageView Image::GetNativeView(ImageView view) const
    {
        switch (view)
        {
        case LeoVK::ImageView::NATIVE:
            return this->mImageViews.NativeView;
        case LeoVK::ImageView::DEPTH_ONLY:
            return this->mImageViews.DepthOnlyView;
        case LeoVK::ImageView::STENCIL_ONLY:
            return this->mImageViews.StencilOnlyView;
        default:
            assert(false);
            return this->mImageViews.NativeView;
        }
    }

    vk::ImageView Image::GetNativeView(ImageView view, uint32_t layer) const
    {
        if (this->mLayerCount == 1) return this->GetNativeView(view);

        switch (view)
        {
        case LeoVK::ImageView::NATIVE:
            return this->mCubeImageViews[layer].NativeView;
        case LeoVK::ImageView::DEPTH_ONLY:
            return this->mCubeImageViews[layer].DepthOnlyView;
        case LeoVK::ImageView::STENCIL_ONLY:
            return this->mCubeImageViews[layer].StencilOnlyView;
        default:
            assert(false);
            return this->mCubeImageViews[layer].NativeView;
        }
    }

    uint32_t Image::GetMipLevelWidth(uint32_t mipLevel) const
    {
        auto safeWidth = std::max(this->GetWidth(), 1u << mipLevel);
        return safeWidth >> mipLevel;
    }
    
    uint32_t Image::GetMipLevelHeight(uint32_t mipLevel) const
    {
        auto safeHeight = std::max(this->GetHeight(), 1u << mipLevel);
        return safeHeight >> mipLevel;
    }

    void Image::Destroy()
    {
        if ((bool)this->mImage)
        {
            if ((bool)this->mAllocation)
            {
                DestroyImage(this->mImage, this->mAllocation);
            }
            GetCurrentVulkanContext().GetDevice().destroyImageView(this->mImageViews.NativeView);
            if ((bool)this->mImageViews.DepthOnlyView)
            {
                GetCurrentVulkanContext().GetDevice().destroyImageView(this->mImageViews.DepthOnlyView);
            }
            if ((bool)this->mImageViews.StencilOnlyView)
            {
                GetCurrentVulkanContext().GetDevice().destroyImageView(this->mImageViews.StencilOnlyView);
            }

            for (auto& imageViewLayer : this->mCubeImageViews)
            {
                GetCurrentVulkanContext().GetDevice().destroyImageView(imageViewLayer.NativeView);
                if ((bool)imageViewLayer.DepthOnlyView)
                {
                    GetCurrentVulkanContext().GetDevice().destroyImageView(imageViewLayer.DepthOnlyView);
                }
                if ((bool)imageViewLayer.StencilOnlyView)
                {
                    GetCurrentVulkanContext().GetDevice().destroyImageView(imageViewLayer.StencilOnlyView);
                }
            }
            this->mImage = vk::Image();
            this->mImageViews = { };
            this->mCubeImageViews.clear();
            this->mExtent = vk::Extent2D {0u, 0u};
            this->mMipLevelCount = 1;
            this->mLayerCount = 1;
        }
    }

    void Image::InitViews(const vk::Image &image, Format format)
    {
        this->mImage = image;
        this->mFormat = format;

        auto& vulkan = GetCurrentVulkanContext();
        auto subresourceRange = GetDefaultImageSubresourceRange(*this);

        vk::ImageViewCreateInfo imageViewCI {};
        imageViewCI.setImage(this->mImage);
        imageViewCI.setViewType(GetImageViewType(*this));
        imageViewCI.setFormat(ToNative(format));
        imageViewCI.setComponents(vk::ComponentMapping{
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
        });
        imageViewCI.setSubresourceRange(subresourceRange);

        auto nativeSubresourceRange = GetDefaultImageSubresourceRange(*this);
        imageViewCI.setSubresourceRange(nativeSubresourceRange);
        this->mImageViews.NativeView = vulkan.GetDevice().createImageView(imageViewCI);

        auto depthSubresourceRange = GetDefaultImageSubresourceRange(*this);
        depthSubresourceRange.setAspectMask(depthSubresourceRange.aspectMask & vk::ImageAspectFlagBits::eDepth);
        if (depthSubresourceRange.aspectMask != vk::ImageAspectFlags{})
        {
            imageViewCI.setSubresourceRange(depthSubresourceRange);
            this->mImageViews.DepthOnlyView = vulkan.GetDevice().createImageView(imageViewCI);
        }

        auto stencilSubresourceRange = GetDefaultImageSubresourceRange(*this);
        stencilSubresourceRange.setAspectMask(stencilSubresourceRange.aspectMask & vk::ImageAspectFlagBits::eStencil);
        if (stencilSubresourceRange.aspectMask != vk::ImageAspectFlags{})
        {
            imageViewCI.setSubresourceRange(stencilSubresourceRange);
            this->mImageViews.StencilOnlyView = vulkan.GetDevice().createImageView(imageViewCI);
        }

        if (this->mLayerCount > 1)
        {
            this->mCubeImageViews.resize(this->mLayerCount);
            imageViewCI.setViewType(vk::ImageViewType::e2DArray);
        }
        uint32_t layer = 0;
        for (auto& imageViewLayer : this->mCubeImageViews)
        {
            auto nativeSubresourceRange = GetDefaultImageSubresourceRange(*this);
            nativeSubresourceRange.setBaseArrayLayer(layer);
            nativeSubresourceRange.setLayerCount(VK_REMAINING_ARRAY_LAYERS);
            imageViewCI.setSubresourceRange(nativeSubresourceRange);
            imageViewLayer.NativeView = vulkan.GetDevice().createImageView(imageViewCI);

            auto depthSubresourceRange = GetDefaultImageSubresourceRange(*this);
            depthSubresourceRange.setBaseArrayLayer(layer);
            depthSubresourceRange.setLayerCount(VK_REMAINING_ARRAY_LAYERS);
            depthSubresourceRange.setAspectMask(depthSubresourceRange.aspectMask & vk::ImageAspectFlagBits::eDepth);
            if (depthSubresourceRange.aspectMask != vk::ImageAspectFlags {})
            {
                imageViewCI.setSubresourceRange(depthSubresourceRange);
                imageViewLayer.DepthOnlyView = vulkan.GetDevice().createImageView(imageViewCI);
            }

            auto stencilSubresourceRange = GetDefaultImageSubresourceRange(*this);
            stencilSubresourceRange.setBaseArrayLayer(layer);
            stencilSubresourceRange.setLayerCount(VK_REMAINING_ARRAY_LAYERS);
            stencilSubresourceRange.setAspectMask(stencilSubresourceRange.aspectMask & vk::ImageAspectFlagBits::eStencil);
            if (stencilSubresourceRange.aspectMask != vk::ImageAspectFlags {})
            {
                imageViewCI.setSubresourceRange(stencilSubresourceRange);
                imageViewLayer.StencilOnlyView = vulkan.GetDevice().createImageView(imageViewCI);
            }
            layer++;
        }
    }

    // ======================================= Public Function ======================================= //

    vk::ImageViewType GetImageViewType(const Image& image)
    {
        if (image.GetLayerCount() == 1)
            return vk::ImageViewType::e2D;
        else
            return vk::ImageViewType::eCube;
    }

    vk::ImageAspectFlags ImageFormatToImageAspect(Format format)
    {
        switch (format)
        {
        case Format::D16_UNORM:
            return vk::ImageAspectFlagBits::eDepth;
        case Format::X8D24_UNORM_PACK_32:
            return vk::ImageAspectFlagBits::eDepth;
        case Format::D32_SFLOAT:
            return vk::ImageAspectFlagBits::eDepth;
        case Format::D16_UNORM_S8_UINT:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        case Format::D24_UNORM_S8_UINT:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        case Format::D32_SFLOAT_S8_UINT:
            return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        default:
            return vk::ImageAspectFlagBits::eColor;
        }
    }

    vk::ImageLayout ImageUsageToImageLayout(ImageUsage::Bits usage)
    {
        switch (usage)
        {
        case LeoVK::ImageUsage::UNKNOWN:
            return vk::ImageLayout::eUndefined;
        case LeoVK::ImageUsage::TRANSFER_SOURCE:
            return vk::ImageLayout::eTransferSrcOptimal;
        case LeoVK::ImageUsage::TRANSFER_DESTINATION:
            return vk::ImageLayout::eTransferDstOptimal;
        case LeoVK::ImageUsage::SHADER_READ:
            return vk::ImageLayout::eShaderReadOnlyOptimal;
        case LeoVK::ImageUsage::STORAGE:
            return vk::ImageLayout::eGeneral;
        case LeoVK::ImageUsage::COLOR_ATTACHMENT:
            return vk::ImageLayout::eColorAttachmentOptimal;
        case LeoVK::ImageUsage::DEPTH_STENCIL_ATTACHMENT:
            return vk::ImageLayout::eDepthStencilAttachmentOptimal;
        case LeoVK::ImageUsage::INPUT_ATTACHMENT:
            return vk::ImageLayout::eAttachmentOptimalKHR; // TODO: is it ok?
        case LeoVK::ImageUsage::FRAGMENT_SHADING_RATE_ATTACHMENT:
            return vk::ImageLayout::eFragmentShadingRateAttachmentOptimalKHR;
        default:
            assert(false);
            return vk::ImageLayout::eUndefined;
        }
    }

    vk::AccessFlags ImageUsageToAccessFlags(ImageUsage::Bits usage)
    {
        switch (usage)
        {
        case ImageUsage::UNKNOWN:
            return vk::AccessFlags{ };
        case ImageUsage::TRANSFER_SOURCE:
            return vk::AccessFlagBits::eTransferRead;
        case ImageUsage::TRANSFER_DESTINATION:
            return vk::AccessFlagBits::eTransferWrite;
        case ImageUsage::SHADER_READ:
            return vk::AccessFlagBits::eShaderRead;
        case ImageUsage::STORAGE:
            return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite; // TODO: what if storage is not read or write?
        case ImageUsage::COLOR_ATTACHMENT:
            return vk::AccessFlagBits::eColorAttachmentWrite;
        case ImageUsage::DEPTH_STENCIL_ATTACHMENT:
            return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
        case ImageUsage::INPUT_ATTACHMENT:
            return vk::AccessFlagBits::eInputAttachmentRead;
        case ImageUsage::FRAGMENT_SHADING_RATE_ATTACHMENT:
            return vk::AccessFlagBits::eFragmentShadingRateAttachmentReadKHR;
        default:
            assert(false);
            return vk::AccessFlags{ };
        }
    }

    vk::PipelineStageFlags ImageUsageToPipelineStage(ImageUsage::Bits usage)
    {
        switch (usage)
        {
        case ImageUsage::UNKNOWN:
            return vk::PipelineStageFlagBits::eTopOfPipe;
        case ImageUsage::TRANSFER_SOURCE:
            return vk::PipelineStageFlagBits::eTransfer;
        case ImageUsage::TRANSFER_DESTINATION:
            return vk::PipelineStageFlagBits::eTransfer;
        case ImageUsage::SHADER_READ:
            return vk::PipelineStageFlagBits::eFragmentShader; // TODO: whats for vertex shader reads?
        case ImageUsage::STORAGE:
            return vk::PipelineStageFlagBits::eFragmentShader; // TODO: whats for vertex shader reads?
        case ImageUsage::COLOR_ATTACHMENT:
            return vk::PipelineStageFlagBits::eColorAttachmentOutput;
        case ImageUsage::DEPTH_STENCIL_ATTACHMENT:
            return vk::PipelineStageFlagBits::eEarlyFragmentTests; // TODO: whats for late fragment test?
        case ImageUsage::INPUT_ATTACHMENT:
            return vk::PipelineStageFlagBits::eFragmentShader; // TODO: check if at least works
        case ImageUsage::FRAGMENT_SHADING_RATE_ATTACHMENT:
            return vk::PipelineStageFlagBits::eFragmentShadingRateAttachmentKHR;
        default:
            assert(false);
            return vk::PipelineStageFlags{ };
        }
    }

    vk::ImageSubresourceLayers GetDefaultImageSubresourceLayers(const Image &image)
    {
        auto subresourceRange = GetDefaultImageSubresourceRange(image);
        return vk::ImageSubresourceLayers{
            subresourceRange.aspectMask,
            subresourceRange.baseMipLevel,
            subresourceRange.baseArrayLayer,
            subresourceRange.layerCount
        };
    }

    vk::ImageSubresourceLayers GetDefaultImageSubresourceLayers(const Image &image, uint32_t mipLevel, uint32_t layer)
    {
        return vk::ImageSubresourceLayers{
            ImageFormatToImageAspect(image.GetFormat()),
            mipLevel,
            layer,
            1
        };
    }

    vk::ImageSubresourceRange GetDefaultImageSubresourceRange(const Image &image)
    {
        return vk::ImageSubresourceRange{
            ImageFormatToImageAspect(image.GetFormat()),
            0, // base mip level
            image.GetMipLevelCount(),
            0, // base layer
            image.GetLayerCount()
        };
    }

    uint32_t CalculateImageMipLevelCount(ImageOptions::Value options, uint32_t width, uint32_t height)
    {
        if (options & ImageOptions::MIPMAPS)
            return (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
        else
            return 1;
    }

    uint32_t CalculateImageLayerCount(ImageOptions::Value options)
    {
        if (options & ImageOptions::CUBEMAP)
            return 6;
        else
            return 1;
    }
}