#include "VulkanRenderer.hpp"

VulkanRenderer::VulkanRenderer() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "Test Render";
    mCamera.mType = CameraType::LookAt;
    // mCamera.mbFlipY = true;
    mCamera.SetPosition(glm::vec3(0.0f, 0.0f, -1.0f));
    mCamera.SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.001f, 256.0f);
    mCamera.SetMovementSpeed(0.5f);
    mCamera.SetRotationSpeed(0.3f);
}

VulkanRenderer::~VulkanRenderer()
{
    if (mDevice)
    {
        for (auto& pipeline : mPipelines)
        {
            vkDestroyPipeline(mDevice, pipeline.second, nullptr);
        }
        
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);

        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mUniformDescSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mTextureDescSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mNodeDescSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, mDescSetLayout.mMaterialBufferDescSetLayout, nullptr);

        mUniformBuffers.mObjectUBO.Destroy();
        mUniformBuffers.mParamsUBO.Destroy();
        mUniformBuffers.mSkyboxUBO.Destroy();
        mUniformBuffers.mMaterialParamsBuffer.Destroy();

        mTextures.mLUTBRDF.Destroy();
        mTextures.mEnvCube.Destroy();
        mTextures.mIrradianceCube.Destroy();
        mTextures.mPreFilteredCube.Destroy();

        mScenes.mRenderScene.Destroy(mDevice);
        mScenes.mSkybox.Destroy(mDevice);
    }
}

void VulkanRenderer::GetEnabledFeatures()
{
    mEnabledFeatures.samplerAnisotropy = mDeviceFeatures.samplerAnisotropy;
}

void VulkanRenderer::SetupDescriptors()
{
    uint32_t imageSamplerCount = 3;
    uint32_t materialCount = 0;
    uint32_t meshCount = 0;
    uint32_t uniformCount = 2;

    std::vector<LeoVK::GLTFScene*> modelList = {&mScenes.mSkybox, &mScenes.mRenderScene};
    for (auto & model : modelList)
    {
        for (auto& mat : model->mMaterials)
        {
            imageSamplerCount += 5;
            materialCount++;
        }
        for (auto& node : model->mLinearNodes)
        {
            if (node->mpMesh) meshCount++;
        }
    }
    
    std::vector<VkDescriptorPoolSize> poolSize = {
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 + meshCount),
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount),
        LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)
    };
    const uint32_t maxSetCount = materialCount + meshCount + 2;
    VkDescriptorPoolCreateInfo descSetPoolCI = LeoVK::Init::DescPoolCreateInfo(poolSize, maxSetCount);
    VK_CHECK(vkCreateDescriptorPool(mDevice, &descSetPoolCI, nullptr, &mDescPool));

    // Scene
    {
        // UniformBuffer的DescSetLayout
        std::vector<VkDescriptorSetLayoutBinding> uniformSetLayoutBindings = {
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
        };
        VkDescriptorSetLayoutCreateInfo uniformDescSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(uniformSetLayoutBindings);
        VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &uniformDescSetLayoutCI, nullptr, &mDescSetLayout.mUniformDescSetLayout));

        // 分配Uniform的DescSet
        VkDescriptorSetAllocateInfo descSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mUniformDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &descSetAI, &mDescSets.mObjectDescSet));
        std::vector<VkWriteDescriptorSet> objWriteDescSet = {
            LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mUniformBuffers.mObjectUBO.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &mUniformBuffers.mParamsUBO.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &mTextures.mLUTBRDF.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &mTextures.mIrradianceCube.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mObjectDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &mTextures.mPreFilteredCube.mDescriptor)
        };
        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(objWriteDescSet.size()), objWriteDescSet.data(), 0, nullptr);
    }

    // Materials
    {
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

        // 分配Texture的DescSet
        for (auto& mat : mScenes.mRenderScene.mMaterials)
        {
            const VkDescriptorSetAllocateInfo texDescAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mTextureDescSetLayout, 1);
            VK_CHECK(vkAllocateDescriptorSets(mDevice, &texDescAI, &mat.mDescriptorSet));
            std::vector<VkDescriptorImageInfo> imageDescs = {
                mScenes.mRenderScene.mTextures.back().mDescriptor,
                mScenes.mRenderScene.mTextures.back().mDescriptor,
                mat.mpNormalTexture ? mat.mpNormalTexture->mDescriptor : mScenes.mRenderScene.mTextures.back().mDescriptor,
                mat.mpOcclusionTexture ? mat.mpOcclusionTexture->mDescriptor : mScenes.mRenderScene.mTextures.back().mDescriptor,
                mat.mpEmissiveTexture ? mat.mpEmissiveTexture->mDescriptor : mScenes.mRenderScene.mTextures.back().mDescriptor
            };
            if (mat.mPBRWorkFlows.mbMetallicRoughness)
            {
                if (mat.mpBaseColorTexture) imageDescs[0] = mat.mpBaseColorTexture->mDescriptor;
                if (mat.mpMetallicRoughnessTexture) imageDescs[1] = mat.mpMetallicRoughnessTexture->mDescriptor;
            }
            if (mat.mPBRWorkFlows.mbSpecularGlossiness)
            {
                if (mat.mExtension.mpDiffuseTexture) imageDescs[0] = mat.mExtension.mpDiffuseTexture->mDescriptor;
                if (mat.mExtension.mpSpecularGlossinessTexture) imageDescs[1] = mat.mExtension.mpSpecularGlossinessTexture->mDescriptor;
            }
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

    // Node Desc Set
    {
        std::vector<VkDescriptorSetLayoutBinding> nodeDescSetLayoutBinding = {
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
        };
        VkDescriptorSetLayoutCreateInfo nodeDescSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(nodeDescSetLayoutBinding);
        VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &nodeDescSetLayoutCI, nullptr, &mDescSetLayout.mNodeDescSetLayout));
        for (auto & node : mScenes.mRenderScene.mNodes)
        {
            SetupNodeDescriptors(node);
        }
    }

    // Material Buffer Descriptor
    {
        std::vector<VkDescriptorSetLayoutBinding> matDescSetLayoutBinding = {
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        };
        VkDescriptorSetLayoutCreateInfo nodeDescSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(matDescSetLayoutBinding);
        VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &nodeDescSetLayoutCI, nullptr, &mDescSetLayout.mMaterialBufferDescSetLayout))

        VkDescriptorSetAllocateInfo matDescSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mMaterialBufferDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &matDescSetAI, &mDescSets.mMaterialParamsDescSet))

        VkWriteDescriptorSet matWriteDescSet = LeoVK::Init::WriteDescriptorSet(mDescSets.mMaterialParamsDescSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &mUniformBuffers.mMaterialParamsBuffer.mDescriptor);
        vkUpdateDescriptorSets(mDevice, 1, &matWriteDescSet, 0, nullptr);
    }

    // Skybox
    {
        VkDescriptorSetAllocateInfo skyboxDescSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mUniformDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &skyboxDescSetAI, &mDescSets.mSkyboxDescSet))

        std::vector<VkWriteDescriptorSet> skyboxWriteDescSet = {
            LeoVK::Init::WriteDescriptorSet(mDescSets.mSkyboxDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &mUniformBuffers.mSkyboxUBO.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mSkyboxDescSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &mUniformBuffers.mParamsUBO.mDescriptor),
            LeoVK::Init::WriteDescriptorSet(mDescSets.mSkyboxDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &mTextures.mPreFilteredCube.mDescriptor)
        };
        vkUpdateDescriptorSets(mDevice, static_cast<uint32_t>(skyboxWriteDescSet.size()), skyboxWriteDescSet.data(), 0, nullptr);
    }
}

void VulkanRenderer::SetupNodeDescriptors(LeoVK::Node* node)
{
    if (node->mpMesh)
    {
        VkDescriptorSetAllocateInfo descSetAI = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout.mNodeDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &descSetAI, &node->mpMesh->mUniformBuffer.mDescriptorSet))

        VkWriteDescriptorSet writeDescSet = LeoVK::Init::WriteDescriptorSet(node->mpMesh->mUniformBuffer.mDescriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &node->mpMesh->mUniformBuffer.mDescriptor);
        vkUpdateDescriptorSets(mDevice, 1, &writeDescSet, 0, nullptr);
    }
    for (auto & child : node->mChildren)
    {
        SetupNodeDescriptors(child);
    }
}

void VulkanRenderer::AddPipelineSet(const std::string prefix, const std::string vertexShader, const std::string pixelShader)
{
    VkPipelineInputAssemblyStateCreateInfo iaStateCI = LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsStateCI = LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cbAttachCI = LeoVK::Init::PipelineCBAState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbStateCI = LeoVK::Init::PipelineCBStateCreateInfo(1, &cbAttachCI);
    VkPipelineDepthStencilStateCreateInfo dsStateCI = LeoVK::Init::PipelineDSStateCreateInfo(prefix == "Skybox" ? VK_FALSE : VK_TRUE, prefix == "Skybox" ? VK_FALSE : VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
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
        LeoVK::Init::VIAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(LeoVK::Vertex, mUV0)),
        LeoVK::Init::VIAttributeDescription(0, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(LeoVK::Vertex, mUV1)),
        LeoVK::Init::VIAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoVK::Vertex, mColor)),
        LeoVK::Init::VIAttributeDescription(0, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoVK::Vertex, mJoint0)),
        LeoVK::Init::VIAttributeDescription(0, 6, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoVK::Vertex, mWeight0)),
        LeoVK::Init::VIAttributeDescription(0, 7, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoVK::Vertex, mTangent)),
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

    ssStateCIs[0] = LoadShader(GetShadersPath() + vertexShader, VK_SHADER_STAGE_VERTEX_BIT);
    ssStateCIs[1] = LoadShader(GetShadersPath() + pixelShader, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipeline pipeline;
    // Default Pipeline
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline))
    mPipelines[prefix] = pipeline;
    // Double Sided
    rsStateCI.cullMode = VK_CULL_MODE_NONE;
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline))
    mPipelines[prefix + "_Double_Sided"] = pipeline;
    // Alpha Blending
    rsStateCI.cullMode = VK_CULL_MODE_NONE;
    cbAttachCI.blendEnable = VK_TRUE;
    cbAttachCI.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAttachCI.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbAttachCI.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachCI.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttachCI.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachCI.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachCI.alphaBlendOp = VK_BLEND_OP_ADD;
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline))
    mPipelines[prefix + "_Alpha_Blend"] = pipeline;
}

void VulkanRenderer::PreparePipelines()
{
    // 确定pipelineLayout
    std::array<VkDescriptorSetLayout, 4> descSetLayouts = { mDescSetLayout.mUniformDescSetLayout, mDescSetLayout.mTextureDescSetLayout, mDescSetLayout.mNodeDescSetLayout, mDescSetLayout.mMaterialBufferDescSetLayout };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo(descSetLayouts.data(), static_cast<uint32_t>(descSetLayouts.size()));
    VkPushConstantRange pushConstRange{};
    pushConstRange.size = sizeof(uint32_t);
    pushConstRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstRange;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &mPipelineLayout));

    AddPipelineSet("Skybox", "Base/Skybox.vert.spv", "Base/Skybox.frag.spv");
    AddPipelineSet("PBR", "VulkanRenderer/PBRShader.vert.spv", "VulkanRenderer/PBRShader.frag.spv");
    AddPipelineSet("Unlit", "VulkanRenderer/PBRShader.vert.spv", "VulkanRenderer/PBRUnlitShader.frag.spv");
}

void VulkanRenderer::PrepareUniformBuffers()
{
    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniformBuffers.mObjectUBO,
        sizeof(mSceneUBOMatrices)
    ));
    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniformBuffers.mSkyboxUBO,
        sizeof(mSkyboxUBOMatrices)
    ));
    VK_CHECK(mpVulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mUniformBuffers.mParamsUBO,
        sizeof(mUBOParams)
    ));

    VK_CHECK(mUniformBuffers.mObjectUBO.Map())
    VK_CHECK(mUniformBuffers.mSkyboxUBO.Map())
    VK_CHECK(mUniformBuffers.mParamsUBO.Map())
    UpdateUniformBuffers();
    UpdateParams();
}

void VulkanRenderer::UpdateUniformBuffers()
{
    mSceneUBOMatrices.mProj = mCamera.mMatrices.mPerspective;
    mSceneUBOMatrices.mView = mCamera.mMatrices.mView;

    float a = mScenes.mRenderScene.mAABB[0][0];
    float b = mScenes.mRenderScene.mAABB[1][1];
    float c = mScenes.mRenderScene.mAABB[2][2];
    float scale = (1.0 / (std::max(a, std::max(b, c)))) * 0.5f;
    glm::vec3 translate = -glm::vec3(mScenes.mRenderScene.mAABB[3][0], mScenes.mRenderScene.mAABB[3][1], mScenes.mRenderScene.mAABB[3][2]);
    translate += -0.5f * glm::vec3(mScenes.mRenderScene.mAABB[0][0], mScenes.mRenderScene.mAABB[1][1], mScenes.mRenderScene.mAABB[2][2]);
    mSceneUBOMatrices.mModel = glm::mat4(1.0f);
    mSceneUBOMatrices.mModel[0][0] = scale;
    mSceneUBOMatrices.mModel[1][1] = scale;
    mSceneUBOMatrices.mModel[2][2] = scale;
    mSceneUBOMatrices.mModel = glm::translate(mSceneUBOMatrices.mModel, translate);

    mSceneUBOMatrices.mCamPos = glm::vec3(
        -mCamera.mPosition.z * sin(glm::radians(mCamera.mRotation.y)) * cos(glm::radians(mCamera.mRotation.x)),
        -mCamera.mPosition.z * sin(glm::radians(mCamera.mRotation.x)),
         mCamera.mPosition.z * cos(glm::radians(mCamera.mRotation.y)) * cos(glm::radians(mCamera.mRotation.x))
    );

    memcpy(mUniformBuffers.mObjectUBO.mpMapped, &mSceneUBOMatrices, sizeof(mSceneUBOMatrices));

    mSkyboxUBOMatrices.mProj = mCamera.mMatrices.mPerspective;
    mSkyboxUBOMatrices.mView = mCamera.mMatrices.mView;
    mSkyboxUBOMatrices.mModel = glm::mat4(glm::mat3(mCamera.mMatrices.mView));

    memcpy(mUniformBuffers.mSkyboxUBO.mpMapped, &mSkyboxUBOMatrices, sizeof(mSkyboxUBOMatrices));
}

void VulkanRenderer::UpdateParams()
{
    struct LightSource 
    {
        glm::vec3 color = glm::vec3(1.0f);
        glm::vec3 rotation = glm::vec3(75.0f, 40.0f, 0.0f);
    } lightSource;

    mUBOParams.mLight = glm::vec4(
        sin(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
        sin(glm::radians(lightSource.rotation.y)),
        cos(glm::radians(lightSource.rotation.x)) * cos(glm::radians(lightSource.rotation.y)),
        0.0f);
    memcpy(mUniformBuffers.mParamsUBO.mpMapped, &mUBOParams, sizeof(mUBOParams));
}

void VulkanRenderer::LoadScene(std::string filename)
{
    std::cout << "Loading scen from: " << filename << std::endl;
    mScenes.mRenderScene.Destroy(mDevice);
    mAnimIndex = 0;
    mAnimTimer = 0.0f;
    auto tStart = std::chrono::high_resolution_clock::now();
    mScenes.mRenderScene.LoadFromFile(filename, mpVulkanDevice, mQueue);
    mScenes.mRenderScene.LoadMaterialBuffer(mUniformBuffers.mMaterialParamsBuffer, mQueue);
    auto tFileLoad = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
    std::cout << "Loading took " << tFileLoad << " ms" << std::endl;
    mCamera.SetPosition(glm::vec3(0.0f, 0.0f, -0.5f));
    mCamera.SetRotation({ 0.0f, 0.0f, 0.0f });
}

void VulkanRenderer::LoadEnvironment(std::string filename)
{
    std::cout << "Loading environment from " << filename << std::endl;
    if (mTextures.mEnvCube.mImage) 
    {
        mTextures.mEnvCube.Destroy();
        mTextures.mIrradianceCube.Destroy();
        mTextures.mPreFilteredCube.Destroy();
    }
    mTextures.mEnvCube.LoadFromFile(filename, VK_FORMAT_R16G16B16A16_SFLOAT, mpVulkanDevice, mQueue);
    GenerateCubeMaps();
}

void VulkanRenderer::LoadAssets()
{
    LoadScene(GetAssetsPath() + "Models/BusterDrone/busterDrone.gltf");
    // LoadScene(GetAssetsPath() + "Models/DamagedHelmet/glTF/DamagedHelmet.gltf");
    // LoadScene(GetAssetsPath() + "Models/FlightHelmet/glTF/FlightHelmet.gltf");
    // LoadScene(GetAssetsPath() + "Models/Sponza/glTF/Sponza.gltf");
    // LoadScene(GetAssetsPath() + "Models/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf");
    // LoadScene(GetAssetsPath() + "Models/SciFiHelmet/glTF/SciFiHelmet.gltf");
    // LoadScene(GetAssetsPath() + "Models/TransmissionTest/glTF/TransmissionTest.gltf");
    // LoadScene(GetAssetsPath() + "Models/KnightArtorias/scene.gltf");
    // LoadScene(GetAssetsPath() + "Models/MechDrone/scene.gltf");
    // LoadScene(GetAssetsPath() + "Models/CyberSamurai/scene.gltf");

    mScenes.mSkybox.LoadFromFile(GetAssetsPath() + "Models/Box/glTF-Embedded/Box.gltf", mpVulkanDevice, mQueue);
    LeoVK::VKTools::ReadDirectory(GetAssetsPath() + "Environments", "*.ktx", mEnvMaps, false);
    LoadEnvironment(GetAssetsPath() + "Environments/pisa.ktx");
}

void VulkanRenderer::DrawNode(LeoVK::Node* node, uint32_t cbIndex , LeoVK::Material::AlphaMode alphaMode)
{
    if (node->mpMesh)
    {
        for (LeoVK::Primitive* primitive : node->mpMesh->mPrimitives)
        {
            if (primitive->mMaterial.mAlphaMode == alphaMode)
            {
                std::string pipelineName = "PBR";
                std::string pipelineVariant = "";

                if (primitive->mMaterial.mbUnlit)
                {
                    pipelineName = "Unlit";
                }
                if (alphaMode == LeoVK::Material::ALPHA_MODE_BLEND)
                {
                    pipelineVariant = "_Alpha_Blend";
                }
                else
                {
                    if (primitive->mMaterial.mbDoubleSided)
                    {
                        pipelineVariant = "_Double_Sided";
                    }
                }
                const VkPipeline pipeline = mPipelines[pipelineName + pipelineVariant];
                if (pipeline != mBoundPipeline)
                {
                    vkCmdBindPipeline(mDrawCmdBuffers[cbIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                    mBoundPipeline = pipeline;
                }

                const std::vector<VkDescriptorSet> descSets = {
                    mDescSets.mObjectDescSet,
                    primitive->mMaterial.mDescriptorSet,
                    node->mpMesh->mUniformBuffer.mDescriptorSet,
                    mDescSets.mMaterialParamsDescSet
                };
                vkCmdBindDescriptorSets(mDrawCmdBuffers[cbIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, static_cast<uint32_t>(descSets.size()), descSets.data(), 0, nullptr);
                vkCmdPushConstants(mDrawCmdBuffers[cbIndex], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &primitive->mMaterial.mIndex);
                if (primitive->mbHasIndices)
                {
                    vkCmdDrawIndexed(mDrawCmdBuffers[cbIndex], primitive->mIndexCount, 1, primitive->mFirstIndex, 0, 0);
                }
                else
                {
                    vkCmdDraw(mDrawCmdBuffers[cbIndex], primitive->mVertexCount, 1, 0, 0);
                }
            }
        }
    }
    for (auto child : node->mChildren)
    {
        DrawNode(child, cbIndex, alphaMode);
    }
}

void VulkanRenderer::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBI = LeoVK::Init::CmdBufferBeginInfo();

    VkClearValue clearValues[3];
    if (mSettings.multiSampling)
    {
        clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[2].depthStencil = { 1.0f, 0 };
    }
    else
    {
        clearValues[0].color = { { 0.0f, 0.1f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
    }

    VkRenderPassBeginInfo rpBI = LeoVK::Init::RenderPassBeginInfo();
    rpBI.renderPass = mRenderPass;
    rpBI.renderArea.offset = {0, 0};
    rpBI.renderArea.extent = {mWidth, mHeight};
    rpBI.clearValueCount = mSettings.multiSampling ? 3 : 2;
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

        VkDeviceSize offsets[1] = {0};
        if (mbShowBackground) 
        {
            vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSets.mSkyboxDescSet, 0, nullptr);
            vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelines["Skybox"]);
            mScenes.mSkybox.Draw(mDrawCmdBuffers[i], mPipelineLayout);
        }

        vkCmdBindVertexBuffers(mDrawCmdBuffers[i], 0, 1, &mScenes.mRenderScene.mVertices.mBuffer, offsets);
        vkCmdBindIndexBuffer(mDrawCmdBuffers[i], mScenes.mRenderScene.mIndices.mBuffer, 0, VK_INDEX_TYPE_UINT32);

        mBoundPipeline = VK_NULL_HANDLE;
        for (auto& node : mScenes.mRenderScene.mNodes)
        {
            DrawNode(node, i, LeoVK::Material::ALPHA_MODE_OPAQUE);
        }
        for (auto& node : mScenes.mRenderScene.mNodes)
        {
            DrawNode(node, i, LeoVK::Material::ALPHA_MODE_MASK);
        }
        for (auto& node : mScenes.mRenderScene.mNodes)
        {
            DrawNode(node, i, LeoVK::Material::ALPHA_MODE_BLEND);
        }

        DrawUI(mDrawCmdBuffers[i]);

        vkCmdEndRenderPass(mDrawCmdBuffers[i]);
        VK_CHECK(vkEndCommandBuffer(mDrawCmdBuffers[i]))
    }
}

void VulkanRenderer::Prepare()
{
    VKRendererBase::Prepare();
    LoadAssets();
    GenerateBRDFLUT();
    
    PrepareUniformBuffers();
    SetupDescriptors();
    PreparePipelines();
    BuildCommandBuffers();

    mbPrepared = true;
}

void VulkanRenderer::Render()
{
    RenderFrame();
    if (mCamera.mbUpdated) UpdateUniformBuffers();
    if (mbAnimate && !mScenes.mRenderScene.mAnimations.empty())
    {
        mAnimTimer += mFrameTimer * mAnimateSpeed;
        if (mAnimTimer > mScenes.mRenderScene.mAnimations[mAnimIndex].mEnd)
        {
            mAnimTimer -= mScenes.mRenderScene.mAnimations[mAnimIndex].mEnd;
        }
        mScenes.mRenderScene.UpdateAnimation(mAnimIndex, mAnimTimer);
    }
}

void VulkanRenderer::ViewChanged()
{
    UpdateUniformBuffers();
}

void VulkanRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    bool bUpdateShaderParams = false;
    bool bUpdateCBs = false;
    if (overlay->Header("Scene"))
    {
        if (overlay->Button("Open GLTF File"))
        {
            std::string filename = "";
            char buffer[256];
            OPENFILENAMEA ofn;
            ZeroMemory(&buffer, sizeof(buffer));
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "glTF files\0*.gltf;*.glb\0";
            ofn.lpstrFile = buffer;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Select a glTF file to load";
            ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn)) filename = buffer;
            if (!filename.empty())
            {
                vkDeviceWaitIdle(mDevice);
                LoadScene(filename);
                SetupDescriptors();
                bUpdateCBs = true;
            }
        }

        if (overlay->ComboBox("Environment", mSelectEnvMap, mEnvMaps))
        {
            vkDeviceWaitIdle(mDevice);
            LoadEnvironment(mEnvMaps[mSelectEnvMap]);
            SetupDescriptors();
            bUpdateCBs = true;
        }
    }

    if (overlay->Header("Environment"))
    {
        if (overlay->ColorPicker("Light Color", glm::value_ptr(mUBOParams.mLightColor)))
        {
            bUpdateShaderParams = true;
        }
        if (overlay->SliderFloat("DirectLight Intensity", &mUBOParams.mLightIntensity, 0.1f, 10.0f))
        {
            bUpdateShaderParams = true;
        }
        if (overlay->CheckBox("Background", &mbShowBackground))
        {
            bUpdateShaderParams = true;
        }
        if (overlay->SliderFloat("Exposure", &mUBOParams.mExposure, 0.1f, 10.0f))
        {
            bUpdateShaderParams = true;
        }
        if (overlay->SliderFloat("Gamma", &mUBOParams.mGamma, 0.1f, 4.0f))
        {
            bUpdateShaderParams = true;
        }
        if (overlay->SliderFloat("IBL", &mUBOParams.mScaleIBLAmbient, 0.0f, 2.0f))
        {
            bUpdateShaderParams = true;
        }
    }

    if (overlay->Header("Settings")) 
    {
        if (!mScenes.mRenderScene.mAnimations.empty())
        {
            if (overlay->Header("Animations"))
            {
                overlay->CheckBox("Animate", &mbAnimate);
                overlay->SliderFloat("Animation Speed", &mAnimateSpeed, 0.00001, 10);
            }
        }
        const std::vector<std::string> camType = {"LookAt", "FirstPerson"};
        if (overlay->Header("Camera Settings"))
        {
            if (overlay->ComboBox("CameraType", &mCamTypeIndex, camType))
            {
                mCamera.mType = static_cast<CameraType>(mCamTypeIndex);
            }
        }
    }

    if (bUpdateShaderParams) UpdateParams();
    if (bUpdateCBs) 
    {
        vkDeviceWaitIdle(mDevice);
        BuildCommandBuffers();
        vkDeviceWaitIdle(mDevice);
    }
}

VulkanRenderer * testRenderer;
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
    for (int32_t i = 0; i < __argc; i++) { VulkanRenderer::mArgs.push_back(__argv[i]); };
    testRenderer = new VulkanRenderer();
    testRenderer->InitVulkan();
    testRenderer->SetupWindow(hInstance, WndProc);
    testRenderer->Prepare();
    testRenderer->RenderLoop();
    delete(testRenderer);
    return 0;
}