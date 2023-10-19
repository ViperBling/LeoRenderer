#pragma once

#include "ProjectPCH.hpp"

#include "VKTools.hpp"
#include "VKDebug.hpp"
#include "VKDevice.hpp"
#include "VKBuffer.hpp"

#include <imgui/imgui.h>

namespace LeoVK
{
    class UIOverlay
    {
    public:
        UIOverlay();
        ~UIOverlay();

        void PreparePipeline(VkPipelineCache pipelineCache, VkRenderPass renderPass, VkFormat colorFormat, VkFormat depthFormat);
        void PrepareResources();

        bool Update();
        void Draw(VkCommandBuffer cmdBuffer);
        void Resize(uint32_t width, uint32_t height);

        void FreeResources();

        bool Header(const char* caption);
        bool CheckBox(const char* caption, bool* value);
        bool CheckBox(const char* caption, int32_t* value);
        bool RadioButton(const char* caption, bool value);
        bool InputFloat(const char* caption, float* value, float step, uint32_t precision);
        bool SliderFloat(const char* caption, float* value, float min, float max);
        bool SliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
        bool ComboBox(const char* caption, int32_t* itemIndex, std::vector<std::string> items);
        bool ComboBox(const char* caption, std::string &selectedkey, std::map<std::string, std::string> items);
        bool Button(const char* caption);
        bool ColorPicker(const char* caption, float* color);
        void Text(const char* formatStr, ...);

    public:
        LeoVK::VulkanDevice* mpDevice;
        VkQueue mQueue;
        VkSampleCountFlagBits mMSAA = VK_SAMPLE_COUNT_1_BIT;
        uint32_t mSubpass = 0;

        LeoVK::Buffer mVertexBuffer;
        LeoVK::Buffer mIndexBuffer;
        uint32_t mVertexCount = 0;
        uint32_t mIndexCount = 0;

        std::vector<VkPipelineShaderStageCreateInfo> mShaders;

        VkDescriptorPool mDescPool;
        VkDescriptorSetLayout mDescSetLayout;
        VkDescriptorSet mDescSet;
        VkPipelineLayout mPipelineLayout;
        VkPipeline mPipeline;

        VkDeviceMemory mFontMemory = VK_NULL_HANDLE;
        VkImage mFontImage = VK_NULL_HANDLE;
        VkImageView mFontView = VK_NULL_HANDLE;
        VkSampler mSampler;

        struct PushConstantBlock {
            glm::vec2 scale;
            glm::vec2 translate;
        } mPushConstantBlock;

        bool mbVisible = true;
        bool mbUpdated = false;
        float mScale = 1.0f;
    };
}