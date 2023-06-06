#pragma once

#include "ProjectPCH.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#ifdef VK_USE_PLATFORM_ANDROID_KHR
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
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