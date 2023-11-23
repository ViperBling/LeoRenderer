#include "VKStageBuffer.hpp"

namespace LeoVK
{
    StageBuffer::StageBuffer(size_t byteSize)
        : mBuffer(byteSize, BufferUsage::TRANSFER_SOURCE, MemoryUsage::CPU_TO_GPU)
        , mCurrentOffset(0)
    {
        (void)this->mBuffer.MapMemory();
    }

    StageBuffer::Allocation StageBuffer::Submit(const uint8_t *data, uint32_t byteSize)
    {
        assert(this->mCurrentOffset + byteSize <= this->mBuffer.GetSize());

        if (data != nullptr)
        {
            this->mBuffer.CopyData(data, byteSize, this->mCurrentOffset);
        }

        this->mCurrentOffset += byteSize;
        return Allocation { byteSize, this->mCurrentOffset - byteSize };
    }

    void StageBuffer::Flush()
    {
        this->mBuffer.FlushMemory(this->mCurrentOffset, 0);
    }

    void StageBuffer::Reset()
    {
        this->mCurrentOffset = 0;
    }
}