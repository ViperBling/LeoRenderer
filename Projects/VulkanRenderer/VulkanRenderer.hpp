#pragma once

#include "ProjectPCH.hpp"

#include "VKRendererBase.hpp"
#include "AssetsLoader.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct UBOMatrices
{
    glm::mat4 mProj;
    glm::mat4 mModel;
    glm::mat4 mView;
    glm::vec3 mCamPos;
};

struct UBOParams
{
    glm::vec4 mLight;
    float mExposure = 4.5f;
    float mGamma = 2.2f;
};

struct PBRPipelines
{
    VkPipeline mPBRPipeline;
};

struct PBRDescSets
{
    VkDescriptorSet mObjectDescSet;
};

struct UBOBuffers
{
    LeoVK::Buffer mObjectUBO;
    LeoVK::Buffer mParamsUBO;
};

struct DescSetLayouts
{
    VkDescriptorSetLayout mUniformDescSetLayout;    // 匹配ObjectDestSet
    VkDescriptorSetLayout mTextureDescSetLayout;    // 匹配Material中的DescSet
    VkDescriptorSetLayout mNodeDescSetLayout;       // 匹配Mesh中的Uniform DescSet
};

class VulkanRenderer : public VKRendererBase
{
public:
    VulkanRenderer();
    virtual ~VulkanRenderer();
    void GetEnabledFeatures() override;
    void BuildCommandBuffers() override;
    void Prepare() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void SetupDescriptors();
    void SetupNodeDescriptors(LeoVK::Node* node);
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void UpdateParams();

    void LoadAssets();

public:
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR mDynamicRenderingFeaturesKHR {};

    LeoVK::GLTFScene mRenderScene;
    UBOBuffers mUniformBuffers;
    UBOMatrices mUBOMatrices;
    UBOParams mUBOParams;
    PBRPipelines mPipelines;
    VkPipeline mBoundPipeline = VK_NULL_HANDLE;
    PBRDescSets mDescSets;

    VkPipelineLayout mPipelineLayout;
    DescSetLayouts mDescSetLayout;
};
