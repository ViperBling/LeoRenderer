#pragma once

#include "VKBuffer.hpp"
#include "ArrayUtils.hpp"

namespace LeoVK
{
    class StageBuffer
    {
    public:
        struct Allocation
        {
            uint32_t Size;
            uint32_t Offset;
        };

        StageBuffer(size_t byteSize);

        Allocation Submit(const uint8_t *data, uint32_t byteSize);
        void Flush();
        void Reset();
        Buffer &GetBuffer() { return this->mBuffer; }
        const Buffer &GetBuffer() const { return this->mBuffer; }
        uint32_t GetCurrentOffset() const { return this->mCurrentOffset; }

        template <typename T>
        Allocation Submit(ArrayView<const T> view)
        {
            return this->Submit((const uint8_t *)view.data(), uint32_t(view.size() * sizeof(T)));
        }

        template <typename T>
        Allocation Submit(ArrayView<T> view)
        {
            return this->Submit((const uint8_t *)view.data(), uint32_t(view.size() * sizeof(T)));
        }

        template <typename T>
        Allocation Submit(const T *value)
        {
            return this->Submit((uint8_t *)value, uint32_t(sizeof(T)));
        }
    
    private:
        Buffer mBuffer;
        uint32_t mCurrentOffset;
    };
}