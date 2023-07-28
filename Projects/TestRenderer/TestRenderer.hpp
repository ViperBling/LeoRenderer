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
    glm::vec4 mLights[4];
    float mExposure = 4.5f;
    float mGamma = 2.2f;
};



class TestRenderer : public VKRendererBase
{
public:
    TestRenderer();
    virtual ~TestRenderer();
    void GetEnabledFeatures() override;
    void BuildCommandBuffers() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void LoadScene(std::string filename);
    void LoadAssets();
    void SetupDescriptors();
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void PreRender();

public:
    bool mbDisableSkybox = true;


    RenderScene mRenderScene;

};
