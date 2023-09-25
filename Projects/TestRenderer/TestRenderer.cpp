#include "TestRenderer.hpp"

TestRenderer::TestRenderer() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "Test Render";
    mCamera.mType = CameraType::LookAt;
    mCamera.mbFlipY = true;
    mCamera.SetPosition(glm::vec3(0.0f, 0.0f, -5.0f));
    mCamera.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);
}

TestRenderer::~TestRenderer()
{
    if (mDevice)
    {
        vkDestroyPipeline(mDevice, mPipelines.mPBRPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mUniformDescSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mTextureDescSetLayout, nullptr);

        mUniformBuffers.mObjectUBO.Destroy();
        mUniformBuffers.mParamsUBO.Destroy();

        mRenderScene.Destroy(mDevice);
    }
}

void TestRenderer::GetEnabledFeatures()
{
    mEnabledFeatures.samplerAnisotropy = mDeviceFeatures.samplerAnisotropy;
}

void TestRenderer::SetupDescriptors()
{
    uint32_t imageSamperCount = 0;
    uint32_t materialCount = 0;
    uint32_t meshCount = 0;
    uint32_t uniformCount = 2;

    for (auto& mat : mRenderScene.mMaterials)
    {
        imageSamperCount += 5;
        materialCount++;
    }
    
    // 两种类型的DescSet，分别创建DescSetPool。UniformBuffer两个，TexSampler根据Material中的贴图数量创建
    std::vector<VkDescriptorPoolSize> poolSize = {
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformCount),
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamperCount)
    };
    const uint32_t maxSetCount = static_cast<uint32_t>(mRenderScene.mTextures.size()) + 1;
    VkDescriptorPoolCreateInfo descSetPoolCI = LeoVK::Init::DescPoolCreateInfo(poolSize, maxSetCount + 1);
    VK_CHECK(vkCreateDescriptorPool(mDevice, &descSetPoolCI, nullptr, &mDescPool));

    // UniformBuffer的DescSetLayout
    std::vector<VkDescriptorSetLayoutBinding> uniformSetLayoutBindings = {
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo uniformDescSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(uniformSetLayoutBindings);
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &uniformDescSetLayoutCI, nullptr, &mDescSetLayout.mUniformDescSetLayout));

    // Sampler的DescSetLayout
    std::vector<VkDescriptorSetLayoutBinding> samplerDescSetLayoutBinding = {
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
    };
    VkDescriptorSetLayoutCreateInfo samplerDescSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(samplerDescSetLayoutBinding);
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &samplerDescSetLayoutCI, nullptr, &mDescSetLayout.mTextureDescSetLayout));

    // 分配Uniform的DescSet
    VkDescriptorSetAllocateInfo descSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mUniformDescSetLayout, uniformCount);
    VK_CHECK(vkAllocateDescriptorSets(mDevice, &descSetAI, &mDescSets.mObjectDescSet));
    std::vector<VkWriteDescriptorSet> objWriteDescSet = {
        LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mUniformBuffers.mObjectUBO.mDescriptor),
        LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &mUniformBuffers.mParamsUBO.mDescriptor),
    };
    vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(objWriteDescSet.size()), objWriteDescSet.data(), 0, nullptr);

    // 分配Texture的DescSet
    for (auto& mat : mRenderScene.mMaterials)
    {
        const VkDescriptorSetAllocateInfo texDescAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mTextureDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &texDescAI, &mat.mDescriptorSet));
        std::vector<VkDescriptorImageInfo> imageDescs = {
            mat.mpBaseColorTexture ? mat.mpBaseColorTexture->mDescriptor : mRenderScene.mTextures.back().mDescriptor,
            mat.mpMetallicRoughnessTexture ? mat.mpMetallicRoughnessTexture->mDescriptor : mRenderScene.mTextures.back().mDescriptor,
            mat.mpNormalTexture ? mat.mpNormalTexture->mDescriptor : mRenderScene.mTextures.back().mDescriptor,
            mat.mpOcclusionTexture ? mat.mpOcclusionTexture->mDescriptor : mRenderScene.mTextures.back().mDescriptor,
            mat.mpEmissiveTexture ? mat.mpEmissiveTexture->mDescriptor : mRenderScene.mTextures.back().mDescriptor
        };
        std::vector<VkWriteDescriptorSet> texWriteDescSet = {
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescs[0]),
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescs[1]),
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescs[2]),
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescs[3]),
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &imageDescs[4]),
        };
        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(texWriteDescSet.size()), texWriteDescSet.data(), 0, nullptr);
    }
}

void TestRenderer::PreparePipelines()
{
    VkPipelineInputAssemblyStateCreateInfo iaStateCI = LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsStateCI = LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cbAttachCI = LeoVK::Init::PipelineCBAState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbStateCI = LeoVK::Init::PipelineCBStateCreateInfo(1, &cbAttachCI);
    VkPipelineDepthStencilStateCreateInfo dsStateCI = LeoVK::Init::PipelineDSStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo vpStateCI = LeoVK::Init::PipelineVPStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo msStateCI = LeoVK::Init::PipelineMSStateCreateInfo(mSettings.multiSampling ? mSettings.sampleCount : VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyStateCI = LeoVK::Init::PipelineDYStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
    std::array<VkPipelineShaderStageCreateInfo, 2> ssStateCIs{};

    // 确定pipelineLayout
    std::array<VkDescriptorSetLayout, 2> descSetLayouts = { mDescSetLayout.mUniformDescSetLayout, mDescSetLayout.mTextureDescSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo(descSetLayouts.data(), static_cast<uint32_t>(descSetLayouts.size()));
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &mPipelineLayout));

    const std::vector<VkVertexInputBindingDescription> viBindings = {
        LeoVK::Init::VIBindingDescription(0, sizeof(LeoVK::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> viAttributes = {
        LeoVK::Init::VIAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mPos)),
        LeoVK::Init::VIAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mNormal)),
        LeoVK::Init::VIAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mUV0)),
        LeoVK::Init::VIAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mTangent)),
    };
    VkPipelineVertexInputStateCreateInfo viStateCI = LeoVK::Init::PipelineVIStateCreateInfo(viBindings, viAttributes);

    VkGraphicsPipelineCreateInfo pipelineCI = LeoVK::Init::PipelineCreateInfo(mPipelineLayout, mRenderPass, 0);
    pipelineCI.pVertexInputState = &viStateCI;
    pipelineCI.pInputAssemblyState = &iaStateCI;
    pipelineCI.pRasterizationState = &rsStateCI;
    pipelineCI.pColorBlendState = &cbStateCI;
    pipelineCI.pMultisampleState = &msStateCI;
    pipelineCI.pViewportState = &vpStateCI;
    pipelineCI.pDepthStencilState = &dsStateCI;
    pipelineCI.pDynamicState = &dyStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(ssStateCIs.size());
    pipelineCI.pStages = ssStateCIs.data();

    ssStateCIs[0] = LoadShader(GetShadersPath() + "TestRenderer/PBRShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ssStateCIs[1] = LoadShader(GetShadersPath() + "TestRenderer/PBRShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPBRPipeline));
}

void TestRenderer::PrepareUniformBuffers()
{
    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniformBuffers.mObjectUBO,
        sizeof(mUBOMatrices)));

    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniformBuffers.mParamsUBO,
        sizeof(mUBOParams)));

    VK_CHECK(mUniformBuffers.mObjectUBO.Map())
    VK_CHECK(mUniformBuffers.mParamsUBO.Map())
    UpdateUniformBuffers();
    UpdateParams();
}

void TestRenderer::UpdateUniformBuffers()
{
    mUBOMatrices.mProj = mCamera.mMatrices.mPerspective;
    mUBOMatrices.mView = mCamera.mMatrices.mView;
    mUBOMatrices.mModel = glm::mat4(1.0);
    mUBOMatrices.mCamPos = mCamera.mPosition;

    memcpy(mUniformBuffers.mObjectUBO.mpMapped, &mUBOMatrices, sizeof(mUBOMatrices));
}

void TestRenderer::UpdateParams()
{
    mUBOParams.mLight = glm::vec4(0.0f, 10.0f, 20.0f, 1.0f);
    memcpy(mUniformBuffers.mParamsUBO.mpMapped, &mUBOParams, sizeof(mUBOParams));
}

void TestRenderer::LoadAssets()
{
    mRenderScene.LoadFromFile(GetAssetsPath() + "Models/BusterDrone/busterDrone.gltf", mpVulkanDevice, mQueue, LeoVK::FileLoadingFlags::PreTransformVertices);
    // mRenderScene.LoadFromFile(GetAssetsPath() + "Models/DamagedHelmet/glTF-Embedded/DamagedHelmet.gltf", mpVulkanDevice, mQueue, LeoVK::FileLoadingFlags::PreTransformVertices | LeoVK::FileLoadingFlags::FlipY);
    // mRenderScene.LoadFromFile(GetAssetsPath() + "Models/FlightHelmet/glTF/FlightHelmet.gltf", mpVulkanDevice, mQueue);
}

void TestRenderer::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBI = LeoVK::Init::CmdBufferBeginInfo();

    VkClearValue clearValues[3];
    clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
    clearValues[1].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };
    clearValues[2].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpBI = LeoVK::Init::RenderPassBeginInfo();
    rpBI.renderPass = mRenderPass;
    rpBI.renderArea.offset = {0, 0};
    rpBI.renderArea.extent = {mWidth, mHeight};
    rpBI.clearValueCount = 3;
    rpBI.pClearValues = clearValues;

    const VkViewport viewport = LeoVK::Init::Viewport((float)mWidth, (float)mHeight, 0.0f, 1.0f);
    const VkRect2D scissor = LeoVK::Init::Rect2D((int)mWidth, (int)mHeight, 0, 0);

    for (int i = 0; i < mDrawCmdBuffers.size(); i++)
    {
        rpBI.framebuffer = mFrameBuffers[i];
        VK_CHECK(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBI))
        vkCmdBeginRenderPass(mDrawCmdBuffers[i], &rpBI, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

        vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSets.mObjectDescSet, 0, nullptr);
        vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines.mPBRPipeline);

        mRenderScene.Draw(mDrawCmdBuffers[i], mPipelineLayout, 1, LeoVK::Material::BINDIMAGES);

        DrawUI(mDrawCmdBuffers[i]);

        vkCmdEndRenderPass(mDrawCmdBuffers[i]);
        VK_CHECK(vkEndCommandBuffer(mDrawCmdBuffers[i]))
    }
}

void TestRenderer::Prepare()
{
    VKRendererBase::Prepare();
    LoadAssets();
    PrepareUniformBuffers();
    SetupDescriptors();
    PreparePipelines();
    BuildCommandBuffers();

    mbPrepared = true;
}

void TestRenderer::Render()
{
    RenderFrame();
    // std::cout << "Camera Position: " << mCamera.mPosition[0] << ", " << mCamera.mPosition[1] << ", " << mCamera.mPosition[2] << std::endl;
    if (mCamera.mbUpdated) UpdateUniformBuffers();
}

void TestRenderer::ViewChanged()
{
    UpdateUniformBuffers();
}

void TestRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    if (overlay->Header("Settings")) 
    {
        if (overlay->InputFloat("Exposure", &mUBOParams.mExposure, 0.1f, 2)) 
        {
            UpdateParams();
        }
        if (overlay->InputFloat("Gamma", &mUBOParams.mGamma, 0.1f, 2)) 
        {
            UpdateParams();
        }
    }
}

TestRenderer * testRenderer;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (testRenderer != nullptr)
    {
        testRenderer->HandleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    for (int32_t i = 0; i < __argc; i++) { TestRenderer::mArgs.push_back(__argv[i]); };
    testRenderer = new TestRenderer();
    testRenderer->InitVulkan();
    testRenderer->SetupWindow(hInstance, WndProc);
    testRenderer->Prepare();
    testRenderer->RenderLoop();
    delete(testRenderer);
    return 0;
}