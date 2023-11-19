#pragma once

#include "ArrayUtils.hpp"
#include "VKImage.hpp"
#include "VKBuffer.hpp"

#include <vulkan/vulkan.hpp>

namespace LeoVK
{
    struct NativeRenderPass;

    struct Rect2D
    {
        int32_t OffsetWidth = 0;
        int32_t OffSetHeight = 0;
        uint32_t Width = 0;
        uint32_t Height = 0;
    };

    struct Viewport
    {
        float OffsetWidth = 0.0f;
        float OffsetHeight = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;
        float MinDepth = 0.0f;
        float MaxDepth = 0.0f;
    };

    struct ClearColor
    {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
        float A = 1.0f;
    };

    struct ClearDepthStencil
    {
        float Depth = 1.0f;
        uint32_t Stencil = 0;
    };

    enum class BlitFilter
    {
        NEAREST = 0,
        LINEAR,
        CUBIC,
    };

    struct ImageInfo
    {
        ImageReference Resource;
        ImageUsage::Bits Usage = ImageUsage::UNKNOWN;
        uint32_t MipLevel = 0;
        uint32_t Layer = 0;
    };

    struct BufferInfo
    {
        BufferReference Resource;
        uint32_t Offset = 0;
    };

    class CommandBuffer
    {
    public:
        CommandBuffer(vk::CommandBuffer cmdBuffer)
            : mCmdBuffer(std::move(cmdBuffer)) {}
        
        const vk::CommandBuffer& GetNativeCmdBuffer() const { return mCmdBuffer; }
        void Begin();
        void End();
        void BeginPass(const NativeRenderPass& renderPass);
        void EndPass(const NativeRenderPass& renderPass);
        void Draw(uint32_t vertexCount, uint32_t instanceCount);
        void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount);
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
        void BindIndexBufferUInt32(const Buffer& indexBuffer);
        void BindIndexBufferUInt16(const Buffer& indexBuffer);
        void SetViewport(const Viewport& viewport);
        void SetScissor(const Rect2D& scissor);
        void SetRenderArea(const Image& image);

        void PushConstants(const NativeRenderPass& renderPass, const uint8_t* data, size_t size);
        void Dispatch(uint32_t x, uint32_t y, uint32_t z);
        
        void CopyImage(const ImageInfo& source, const ImageInfo& distance);
        void CopyBuffer(const BufferInfo& source, const BufferInfo& distance, size_t byteSize);
        void CopyBufferToImage(const BufferInfo& source, const ImageInfo& distance);
        void CopyImageToBuffer(const ImageInfo& source, const BufferInfo& distance);
        
        void BlitImage(const Image& source, ImageUsage::Bits sourceUsage, const Image& distance, ImageUsage::Bits distanceUsage, BlitFilter filter);
        void GenerateMipLevels(const Image& image, ImageUsage::Bits initialUsage, BlitFilter filter);
    
        void TransferLayout(const Image& image, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout);
        void TransferLayout(ArrayView<ImageReference> images, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout);
        void TransferLayout(ArrayView<Image> images, ImageUsage::Bits oldLayout, ImageUsage::Bits newLayout);

        template<typename... Buffers>
        void BindVertexBuffers(const Buffers&... vertexBuffers)
        {
            constexpr size_t BufferCount = sizeof...(Buffers);
            std::array buffers = { vertexBuffers.GetNativeBuffer()... };
            uint64_t offsets[BufferCount] = { 0 };
            this->GetNativeCmdBuffer().bindVertexBuffers(0, BufferCount, buffers.data(), offsets);
        }

        template<typename T>
        void PushConstants(const NativeRenderPass& renderPass, ArrayView<const T> constants)
        {
            this->PushConstants(renderPass, (const uint8_t*)constants.data(), constants.size() * sizeof(T));
        }

        template<typename T>
        void PushConstants(const NativeRenderPass& renderPass, const T* constants)
        {
            this->PushConstants(renderPass, (const uint8_t*)constants, sizeof(T));
        }

    private:
        vk::CommandBuffer mCmdBuffer;
    };
}