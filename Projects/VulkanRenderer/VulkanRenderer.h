#pragma once

#include "ProjectPCH.h"
#include "tiny_gltf.h"
#include "VulkanFramework.h"
#include "GLTFLoader.h"

#define ENABLE_VALIDATION true

namespace LeoRenderer
{
    struct ShaderData
    {
        vks::Buffer mBuffer;
        struct Values
        {
            glm::mat4 projection;
            glm::mat4 view;
            glm::vec4 lightPos = glm::vec4(0.0f, 2.5f, 0.0f, 1.0f);
            glm::vec4 viewPos;
        } mValues;
    };

    struct DescriptorSetLayouts
    {
        VkDescriptorSetLayout mMatrices;
        VkDescriptorSetLayout mTextures;
    };

    class VulkanRenderer : public VulkanFramework
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer() override;
        void GetEnabledFeatures() override;
        void BuildCommandBuffers() override;
        void LoadGLTFFile(std::string& filename);
        void LoadAssets();
        void SetupDescriptors();
        void PreparePipelines();
        void PrepareUniformBuffers();
        void UpdateUniformBuffers();
        void Prepare() override;
        void Render() override;
        void ViewChanged() override;
        void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

    public:
        LeoRenderer::GLTFModel mScene;

        ShaderData mShaderData;
        DescriptorSetLayouts mCustomDescSetLayouts;

        VkPipelineLayout mPipelineLayout;
        VkDescriptorSet mDescSet;
    };
}