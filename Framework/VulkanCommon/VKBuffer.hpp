#pragma once

#include "VKMemoryAllocator.hpp"

namespace LeoVK
{
    struct BufferUsage
    {
        using Value = uint32_t;

        enum Bits : Value
        {
            UNKNOWN                                     = (Value)vk::BufferUsageFlags{ },
            TRANSFER_SOURCE                             = (Value)vk::BufferUsageFlagBits::eTransferSrc,
            TRANSFER_DESTINATION                        = (Value)vk::BufferUsageFlagBits::eTransferDst,
            UNIFORM_TEXEL_BUFFER                        = (Value)vk::BufferUsageFlagBits::eUniformTexelBuffer,
            STORAGE_TEXEL_BUFFER                        = (Value)vk::BufferUsageFlagBits::eStorageTexelBuffer,
            UNIFORM_BUFFER                              = (Value)vk::BufferUsageFlagBits::eUniformBuffer,
            STORAGE_BUFFER                              = (Value)vk::BufferUsageFlagBits::eStorageBuffer,
            INDEX_BUFFER                                = (Value)vk::BufferUsageFlagBits::eIndexBuffer,
            VERTEX_BUFFER                               = (Value)vk::BufferUsageFlagBits::eVertexBuffer,
            INDIRECT_BUFFER                             = (Value)vk::BufferUsageFlagBits::eIndirectBuffer,
            SHADER_DEVICE_ADDRESS                       = (Value)vk::BufferUsageFlagBits::eShaderDeviceAddress,
            TRANSFORM_FEEDBACK_BUFFER                   = (Value)vk::BufferUsageFlagBits::eTransformFeedbackBufferEXT,
            TRANSFORM_FEEDBACK_COUNTER_BUFFER           = (Value)vk::BufferUsageFlagBits::eTransformFeedbackCounterBufferEXT,
            CONDITIONAL_RENDERING                       = (Value)vk::BufferUsageFlagBits::eConditionalRenderingEXT,
            ACCELERATION_STRUCTURE_BUILD_INPUT_READONLY = (Value)vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
            ACCELERATION_STRUCTURE_STORAGE              = (Value)vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
            SHADER_BINDING_TABLE                        = (Value)vk::BufferUsageFlagBits::eShaderBindingTableKHR,
        };
    };

    class Buffer
    {
    public:
        Buffer() = default;
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        Buffer(Buffer&& other) noexcept;
        Buffer(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage);
        Buffer& operator=(Buffer&& other) noexcept;
        virtual ~Buffer();

        void Init(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage);

        vk::Buffer GetNativeBuffer() const { return mBuffer; }
        size_t GetSize() const { return mSize; }

        bool IsMemoryMapped() const;
        uint8_t* MapMemory();
        void UnmapMemory();
        void FlushMemory();
        void FlushMemory(size_t size, size_t offset = 0);
        void CopyData(const uint8_t* data, size_t size, size_t offset = 0);
        void CopyDataWithFlush(const uint8_t* data, size_t size, size_t offset = 0);

    private:
        void Destroy();
        
    private:
        vk::Buffer mBuffer;
        size_t mSize = 0;
        VmaAllocation mAllocation = VK_NULL_HANDLE;
        uint8_t* mpMapped = nullptr;
    };

    using BufferReference = std::reference_wrapper<const Buffer>;
}