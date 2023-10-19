#include "VKBuffer.hpp"

namespace LeoVK
{
    /**
	* 映射内存到这个Buffer，如果成功，mpMapped会指向这段buffer
	*
	* @param size (Optional) 映射的内存大小，VK_WHOLE_SIZE会映射整个buffer的范围
	* @param offset (Optional) 起始位置偏移
	*
	* @return VkResult of the buffer mapping call
	*/
    VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
    {
        return vkMapMemory(mDevice, mMemory, offset, size, 0, &mpMapped);
    }

    /**
	* Unmap a mapped memory range
	*
	* @note Does not return a result as vkUnmapMemory can't fail
	*/
    void Buffer::UnMap()
    {
        if (mpMapped)
        {
            vkUnmapMemory(mDevice, mMemory);
            mpMapped = nullptr;
        }
    }

    /**
	* 把内存绑定到Buffer上
	*
	* @param offset (Optional) 要绑定内存的起始位置偏移
	*
	* @return VkResult of the bindBufferMemory call
	*/
    VkResult Buffer::Bind(VkDeviceSize offset)
    {
        return vkBindBufferMemory(mDevice, mBuffer, mMemory, offset);
    }

    /**
	* 为Buffer设置一个默认的描述符
	*
	* @param size (Optional) 描述符内存大小
	* @param offset (Optional) 起始位置偏移
	*
	*/
    void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
    {
        mDescriptor.offset = offset;
        mDescriptor.buffer = mBuffer;
        mDescriptor.range = size;
    }

    /**
	* 把指定的数据拷贝到已映射的Buffer
	*
	* @param data Pointer to the data to copy
	* @param size Size of the data to copy in machine units
	*
	*/
    void Buffer::CopyToBuffer(void *data, VkDeviceSize size)
    {
        assert(mpMapped);
        memcpy(mpMapped, data, size);
    }

    /**
	* 刷新这段内存以让GPU可见
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return VkResult of the flush call
	*/
    VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
    {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = mMemory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(mDevice, 1, &mappedRange);
    }

    /**
	* 让主机可见该段内存
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @return VkResult of the invalidate call
	*/
    VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
    {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = mMemory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(mDevice, 1, &mappedRange);
    }

    /**
	* Release all Vulkan resources held by this buffer
	*/
    void Buffer::Destroy()
    {
        if (mBuffer)
        {
            vkDestroyBuffer(mDevice, mBuffer, nullptr);
        }
        if (mMemory)
        {
            vkFreeMemory(mDevice, mMemory, nullptr);
        }
    }
}