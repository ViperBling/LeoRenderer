#include "VulkanRenderer.h"

LeoRenderer::VulkanRenderer::VulkanRenderer()
{
    title = "GLTF Test";
    camera.type = Camera::CameraType::lookat;
    camera.flipY = true;
    camera.setPosition(glm::vec3(0.0f, 1.0f, 0.0f));
    camera.setRotation(glm::vec3(0.0f, -90.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
}

LeoRenderer::VulkanRenderer::~VulkanRenderer()
{
    vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, mCustomDescSetLayouts.mMatrices, nullptr);
    vkDestroyDescriptorSetLayout(device, mCustomDescSetLayouts.mTextures, nullptr);
    mShaderData.mBuffer.destroy();
}

void LeoRenderer::VulkanRenderer::GetEnabledFeatures()
{
    enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
}

void LeoRenderer::VulkanRenderer::BuildCommandBuffers()
{
    VulkanFramework::BuildCommandBuffers();
}

void LeoRenderer::VulkanRenderer::LoadGLTFFile(std::string& filename)
{

}

void LeoRenderer::VulkanRenderer::LoadAssets()
{
    auto assets = getAssetPath() + "Models/BusterDrone/busterDrone.gltf";
    LoadGLTFFile(assets);
}

void LeoRenderer::VulkanRenderer::SetupDescriptors()
{

}

void LeoRenderer::VulkanRenderer::PreparePipelines()
{

}

void LeoRenderer::VulkanRenderer::PrepareUniformBuffers()
{

}

void LeoRenderer::VulkanRenderer::UpdateUniformBuffers()
{

}

void LeoRenderer::VulkanRenderer::Prepare()
{
    VulkanFramework::Prepare();
}

void LeoRenderer::VulkanRenderer::Render()
{

}

void LeoRenderer::VulkanRenderer::ViewChanged()
{
    VulkanFramework::ViewChanged();
}

void LeoRenderer::VulkanRenderer::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    VulkanFramework::OnUpdateUIOverlay(overlay);
}


int main()
{
    return 0;
}