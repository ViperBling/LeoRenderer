#include "PBRRenderer.h"

PBRRenderer::PBRRenderer() : VulkanFramework(ENABLE_MSAA, ENABLE_VALIDATION)
{
    title = "PBRRenderer";

    camera.type = Camera::CameraType::lookat;
    camera.setPerspective(45.0f, (float)width / (float)height, 0.1f, 256.0f);
    camera.rotationSpeed = 0.25f;
    camera.movementSpeed = 0.1f;
    camera.setPosition({ 0.0f, 0.0f, 1.0f });
    camera.setRotation({ 0.0f, 0.0f, 0.0f });
}

PBRRenderer::~PBRRenderer()
{
    vkDestroyPipeline(device, mPipelines.mPipelineSkyBox, nullptr);
    vkDestroyPipeline(device, mPipelines.mPipelinePBR, nullptr);
    vkDestroyPipeline(device, mPipelines.mPipelineDoubleSided, nullptr);
    vkDestroyPipeline(device, mPipelines.mPipelinePBRAlphaBlend, nullptr);

    vkDestroyPipelineLayout(device, mPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutScene, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutMaterial, nullptr);
    vkDestroyDescriptorSetLayout(device, mDescSetLayouts.mDescLayoutNode, nullptr);

    mModels.mModelScene.OnDestroy();
    mModels.mModelSkybox.OnDestroy();

    for (auto buffer : mUniformBuffers)
    {
        buffer.mUBOParams.destroy();
        buffer.mUBOScene.destroy();
        buffer.mUBOSkybox.destroy();
    }
    // for (auto fence : waitFences) vkDestroyFence(device, fence, nullptr);
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
            if (primitive->mMaterial.mAlphaMode == alphaMode)
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
                if (pipeline != mBoundPipeline)
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

                vkCmdPushConstants(drawCmdBuffers[cbIndex], mPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlockMaterial), &pushConstantBlockMat);

                if (primitive->mbHasIndex)
                {
                    vkCmdDrawIndexed(drawCmdBuffers[cbIndex], primitive->mIndexCount, 1, primitive->mFirstIndex, 0, 0);
                }
                else
                {
                    vkCmdDraw(drawCmdBuffers[cbIndex], primitive->mVertexCount, 1, 0, 0);
                }
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
    if (mTextures.mTexEnvCube.image)
    {
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
    std::string skyBox = assetPath + "Models/Box.gltf";
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
        dynamicStates.data(), static_cast<uint32_t>(dynamicStates.size()));

    const std::vector<VkDescriptorSetLayout> descSetLayouts =
    {
        mDescSetLayouts.mDescLayoutScene,
        mDescSetLayouts.mDescLayoutMaterial,
        mDescSetLayouts.mDescLayoutNode
    };
    VkPushConstantRange pushConstRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushConstantBlockMaterial), 0);
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(descSetLayouts.data(), descSetLayouts.size());
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
        vks::initializers::vertexInputAttributeDescription(0, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mJoint0)),
        vks::initializers::vertexInputAttributeDescription(0, 6, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mWeight0)),
        // vks::initializers::vertexInputAttributeDescription(0, 6, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(LeoRenderer::Vertex, mTangent)),
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
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/SkyBox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/SkyBox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mPipelines.mPipelineSkyBox));
//    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);

    shaderStages = {
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/PBR.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/PBR_KHR.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
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

//    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);
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
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/GenerateBRDFLUT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader(getAssetPath() + "Shaders/GLSL/PBR/GenerateBRDFLUT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
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
//    for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);

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
            imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.subresourceRange.levelCount = numMips;
            imageViewCI.subresourceRange.layerCount = 6;
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

        // Create Offscreen Framebuffer
        {
            // Image
            VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
            imageCI.imageType = VK_IMAGE_TYPE_2D;
            imageCI.format = format;
            imageCI.extent = {dim, dim, 1};
            imageCI.mipLevels = 1;
            imageCI.arrayLayers = 1;
            imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &offscreen.osImage));
            VkMemoryRequirements memReqs;
            vkGetImageMemoryRequirements(device, offscreen.osImage, &memReqs);
            VkMemoryAllocateInfo memAI = vks::initializers::memoryAllocateInfo();
            memAI.allocationSize = memReqs.size;
            memAI.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memAI, nullptr, &offscreen.osImageMemory))
            VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.osImage, offscreen.osImageMemory, 0))

            // Image View
            VkImageViewCreateInfo imageViewCI = vks::initializers::imageViewCreateInfo();
            imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
            imageViewCI.format = format;
            imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCI.subresourceRange.baseMipLevel = 0;
            imageViewCI.subresourceRange.levelCount = 1;
            imageViewCI.subresourceRange.baseArrayLayer = 0;
            imageViewCI.subresourceRange.layerCount = 1;
            imageViewCI.image = offscreen.osImage;
            VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCI, nullptr, &offscreen.osImageView))

            // Framebuffer
            VkFramebufferCreateInfo frameBufferCI = vks::initializers::framebufferCreateInfo();
            frameBufferCI.renderPass = renderPass;
            frameBufferCI.attachmentCount = 1;
            frameBufferCI.pAttachments = &offscreen.osImageView;
            frameBufferCI.width = dim;
            frameBufferCI.height = dim;
            frameBufferCI.layers = 1;
            VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &offscreen.osFrameBuffer))

            VkCommandBuffer cmdLayout = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::imageMemoryBarrier();
            imageMemoryBarrier.image = offscreen.osImage;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imageMemoryBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            vkCmdPipelineBarrier(
                cmdLayout,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0, 0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier);
            vulkanDevice->flushCommandBuffer(cmdLayout, queue, true);
        }

        // Descriptors
        VkDescriptorSetLayout descSetLayout{};
        VkDescriptorSetLayoutBinding descSetLayoutBinding = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo descSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&descSetLayoutBinding, 1);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descSetLayoutCI, nullptr, &descSetLayout))

        // DescPool
        VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
        VkDescriptorPoolCreateInfo descPoolCI = vks::initializers::descriptorPoolCreateInfo(1, &poolSize, 2);
        VkDescriptorPool descPool;
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descPoolCI, nullptr, &descPool))

        // DescSets
        VkDescriptorSet descSet;
        VkDescriptorSetAllocateInfo descSetAI = vks::initializers::descriptorSetAllocateInfo(descPool, &descSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descSetAI, &descSet));
        VkWriteDescriptorSet writeDescSet = vks::initializers::writeDescriptorSet(descSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &mTextures.mTexEnvCube.descriptor, 1);
        vkUpdateDescriptorSets(device, 1, &writeDescSet, 0, nullptr);

        struct PushBlockIrradiance
        {
            glm::mat4 mMVP;
            float mDeltaPhi = (2.0f * float(M_PI)) / 180.0f;
            float mDeltaTheta = (0.5f * float(M_PI)) / 64.0f;
        } pushBlockIrradiance;
        struct PushBlockPreFilterEnv
        {
            glm::mat4 mMVP;
            float mRoughness = 0.0f;
            uint32_t mNumSamples = 32u;
        } pushBlockPreFilterEnv;

        // Pipeline Layout
        VkPipelineLayout pipelineLayout;
        VkPushConstantRange pushConstRange{};
        pushConstRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        switch (target)
        {
            case IRRADIANCE:
                pushConstRange.size = sizeof(PushBlockIrradiance);
                break;
            case PREFILTEREDENV:
                pushConstRange.size = sizeof(PushBlockPreFilterEnv);
                break;
            default:
                break;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descSetLayout, 1);
        pipelineLayoutCI.pushConstantRangeCount = 1;
        pipelineLayoutCI.pPushConstantRanges = &pushConstRange;
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout))

        // Pipeline
        VkPipelineInputAssemblyStateCreateInfo iaStateCI{};
        iaStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        iaStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo rsStateCI{};
        rsStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rsStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        rsStateCI.cullMode = VK_CULL_MODE_NONE;
        rsStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rsStateCI.lineWidth = 1.0f;

        VkPipelineColorBlendAttachmentState cbAttachState{};
        cbAttachState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        cbAttachState.blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo cbStateCI{};
        cbStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cbStateCI.attachmentCount = 1;
        cbStateCI.pAttachments = &cbAttachState;

        VkPipelineDepthStencilStateCreateInfo dsStateCI{};
        dsStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        dsStateCI.depthTestEnable = VK_FALSE;
        dsStateCI.depthWriteEnable = VK_FALSE;
        dsStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        dsStateCI.front = dsStateCI.back;
        dsStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;

        VkPipelineViewportStateCreateInfo vpStateCI{};
        vpStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vpStateCI.viewportCount = 1;
        vpStateCI.scissorCount = 1;

        VkPipelineMultisampleStateCreateInfo msStateCI{};
        msStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        std::vector<VkDynamicState> dyStateEnabled = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyStateCI{};
        dyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyStateCI.pDynamicStates = dyStateEnabled.data();
        dyStateCI.dynamicStateCount = static_cast<uint32_t>(dyStateEnabled.size());

        // Vertex Input
        VkVertexInputBindingDescription viBinding = { 0, sizeof(LeoRenderer::Vertex), VK_VERTEX_INPUT_RATE_VERTEX };
        VkVertexInputAttributeDescription viAttribute = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };

        VkPipelineVertexInputStateCreateInfo viStateCI{};
        viStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        viStateCI.vertexBindingDescriptionCount = 1;
        viStateCI.pVertexBindingDescriptions = &viBinding;
        viStateCI.vertexAttributeDescriptionCount = 1;
        viStateCI.pVertexAttributeDescriptions = &viAttribute;

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

        VkGraphicsPipelineCreateInfo gfxPipelineCI{};
        gfxPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
        gfxPipelineCI.renderPass = renderPass;

        shaderStages[0] = LoadShader(getAssetPath() + "Shaders/GLSL/PBR/FilterCube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        switch (target)
        {
            case IRRADIANCE:
                shaderStages[1] = LoadShader(getAssetPath() + "Shaders/GLSL/PBR/IrradianceCube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
                break;
            case PREFILTEREDENV:
                shaderStages[1] = LoadShader(getAssetPath() + "Shaders/GLSL/PBR/PrefilterEnvMap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
                break;
            default:
                break;
        }
        VkPipeline pipeline;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &gfxPipelineCI, nullptr, &pipeline))
//        for (auto ss : shaderStages) vkDestroyShaderModule(device, ss.module, nullptr);

        // Render Cubemap
        VkClearValue clearValue[1];
        clearValue[0].color = { {0.0f, 0.0f, 0.2f, 0.0f} };

        VkRenderPassBeginInfo rpBI = vks::initializers::renderPassBeginInfo();
        rpBI.renderPass = renderPass;
        rpBI.framebuffer = offscreen.osFrameBuffer;
        rpBI.renderArea.extent = {dim, dim};
        rpBI.clearValueCount = 1;
        rpBI.pClearValues = clearValue;

        std::vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        };

        VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

        VkViewport viewport{};
        viewport.width = (float)dim;
        viewport.height = (float)dim;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = dim;
        scissor.extent.height = dim;

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = numMips;
        subresourceRange.layerCount = 6;

        // Change image layout for all cubemap faces to transfer destination
        {
            vulkanDevice->beginCommandBuffer(cmdBuffer);
            VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
            imageMemBarrier.image = cubeMap.image;
            imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemBarrier.srcAccessMask = 0;
            imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
            vulkanDevice->flushCommandBuffer(cmdBuffer, queue, false);
        }

        for (uint32_t m = 0; m < numMips; m++)
        {
            for (uint32_t f = 0; f < 6; f++)
            {
                vulkanDevice->beginCommandBuffer(cmdBuffer);

                viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
                viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                // 从CubeMap的面方向向量渲染
                vkCmdBeginRenderPass(cmdBuffer, &rpBI, VK_SUBPASS_CONTENTS_INLINE);

                switch (target) {
                    case IRRADIANCE:
                        pushBlockIrradiance.mMVP = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
                        vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlockIrradiance), &pushBlockIrradiance);
                        break;
                    case PREFILTEREDENV:
                        pushBlockPreFilterEnv.mMVP = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
                        pushBlockPreFilterEnv.mRoughness = (float)m / (float)(numMips - 1);
                        vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlockPreFilterEnv), &pushBlockPreFilterEnv);
                        break;
                    default:
                        break;
                }
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

                VkDeviceSize offsets[1] = {0};

                mModels.mModelSkybox.Draw(cmdBuffer);

                vkCmdEndRenderPass(cmdBuffer);

                VkImageSubresourceRange subResRange { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                subResRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subResRange.baseMipLevel = 0;
                subResRange.levelCount = numMips;
                subResRange.layerCount = 6;
                {
                    VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
                    imageMemBarrier.image = offscreen.osImage;
                    imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    imageMemBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    imageMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    imageMemBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
                }
                // Copy region for transfer from framebuffer to cube face
                VkImageCopy copyRegion{};
                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = {0, 0, 0};

                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = f;
                copyRegion.dstSubresource.mipLevel = m;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = {0, 0, 0};
                copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
                copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
                copyRegion.extent.depth = 1;

                vkCmdCopyImage(cmdBuffer, offscreen.osImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cubeMap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                {
                    VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
                    imageMemBarrier.image = offscreen.osImage;
                    imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    imageMemBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    imageMemBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
                }
                vulkanDevice->flushCommandBuffer(cmdBuffer, queue, false);
            }
        }

        {
            vulkanDevice->beginCommandBuffer(cmdBuffer);
            VkImageMemoryBarrier imageMemBarrier = vks::initializers::imageMemoryBarrier();
            imageMemBarrier.image = cubeMap.image;
            imageMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemBarrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemBarrier);
            vulkanDevice->flushCommandBuffer(cmdBuffer, queue, false);
        }

        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyFramebuffer(device, offscreen.osFrameBuffer, nullptr);
        vkFreeMemory(device, offscreen.osImageMemory, nullptr);
        vkDestroyImageView(device, offscreen.osImageView, nullptr);
        vkDestroyImage(device, offscreen.osImage, nullptr);
        vkDestroyDescriptorPool(device, descPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        cubeMap.descriptor.imageView = cubeMap.view;
        cubeMap.descriptor.sampler = cubeMap.sampler;
        cubeMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        cubeMap.device = vulkanDevice;

        switch (target) {
            case IRRADIANCE:
                mTextures.mTexIrradianceCube = cubeMap;
                break;
            case PREFILTEREDENV:
                mTextures.mTexPreFilterCube = cubeMap;
                mShaderParams.mParamPrefilteredCubeMipLevels = static_cast<float>(numMips);
                break;
            default:
                break;
        };

        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "Generating cube map with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
    }
}

void PBRRenderer::PrepareUniformBuffers()
{
    for (auto & ub : mUniformBuffers)
    {
        ub.mUBOScene.create(vulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(mShaderValScene));
        ub.mUBOSkybox.create(vulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(mShaderValSkybox));
        ub.mUBOParams.create(vulkanDevice, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(mShaderParams));
    }
    UpdateUniformBuffers();
}

void PBRRenderer::UpdateUniformBuffers()
{
    // Scene
    mShaderValScene.mUBOProj = camera.matrices.perspective;
    mShaderValScene.mUBOView = camera.matrices.view;

    // Center and scale model
    float scale = (1.0f / std::max(mModels.mModelScene.mAABB[0][0], std::max(mModels.mModelScene.mAABB[1][1], mModels.mModelScene.mAABB[2][2]))) * 0.5f;
    glm::vec3 translate = -glm::vec3(mModels.mModelScene.mAABB[3][0], mModels.mModelScene.mAABB[3][1], mModels.mModelScene.mAABB[3][2]);
    translate += -0.5f * glm::vec3(mModels.mModelScene.mAABB[0][0], mModels.mModelScene.mAABB[1][1], mModels.mModelScene.mAABB[2][2]);

    mShaderValScene.mUBOModel = glm::mat4(1.0f);
    mShaderValScene.mUBOModel[0][0] = scale;
    mShaderValScene.mUBOModel[1][1] = scale;
    mShaderValScene.mUBOModel[2][2] = scale;
    mShaderValScene.mUBOModel = glm::translate(mShaderValScene.mUBOModel, translate);

    mShaderValScene.mUBOCamPos = glm::vec3(
        -camera.position.z * sin(glm::radians(camera.rotation.y)) * cos(glm::radians(camera.rotation.x)),
        -camera.position.z * sin(glm::radians(camera.rotation.x)),
        camera.position.z * cos(glm::radians(camera.rotation.y)) * cos(glm::radians(camera.rotation.x)));

    // skybox
    mShaderValSkybox.mUBOProj = camera.matrices.perspective;
    mShaderValSkybox.mUBOView = camera.matrices.view;
    mShaderValSkybox.mUBOModel = glm::mat4(glm::mat3(camera.matrices.view));
}

void PBRRenderer::UpdateParams()
{
    mShaderParams.mParamLightDir = glm::vec4(
        sin(glm::radians(mLight.mRotation.x)) * cos(glm::radians(mLight.mRotation.y)),
        sin(glm::radians(mLight.mRotation.y)),
        cos(glm::radians(mLight.mRotation.x)) * cos(glm::radians(mLight.mRotation.y)),
        0.0f);
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

    for (uint32_t i = 0; i < drawCmdBuffers.size(); i++)
    {
        renderPassBI.framebuffer = frameBuffers[i];

        VkCommandBuffer currentCmdBuffer = drawCmdBuffers[i];

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
        vkCmdEndRenderPass(currentCmdBuffer);
        VK_CHECK_RESULT(vkEndCommandBuffer(currentCmdBuffer));
    }
}

void PBRRenderer::Prepare()
{
    VulkanFramework::Prepare();

    waitFences.resize(mRenderAhead);
    mPresentCompleteSemaphore.resize(mRenderAhead);
    mRenderCompleteSemaphore.resize(mRenderAhead);
    drawCmdBuffers.resize(swapChain.imageCount);
    mUniformBuffers.resize(swapChain.imageCount);
    mDescSets.resize(swapChain.imageCount);

    // Command buffer execution fences
    // for (auto & fence : waitFences)
    // {
    //     VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    //     VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &fence))
    // }
    for (auto & sem : mPresentCompleteSemaphore)
    {
        VkSemaphoreCreateInfo semaphoreCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &sem));
    }
    for (auto & sem : mRenderCompleteSemaphore)
    {
        VkSemaphoreCreateInfo semaphoreCI{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &sem));
    }
    // Command buffers
    {
        VkCommandBufferAllocateInfo cmdBufferAI = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, static_cast<uint32_t>(drawCmdBuffers.size()));
        VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufferAI, drawCmdBuffers.data()));
    }

    LoadAssets();
    GenerateBRDFLUT();
    GenerateCubeMaps();
    PrepareUniformBuffers();
    SetDescriptors();
    PreparePipelines();

    BuildCommandBuffers();

    prepared = true;
}

void PBRRenderer::Render()
{
    if (!prepared) return;
    VulkanFramework::RenderFrame();
    if (camera.updated) UpdateUniformBuffers();

//    VK_CHECK_RESULT(vkWaitForFences(device, 1, &waitFences[mFrameIndex], VK_TRUE, UINT64_MAX));
//    VK_CHECK_RESULT(vkResetFences(device, 1, &waitFences[mFrameIndex]));
//
//    VkResult aquired = swapChain.acquireNextImage(mPresentCompleteSemaphore[mFrameIndex], &currentBuffer);
//    if (aquired == VK_ERROR_OUT_OF_DATE_KHR || aquired == VK_SUBOPTIMAL_KHR) WindowResized();
//    else VK_CHECK_RESULT(aquired);
//
//    UpdateUniformBuffers();
//    UniformBufferSet currentUB = mUniformBuffers[currentBuffer];
//    memcpy(currentUB.mUBOScene.mapped, &mShaderValScene, sizeof(mShaderValScene));
//    memcpy(currentUB.mUBOParams.mapped, &mShaderParams, sizeof(mShaderParams));
//    memcpy(currentUB.mUBOSkybox.mapped, &mShaderValSkybox, sizeof(mShaderValSkybox));
//
//    const VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
//    VkSubmitInfo submitInfo = vks::initializers::submitInfo();
//    submitInfo.pWaitDstStageMask = &waitDstStageMask;
//    submitInfo.pWaitSemaphores = &mPresentCompleteSemaphore[mFrameIndex];
//    submitInfo.waitSemaphoreCount = 1;
//    submitInfo.pSignalSemaphores = &mRenderCompleteSemaphore[mFrameIndex];
//    submitInfo.signalSemaphoreCount = 1;
//    submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
//    submitInfo.commandBufferCount = 1;
//    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, waitFences[mFrameIndex]));
//
//    VkResult present = swapChain.queuePresent(queue, currentBuffer, mRenderCompleteSemaphore[mFrameIndex]);
//    if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR)))
//    {
//        if (present == VK_ERROR_OUT_OF_DATE_KHR) {
//            WindowResized();
//            return;
//        }
//        else {
//            VK_CHECK_RESULT(present);
//        }
//    }

    mFrameIndex += 1;
    mFrameIndex %= mRenderAhead;

    if (!paused) {
        if (mRotateModel) {
            mModelRotation.y += frameTimer * 35.0f;
            if (mModelRotation.y > 360.0f) {
                mModelRotation.y -= 360.0f;
            }
        }
        if ((mbAnimate) && (!mModels.mModelScene.mAnimations.empty())) {
            mAnimationTimer += frameTimer;
            if (mAnimationTimer > mModels.mModelScene.mAnimations[mAnimationIndex].mEnd) {
                mAnimationTimer -= mModels.mModelScene.mAnimations[mAnimationIndex].mEnd;
            }
            mModels.mModelScene.UpdateAnimation(mAnimationIndex, mAnimationTimer);
        }
        UpdateParams();
        if (mRotateModel) {
            UpdateUniformBuffers();
        }
    }
}

void PBRRenderer::ViewChanged()
{
    UpdateUniformBuffers();
}

//void PBRRenderer::WindowResized()
//{
//    BuildCommandBuffers();
//    vkDeviceWaitIdle(device);
//    UpdateUniformBuffers();
//}

void PBRRenderer::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    bool updateShaderParams = false;
    bool updateCBs = false;
    float scale = 1.0f;

    if (UIOverlay.header("Scene"))
    {
        if (UIOverlay.button("Open GLTF File"))
        {
            std::string filename;
            char buffer[MAX_PATH];
            OPENFILENAME ofn;
            ZeroMemory(&buffer, sizeof(buffer));
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFilter = "GLTF Files\0*.gltf;*.glb\0";
            ofn.lpstrFile = buffer;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrTitle = "Select a glTF file to load";
            ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameA(&ofn))
            {
                filename = buffer;
            }
            if (!filename.empty())
            {
                vkDeviceWaitIdle(device);
                LoadScene(filename);
                SetDescriptors();
                updateCBs = true;
            }
        }
        if (UIOverlay.combo("Environment", mSelectedEnvironment, mEnvironments))
        {
            vkDeviceWaitIdle(device);
            LoadEnvironment(mEnvironments[mSelectedEnvironment]);
            SetDescriptors();
            updateCBs = true;
        }
    }

    if (UIOverlay.header("Environment"))
    {
        if (UIOverlay.checkBox("Background", &mbDisplayBackground)) updateShaderParams = true;
        if (UIOverlay.sliderFloat("Exposure", &mShaderParams.mParamExposure, 0.1f, 10.0f)) updateShaderParams = true;
        if (UIOverlay.sliderFloat("Gamma", &mShaderParams.mParamGamma, 0.1f, 4.0f)) updateShaderParams = true;
        if (UIOverlay.sliderFloat("IBL", &mShaderParams.mParamScaleIBLAmbient, 0.0f, 1.0f)) updateShaderParams = true;
    }
    if (UIOverlay.header("Debug view"))
    {
        const std::vector<std::string> debugNameInputs = {
            "None", "Base Color", "Normal", "Occlusion", "Emissive", "Metallic", "Roughness"
        };
        if (UIOverlay.comboBox("Inputs", &mDebugViewInputs, debugNameInputs))
        {
            mShaderParams.mParamDebugViewInputs = static_cast<float>(mDebugViewInputs);
            updateShaderParams = true;
        }
        const std::vector<std::string> debugNameEquation = {
            "None", "Diff(l, n)", "F(l, h)", "G(l, v, h)", "D(h)", "Specular"
        };
        if (UIOverlay.comboBox("PBR Equation", &mDebugViewEquation, debugNameEquation))
        {
            mShaderParams.mParamDebugViewEquation = static_cast<float>(mDebugViewEquation);
            updateShaderParams = true;
        }
    }
    if (!mModels.mModelScene.mAnimations.empty())
    {
        if (UIOverlay.header("Animations"))
        {
            UIOverlay.checkBox("Animate", &mbAnimate);
            std::vector<std::string> animNames;
            for (auto animation : mModels.mModelScene.mAnimations)
            {
                animNames.push_back(animation.mName);
            }
            UIOverlay.comboBox("Animation", &mAnimationIndex, animNames);
        }
    }

    if (updateShaderParams)
    {
        UpdateParams();
    }
}

void PBRRenderer::FileDropped(std::string &filename)
{
    vkDeviceWaitIdle(device);
    LoadScene(filename);
    SetDescriptors();
    BuildCommandBuffers();
}

PBRRenderer* pbrRenderer;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (pbrRenderer != nullptr)
    {
        pbrRenderer->HandleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    for (int32_t i = 0; i < __argc; i++) { VulkanFramework::args.push_back(__argv[i]); };
    pbrRenderer = new PBRRenderer();
    pbrRenderer->InitVulkan();
    pbrRenderer->SetupWindow(hInstance, WndProc);
    pbrRenderer->Prepare();
    pbrRenderer->RenderLoop();
    delete(pbrRenderer);
    return 0;
}