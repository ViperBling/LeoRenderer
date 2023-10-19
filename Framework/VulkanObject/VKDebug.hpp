#pragma once

#include "ProjectPCH.hpp"

namespace LeoVK::Debug
{
    VKAPI_ATTR VkBool32 VKAPI_CALL MessageCallback(
        VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT objType,
        uint64_t srcObject,
        size_t location,
        int32_t msgCode,
        const char* pLayerPrefix,
        const char* pMsg,
        void* pUserData
        );
    // Load debug function pointers and set debug callback
    void SetupDebugging(VkInstance instance);
    // Clear debug callback
    void FreeDebugCallback(VkInstance instance);
}

namespace LeoVK::DebugMarker
{
    // 如果DebugMarker指针可用则为True
    extern bool bActive;

    // 从Debug扩展中获取函数指针
    void Setup(VkDevice device);

    // Sets the debug name of an object
    // All Objects in Vulkan are represented by their 64-bit handles which are passed into this function
    // along with the object type
    void SetObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name);

    // Set the tag for an object
    void SetObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag);

    // Start a new debug marker region
    void BeginRegion(VkCommandBuffer cmdBuffer, const char* pMarkerName, glm::vec4 color);

    // 在Command Buffer中插入一个新的DebugMarker
    void Insert(VkCommandBuffer cmdBuffer, std::string markerName, glm::vec4 color);

    // End the current debug marker region
    void EndRegion(VkCommandBuffer cmdBuffer);

    // Object specific naming functions
    void SetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char * name);
    void SetQueueName(VkDevice device, VkQueue queue, const char * name);
    void SetImageName(VkDevice device, VkImage image, const char * name);
    void SetSamplerName(VkDevice device, VkSampler sampler, const char * name);
    void SetBufferName(VkDevice device, VkBuffer buffer, const char * name);
    void SetDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char * name);
    void SetShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char * name);
    void SetPipelineName(VkDevice device, VkPipeline pipeline, const char * name);
    void SetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char * name);
    void SetRenderPassName(VkDevice device, VkRenderPass renderPass, const char * name);
    void SetFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char * name);
    void SetDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const char * name);
    void SetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char * name);
    void SetSemaphoreName(VkDevice device, VkSemaphore semaphore, const char * name);
    void SetFenceName(VkDevice device, VkFence fence, const char * name);
    void SetEventName(VkDevice device, VkEvent _event, const char * name);
}