#pragma once

#include "ProjectPCH.hpp"

#include "VKRendererBase.hpp"
#include "AssetsLoader.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct SceneTextures
{
    LeoVK::Texture2D mLUTBRDF;
    LeoVK::TextureCube mIrradianceCube;
    LeoVK::TextureCube mPreFilteredCube;
    LeoVK::TextureCube mEnvCube;
};

struct Models
{
    LeoVK::GLTFScene mRenderScene;
    LeoVK::GLTFScene mSkybox;
};

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
    float mPrefilteredCubeMipLevels;
    float mScaleIBLAmbient = 1.0f;
    glm::vec3 mLightColor = glm::vec3(1.0f);
    float mLightIntensity = 1.0f;
};

typedef std::unordered_map<std::string, VkPipeline> PBRPipelines;

struct PBRDescSets
{
    VkDescriptorSet mObjectDescSet;
    VkDescriptorSet mSkyboxDescSet;
    VkDescriptorSet mMaterialParamsDescSet;
};

struct UBOBuffers
{
    LeoVK::Buffer mObjectUBO;
    LeoVK::Buffer mParamsUBO;
    LeoVK::Buffer mSkyboxUBO;
    LeoVK::Buffer mMaterialParamsBuffer;
};

struct DescSetLayouts
{
    VkDescriptorSetLayout mUniformDescSetLayout;    // 匹配ObjectDestSet
    VkDescriptorSetLayout mTextureDescSetLayout;    // 匹配Material中的DescSet
    VkDescriptorSetLayout mNodeDescSetLayout;       // 匹配Mesh中的Uniform DescSet
    VkDescriptorSetLayout mMaterialBufferDescSetLayout;
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
    void AddPipelineSet(const std::string prefix, const std::string vertexShader, const std::string pixelShader);
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void UpdateParams();

    void GenerateBRDFLUT();
    void GenerateCubeMaps();

    void LoadScene(std::string filename);
    void LoadEnvironment(std::string filename);
    void LoadAssets();
    void DrawNode(LeoVK::Node* node, uint32_t cbIndex, LeoVK::Material::AlphaMode alphaMode);

public:

    Models mScenes;
    SceneTextures mTextures;
    UBOBuffers mUniformBuffers;
    UBOMatrices mSceneUBOMatrices;
    UBOMatrices mSkyboxUBOMatrices;
    UBOParams mUBOParams;

    PBRPipelines mPipelines;
    VkPipeline mBoundPipeline = VK_NULL_HANDLE;
    PBRDescSets mDescSets;

    VkPipelineLayout mPipelineLayout;
    DescSetLayouts mDescSetLayout;

    int32_t mAnimIndex = 0;
    float mAnimTimer = 0.0f;
    bool mbAnimate = true;
    float mAnimateSpeed = 1.5f;

    int32_t mCamTypeIndex = 0;

    int32_t mDebugViewInputs = 0;
    int32_t mDebugViewEquations = 0;

    std::map<std::string, std::string> mEnvMaps;
    std::string mSelectEnvMap = "papermill";
    bool mbShowBackground = true;
};
