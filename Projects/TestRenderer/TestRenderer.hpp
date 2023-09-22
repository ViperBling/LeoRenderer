#pragma once

#include "ProjectPCH.hpp"

#include "VKRendererBase.hpp"
#include "AssetsLoader.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct UBOMatrices
{
    glm::mat4 mProj;
    glm::mat4 mModelView;
    glm::mat4 mViewPos;
};

class TestRenderer : public VKRendererBase
{
public:
    TestRenderer();
    virtual ~TestRenderer();
    void GetEnabledFeatures() override;
    void SetupRenderPass() override;
    void BuildCommandBuffers() override;
    void Prepare() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void SetupDescriptorPool();
    void SetupDescriptorSet();
    void SetupDescriptorSetLayout();
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();

    void LoadAssets();
    void Draw();

public:
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR mDynamicRenderingFeaturesKHR {};

    LeoVK::GLTFScene mRenderScene;
    LeoVK::Buffer mUniformBuffer;
    UBOMatrices mUniformData;

    VkPipeline mPipeline;
    VkPipelineLayout mPipelineLayout;
    VkDescriptorSet mDescSet;
    VkDescriptorSetLayout mDescSetLayout;
};
