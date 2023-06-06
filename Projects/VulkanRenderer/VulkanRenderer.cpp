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

/*
 *  构造资源描述符，用来描述管线上用到的资源，这里主要是变换矩阵和使用的材质（BaseColor，NormalMap)
 *  1) 创建资源描述符池
 *  2) 创建两类资源的描述符集布局，描述这两个资源在GPU中怎样被使用。传参有资源类型（uniform还是图像sampler），
 *     管线阶段，绑定位置（对应了Shader中的Location）
 *  3) 创建管线布局
 *  4) 分别为两种资源分配描述符集
 */
void LeoRenderer::VulkanRenderer::SetupDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSize =
    {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(mScene.mMaterials.size()) * 2),
    };
    // 一个Set用于矩阵变换和逐物体材质
    const uint32_t maxSetCount = static_cast<uint32_t>(mScene.mTextures.size()) + 1;
    VkDescriptorPoolCreateInfo descPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSize, maxSetCount);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool));

    // 用于传矩阵的描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings =
    {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(descSetLayoutBindings.data(), static_cast<uint32_t>(descSetLayoutBindings.size()));
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &mCustomDescSetLayouts.mMatrices));

    // 材质贴图描述符集布局
    descSetLayoutBindings =
    {
        // Color Map
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // Normal Map
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    descSetLayoutCI.pBindings = descSetLayoutBindings.data();
    descSetLayoutCI.bindingCount = 2;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &mCustomDescSetLayouts.mTextures));

    // 两个描述符集的管线布局，一个是矩阵，另一个是材质
    std::array<VkDescriptorSetLayout, 2> descSetLayouts = { mCustomDescSetLayouts.mMatrices, mCustomDescSetLayouts.mTextures };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(descSetLayouts.data(), static_cast<uint32_t>(descSetLayouts.size()));
    // 使用pushconstant传递矩阵
    VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &mPipelineLayout));

    // 矩阵描述符集
    VkDescriptorSetAllocateInfo matDescSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mCustomDescSetLayouts.mMatrices, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &matDescSetAI, &mDescSet));
    VkWriteDescriptorSet matWriteDescSet = vks::initializers::writeDescriptorSet(mDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mShaderData.mBuffer.descriptor);
    vkUpdateDescriptorSets(device, 1, &matWriteDescSet, 0, nullptr);

    // 材质的描述符集
    for (auto & mat : mScene.mMaterials)
    {
        const VkDescriptorSetAllocateInfo texDescSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mCustomDescSetLayouts.mTextures, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &texDescSetAI, &mat.mDescriptorSet));
        VkDescriptorImageInfo colorMap = mat.mBaseColorTexture->mDescriptor;
        VkDescriptorImageInfo normalMap = mat.mNormalTexture->mDescriptor;
        std::vector<VkWriteDescriptorSet> writeDescSets =
        {
            vks::initializers::writeDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
            vks::initializers::writeDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
        };
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
    }
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