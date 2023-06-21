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

void PBRRenderer::LoadEnvironment(std::string& filename)
{
    std::cout << "Loading environment from " << filename << std::endl;
    if (mTextures.mTexEnvCube.image) {
        mTextures.mTexEnvCube.destroy();
        mTextures.mTexIrradianceCube.destroy();
        mTextures.mTexPreFilterCube.destroy();
    }
    mTextures.mTexEnvCube.loadFromFile(filename, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
    GenerateCubeMaps();
}

void PBRRenderer::LoadAssets()
{
    std::string assetPath = getAssetPath();
    mTextures.mTexDummy.loadFromFile(assetPath + "Textures/empty.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

    std::string sceneFile = assetPath + "Models/BusterDrone/busterDrone.gltf";
    std::string envMapsFile = assetPath + "Textures/Env/papermill.ktx";
    for (auto & arg : args)
    {
        if ((std::string(arg).find(".gltf") != std::string::npos) ||
            (std::string(arg).find(".glb") != std::string::npos))
        {
            std::ifstream file(arg);
            if (file.good()) sceneFile = arg;
            else std::cout << "Could not load: " << arg << std::endl;
        }
        if (std::string(arg).find(".ktx") != std::string::npos)
        {
            std::ifstream file(arg);
            if (file.good()) envMapsFile = arg;
            else std::cout << "Could not laod: " << arg << std::endl;
        }
    }
    LoadScene(sceneFile);
    std::string skyBox = assetPath + "Models/Box/glTF-Embedded/Box.gltf";
    mModels.mModelSkybox.LoadFromFile(skyBox, vulkanDevice, queue);

    LoadEnvironment(envMapsFile);
}

void PBRRenderer::SetupNodeDescriptorSet(LeoRenderer::Node *node)
{
    if (node->mMesh)
    {
        VkDescriptorSetAllocateInfo descSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mDescSetLayouts.mDescLayoutNode, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descSetAI, &node->mMesh->mUniformBuffer.descriptorSet));

        VkWriteDescriptorSet writeDescSet{};
        writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescSet.descriptorCount = 1;
        writeDescSet.dstSet = node->mMesh->mUniformBuffer.descriptorSet;
        writeDescSet.dstBinding = 0;
        writeDescSet.pBufferInfo = &node->mMesh->mUniformBuffer.descriptor;
        vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);
    }
    for (auto & child : node->mChildren) SetupNodeDescriptorSet(child);
}

void PBRRenderer::SetDescriptors()
{
    /*
     * Descriptor Pool
     */
    uint32_t imageSamplerCount = 0;
    uint32_t materialCount = 0;
    uint32_t meshCount = 0;

    // Environment samplers (radiance, irradiance, brdf lut)
    imageSamplerCount += 3;

    std::vector<LeoRenderer::GLTFModel*> modelList = { &mModels.mModelSkybox, &mModels.mModelScene };
    for (auto &model : modelList)
    {
        for (auto &material : model->mMaterials)
        {
            imageSamplerCount += 5;
            materialCount++;
        }
        for (auto node : model->mLinearNodes)
        {
            if (node->mMesh) meshCount++;
        }
    }

    // 创建两个类型的Desc Pool，用于UBO和Sampler
    std::vector<VkDescriptorPoolSize> poolSize =
    {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (4 + meshCount) * swapChain.imageCount},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageSamplerCount * swapChain.imageCount}
    };
    VkDescriptorPoolCreateInfo descPoolCI = vks::initializers::descriptorPoolCreateInfo(
        poolSize,
        (2 + materialCount + meshCount) * swapChain.imageCount);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descriptorPool));

    /*
     * 分别为场景、材质、Node创建对应的描述符集
     */

    // 场景描述符集包括渲染场景时要用的贴图和Uniform值
    // 1. 场景的Uniform Buffer
    // 2. 场景渲染需要的三个图：辐照度图、预过滤图、BRDF图
    {
        std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings =
        {
            {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };
        VkDescriptorSetLayoutCreateInfo descSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(descSetLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &mDescSetLayouts.mDescLayoutScene));

        for (auto i = 0; i < mDescSets.size(); i++)
        {
            VkDescriptorSetAllocateInfo descSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mDescSetLayouts.mDescLayoutScene, 1);
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descSetAI, &mDescSets[i].mDescScene));

            std::array<VkWriteDescriptorSet, 5> writeDescSets{};

            writeDescSets[0] = vks::initializers::writeDescriptorSet(
                mDescSets[i].mDescScene,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                0,
                &mUniformBuffers[i].mUBOScene.descriptor,
                1);
            writeDescSets[1] = vks::initializers::writeDescriptorSet(
                mDescSets[i].mDescScene,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                1,
                &mUniformBuffers[i].mUBOParams.descriptor,
                1);
            writeDescSets[2] = vks::initializers::writeDescriptorSet(
                mDescSets[i].mDescScene,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                2,
                &mTextures.mTexIrradianceCube.descriptor,
                1);
            writeDescSets[3] = vks::initializers::writeDescriptorSet(
                mDescSets[i].mDescScene,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                3,
                &mTextures.mTexPreFilterCube.descriptor,
                1);
            writeDescSets[4] = vks::initializers::writeDescriptorSet(
                mDescSets[i].mDescScene,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                4,
                &mTextures.mTexLUTBRDF.descriptor,
                1);

            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
    }

    // 需要为物体的每个材质贴图创建描述符集
    {
        std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindings =
        {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        };
        VkDescriptorSetLayoutCreateInfo descSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(descSetLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &mDescSetLayouts.mDescLayoutMaterial));

        // Per-Material descriptor sets
        for (auto & mat : mModels.mModelScene.mMaterials)
        {
            VkDescriptorSetAllocateInfo descSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mDescSetLayouts.mDescLayoutMaterial, 1);
            VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descSetAI, &mat.mDescriptorSet));

            std::vector<VkDescriptorImageInfo> descImageInfo =
            {
                mTextures.mTexDummy.descriptor,
                mTextures.mTexDummy.descriptor,
                mat.mNormalTexture ? mat.mNormalTexture->mDescriptor : mTextures.mTexDummy.descriptor,
                mat.mOcclusionTexture ? mat.mOcclusionTexture->mDescriptor : mTextures.mTexDummy.descriptor,
                mat.mEmissiveTexture ? mat.mEmissiveTexture->mDescriptor : mTextures.mTexDummy.descriptor,
            };

            if (mat.mPBRWorkFlows.mbMetallicRoughness)
            {
                if (mat.mBaseColorTexture) descImageInfo[0] = mat.mBaseColorTexture->mDescriptor;
                if (mat.mMetallicRoughnessTexture) descImageInfo[1] = mat.mMetallicRoughnessTexture->mDescriptor;
            }
            if (mat.mPBRWorkFlows.mbSpecularGlossiness)
            {
                if (mat.mExtension.mDiffuseTexture) descImageInfo[0] = mat.mExtension.mDiffuseTexture->mDescriptor;
                if (mat.mExtension.mSpecularGlossinessTexture) descImageInfo[1] = mat.mExtension.mSpecularGlossinessTexture->mDescriptor;
            }

            std::array<VkWriteDescriptorSet, 5> writeDescSets{};

            for (size_t i = 0; i < descImageInfo.size(); i++)
            {
                writeDescSets[i] = vks::initializers::writeDescriptorSet(
                    mat.mDescriptorSet,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    static_cast<uint32_t>(i),
                    &descImageInfo[i],
                    1);
            }
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
        }
        // Model node (Matrice)
        {
            std::vector<VkDescriptorSetLayoutBinding> descSetLayoutBindingsNodes =
            {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr},
            };

            VkDescriptorSetLayoutCreateInfo descSetLayoutCINodes = vks::initializers::descriptorSetLayoutCreateInfo(descSetLayoutBindingsNodes);
            VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCINodes, nullptr, &mDescSetLayouts.mDescLayoutNode));

            for (auto & node : mModels.mModelScene.mNodes) SetupNodeDescriptorSet(node);
        }
    }

    // 渲染天空球要用的描述符集
    // 1. Uniform Buffer
    // 2. 预过滤的立方体图
    for (auto i = 0; i < mUniformBuffers.size(); i++)
    {
        VkDescriptorSetAllocateInfo descSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &mDescSetLayouts.mDescLayoutScene, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descSetAI, &mDescSets[i].mDescSkybox));

        std::array<VkWriteDescriptorSet, 3> writeDescSets{};

        writeDescSets[0] = vks::initializers::writeDescriptorSet(
            mDescSets[i].mDescSkybox,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            0,
            &mUniformBuffers[i].mUBOSkybox.descriptor,
            1);
        writeDescSets[1] = vks::initializers::writeDescriptorSet(
            mDescSets[i].mDescSkybox,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            1,
            &mUniformBuffers[i].mUBOParams.descriptor,
            1);
        writeDescSets[2] = vks::initializers::writeDescriptorSet(
            mDescSets[i].mDescSkybox,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            2,
            &mTextures.mTexPreFilterCube.descriptor,
            1);
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescSets.size()), writeDescSets.data(), 0, nullptr);
    }

}

void PBRRenderer::PreparePipelines()
{
    VkPipelineInputAssemblyStateCreateInfo iaStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cbAttachState = vks::initializers::pipelineColorBlendAttachmentState(
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(
        1, &cbAttachState);
    VkPipelineDepthStencilStateCreateInfo dsStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo vpStateCI = vks::initializers::pipelineViewportStateCreateInfo(
        1, 1, 0);
    VkPipelineMultisampleStateCreateInfo msStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(
        settings.multiSampling ?  settings.sampleCount : VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyStateCI = vks::initializers::pipelineDynamicStateCreateInfo(
        dynamicStates.data(), static_cast<uint32_t>(dynamicStates.size()), 0);

    const std::vector<VkDescriptorSetLayout> descSetLayouts =
    {
        mDescSetLayouts.mDescLayoutScene,
        mDescSetLayouts.mDescLayoutMaterial,
        mDescSetLayouts.mDescLayoutNode
    };
    VkPushConstantRange pushConstRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstantBlockMaterial), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(descSetLayouts.data());
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &mPipelineLayout));

    // Vertex Binding
    const std::vector<VkVertexInputBindingDescription> viBindings =
    {
        vks::initializers::vertexInputBindingDescription(
            0, sizeof(LeoRenderer::Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
    };

    const std::vector<VkVertexInputAttributeDescription> viAttributes =
    {
        vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoRenderer::Vertex, mPos)),
        vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoRenderer::Vertex, mNormal)),
        vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(LeoRenderer::Vertex, mUV0)),
        vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32_SFLOAT, offsetof(LeoRenderer::Vertex, mUV1)),
        vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mColor)),
        vks::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mJoint0)),
        vks::initializers::vertexInputAttributeDescription(0, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mWeight0)),
        vks::initializers::vertexInputAttributeDescription(0, 6, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mTangent)),
    };
    VkPipelineVertexInputStateCreateInfo viStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(viBindings, viAttributes);

    // Pipelines
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.layout = mPipelineLayout;
    pipelineCI.renderPass = renderPass;
    pipelineCI.pInputAssemblyState = &iaStateCI;
    pipelineCI.pVertexInputState = &viStateCI;
    pipelineCI.pRasterizationState = &rsStateCI;
    pipelineCI.pColorBlendState = &cbStateCI;
    pipelineCI.pMultisampleState = &msStateCI;
    pipelineCI.pViewportState = &vpStateCI;
    pipelineCI.pDepthStencilState = &dsStateCI;
    pipelineCI.pDynamicState = &dyStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    shaderStages = {
        LoadShader("skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader("skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPipelineSkyBox));
    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);

    shaderStages = {
        LoadShader("PBR.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader("PBR_KHR.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };
    dsStateCI.depthWriteEnable = VK_TRUE;
    dsStateCI.depthTestEnable = VK_TRUE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPipelinePBR))
    rsStateCI.cullMode = VK_CULL_MODE_NONE;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPipelineDoubleSided))

    rsStateCI.cullMode = VK_CULL_MODE_NONE;
    cbAttachState.blendEnable = VK_TRUE;
    cbAttachState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAttachState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbAttachState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachState.colorBlendOp = VK_BLEND_OP_ADD;
    cbAttachState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAttachState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbAttachState.alphaBlendOp = VK_BLEND_OP_ADD;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPipelinePBRAlphaBlend));

    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);
}

void PBRRenderer::GenerateBRDFLUT()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
    const int32_t dim = 512;

    // Image
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent = { dim, dim, 1};
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &mTextures.mTexLUTBRDF.image));
    // Image memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, mTextures.mTexLUTBRDF.image, &memReqs);
    VkMemoryAllocateInfo memoryAI = vks::initializers::memoryAllocateInfo();
    memoryAI.allocationSize = memReqs.size;
    memoryAI.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &mTextures.mTexLUTBRDF.deviceMemory))
    VK_CHECK_RESULT(vkBindImageMemory(device, mTextures.mTexLUTBRDF.image, mTextures.mTexLUTBRDF.deviceMemory, 0))

    // Image view
    VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = format;
    imageViewCI.subresourceRange = {};
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.image = mTextures.mTexLUTBRDF.image;
    VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &mTextures.mTexLUTBRDF.view));

    // Sampler
    VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &mTextures.mTexLUTBRDF.sampler))

    VkAttachmentDescription attachDesc{};
    // Color Attachment
    attachDesc.format = format;
    attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc{};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> subpassDependencies{};
    subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass = 0;
    subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    subpassDependencies[1].srcSubpass = 0;
    subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual Render Pass
    VkRenderPassCreateInfo rpCI = vks::initializers::renderPassCreateInfo();
    rpCI.attachmentCount = 1;
    rpCI.pAttachments = &attachDesc;
    rpCI.subpassCount = 1;
    rpCI.pSubpasses = &subpassDesc;
    rpCI.dependencyCount = 2;
    rpCI.pDependencies = subpassDependencies.data();
    VkRenderPass renderPass;
    VK_CHECK_RESULT(vkCreateRenderPass(device, &rpCI, nullptr, &renderPass))

    // FrameBuffer
    VkFramebufferCreateInfo frameBufferCI = vks::initializers::framebufferCreateInfo();
    frameBufferCI.renderPass = renderPass;
    frameBufferCI.attachmentCount = 1;
    frameBufferCI.pAttachments = &mTextures.mTexLUTBRDF.view;
    frameBufferCI.width = dim;
    frameBufferCI.height = dim;
    frameBufferCI.layers = 1;
    VkFramebuffer frameBuffer;
    VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &frameBuffer));

    // Descriptors
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSetLayoutCreateInfo descSetLayoutCI{};
    descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &descSetLayout))

    // Pipeline Layout
    VkPipelineLayout pipelineLayout{};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout))

    VkPipelineInputAssemblyStateCreateInfo iaStateCI{};
    iaStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rsStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE);

    VkPipelineColorBlendAttachmentState cbAttachState = vks::initializers::pipelineColorBlendAttachmentState(
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        VK_FALSE);

    VkPipelineColorBlendStateCreateInfo cbStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &cbAttachState);

    VkPipelineDepthStencilStateCreateInfo dsStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(
        VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo vpStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1);

    VkPipelineMultisampleStateCreateInfo msStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

    std::vector<VkDynamicState> dyStateEnable = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    VkPipelineDynamicStateCreateInfo dyStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dyStateEnable);

    VkPipelineVertexInputStateCreateInfo viStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
        LoadShader("GenerateBRDFLUT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader("GenerateBRDFLUT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkGraphicsPipelineCreateInfo gfxPipelineCI{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gfxPipelineCI.layout = pipelineLayout;
    gfxPipelineCI.renderPass = renderPass;
    gfxPipelineCI.pInputAssemblyState = &iaStateCI;
    gfxPipelineCI.pVertexInputState = &viStateCI;
    gfxPipelineCI.pRasterizationState = &rsStateCI;
    gfxPipelineCI.pColorBlendState = &cbStateCI;
    gfxPipelineCI.pMultisampleState = &msStateCI;
    gfxPipelineCI.pViewportState = &vpStateCI;
    gfxPipelineCI.pDepthStencilState = &dsStateCI;
    gfxPipelineCI.pDynamicState = &dyStateCI;
    gfxPipelineCI.stageCount = 2;
    gfxPipelineCI.pStages = shaderStages.data();

    VkPipeline pipeline;
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &gfxPipelineCI, nullptr, &pipeline))
    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);

    // Render LUT
    VkClearValue clearValue[1];
    clearValue[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkRenderPassBeginInfo rpBI = vks::initializers::renderPassBeginInfo();
    rpBI.renderPass = renderPass;
    rpBI.renderArea.extent = {dim, dim};
    rpBI.clearValueCount = 1;
    rpBI.pClearValues = clearValue;
    rpBI.framebuffer = frameBuffer;

    VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBeginRenderPass(cmdBuffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = float(dim);
    viewport.height = float(dim);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.extent = {dim, dim};

    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuffer);
    vulkanDevice->flushCommandBuffer(cmdBuffer, queue);
    // Sync
    vkQueueWaitIdle(queue);
    // Destroy
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);
    vkDestroyFramebuffer(device, frameBuffer, nullptr);
    vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);

    mTextures.mTexLUTBRDF.descriptor.imageView = mTextures.mTexLUTBRDF.view;
    mTextures.mTexLUTBRDF.descriptor.sampler = mTextures.mTexLUTBRDF.sampler;
    mTextures.mTexLUTBRDF.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    mTextures.mTexLUTBRDF.device = vulkanDevice;

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating BRDF LUT took: " << tDiff << " ms." << std::endl;
}

void PBRRenderer::GenerateCubeMaps()
{
    enum Target { IRRADIANCE = 0, PREFILTEREDENV = 1 };

    for (uint32_t target = 0; target < PREFILTEREDENV + 1; target++)
    {
        vks::TextureCubeMap cubeMap;

        auto tStart = std::chrono::high_resolution_clock::now();

        VkFormat format;
        uint32_t dim;

        switch (target)
        {
            case IRRADIANCE:
                format = VK_FORMAT_R32G32B32A32_SFLOAT;
                dim = 64;
                break;
            case PREFILTEREDENV:
                format = VK_FORMAT_R16G16B16A16_SFLOAT;
                dim = 512;
                break;
        }
        const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

        // Create Cube Map
        {
            // Image
            VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
            imageCI.imageType = VK_IMAGE_TYPE_2D;
            imageCI.format = format;
            imageCI.extent = {dim, dim, 1};
            imageCI.mipLevels = numMips;
            imageCI.arrayLayers = 6;
            imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &cubeMap.image));
            VkMemoryRequirements memReqs;
            vkGetImageMemoryRequirements(device, cubeMap.image, &memReqs);
            VkMemoryAllocateInfo memAI = vks::initializers::memoryAllocateInfo();
            memAI.allocationSize = memReqs.size;
            memAI.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memAI, nullptr, &cubeMap.deviceMemory))
            VK_CHECK_RESULT(vkBindImageMemory(device, cubeMap.image, cubeMap.deviceMemory, 0))

            // Image View
            VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
            imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            imageViewCI.format = format;
            imageViewCI.subresourceRange = {
                VK_IMAGE_ASPECT_COLOR_BIT, numMips, 6
            };
            imageViewCI.image = cubeMap.image;
            VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &cubeMap.view))

            // Sampler
            VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
            samplerCI.magFilter = VK_FILTER_LINEAR;
            samplerCI.minFilter = VK_FILTER_LINEAR;
            samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCI.minLod = 0.0f;
            samplerCI.maxLod = static_cast<float>(numMips);
            samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &cubeMap.sampler));
        }

        // FrameBuffer, Attachment, RenderPass, Pipelines
        VkAttachmentDescription attachDesc{};
        // Color Attachment
        attachDesc.format = format;
        attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpassDesc{};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorReference;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> subpassDependencies{};
        subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependencies[0].dstSubpass = 0;
        subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        subpassDependencies[1].srcSubpass = 0;
        subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        // Renderpass
        VkRenderPassCreateInfo renderPassCI{};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCI.attachmentCount = 1;
        renderPassCI.pAttachments = &attachDesc;
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpassDesc;
        renderPassCI.dependencyCount = 2;
        renderPassCI.pDependencies = subpassDependencies.data();
        VkRenderPass renderPass;
        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass))

        struct Offscreen
        {
            VkImage osImage;
            VkImageView osImageView;
            VkDeviceMemory osImageMemory;
            VkFramebuffer osFrameBuffer;
        } offscreen;


    }
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
