#pragma once

#include "ProjectPCH.hpp"

#include "VKRendererBase.hpp"
#include "AssetsLoader.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct SceneTextures
{
    LeoVK::TextureCube mEnvCube;
    LeoVK::TextureCube mIrradianceCube;
    LeoVK::TextureCube mPreFilterCube;
    LeoVK::Texture2D mLUTBRDF;
    LeoVK::Texture2D mDummy;
};

struct RenderScene
{
    LeoVK::GLTFScene mScene;
    LeoVK::GLTFScene mSkybox;
};

struct UBOBuffer
{
    LeoVK::Buffer mSceneUBO;
    LeoVK::Buffer mSkyboxUBO;
    LeoVK::Buffer mParamsUBO;
};

struct UBOMatrices
{
    glm::mat4 mProj;
    glm::mat4 mModel;
    glm::mat4 mView;
    glm::mat4 mCamPos;
};

struct UBOParams
{
    glm::vec4 mLights;
    float mExposure = 4.5f;
    float mGamma = 2.2f;
    float mPFCubeMipLevels;
    float mScaleIBLAmbient = 1.0f;
    float mDebugViewInputs = 0;
    float mDebugViewEquation = 0;
};

struct LightSource
{
    glm::vec3 mColor = glm::vec3(1.0f);
    glm::vec3 mRotation = glm::vec3(75.0f, 40.0f, 0.0f);
};

struct PushConstBlockMaterial
{
    glm::vec4   mBaseColorFactor;
    glm::vec4   mEmissiveFactor;
    glm::vec4   mDiffuseFactor;
    glm::vec4   mSpecularFactor;
    float       mWorkflow;
    uint32_t    mColorTextureSet;
    uint32_t    mPhysicalDescTexSet;
    uint32_t    mNormalTextureSet;
    uint32_t    mOcclusionTextureSet;
    uint32_t    mEmissiveTextureSet;
    float       mMetallicFactor;
    float       mRoughnessFactor;
    float       mAlphaMask;
    float       mAlphaMaskCutoff;
};

struct Pipelines
{
    VkPipeline mScenePipe;
    VkPipeline mSkyboxPipe;
    VkPipeline mDoubleSided;
    VkPipeline mAlphaBlend;
};

struct DescSetLayouts
{
    VkDescriptorSetLayout mSceneLayout;
    VkDescriptorSetLayout mMaterialLayout;
    VkDescriptorSetLayout mNodeLayout;
};

struct DescSets
{
    VkDescriptorSet mSceneDesc;
    VkDescriptorSet mSkyboxDesc;
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

    void RenderNode(LeoVK::Node* node, uint32_t cbIdx, LeoVK::Material::AlphaMode alphaMode);
    void LoadScene(const std::string& filename);
    void LoadAssets();
    void LoadEnv(const std::string& filename);
    void GenerateCubeMap();
    void GeneratePrefilterCube();
    void GenerateLUT();

    void SetupDescriptors();
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void PreRender();

public:
    bool    mbDisplayBackground = true;
    bool    mbAnimate = true;
    int32_t mAnimIndex = 0;
    float   mAnimTimer = 0.0f;

    uint32_t        mFrameIdx = 0;
    const uint32_t  mRenderAhead = 2;

    bool      mbRotateModel = false;
    glm::vec3 mModelRot = glm::vec3(0.0f);
    glm::vec3 mModelPos = glm::vec3(0.0f);

    enum PBRWorkflows{ PBR_WORKFLOW_METALLIC_ROUGHNESS = 0, PBR_WORKFLOW_SPECULAR_GLOSSINESS = 1 };
    PushConstBlockMaterial mPushConst;

    std::map<std::string, std::string> mEnvs;
    std::string mCurrEnv = "papermill";

    int32_t     mDebugViewInputs = 0;
    int32_t     mDebugViewEquation = 0;

    RenderScene         mRenderScene;
    SceneTextures       mSceneTextures;
    LightSource         mLight;
    UBOMatrices         mSceneMats, mSkyboxMats;
    UBOParams           mShaderParams;
    DescSetLayouts      mDescSetLayouts;
    Pipelines           mPipelines;
    VkPipeline          mBoundPipeline = VK_NULL_HANDLE;
    VkPipelineLayout    mPipelineLayout;

    std::vector<DescSets>   mDescSets;
    std::vector<UBOBuffer>  mUBOBuffers;
};
