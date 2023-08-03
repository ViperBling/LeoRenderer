#pragma once

#include "VKRendererBase.hpp"
#include "AssetsLoader.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct UBOAndParams
{
    LeoVK::Buffer mBuffer;
    struct Values
    {
        glm::mat4 mProj;
        glm::mat4 mView;
        glm::vec4 mLightPos = glm::vec4(0.0f, 2.5f, 0.0f, 0.0f);
        glm::vec4 mViewPos;
    } mValues;
};

struct DescriptorSetLayouts
{
    VkDescriptorSetLayout mMatricesDesc;
    VkDescriptorSetLayout mTexturesDesc;
};

class GLTFTest : public VKRendererBase
{
public:
    GLTFTest();
    virtual ~GLTFTest();
    void GetEnabledFeatures() override;
    void BuildCommandBuffers() override;
    void Prepare() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void RenderNode(LeoVK::Node* node, uint32_t cbIdx, LeoVK::Material::AlphaMode alphaMode);
    void LoadScene(const std::string& filename);
    void LoadAssets();

    void SetupDescriptors();
    void SetupPipelines();
    void SetupUniformBuffers();
    void UpdateUniformBuffers();
    void PreRender();

public:
    LeoVK::GLTFScene mRenderScene;

    UBOAndParams mUniforms;
    VkPipelineLayout  mPipelineLayout;
    VkDescriptorSet mDescSet;
    DescriptorSetLayouts mDescSetLayouts;
};