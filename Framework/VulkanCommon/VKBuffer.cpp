#include <cassert>

#include "VKBuffer.hpp"
#include "VKContext.hpp"
#include "VKMemoryAllocator.hpp"

namespace LeoVK
{
    Buffer::Buffer(Buffer &&other) noexcept
    {
        this->mBuffer = other.mBuffer;
        this->mSize = other.mSize;
        this->mAllocation = other.mAllocation;
        this->mpMapped = other.mpMapped;

        other.mBuffer = vk::Buffer();
        other.mSize = 0;
        other.mAllocation = {};
        other.mpMapped = nullptr;
    }

    Buffer::Buffer(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage)
    {
        this->Init(size, usage, memoryUsage);
    }

    Buffer &Buffer::operator=(Buffer &&other) noexcept
    {
        this->Destroy();

        this->mBuffer = other.mBuffer;
        this->mSize = other.mSize;
        this->mAllocation = other.mAllocation;
        this->mpMapped = other.mpMapped;

        other.mBuffer = vk::Buffer();
        other.mSize = 0;
        other.mAllocation = {};
        other.mpMapped = nullptr;

        return *this;
    }

    Buffer::~Buffer()
    {
        this->Destroy();
    }

    void Buffer::Init(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage)
    {
        constexpr std::array BufferQueueFamilyIndices = { VK_QUEUE_FAMILY_IGNORED };
        this->Destroy();

        this->mSize = size;
        vk::BufferCreateInfo bufferCI {};
        bufferCI.size = size;
        bufferCI.usage = static_cast<vk::BufferUsageFlags>(usage);
        bufferCI.sharingMode = vk::SharingMode::eExclusive;
        bufferCI.queueFamilyIndexCount = BufferQueueFamilyIndices.size();

        this->mAllocation = AllocateBuffer(bufferCI, memoryUsage, &this->mBuffer);
    }

    bool Buffer::IsMemoryMapped() const
    {
        return this->mpMapped != nullptr;
    }

    uint8_t *Buffer::MapMemory()
    {
        if (this->mpMapped == nullptr)
        {
            this->mpMapped = LeoVK::MapMemory(this->mAllocation);
        }
        return this->mpMapped;
    }

    void Buffer::UnmapMemory()
    {
        LeoVK::UnmapMemory(this->mAllocation);
        this->mpMapped = nullptr;
    }

    void Buffer::FlushMemory()
    {
        this->FlushMemory(this->mSize, 0);
    }

    void Buffer::FlushMemory(size_t size, size_t offset)
    {
        LeoVK::FlushMemory(this->mAllocation, size, offset);
    }

    void Buffer::CopyData(const uint8_t *data, size_t size, size_t offset)
    {
        assert(offset + size <= this->mSize);
        if (this->mpMapped == nullptr)
        {
            (void)this->MapMemory();
            std::memcpy((void*)(this->mpMapped + offset), (const void*)data, size);
            this->FlushMemory(size, offset);
            this->UnmapMemory();
        }
        else 
        {
            std::memcpy((void*)(this->mpMapped + offset), (const void*)data, size);
        }
    }

    void Buffer::CopyDataWithFlush(const uint8_t *data, size_t size, size_t offset)
    {
        this->CopyData(data, size, offset);
        if (this->IsMemoryMapped())
        {
            this->FlushMemory(size, offset);
        }
    }

    void Buffer::Destroy()
    {
        if (this->mBuffer)
        {
            if (this->mpMapped != nullptr) this->UnmapMemory();
            DestroyBuffer(this->mBuffer, this->mAllocation);
            this->mBuffer = vk::Buffer();
        }
    }
}