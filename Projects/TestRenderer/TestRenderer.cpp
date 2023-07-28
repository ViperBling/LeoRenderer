#include "TestRenderer.hpp"

TestRenderer::TestRenderer() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "Test Renderer";
    mWidth = 1280;
    mHeight = 720;
    mCamera.mType = CameraType::LookAt;
    mCamera.mbFlipY = true;
    mCamera.SetPosition(glm::vec3(0.0f, -0.1f, -1.0f));
    mCamera.SetRotation(glm::vec3(0.0f, 45.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);
}

TestRenderer::~TestRenderer()
{
    vkDestroyPipeline(mDevice, mPipelines.mScenePipe, nullptr);
    vkDestroyPipeline(mDevice, mPipelines.mSkyboxPipe, nullptr);
    vkDestroyPipeline(mDevice, mPipelines.mAlphaBlend, nullptr);

    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescSetLayouts.mSceneLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescSetLayouts.mMaterialLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescSetLayouts.mNodeLayout, nullptr);

    mRenderScene.mScene.Destroy(mDevice);
    mRenderScene.mSkybox.Destroy(mDevice);

    for (auto buffer : mUBOBuffers)
    {
        buffer.mParamsUBO.Destroy();
        buffer.mSceneUBO.Destroy();
        buffer.mSkyboxUBO.Destroy();
    }

    mSceneTextures.mEnvCube.Destroy();
    mSceneTextures.mIrradianceCube.Destroy();
    mSceneTextures.mPreFilterCube.Destroy();
    mSceneTextures.mLUTBRDF.Destroy();
    mSceneTextures.mDummy.Destroy();
}

void TestRenderer::GetEnabledFeatures()
{
    if (mDeviceFeatures.samplerAnisotropy) mEnabledFeatures.samplerAnisotropy = VK_TRUE;
}

void TestRenderer::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufferBI = LeoVK::Init::CmdBufferBeginInfo();

    VkClearValue clearValues[3];
    if (mSettings.multiSampling)
    {
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[2].depthStencil = { 1.0f, 0 };
    }
    else
    {
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
    }

    VkRenderPassBeginInfo rpBI = LeoVK::Init::RenderPassBeginInfo();
    rpBI.renderPass = mRenderPass;
    rpBI.renderArea.offset = {0, 0};
    rpBI.renderArea.extent = {mWidth, mHeight};
    rpBI.clearValueCount = mSettings.multiSampling ? 3 : 2;
    rpBI.pClearValues = clearValues;

    for (uint32_t i = 0; i < mDrawCmdBuffers.size(); i++)
    {
        rpBI.framebuffer = mFrameBuffers[i];
        VkCommandBuffer currentCmdBuffer = mDrawCmdBuffers[i];

        VK_CHECK(vkBeginCommandBuffer(currentCmdBuffer, &cmdBufferBI))

        VkViewport vp = LeoVK::Init::Viewport((float)mWidth, (float)mHeight, 0.0f, 1.0f);
        vkCmdSetViewport(currentCmdBuffer, 0, 1, &vp);
        VkRect2D sc = LeoVK::Init::Rect2D((int)mWidth, (int)mHeight, 0, 0);
        vkCmdSetScissor(currentCmdBuffer, 0, 1, &sc);

        VkDeviceSize offsets[1] = {0};

        if (mbDisplayBackground)
        {
            vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSets[i].mSkyboxDesc, 0, nullptr);
            vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.mSkyboxPipe);
            mRenderScene.mSkybox.Draw(currentCmdBuffer);
        }

        mBoundPipeline = VK_NULL_HANDLE;

        // Opaque primitives first
        for (auto node : mRenderScene.mScene.mNodes)
        {
            RenderNode(node, i, LeoVK::Material::ALPHA_MODE_OPAQUE);
        }
        // Alpha masked primitives
        for (auto node : mRenderScene.mScene.mNodes)
        {
            RenderNode(node, i, LeoVK::Material::ALPHA_MODE_MASK);
        }
        // Transparent primitives
        for (auto node : mRenderScene.mScene.mNodes)
        {
            RenderNode(node, i, LeoVK::Material::ALPHA_MODE_BLEND);
        }

        mRenderScene.mScene.Draw(currentCmdBuffer);

        DrawUI(currentCmdBuffer);

        vkCmdEndRenderPass(currentCmdBuffer);

        VK_CHECK(vkEndCommandBuffer(currentCmdBuffer))

    }
}

void TestRenderer::Render()
{

}

void TestRenderer::ViewChanged()
{
    VKRendererBase::ViewChanged();
}

void TestRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    VKRendererBase::OnUpdateUIOverlay(overlay);
}



void TestRenderer::RenderNode(LeoVK::Node *node, uint32_t cbIdx, LeoVK::Material::AlphaMode alphaMode)
{
    if (node->mpMesh)
    {
        for (LeoVK::Primitive* primitive : node->mpMesh->mPrimitives)
        {
            if (primitive->mMaterial.mAlphaMode == alphaMode)
            {
                VkPipeline pipeline = VK_NULL_HANDLE;
                switch (alphaMode)
                {
                    case LeoVK::Material::ALPHA_MODE_OPAQUE:
                    case LeoVK::Material::ALPHA_MODE_MASK:
                        pipeline = primitive->mMaterial.mbDoubleSided ? mPipelines.mDoubleSided : mPipelines.mScenePipe;
                        break;
                    case LeoVK::Material::ALPHA_MODE_BLEND:
                        pipeline = mPipelines.mAlphaBlend;
                        break;
                    default:
                        break;
                }

                if (pipeline != mBoundPipeline)
                {
                    vkCmdBindPipeline(mDrawCmdBuffers[cbIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    mBoundPipeline = pipeline;
                }

                const std::vector<VkDescriptorSet> descSets = {
                    mDescSets[cbIdx].mSceneDesc,
                    primitive->mMaterial.mDescriptorSet,
                    node->mpMesh->mUniformBuffer.mDescriptorSet,
                };
                vkCmdBindDescriptorSets(
                    mDrawCmdBuffers[cbIdx],
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    mPipelineLayout, 0,
                    static_cast<uint32_t>(descSets.size()),
                    descSets.data(), 0,
                    nullptr);

                // Pass material parameters as push constants
                PushConstBlockMaterial pushConstBlockMaterial{};
                pushConstBlockMaterial.mEmissiveFactor = primitive->mMaterial.mEmissiveFactor;
                // To save push constant space, availabilty and texture coordiante set are combined
                // -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
                pushConstBlockMaterial.mColorTextureSet = primitive->mMaterial.mpBaseColorTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mBaseColor : -1;
                pushConstBlockMaterial.mNormalTextureSet = primitive->mMaterial.mpNormalTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mNormal : -1;
                pushConstBlockMaterial.mOcclusionTextureSet = primitive->mMaterial.mpOcclusionTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mOcclusion : -1;
                pushConstBlockMaterial.mEmissiveTextureSet = primitive->mMaterial.mpEmissiveTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mEmissive : -1;
                pushConstBlockMaterial.mAlphaMask = static_cast<float>(primitive->mMaterial.mAlphaMode == LeoVK::Material::ALPHA_MODE_MASK);
                pushConstBlockMaterial.mAlphaMaskCutoff = primitive->mMaterial.mAlphaCutoff;

                // TODO: glTF specs states that metallic roughness should be preferred, even if specular glosiness is present

                if (primitive->mMaterial.mPBRWorkFlows.mbMetallicRoughness)
                {
                    // Metallic roughness workflow
                    pushConstBlockMaterial.mWorkflow = static_cast<float>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
                    pushConstBlockMaterial.mBaseColorFactor = primitive->mMaterial.mBaseColorFactor;
                    pushConstBlockMaterial.mMetallicFactor = primitive->mMaterial.mMetallicFactor;
                    pushConstBlockMaterial.mRoughnessFactor = primitive->mMaterial.mRoughnessFactor;
                    pushConstBlockMaterial.mPhysicalDescTexSet = primitive->mMaterial.mpMetallicRoughnessTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mMetallicRoughness : -1;
                    pushConstBlockMaterial.mColorTextureSet = primitive->mMaterial.mpBaseColorTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mBaseColor : -1;
                }

                if (primitive->mMaterial.mPBRWorkFlows.mbSpecularGlossiness)
                {
                    // Specular glossiness workflow
                    pushConstBlockMaterial.mWorkflow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSSINESS);
                    pushConstBlockMaterial.mPhysicalDescTexSet = primitive->mMaterial.mExtension.mpSpecularGlossinessTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mSpecularGlossiness : -1;
                    pushConstBlockMaterial.mColorTextureSet = primitive->mMaterial.mExtension.mpDiffuseTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mBaseColor : -1;
                    pushConstBlockMaterial.mDiffuseFactor = primitive->mMaterial.mExtension.mDiffuseFactor;
                    pushConstBlockMaterial.mSpecularFactor = glm::vec4(primitive->mMaterial.mExtension.mSpecularFactor, 1.0f);
                }

                vkCmdPushConstants(mDrawCmdBuffers[cbIdx], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstBlockMaterial), &pushConstBlockMaterial);
            }
        }
    }
}

void TestRenderer::LoadScene(const std::string& filename)
{
    std::cout << "Loading scene from " << filename << std::endl;
    mRenderScene.mScene.Destroy(mDevice);
    mAnimIndex = 0;
    mAnimTimer = 0.0f;
    auto tStart = std::chrono::high_resolution_clock::now();
    mRenderScene.mScene.LoadFromFile(filename, mpVulkanDevice, mQueue);
    auto tFileLoad = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
    std::cout << "Loading took " << tFileLoad << " ms" << std::endl;
}

void TestRenderer::LoadAssets()
{
    mSceneTextures.mDummy.LoadFromFile(GetAssetsPath() + "Textures/empty.ktx", VK_FORMAT_R8G8B8A8_UNORM, mpVulkanDevice, mQueue);

    std::string sceneFile = GetAssetsPath() + "Models/BusterDrone/busterDrone.gltf";
    std::string envFile = GetAssetsPath() + "Textures/Env/papermill.ktx";

    for (auto & mArg : mArgs)
    {
        if ((std::string(mArg).find(".gltf") != std::string::npos) ||
            (std::string(mArg).find(".glb") != std::string::npos))
        {
            std::ifstream file(mArg);
            if (file.good())
            {
                sceneFile = mArg;
            }
            else
            {
                std::cout << "could not load \"" << mArg << "\"" << std::endl;
            }
        }
        if (std::string(mArg).find(".ktx") != std::string::npos)
        {
            std::ifstream file(mArg);
            if (file.good())
            {
                envFile = mArg;
            }
            else
            {
                std::cout << "could not load \"" << mArg << "\"" << std::endl;
            }
        }
    }
    LoadScene(sceneFile);
    mRenderScene.mSkybox.LoadFromFile(GetAssetsPath() + "Models/Box.gltf", mpVulkanDevice, mQueue);
    LoadEnv(envFile);
}

void TestRenderer::LoadEnv(const std::string& filename)
{
    std::cout << "Loading environment from " << filename << std::endl;
    if (mSceneTextures.mEnvCube.mImage)
    {
        mSceneTextures.mEnvCube.Destroy();
        mSceneTextures.mIrradianceCube.Destroy();
        mSceneTextures.mPreFilterCube.Destroy();
    }
    mSceneTextures.mEnvCube.LoadFromFile(filename, VK_FORMAT_R16G16B16A16_SFLOAT, mpVulkanDevice, mQueue);
    GenerateCubeMap();
}

void TestRenderer::GenerateCubeMap()
{

}

void TestRenderer::GeneratePrefilterCube()
{

}

void TestRenderer::GenerateLUT()
{

}

void TestRenderer::SetupDescriptors()
{

}

void TestRenderer::PreparePipelines()
{

}

void TestRenderer::PrepareUniformBuffers()
{

}

void TestRenderer::UpdateUniformBuffers()
{

}

void TestRenderer::PreRender()
{

}
