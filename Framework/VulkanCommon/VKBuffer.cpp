#include <cassert>

#include "VKBuffer.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    Buffer::Buffer(Buffer &&other) noexcept
    {
    }
    Buffer::Buffer(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage)
    {
    }

    Buffer &Buffer::operator=(Buffer &&other) noexcept
    {
        // TODO: insert return statement here
    }
    Buffer::~Buffer()
    {
    }
    void Buffer::Init(size_t size, BufferUsage::Value usage, MemoryUsage memoryUsage)
    {
    }
    bool Buffer::IsMemoryMapped() const
    {
        return false;
    }
    uint8_t *Buffer::MapMemory()
    {
        return nullptr;
    }
    void Buffer::UnmapMemory()
    {

    }
    void Buffer::FlushMemory()
    {
    }
    void Buffer::FlushMemory(size_t size, size_t offset)
    {
    }
    void Buffer::CopyData(const uint8_t *data, size_t size, size_t offset)
    {
    }
    void Buffer::CopyDataWithFlush(const uint8_t *data, size_t size, size_t offset)
    {
    }
    void Buffer::Destroy()
    {
    }
}