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
    VkCommandBufferBeginInfo cmdBI = vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBI = vks::initializers::renderPassBeginInfo();
    renderPassBI.renderPass = renderPass;
    renderPassBI.renderArea.offset.x = 0;
    renderPassBI.renderArea.offset.y = 0;
    renderPassBI.renderArea.extent.width = width;
    renderPassBI.renderArea.extent.height = height;
    renderPassBI.clearValueCount = 2;
    renderPassBI.pClearValues = clearValues;

    const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    const VkRect2D scissor = vks::initializers::rect2D((int32_t)width, (int32_t)height, 0, 0);
    for (int32_t i = 0; i < drawCmdBuffers.size(); i++)
    {
        renderPassBI.framebuffer = frameBuffers[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBI));
        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
        // Bind Scene Matrices Descriptor to Set 0
        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSet, 0, nullptr);
        // Draw
        mScene.Draw(drawCmdBuffers[i], 0,mPipelineLayout);
        DrawUI(drawCmdBuffers[i]);

        vkCmdEndRenderPass(drawCmdBuffers[i]);
        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void LeoRenderer::VulkanRenderer::LoadGLTFFile(std::string& filename)
{
    mScene.LoadFromFile(filename, vulkanDevice, queue);
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