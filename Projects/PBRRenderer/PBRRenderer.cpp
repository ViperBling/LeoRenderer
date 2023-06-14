//
// Created by Administrator on 2023/6/13.
//

#include "PBRRenderer.h"

PBRRenderer::PBRRenderer() : VulkanFramework(ENABLE_VALIDATION)
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
            pushConstantBlockMat.mFactorEmissive = primitive->mMaterial.mEmissive;
        }
    }
}

void PBRRenderer::LoadScene(std::string)
{

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
    VulkanFramework::BuildCommandBuffers();
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
