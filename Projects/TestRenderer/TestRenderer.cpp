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

        DrawUI(currentCmdBuffer);

        vkCmdEndRenderPass(currentCmdBuffer);

        VK_CHECK(vkEndCommandBuffer(currentCmdBuffer))
    }
}

void TestRenderer::Prepare()
{
    VKRendererBase::Prepare();
    LoadAssets();
    GenerateLUT();
//    GenerateCubeMap();
//    PrepareUniformBuffers();
//    SetupDescriptors();
//    PreparePipelines();
//
//    BuildCommandBuffers();

    mbPrepared = true;
}

void TestRenderer::Render()
{
    if (!mbPrepared) return;
    mSubmitInfo.commandBufferCount = 1;
    mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];
    VK_CHECK(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE))

    VKRendererBase::SubmitFrame();
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
    PushConstBlockMaterial pushConstBlockMaterial{};
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

                pushConstBlockMaterial.mEmissiveFactor = primitive->mMaterial.mEmissiveFactor;
                // To save push constant space, availabilty and texture coordiante set are combined
                // -1 = texture not used for this material, >= 0 texture used and index of texture coordinate set
                pushConstBlockMaterial.mColorTextureSet = primitive->mMaterial.mpBaseColorTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mBaseColor : -1;
                pushConstBlockMaterial.mNormalTextureSet = primitive->mMaterial.mpNormalTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mNormal : -1;
                pushConstBlockMaterial.mOcclusionTextureSet = primitive->mMaterial.mpOcclusionTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mOcclusion : -1;
                pushConstBlockMaterial.mEmissiveTextureSet = primitive->mMaterial.mpEmissiveTexture != nullptr ? primitive->mMaterial.mTexCoordSets.mEmissive : -1;
                pushConstBlockMaterial.mAlphaMask = static_cast<float>(primitive->mMaterial.mAlphaMode == LeoVK::Material::ALPHA_MODE_MASK);
                pushConstBlockMaterial.mAlphaMaskCutoff = primitive->mMaterial.mAlphaCutoff;

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

            }
        }
    }
    vkCmdPushConstants(mDrawCmdBuffers[cbIdx], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstBlockMaterial), &pushConstBlockMaterial);
    mRenderScene.mScene.Draw(mDrawCmdBuffers[cbIdx], alphaMode);
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
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
    const int32_t dim = 512;

    // Image
    VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &mSceneTextures.mLUTBRDF.mImage));
    VkMemoryAllocateInfo memAlloc = LeoVK::Init::MemoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mDevice, mSceneTextures.mLUTBRDF.mImage, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(mDevice, &memAlloc, nullptr, &mSceneTextures.mLUTBRDF.mDeviceMemory));
    VK_CHECK(vkBindImageMemory(mDevice, mSceneTextures.mLUTBRDF.mImage, mSceneTextures.mLUTBRDF.mDeviceMemory, 0));
    // Image view
    VkImageViewCreateInfo viewCI = LeoVK::Init::ImageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;
    viewCI.image = mSceneTextures.mLUTBRDF.mImage;
    VK_CHECK(vkCreateImageView(mDevice, &viewCI, nullptr, &mSceneTextures.mLUTBRDF.mView));
    // Sampler
    VkSamplerCreateInfo samplerCI = LeoVK::Init::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(mDevice, &samplerCI, nullptr, &mSceneTextures.mLUTBRDF.mSampler));

    mSceneTextures.mLUTBRDF.mDescriptor.imageView = mSceneTextures.mLUTBRDF.mView;
    mSceneTextures.mLUTBRDF.mDescriptor.sampler = mSceneTextures.mLUTBRDF.mSampler;
    mSceneTextures.mLUTBRDF.mDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    mSceneTextures.mLUTBRDF.mpDevice = mpVulkanDevice;

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = LeoVK::Init::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VkRenderPass renderpass;
    VK_CHECK(vkCreateRenderPass(mDevice, &renderPassCI, nullptr, &renderpass));

    VkFramebufferCreateInfo framebufferCI = LeoVK::Init::FrameBufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &mSceneTextures.mLUTBRDF.mView;
    framebufferCI.width = dim;
    framebufferCI.height = dim;
    framebufferCI.layers = 1;

    VkFramebuffer framebuffer;
    VK_CHECK(vkCreateFramebuffer(mDevice, &framebufferCI, nullptr, &framebuffer));

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = { LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
    VkDescriptorPoolCreateInfo descriptorPoolCI = LeoVK::Init::DescPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    VK_CHECK(vkCreateDescriptorPool(mDevice, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = LeoVK::Init::DescSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
    VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocInfo, &descriptorset));

    // Pipeline layout
    VkPipelineLayout pipelinelayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = LeoVK::Init::PipelineCBAState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = LeoVK::Init::PipelineCBStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = LeoVK::Init::PipelineDSStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = LeoVK::Init::PipelineVPStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = LeoVK::Init::PipelineMSStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = LeoVK::Init::PipelineDYStateCreateInfo(dynamicStateEnables);
    VkPipelineVertexInputStateCreateInfo emptyInputState = LeoVK::Init::PipelineVIStateCreateInfo();
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

    VkGraphicsPipelineCreateInfo pipelineCI = LeoVK::Init::PipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Look-up-table (from BRDF) pipeline
    shaderStages[0] = LoadShader(GetShadersPath() + "TestRenderer/GenBRDFLUT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(GetShadersPath() + "TestRenderer/GenBRDFLUT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render
    VkClearValue clearValues[1];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkRenderPassBeginInfo renderPassBeginInfo = LeoVK::Init::RenderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = framebuffer;

    VkCommandBuffer cmdBuf = mpVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = LeoVK::Init::Viewport((float)dim, (float)dim, 0.0f, 1.0f);
    VkRect2D scissor = LeoVK::Init::Rect2D(dim, dim, 0, 0);
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuf);
    mpVulkanDevice->FlushCommandBuffer(cmdBuf, mQueue);

    vkQueueWaitIdle(mQueue);

    vkDestroyPipeline(mDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, pipelinelayout, nullptr);
    vkDestroyRenderPass(mDevice, renderpass, nullptr);
    vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, descriptorsetlayout, nullptr);
    vkDestroyDescriptorPool(mDevice, descriptorpool, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
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