#pragma once

#include "ProjectPCH.h"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct Textures
{
    vks::TextureCubeMap     mTexEnvCube;
    vks::Texture2D          mTexDummy;
    vks::Texture2D          mTexLUTBRDF;
    vks::TextureCubeMap     mTexIrradianceCube;
    vks::TextureCubeMap     mTexPreFilterCube;
};

struct Models
{
    LeoRenderer::GLTFModel mModelScene;
    LeoRenderer::GLTFModel mModelSkybox;
};

struct UniformBufferSet
{
    vks::Buffer mUBOScene;
    vks::Buffer mUBOSkybox;
    vks::Buffer mUBOParams;
};

struct UBOMatrices
{
    glm::mat4 mUBOProj;
    glm::mat4 mUBOModel;
    glm::mat4 mUBOView;
    glm::vec3 mUBOCamPos;
};

struct ShaderParams
{
    glm::vec4   mParamLightDir;
    float       mParamExposure = 4.5f;
    float       mParamGamma = 2.2f;
    float       mParamPrefilteredCubeMipLevels;
    float       mParamScaleIBLAmbient = 1.0f;
    float       mParamDebugViewInputs = 0;
    float       mParamDebugViewEquation = 0;
};

struct Pipelines
{
    VkPipeline mPipelineSkyBox;
    VkPipeline mPipelinePBR;
    VkPipeline mPipelineDoubleSided;
    VkPipeline mPipelinePBRAlphaBlend;
};

struct DescriptorSetLayouts
{
    VkDescriptorSetLayout mDescLayoutScene;
    VkDescriptorSetLayout mDescLayoutMaterial;
    VkDescriptorSetLayout mDescLayoutNode;
};

struct DescriptorSets
{
    VkDescriptorSet mDescScene;
    VkDescriptorSet mDescSkybox;
};

struct LightSource
{
    glm::vec3 mColor = glm::vec3(1.0f);
    glm::vec3 mRotation = glm::vec3(75.0f, 40.0f, 0.0f);
};

enum PBRWorkFlows
{
    PBR_WORKFLOW_METALLIC_ROUGHNESS,
    PBR_WORKFLOW_SPECULAR_GLOSINESS
};

struct PushConstantBlockMaterial
{
    glm::vec4   mFactorBaseColor;
    glm::vec4   mFactorEmissive;
    glm::vec4   mFactorDiffuse;
    glm::vec4   mFactorSpecular;
    float       mWorkFlow;

    int         mTextureSetColor;
    int         mTextureSetPhysicalDescriptor;
    int         mTextureSetNormal;
    int         mTextureSetOcclusion;
    int         mTextureSetEmissive;

    float       mFactorMetallic;
    float       mFactorRoughness;
    float       mFactorAlphaMask;
    float       mFactorAlphaMaskCutoff;
};

class PBRRenderer : public VulkanFramework
{
public:
    PBRRenderer();
    ~PBRRenderer();
    void GetEnabledFeatures() override;

    void RenderNode(LeoRenderer::Node* node, uint32_t cbIndex, LeoRenderer::Material::AlphaMode alphaMode);

    void LoadScene(std::string& filename);
    void LoadEnvironment(std::string& filename);
    void LoadAssets();
    void SetupNodeDescriptorSet(LeoRenderer::Node* node);
    void SetDescriptors();
    void PreparePipelines();
    void GenerateBRDFLUT();
    void GenerateCubeMaps();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void UpdateParams();

    void BuildCommandBuffers() override;
    void Prepare() override;
    void Render() override;
    void ViewChanged() override;
    void WindowResized() override;
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    void FileDropped(std::string & filename) override;

public:
    Textures mTextures;
    Models mModels;
    UBOMatrices mShaderValScene;
    UBOMatrices mShaderValSkybox;
    ShaderParams mShaderParams;
    PushConstantBlockMaterial mPushConstBlockMaterial;

    Pipelines mPipelines;
    DescriptorSetLayouts mDescSetLayouts;

    VkPipelineLayout mPipelineLayout;
    VkPipeline mBoundPipeline = VK_NULL_HANDLE;

    std::vector<DescriptorSets> mDescSets;
    std::vector<VkCommandBuffer> mCmdBuffers;
    std::vector<UniformBufferSet> mUniformBuffers;

    std::vector<VkFence> mWaitFence;
    std::vector<VkSemaphore> mRenderCompleteSemaphore;
    std::vector<VkSemaphore> mPresentCompleteSemaphore;

    LightSource mLight;

    std::map<std::string, std::string> mEnvironments;
    std::string mSelectedEnvironment = "papermill";

    bool mRotateModel = false;
    glm::vec3 mModelRotation = glm::vec3(0.0f);
    glm::vec3 mModelPosition = glm::vec3(0.0f);

    int32_t mDebugViewInputs = 0;
    int32_t mDebugViewEquation = 0;

    const uint32_t mRenderAhead = 2;
    uint32_t mFrameIndex = 0;

    int32_t mAnimationIndex = 0;
    float mAnimationTimer = 0.0f;
    bool mbAnimate = true;
    bool mbDisplayBackground = true;
};