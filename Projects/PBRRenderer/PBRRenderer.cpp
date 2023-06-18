#include "PBRRenderer.h"

PBRRenderer::PBRRenderer() : VulkanFramework(ENABLE_MSAA, ENABLE_VALIDATION)
{
    title = "PBRRenderer";
}

PBRRenderer::~PBRRenderer()
{
    vkDestroyPipeline(device, mPipelines.mPipelineSkyBox, nullptr);
    vkDestroyPipeline(device, mPipelines.mPipelinePBR, nullptr);
    vkDestroyPipeline(device, mPipelines.mPipelinePBRAlphaBlend, nullptr);

    vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutScene, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutMaterial, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutNode, nullptr);

    for (auto buffer : mUniformBuffers)
    {
        buffer.mUBOParams.destroy();
        buffer.mUBOScene.destroy();
        buffer.mUBOSkybox.destroy();
    }
    for (auto fence : mWaitFence) vkDestroyFence(device, fence, nullptr);
    for (auto sem : mRenderCompleteSemaphore) vkDestroySemaphore(device, sem, nullptr);
    for (auto sem : mPresentCompleteSemaphore) vkDestroySemaphore(device, sem, nullptr);

    mTextures.mTexEnvCube.destroy();
    mTextures.mTexIrradianceCube.destroy();
    mTextures.mTexPreFilterCube.destroy();
    mTextures.mTexLUTBRDF.destroy();
    mTextures.mTexDummy.destroy();
}

void PBRRenderer::GetEnabledFeatures()
{
    enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
}

void PBRRenderer::RenderNode(LeoRenderer::Node *node, uint32_t cbIndex, LeoRenderer::Material::AlphaMode alphaMode)
{
    if (node->mMesh)
    {
        for (LeoRenderer::Primitive* primitive : node->mMesh->mPrimitives)
        {
            VkPipeline pipeline = VK_NULL_HANDLE;
            switch (alphaMode)
            {
                case LeoRenderer::Material::ALPHA_MODE_OPAQUE:
                case LeoRenderer::Material::ALPHA_MODE_MASK:
                    pipeline = primitive->mMaterial.m_bDoubleSided ? mPipelines.mPipelineDoubleSided : mPipelines.mPipelinePBR;
                    break;
                case LeoRenderer::Material::ALPHA_MODE_BLEND:
                    pipeline = mPipelines.mPipelinePBRAlphaBlend;
                    break;
            }
            if (pipeline != VK_NULL_HANDLE)
            {
                vkCmdBindPipeline(drawCmdBuffers[cbIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                mBoundPipeline = pipeline;
            }

            const std::vector<VkDescriptorSet> descSets =
            {
                mDescSets[cbIndex].mDescScene,
                primitive->mMaterial.mDescriptorSet,
                node->mMesh->mUniformBuffer.descriptorSet,
            };
            vkCmdBindDescriptorSets(
                drawCmdBuffers[cbIndex],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                mPipelineLayout, 0,
                static_cast<uint32_t>(descSets.size()),
                descSets.data(), 0,
                nullptr);

            // Push constant
            PushConstantBlockMaterial pushConstantBlockMat{};
            pushConstantBlockMat.mFactorEmissive = primitive->mMaterial.mEmissiveFactor;
            // 读取对应texture的index
            pushConstantBlockMat.mTextureSetColor = primitive->mMaterial.mBaseColorTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mBaseColor : -1;
            pushConstantBlockMat.mTextureSetNormal = primitive->mMaterial.mNormalTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mNormal : -1;
            pushConstantBlockMat.mTextureSetOcclusion = primitive->mMaterial.mOcclusionTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mOcclusion : -1;
            pushConstantBlockMat.mTextureSetEmissive = primitive->mMaterial.mEmissiveTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mEmissive : -1;
            pushConstantBlockMat.mFactorAlphaMask = static_cast<float>(primitive->mMaterial.mAlphaMode == LeoRenderer::Material::ALPHA_MODE_MASK);
            pushConstantBlockMat.mFactorAlphaMaskCutoff = primitive->mMaterial.mAlphaCutoff;

            if (primitive->mMaterial.mPBRWorkFlows.mbMetallicRoughness)
            {
                pushConstantBlockMat.mWorkFlow = static_cast<float>(PBR_WORKFLOW_METALLIC_ROUGHNESS);
                pushConstantBlockMat.mFactorBaseColor = primitive->mMaterial.mBaseColorFactor;
                pushConstantBlockMat.mFactorMetallic = primitive->mMaterial.mMetallicFactor;
                pushConstantBlockMat.mFactorRoughness = primitive->mMaterial.mRoughnessFactor;
                pushConstantBlockMat.mTextureSetPhysicalDescriptor = primitive->mMaterial.mMetallicRoughnessTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mMetallicRoughness : -1;
                pushConstantBlockMat.mTextureSetColor = primitive->mMaterial.mBaseColorTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mBaseColor : -1;
            }

            if (primitive->mMaterial.mPBRWorkFlows.mbSpecularGlossiness)
            {
                pushConstantBlockMat.mWorkFlow = static_cast<float>(PBR_WORKFLOW_SPECULAR_GLOSINESS);
                pushConstantBlockMat.mTextureSetPhysicalDescriptor = primitive->mMaterial.mExtension.mSpecularGlossinessTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mSpecularGlossiness : -1;
                pushConstantBlockMat.mTextureSetColor = primitive->mMaterial.mExtension.mDiffuseTexture != nullptr ? primitive->mMaterial.mTexCoordSet.mBaseColor : -1;
                pushConstantBlockMat.mFactorDiffuse = primitive->mMaterial.mExtension.mDiffuseFactor;
                pushConstantBlockMat.mFactorSpecular = glm::vec4(primitive->mMaterial.mExtension.mSpecularFactor, 1.0f);
            }

            vkCmdPushConstants(mCmdBuffers[cbIndex], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlockMaterial), &pushConstantBlockMat);

            if (primitive->mbHasIndex)
            {
                vkCmdDrawIndexed(mCmdBuffers[cbIndex], primitive->mIndexCount, 1, primitive->mFirstIndex, 0, 0);
            }
            else
            {
                vkCmdDraw(mCmdBuffers[cbIndex], primitive->mVertexCount, 1, 0, 0);
            }
        }
    }
    for (auto child : node->mChildren) RenderNode(child, cbIndex, alphaMode);
}

void PBRRenderer::LoadScene(std::string& filename)
{
    std::cout << "Loading scene from " << filename << std::endl;
    mModels.mModelScene.OnDestroy();
    mAnimationIndex = 0;
    mAnimationTimer = 0.0f;

    auto tStart = std::chrono::high_resolution_clock::now();
    mModels.mModelScene.LoadFromFile(filename, vulkanDevice, queue);
    auto tFileLoad = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
    std::cout << "Loading took " << tFileLoad << " ms" << std::endl;
    camera.setPosition({ 0.0f, 0.0f, 1.0f });
    camera.setRotation({ 0.0f, 0.0f, 0.0f });
}

void PBRRenderer::LoadEnvironment(std::string)
{

}

void PBRRenderer::LoadAssets()
{

}

void PBRRenderer::SetupNodeDescriptorSet(LeoRenderer::Node *node)
{

}

void PBRRenderer::SetDescriptors()
{

}

void PBRRenderer::PreparePipelines()
{

}

void PBRRenderer::GenerateBRDFLUT()
{

}

void PBRRenderer::GenerateCubeMaps()
{

}

void PBRRenderer::PrepareUniformBuffers()
{

}

void PBRRenderer::UpdateUniformBuffers()
{

}

void PBRRenderer::UpdateParams()
{

}

void PBRRenderer::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufferBI = vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[3];
    if (settings.multiSampling)
    {
        clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
        clearValues[1].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
        clearValues[2].depthStencil = { 1.0f, 0 };
    }
    else
    {
        clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };
    }

    VkRenderPassBeginInfo renderPassBI = vks::initializers::renderPassBeginInfo();
    renderPassBI.renderPass = renderPass;
    renderPassBI.renderArea.offset = {0, 0};
    renderPassBI.renderArea.extent = {width, height};
    renderPassBI.clearValueCount = settings.multiSampling ? 3 : 2;
    renderPassBI.pClearValues = clearValues;

    for (uint32_t i = 0; i < mCmdBuffers.size(); i++)
    {
        renderPassBI.framebuffer = frameBuffers[i];

        VkCommandBuffer currentCmdBuffer = mCmdBuffers[i];

        VK_CHECK_RESULT(vkBeginCommandBuffer(currentCmdBuffer, &cmdBufferBI))
        vkCmdBeginRenderPass(currentCmdBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
        vkCmdSetViewport(currentCmdBuffer, 0, 1, &viewport);

        VkRect2D  scissor = vks::initializers::rect2D((int32_t)width, (int32_t)height, 0, 0);
        vkCmdSetScissor(currentCmdBuffer, 0, 1, &scissor);

        VkDeviceSize offsets[1]{0};
        if (mbDisplayBackground)
        {
            vkCmdBindDescriptorSets(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSets[i].mDescSkybox, 0, nullptr);
            vkCmdBindPipeline(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.mPipelineSkyBox);
            mModels.mModelSkybox.Draw(currentCmdBuffer);
        }

        LeoRenderer::GLTFModel& model = mModels.mModelScene;

        vkCmdBindVertexBuffers(currentCmdBuffer, 0, 1, &model.mVertices.mVBuffer, offsets);
        if (model.mIndices.mIBuffer != VK_NULL_HANDLE) vkCmdBindIndexBuffer(currentCmdBuffer, model.mIndices.mIBuffer, 0, VK_INDEX_TYPE_UINT32);

        mBoundPipeline = VK_NULL_HANDLE;

        for (auto node : model.mNodes) RenderNode(node, i, LeoRenderer::Material::ALPHA_MODE_OPAQUE);
        for (auto node : model.mNodes) RenderNode(node, i, LeoRenderer::Material::ALPHA_MODE_MASK);
        for (auto node : model.mNodes) RenderNode(node, i, LeoRenderer::Material::ALPHA_MODE_BLEND);

        DrawUI(currentCmdBuffer);
        VK_CHECK_RESULT(vkEndCommandBuffer(currentCmdBuffer));
    }
}

void PBRRenderer::Prepare()
{
    VulkanFramework::Prepare();
}

void PBRRenderer::Render()
{

}

void PBRRenderer::ViewChanged()
{
    VulkanFramework::ViewChanged();
}

void PBRRenderer::WindowResized()
{
    VulkanFramework::WindowResized();
}

void PBRRenderer::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    VulkanFramework::OnUpdateUIOverlay(overlay);
}

void PBRRenderer::FileDropped(std::string &filename)
{
    VulkanFramework::FileDropped(filename);
}
