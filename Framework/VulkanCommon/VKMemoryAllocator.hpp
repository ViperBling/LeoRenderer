#pragma once

#include <cstdint>
#include <cstddef>

#include <vulkan/vulkan.hpp>
#include <vulkan/vk_mem_alloc.h>

struct VmaAllocator_T;
struct VmaAllocation_T;
using VmaAllocator = VmaAllocator_T*;
using VmaAllocation = VmaAllocation_T*;

namespace LeoVK
{
    enum class MemoryUsage
    {
        GPU_ONLY = 0,           // device local for fast GPU access
        CPU_ONLY,               // heap allocated for staging resources
        CPU_TO_GPU,             // dynamic resources with frequent update from CPU
        GPU_TO_CPU,             // readback from GPU to CPU
        CPU_COPY,               // cpu memory used to cache GPU resources in heap
        GPU_LAZILY_ALLOCATED,   // used only on mobile platforms
    };

    VmaAllocator GetVulkanAllocator();
    void DestroyImage(const vk::Image& image, VmaAllocation allocation);
    void DestroyBuffer(const vk::Buffer& buffer, VmaAllocation allocation);
    VmaAllocation AllocateImage(const vk::ImageCreateInfo& imageCreateInfo, MemoryUsage usage, vk::Image* image);
    VmaAllocation AllocateBuffer(const vk::BufferCreateInfo& bufferCreateInfo, MemoryUsage usage, vk::Buffer* buffer);
    uint8_t* MapMemory(VmaAllocation allocation);
    void UnmapMemory(VmaAllocation allocation);
    void FlushMemory(VmaAllocation allocation, size_t byteSize, size_t offset);
}