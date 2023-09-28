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

typedef std::unordered_map<std::string, VkPipeline> PBRPipelines;

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

class TestRenderer : public VKRendererBase
{
public:
    TestRenderer();
    virtual ~TestRenderer();
    void GetEnabledFeatures() override;
    void BuildCommandBuffers() override;
    void Prepare() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void SetupDescriptors();
    void SetupNodeDescriptors(LeoVK::Node* node);
    void AddPipelineSet(const std::string prefix, const std::string vertexShader, const std::string pixelShader);
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void UpdateParams();

    void LoadAssets();
    void DrawNode(LeoVK::Node* node, uint32_t cbIndex, LeoVK::Material::AlphaMode alphaMode);

public:

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
