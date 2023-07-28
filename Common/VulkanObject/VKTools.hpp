#pragma once

#include "ProjectPCH.hpp"
#include "VKInitializers.hpp"

#define VK_FLAGS_NONE 0
#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CHECK(f)																				        \
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << LeoVK::VKTools::ErrorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

#define GET_INSTANCE_PROC_ADDR(inst, entrypoint)                        \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetInstanceProcAddr(inst, "vk"#entrypoint)); \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

#define GET_DEVICE_PROC_ADDR(dev, entrypoint)                           \
{                                                                       \
	fp##entrypoint = reinterpret_cast<PFN_vk##entrypoint>(vkGetDeviceProcAddr(dev, "vk"#entrypoint));   \
	if (fp##entrypoint == NULL)                                         \
	{																    \
		exit(1);                                                        \
	}                                                                   \
}

std::string GetAssetsPath();

namespace LeoVK::VKTools
{
    /** @brief Disable message boxes on fatal errors */
    extern bool bErrorModeSilent;

    /** @brief Returns an error code as a string */
    std::string ErrorString(VkResult errorCode);

    /** @brief Returns the device type as a string */
    std::string PhysicalDeviceTypeString(VkPhysicalDeviceType type);

    VkBool32 GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat);

    /** @brief Format是否支持LINEAR过滤 */
    VkBool32 FormatFilterable(VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling);
    /** Format是否有stencil */
    VkBool32 FormatHasStencil(VkFormat);

    /** @brief 为往给定的CommandBuffer中给Subresource设置ImageLayout时设置Barrier */
    void SetImageLayout(
        VkCommandBuffer         cmdBuffer,
        VkImage                 image,
        VkImageLayout           oldImageLayout,
        VkImageLayout           newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags    srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags    dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    );
    /** @brief 对第一层Mip使用固定的Sub Resource */
    void SetImageLayout(
        VkCommandBuffer         cmdBuffer,
        VkImage                 image,
        VkImageAspectFlags      aspectMask,
        VkImageLayout           oldImageLayout,
        VkImageLayout           newImageLayout,
        VkPipelineStageFlags    srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags    dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
    );

    /** @brief Insert an image memory barrier into the command buffer */
    void InsertImageMemoryBarrier(
        VkCommandBuffer         cmdBuffer,
        VkImage                 image,
        VkAccessFlags           srcAccessMask,
        VkAccessFlags           dstAccessMask,
        VkImageLayout           oldImageLayout,
        VkImageLayout           newImageLayout,
        VkPipelineStageFlags    srcStageMask,
        VkPipelineStageFlags    dstStageMask,
        VkImageSubresourceRange subresourceRange
    );

    /** @brief 报告Fata Error */
    void ExitFatal(const std::string& message, int32_t exitCode);
    void ExitFatal(const std::string& message, VkResult resCode);

    VkShaderModule LoadShader(const char* filename, VkDevice device);

    bool FileExists(const std::string& filename);

    uint32_t AlignedSize(uint32_t value, uint32_t alignment);
}