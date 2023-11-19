#include "VKCommandBuffer.hpp"
#include "VKRenderPass.hpp"
#include "VKImage.hpp"
#include "VKBuffer.hpp"

namespace LeoVK
{
    vk::Filter BlitFilterToNative(BlitFilter filter)
    {
        switch (filter)
        {
        case BlitFilter::NEAREST:
            return vk::Filter::eNearest;
        case BlitFilter::LINEAR:    
            return vk::Filter::eLinear;
        case BlitFilter::CUBIC:
            return vk::Filter::eCubicEXT;
        default:
            assert(false);
            return vk::Filter::eNearest;
        }
    }

    vk::ShaderStageFlags PipelineTypeToShaderStages(vk::PipelineBindPoint pipelineType);

    void CommandBuffer::Begin()
    {
        vk::CommandBufferBeginInfo cmdBufferBI {};
        cmdBufferBI.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        this->mCmdBuffer.begin(cmdBufferBI);
    }

    void CommandBuffer::End()
    {
        mCmdBuffer.end();
    }

    void CommandBuffer::BeginPass(const NativeRenderPass &renderPass)
    {
        if ((bool)renderPass.RenderPassHandle)
        {
            vk::RenderPassBeginInfo rpBI {};
            rpBI
                .setRenderPass(renderPass.RenderPassHandle)
                .setRenderArea(renderPass.RenderArea)
                .setFramebuffer(renderPass.Framebuffer)
                .setClearValues(renderPass.ClearValues);
            this->mCmdBuffer.beginRenderPass(rpBI, vk::SubpassContents::eInline);
        }

        vk::Pipeline pipeline = renderPass.Pipeline;
        vk::PipelineLayout pipelineLayout = renderPass.PipelineLayout;
        vk::PipelineBindPoint pipelindType = renderPass.PipelineType;
        vk::DescriptorSet descSet = renderPass.DescriptorSet;
        
        if ((bool)pipeline)
        {
            this->mCmdBuffer.bindPipeline(pipelindType, pipeline);
        }
        if ((bool)descSet)
        {
            this->mCmdBuffer.bindDescriptorSets(pipelindType, pipelineLayout, 0, descSet, {});
        }
    }

    void CommandBuffer::EndPass(const NativeRenderPass &renderPass)
    {
        if ((bool)renderPass.RenderPassHandle)
        {
            this->mCmdBuffer.endRenderPass();
        }
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount)
    {
        this->mCmdBuffer.draw(vertexCount, instanceCount, 0, 0);
    }

    void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
    {
        this->mCmdBuffer.draw(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount)
    {
        this->mCmdBuffer.drawIndexed(indexCount, instanceCount, 0, 0, 0);
    }

    void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
    {
        this->mCmdBuffer.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void CommandBuffer::BindIndexBufferUInt32(const Buffer &indexBuffer)
    {
        this->mCmdBuffer.bindIndexBuffer(indexBuffer.GetNativeBuffer(), 0, vk::IndexType::eUint32);
    }

    void CommandBuffer::BindIndexBufferUInt16(const Buffer &indexBuffer)
    {
        this->mCmdBuffer.bindIndexBuffer(indexBuffer.GetNativeBuffer(), 0, vk::IndexType::eUint16);
    }

    void CommandBuffer::SetViewport(const Viewport &viewport)
    {
        this->mCmdBuffer.setViewport(0, vk::Viewport{ 
            viewport.OffsetWidth, 
            viewport.OffsetHeight + viewport.Height, 
            viewport.Width, 
            -viewport.Height, // inverse viewport height to invert coordinate system
            viewport.MinDepth, 
            viewport.MaxDepth 
        });
    }

    void CommandBuffer::SetScissor(const Rect2D &scissor)
    {
        this->mCmdBuffer.setScissor(0, vk::Rect2D{
            vk::Offset2D{
                scissor.OffsetWidth,
                scissor.OffSetHeight
            },
            vk::Extent2D{
                scissor.Width,
                scissor.Height
            }
        });
    }

    void CommandBuffer::SetRenderArea(const Image &image)
    {
        this->SetViewport(Viewport{ 0.0f, 0.0f, (float)image.GetWidth(), (float)image.GetHeight(), 0.0f, 1.0f });
        this->SetScissor(Rect2D{ 0, 0, image.GetWidth(), image.GetHeight() });
    }

    void CommandBuffer::PushConstants(const NativeRenderPass &renderPass, const uint8_t *data, size_t size)
    {
        constexpr size_t MaxPushConstantByteSize = 128;
        std::array<uint8_t, MaxPushConstantByteSize> pushConstants = { };

        std::memcpy(pushConstants.data(), data, size);

        this->mCmdBuffer.pushConstants(
            renderPass.PipelineLayout,
            PipelineTypeToShaderStages(renderPass.PipelineType),
            0,
            pushConstants.size(),
            pushConstants.data()
        );
    }

    void CommandBuffer::Dispatch(uint32_t x, uint32_t y, uint32_t z)
    {
        this->mCmdBuffer.dispatch(x, y, z);
    }

    void CommandBuffer::CopyImage(const ImageInfo &source, const ImageInfo &dest)
    {
        auto srcRange = GetDefaultImageSubresourceRange(source.Resource.get());
        auto distRange = GetDefaultImageSubresourceRange(dest.Resource.get());

        std::array<vk::ImageMemoryBarrier, 2> barriers;
        size_t barrierCount = 0;

        vk::ImageMemoryBarrier toTransSrcBarrier;
        toTransSrcBarrier.srcAccessMask = ImageUsageToAccessFlags(source.Usage);
        toTransSrcBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
        toTransSrcBarrier.oldLayout = ImageUsageToImageLayout(source.Usage);
        toTransSrcBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
        toTransSrcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransSrcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransSrcBarrier.image = source.Resource.get().GetNativeImage();
        toTransSrcBarrier.subresourceRange = srcRange;
        
        vk::ImageMemoryBarrier toTransDstBarrier;
        toTransDstBarrier.srcAccessMask = ImageUsageToAccessFlags(dest.Usage);
        toTransDstBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
        toTransDstBarrier.oldLayout = ImageUsageToImageLayout(dest.Usage);
        toTransDstBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        toTransDstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransDstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransDstBarrier.image = dest.Resource.get().GetNativeImage();
        toTransDstBarrier.subresourceRange = distRange;

        if (source.Usage != ImageUsage::TRANSFER_SOURCE) barriers[barrierCount++] = toTransSrcBarrier;
        if (dest.Usage != ImageUsage::TRANSFER_DISTINATION) barriers[barrierCount++] = toTransDstBarrier;

        if (barrierCount > 0)
        {
            this->mCmdBuffer.pipelineBarrier(
                ImageUsageToPipelineStage(source.Usage) | ImageUsageToPipelineStage(dest.Usage),
                vk::PipelineStageFlagBits::eTransfer,
                { }, // dependency flags
                0, nullptr, // memory barriers
                0, nullptr, // buffer barriers
                barrierCount, barriers.data()
            );
        }

        auto sourceLayers = GetDefaultImageSubresourceLayers(source.Resource.get(), source.MipLevel, source.Layer);
        auto destLayers = GetDefaultImageSubresourceLayers(dest.Resource.get(), dest.MipLevel, dest.Layer);

        vk::ImageCopy imageCopyInfo;
        imageCopyInfo
            .setSrcOffset(0)
            .setDstOffset(0)
            .setSrcSubresource(sourceLayers)
            .setDstSubresource(destLayers)
            .setExtent(vk::Extent3D{ 
                dest.Resource.get().GetMipLevelWidth(dest.MipLevel), 
                dest.Resource.get().GetMipLevelHeight(dest.MipLevel),
                1 
            });

        this->mCmdBuffer.copyImage(
            source.Resource.get().GetNativeImage(), 
            vk::ImageLayout::eTransferSrcOptimal, 
            dest.Resource.get().GetNativeImage(), 
            vk::ImageLayout::eTransferDstOptimal, 
            imageCopyInfo
        );
    }

    void CommandBuffer::CopyBuffer(const BufferInfo &source, const BufferInfo &dest, size_t byteSize)
    {
        assert(source.Resource.get().GetSize() >= source.Offset + byteSize);
        assert(dest.Resource.get().GetSize() >= dest.Offset + byteSize);

        vk::BufferCopy bufferCopyInfo;
        bufferCopyInfo
            .setDstOffset(dest.Offset)
            .setSize(byteSize)
            .setSrcOffset(source.Offset);

        this->mCmdBuffer.copyBuffer(source.Resource.get().GetNativeBuffer(), dest.Resource.get().GetNativeBuffer(), bufferCopyInfo);
    }

    void CommandBuffer::CopyBufferToImage(const BufferInfo &source, const ImageInfo &dest)
    {
        if (dest.Usage != ImageUsage::TRANSFER_DISTINATION)
        {
            auto destRange = GetDefaultImageSubresourceRange(dest.Resource.get());

            vk::ImageMemoryBarrier toTransferDstBarrier;
            toTransferDstBarrier
                .setSrcAccessMask(ImageUsageToAccessFlags(dest.Usage))
                .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setOldLayout(ImageUsageToImageLayout(dest.Usage))
                .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(dest.Resource.get().GetNativeImage())
                .setSubresourceRange(destRange);

            this->mCmdBuffer.pipelineBarrier(
                ImageUsageToPipelineStage(dest.Usage),
                vk::PipelineStageFlagBits::eTransfer,
                { }, // dependency flags
                { }, // memory barriers
                { }, // buffer barriers
                toTransferDstBarrier
            );
        }

        auto destLayers = GetDefaultImageSubresourceLayers(dest.Resource.get(), dest.MipLevel, dest.Layer);

        vk::BufferImageCopy bufferToImageCopyInfo;
        bufferToImageCopyInfo
            .setBufferOffset(source.Offset)
            .setBufferImageHeight(0)
            .setBufferRowLength(0)
            .setImageSubresource(destLayers)
            .setImageOffset(vk::Offset3D{ 0, 0, 0 })
            .setImageExtent(vk::Extent3D{ 
                dest.Resource.get().GetMipLevelWidth(dest.MipLevel), 
                dest.Resource.get().GetMipLevelHeight(dest.MipLevel), 
                1 
            });

        this->mCmdBuffer.copyBufferToImage(
            source.Resource.get().GetNativeBuffer(), 
            dest.Resource.get().GetNativeImage(), 
            vk::ImageLayout::eTransferDstOptimal, 
            bufferToImageCopyInfo
        );
    }

    void CommandBuffer::CopyImageToBuffer(const ImageInfo &source, const BufferInfo &dest)
    {
        if (source.Usage != ImageUsage::TRANSFER_SOURCE)
        {
            auto sourceRange = GetDefaultImageSubresourceRange(source.Resource.get());

            vk::ImageMemoryBarrier toTransferSrcBarrier;
            toTransferSrcBarrier
                .setSrcAccessMask(ImageUsageToAccessFlags(source.Usage))
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
                .setOldLayout(ImageUsageToImageLayout(source.Usage))
                .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(source.Resource.get().GetNativeImage())
                .setSubresourceRange(sourceRange);

            this->mCmdBuffer.pipelineBarrier(
                ImageUsageToPipelineStage(source.Usage),
                vk::PipelineStageFlagBits::eTransfer,
                { }, // dependency flags
                { }, // memory barriers
                { }, // buffer barriers
                toTransferSrcBarrier
            );
        }

        auto sourceLayers = GetDefaultImageSubresourceLayers(source.Resource.get(), source.MipLevel, source.Layer);

        vk::BufferImageCopy imageToBufferCopyInfo;
        imageToBufferCopyInfo
            .setBufferOffset(dest.Offset)
            .setBufferImageHeight(0)
            .setBufferRowLength(0)
            .setImageSubresource(sourceLayers)
            .setImageOffset(vk::Offset3D{ 0, 0, 0 })
            .setImageExtent(vk::Extent3D{ 
                source.Resource.get().GetMipLevelWidth(source.MipLevel), 
                source.Resource.get().GetMipLevelHeight(source.MipLevel), 
                1 
            });

        this->mCmdBuffer.copyImageToBuffer(
            source.Resource.get().GetNativeImage(), 
            vk::ImageLayout::eTransferSrcOptimal, 
            dest.Resource.get().GetNativeBuffer(), imageToBufferCopyInfo);
    }

    void CommandBuffer::BlitImage(const Image &source, ImageUsage::Bits sourceUsage, const Image &dest, ImageUsage::Bits destUsage, BlitFilter filter)
    {
        auto sourceRange = GetDefaultImageSubresourceRange(source);
        auto destRange = GetDefaultImageSubresourceRange(dest);

        std::array<vk::ImageMemoryBarrier, 2> barriers;
        size_t barrierCount = 0;

        vk::ImageMemoryBarrier toTransferSrcBarrier;
        toTransferSrcBarrier
            .setSrcAccessMask(ImageUsageToAccessFlags(sourceUsage))
            .setDstAccessMask(vk::AccessFlagBits::eTransferRead)
            .setOldLayout(ImageUsageToImageLayout(sourceUsage))
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(source.GetNativeImage())
            .setSubresourceRange(sourceRange);

        vk::ImageMemoryBarrier toTransferDstBarrier;
        toTransferDstBarrier
            .setSrcAccessMask(ImageUsageToAccessFlags(destUsage))
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setOldLayout(ImageUsageToImageLayout(destUsage))
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(dest.GetNativeImage())
            .setSubresourceRange(destRange);

        if (sourceUsage != ImageUsage::TRANSFER_SOURCE)
            barriers[barrierCount++] = toTransferSrcBarrier;
        if (destUsage != ImageUsage::TRANSFER_DISTINATION)
            barriers[barrierCount++] = toTransferDstBarrier;

        if (barrierCount > 0)
        {
            this->mCmdBuffer.pipelineBarrier(
                ImageUsageToPipelineStage(sourceUsage) | ImageUsageToPipelineStage(destUsage),
                vk::PipelineStageFlagBits::eTransfer,
                { }, // dependency flags
                0, nullptr, // memory barriers
                0, nullptr, // buffer barriers
                barrierCount, barriers.data()
            );
        }

        auto sourceLayers = GetDefaultImageSubresourceLayers(source);
        auto destLayers = GetDefaultImageSubresourceLayers(dest);

        vk::ImageBlit imageBlitInfo;
        imageBlitInfo
            .setSrcOffsets({
                vk::Offset3D{ 0, 0, 0 },
                vk::Offset3D{ (int32_t)source.GetWidth(), (int32_t)source.GetHeight(), 1 }
            })
            .setDstOffsets({
                vk::Offset3D{ 0, 0, 0 },
                vk::Offset3D{ (int32_t)dest.GetWidth(), (int32_t)dest.GetHeight(), 1 }
            })
            .setSrcSubresource(sourceLayers)
            .setDstSubresource(destLayers);

        this->mCmdBuffer.blitImage(
            source.GetNativeImage(),
            vk::ImageLayout::eTransferSrcOptimal,
            dest.GetNativeImage(),
            vk::ImageLayout::eTransferDstOptimal,
            imageBlitInfo,
            BlitFilterToNative(filter)
        );
    }

    void CommandBuffer::GenerateMipLevels(const Image &image, ImageUsage::Bits initialUsage, BlitFilter filter)
    {
        if (image.GetMipLevelCount() < 2) return;

        auto srcRange = GetDefaultImageSubresourceRange(image);
        auto dstRange = GetDefaultImageSubresourceRange(image);
        auto srcLayers = GetDefaultImageSubresourceLayers(image);
        auto dstLayers = GetDefaultImageSubresourceLayers(image);
        auto srcUsage = initialUsage;
        uint32_t srcWidth = image.GetWidth();
        uint32_t srcHeight = image.GetHeight();
        uint32_t dstWidth = image.GetWidth();
        uint32_t dstHeight = image.GetHeight();

        for (size_t i = 0; i + 1 < image.GetMipLevelCount(); i++)
        {
            srcWidth = dstWidth;
            srcHeight = dstHeight;
            dstWidth = std::max(srcWidth / 2, 1u);
            dstHeight = std::max(srcHeight / 2, 1u);
            
            srcLayers.setMipLevel(i);
            srcRange.setBaseMipLevel(i);
            srcRange.setLevelCount(1);

            dstLayers.setMipLevel(i + 1);
            dstRange.setBaseMipLevel(i + 1);
            dstRange.setLevelCount(1);

            std::array<vk::ImageMemoryBarrier, 2> imageBarriers;
            imageBarriers[0] // to transfer source
                .setSrcAccessMask(ImageUsageToAccessFlags(srcUsage))
                .setDstAccessMask(ImageUsageToAccessFlags(ImageUsage::TRANSFER_SOURCE))
                .setOldLayout(ImageUsageToImageLayout(srcUsage))
                .setNewLayout(ImageUsageToImageLayout(ImageUsage::TRANSFER_SOURCE))
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image.GetNativeImage())
                .setSubresourceRange(srcRange);

            imageBarriers[1] // to transfer distance
                .setSrcAccessMask(ImageUsageToAccessFlags(ImageUsage::UNKNOWN))
                .setDstAccessMask(ImageUsageToAccessFlags(ImageUsage::TRANSFER_DISTINATION))
                .setOldLayout(ImageUsageToImageLayout(ImageUsage::UNKNOWN))
                .setNewLayout(ImageUsageToImageLayout(ImageUsage::TRANSFER_DISTINATION))
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(image.GetNativeImage())
                .setSubresourceRange(dstRange);

            this->mCmdBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eTransfer,
                vk::PipelineStageFlagBits::eTransfer,
                { }, // dependencies
                { }, // memory barriers
                { }, // buffer barriers,
                imageBarriers
            );
            srcUsage = ImageUsage::TRANSFER_DISTINATION;

            vk::ImageBlit imageBlitInfo;
            imageBlitInfo
                .setSrcOffsets({
                    vk::Offset3D{ 0, 0, 0 },
                    vk::Offset3D{ (int32_t)srcWidth, (int32_t)srcHeight, 1 }
                })
                .setDstOffsets({
                    vk::Offset3D{ 0, 0, 0 },
                    vk::Offset3D{ (int32_t)dstWidth, (int32_t)dstHeight, 1 }
                })
                .setSrcSubresource(srcLayers)
                .setDstSubresource(dstLayers);

            this->mCmdBuffer.blitImage(
                image.GetNativeImage(),
                vk::ImageLayout::eTransferSrcOptimal,
                image.GetNativeImage(),
                vk::ImageLayout::eTransferDstOptimal,
                imageBlitInfo,
                BlitFilterToNative(filter)
            );
        }

        auto mipLevelsSubresourceRange = GetDefaultImageSubresourceRange(image);
        mipLevelsSubresourceRange.setLevelCount(mipLevelsSubresourceRange.levelCount - 1);
        vk::ImageMemoryBarrier mipLevelsTransfer;
        mipLevelsTransfer
            .setSrcAccessMask(vk::AccessFlagBits::eTransferRead)
            .setDstAccessMask(vk::AccessFlagBits::eTransferWrite)
            .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image.GetNativeImage())
            .setSubresourceRange(mipLevelsSubresourceRange);

        this->mCmdBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer,
            { }, // dependecies
            { }, // memory barriers
            { }, // buffer barriers
            mipLevelsTransfer
        );
    }

    static vk::ImageMemoryBarrier GetImageMemoryBarrier(const Image& image, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout)
    {
        auto subresourceRange = GetDefaultImageSubresourceRange(image);
        vk::ImageMemoryBarrier barrier;
        barrier
            .setSrcAccessMask(ImageUsageToAccessFlags(oldLayout))
            .setDstAccessMask(ImageUsageToAccessFlags(newLayout))
            .setOldLayout(ImageUsageToImageLayout(oldLayout))
            .setNewLayout(ImageUsageToImageLayout(newLayout))
            .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
            .setImage(image.GetNativeImage())
            .setSubresourceRange(subresourceRange);

        return barrier;
    }

    void CommandBuffer::TransferLayout(const Image &image, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout)
    {
        auto barrier = GetImageMemoryBarrier(image, oldLayout, newLayout);

        this->mCmdBuffer.pipelineBarrier(
            ImageUsageToPipelineStage(oldLayout),
            ImageUsageToPipelineStage(newLayout),
            { }, // dependecies
            { }, // memory barriers
            { }, // buffer barriers
            barrier
        );
    }

    void CommandBuffer::TransferLayout(ArrayView<ImageReference> images, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout)
    {
        std::vector<vk::ImageMemoryBarrier> barriers;
        barriers.reserve(images.size());

        for (const auto& image : images)
        {
            barriers.push_back(GetImageMemoryBarrier(image.get(), oldLayout, newLayout));
        }

        this->mCmdBuffer.pipelineBarrier(
            ImageUsageToPipelineStage(oldLayout),
            ImageUsageToPipelineStage(newLayout),
            { }, // dependecies
            { }, // memory barriers
            { }, // buffer barriers
            barriers
        );
    }

    void CommandBuffer::TransferLayout(ArrayView<Image> images, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout)
    {
        std::vector<vk::ImageMemoryBarrier> barriers;
        barriers.reserve(images.size());

        for (const auto& image : images)
        {
            barriers.push_back(GetImageMemoryBarrier(image, oldLayout, newLayout));
        }

        this->mCmdBuffer.pipelineBarrier(
            ImageUsageToPipelineStage(oldLayout),
            ImageUsageToPipelineStage(newLayout),
            { }, // dependecies
            { }, // memory barriers
            { }, // buffer barriers
            barriers
        );
    }
}