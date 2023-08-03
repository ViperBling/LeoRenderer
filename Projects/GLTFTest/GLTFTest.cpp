#include "GLTFTest.hpp"

GLTFTest::GLTFTest() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "GLTFTest";
    mCamera.mType = CameraType::LookAt;
    mCamera.mbFlipY = true;
    mCamera.SetPosition(glm::vec3(0.0f, -0.1f, -1.0f));
    mCamera.SetRotation(glm::vec3(0.0f, 45.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);
}

GLTFTest::~GLTFTest()
{
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescSetLayouts.mMatricesDesc, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, mDescSetLayouts.mTexturesDesc, nullptr);
    mUniforms.mBuffer.Destroy();
}

void GLTFTest::GetEnabledFeatures()
{
    mEnabledFeatures.samplerAnisotropy = mDeviceFeatures.samplerAnisotropy;
}

void GLTFTest::BuildCommandBuffers()
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

        vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSet, 0, nullptr);

        mRenderScene.Draw(mDrawCmdBuffers[i]);

        DrawUI(mDrawCmdBuffers[i]);

        vkCmdEndRenderPass(mDrawCmdBuffers[i]);
        VK_CHECK(vkEndCommandBuffer(mDrawCmdBuffers[i]))
    }
}

void GLTFTest::Prepare()
{
    VKRendererBase::Prepare();
    LoadAssets();
    SetupUniformBuffers();
    SetupDescriptors();
    SetupPipelines();
    BuildCommandBuffers();

    mbPrepared = true;
}

void GLTFTest::Render()
{
    RenderFrame();
    if (mCamera.mbUpdated) UpdateUniformBuffers();
}

void GLTFTest::ViewChanged()
{
    UpdateUniformBuffers();
}

void GLTFTest::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    if (overlay->Header("Settings"))
    {

    }
}

void GLTFTest::LoadAssets()
{
    mRenderScene.LoadFromFile(GetAssetsPath() + "Models/BusterDrone/busterDrone.gltf", mpVulkanDevice, mQueue);
}

void GLTFTest::SetupDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSize = {
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(mRenderScene.mMaterials.size()) * 2)
    };
    const uint32_t maxSetCount = static_cast<uint32_t>(mRenderScene.mTextures.size()) + 1;
    VkDescriptorPoolCreateInfo descPoolCI = LeoVK::Init::DescPoolCreateInfo(poolSize, maxSetCount);
    VK_CHECK(vkCreateDescriptorPool(mDevice, &descPoolCI, nullptr, &mDescPool))

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
    };
    VkDescriptorSetLayoutCreateInfo descSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descSetLayoutCI, nullptr, &mDescSetLayouts.mMatricesDesc))

    setLayoutBindings = {
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
    };
    descSetLayoutCI.pBindings = setLayoutBindings.data();
    descSetLayoutCI.bindingCount = 2;
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descSetLayoutCI, nullptr, &mDescSetLayouts.mTexturesDesc))

    std::array<VkDescriptorSetLayout, 2> setLayouts = {
        mDescSetLayouts.mMatricesDesc, mDescSetLayouts.mTexturesDesc
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    VkPushConstantRange pushConstRange = LeoVK::Init::PushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), 0);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstRange;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &mPipelineLayout))

    VkDescriptorSetAllocateInfo descSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayouts.mMatricesDesc, 1);
    VK_CHECK(vkAllocateDescriptorSets(mDevice, &descSetAI, &mDescSet))
    VkWriteDescriptorSet writeDescSet = LeoVK::Init::WriteDescriptorSet(mDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mUniforms.mBuffer.mDescriptor);
    vkUpdateDescriptorSets(mDevice, 1, &writeDescSet, 0, nullptr);

    for (auto & mat : mRenderScene.mMaterials)
    {
        descSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayouts.mTexturesDesc, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &descSetAI, &mat.mDescriptorSet))
        VkDescriptorImageInfo colorMap = mat.mpBaseColorTexture->mDescriptor;
        VkDescriptorImageInfo normalMap = mat.mpNormalTexture->mDescriptor;
        std::vector<VkWriteDescriptorSet> writeDescSets = {
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
            LeoVK::Init::WriteDescriptorSet(mat.mDescriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap)
        };
        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
    }
}

void GLTFTest::SetupPipelines()
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

    const std::vector<VkVertexInputBindingDescription> viBindings = {
        LeoVK::Init::VIBindingDescription(0, sizeof(LeoVK::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> viAttributes = {
        LeoVK::Init::VIAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mPos)),
        LeoVK::Init::VIAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mNormal)),
        LeoVK::Init::VIAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mUV0)),
        LeoVK::Init::VIAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mColor)),
        LeoVK::Init::VIAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mTangent)),
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

    ssStateCIs[0] = LoadShader(GetShadersPath() + "GLTFTest/Scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    ssStateCIs[1] = LoadShader(GetShadersPath() + "GLTFTest/Scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &mPipeline));
}

void GLTFTest::SetupUniformBuffers()
{
    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniforms.mBuffer,
        sizeof(mUniforms.mValues)))

    VK_CHECK(mUniforms.mBuffer.Map())
    UpdateUniformBuffers();
}

void GLTFTest::UpdateUniformBuffers()
{
    mUniforms.mValues.mProj = mCamera.mMatrices.mPerspective;
    mUniforms.mValues.mView = mCamera.mMatrices.mView;
    mUniforms.mValues.mViewPos = mCamera.mViewPos;
    memcpy(mUniforms.mBuffer.mpMapped, &mUniforms.mValues, sizeof(mUniforms.mValues));
}

GLTFTest * testRenderer;
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
    for (int32_t i = 0; i < __argc; i++) { GLTFTest::mArgs.push_back(__argv[i]); };
    testRenderer = new GLTFTest();
    testRenderer->InitVulkan();
    testRenderer->SetupWindow(hInstance, WndProc);
    testRenderer->Prepare();
    testRenderer->RenderLoop();
    delete(testRenderer);
    return 0;
}