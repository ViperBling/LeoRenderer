#include "VulkanRenderer.h"

LeoRenderer::VulkanRenderer::VulkanRenderer() : VulkanFramework(true)
{
    title = "GLTF Test";
    camera.type = Camera::CameraType::lookat;
    camera.flipY = true;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
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
        // vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        mScene.BindBuffers(drawCmdBuffers[i]);
        // Draw
        mScene.Draw(drawCmdBuffers[i]);
        DrawUI(drawCmdBuffers[i]);

        vkCmdEndRenderPass(drawCmdBuffers[i]);
        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void LeoRenderer::VulkanRenderer::LoadGLTFFile(std::string& filename)
{
    const uint32_t glTFLoadingFlags =
        LeoRenderer::FileLoadingFlags::PreTransformVertices |
        LeoRenderer::FileLoadingFlags::PreMultiplyVertexColors;
    mScene.LoadFromFile(filename, vulkanDevice, queue, glTFLoadingFlags);
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
    VkPipelineInputAssemblyStateCreateInfo iaStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState cbAttachState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &cbAttachState);
    VkPipelineDepthStencilStateCreateInfo dsStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo vpStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo msStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates.data(), static_cast<uint32_t>(dynamicStates.size()), 0);

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCIs{};
    shaderStageCIs[0] = LoadShader(getShadersPath() + "VulkanRenderer/Lambert.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStageCIs[1] = LoadShader(getShadersPath() + "VulkanRenderer/Lambert.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    const std::vector<VkVertexInputBindingDescription> viBindings =
    {
        vks::initializers::vertexInputBindingDescription(0, sizeof(LeoRenderer::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    std::vector<LeoRenderer::VertexComponent> vertexComponents =
    {
        LeoRenderer::VertexComponent::Position,
        LeoRenderer::VertexComponent::Normal,
        LeoRenderer::VertexComponent::UV,
        LeoRenderer::VertexComponent::Color,
        LeoRenderer::VertexComponent::Tangent,
    };
    const std::vector<VkVertexInputAttributeDescription> viAttributes = LeoRenderer::Vertex::InputAttributeDescriptions(0, vertexComponents);
    VkPipelineVertexInputStateCreateInfo viStateCI = vks::initializers::pipelineVertexInputStateCreateInfo(viBindings, viAttributes);

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(mPipelineLayout, renderPass, 0);
    pipelineCI.pInputAssemblyState = &iaStateCI;
    pipelineCI.pVertexInputState = &viStateCI;
    pipelineCI.pRasterizationState = &rsStateCI;
    pipelineCI.pMultisampleState = &msStateCI;
    pipelineCI.pColorBlendState = &cbStateCI;
    pipelineCI.pDepthStencilState = &dsStateCI;
    pipelineCI.pViewportState = &vpStateCI;
    pipelineCI.pDynamicState = &dyStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStageCIs.size());
    pipelineCI.pStages = shaderStageCIs.data();

    for (auto & mat : mScene.mMaterials)
    {
        struct MatSpecialData
        {
            VkBool32 alphaMask{};
            float alphaMaskCutOff{};
        } matSpecialData;

        matSpecialData.alphaMask = mat.mAlphaMode == Material::ALPHA_MODE_MASK;
        matSpecialData.alphaMaskCutOff = mat.mAlphaCutoff;

        std::vector<VkSpecializationMapEntry> specializeMapEntries =
        {
            vks::initializers::specializationMapEntry(0, offsetof(MatSpecialData, alphaMask), sizeof(MatSpecialData::alphaMask)),
            vks::initializers::specializationMapEntry(1, offsetof(MatSpecialData, alphaMaskCutOff), sizeof(MatSpecialData::alphaMaskCutOff)),
        };
        VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(specializeMapEntries, sizeof(matSpecialData), &matSpecialData);
        shaderStageCIs[1].pSpecializationInfo = &specializationInfo;

        rsStateCI.cullMode = mat.m_bDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &mat.mPipeline));
    }
}

void LeoRenderer::VulkanRenderer::PrepareUniformBuffers()
{
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &mShaderData.mBuffer,
        sizeof(mShaderData.mValues)));
    VK_CHECK_RESULT(mShaderData.mBuffer.map());
    UpdateUniformBuffers();
}

void LeoRenderer::VulkanRenderer::UpdateUniformBuffers()
{
    mShaderData.mValues.projection = camera.matrices.perspective;
    mShaderData.mValues.view = camera.matrices.view;
    mShaderData.mValues.viewPos = camera.viewPos;
    memcpy(mShaderData.mBuffer.mapped, &mShaderData.mValues, sizeof(mShaderData.mValues));
}

void LeoRenderer::VulkanRenderer::Prepare()
{
    VulkanFramework::Prepare();
    LoadAssets();
    PrepareUniformBuffers();
    SetupDescriptors();
    PreparePipelines();
    BuildCommandBuffers();
    prepared = true;
}

void LeoRenderer::VulkanRenderer::Render()
{
    RenderFrame();
    if (camera.updated) UpdateUniformBuffers();
}

void LeoRenderer::VulkanRenderer::ViewChanged()
{
    UpdateUniformBuffers();
}

void LeoRenderer::VulkanRenderer::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    if (overlay->button("All"))
    {
        std::for_each(mScene.mNodes.begin(), mScene.mNodes.end(), [](LeoRenderer::Node* node) { node->visible = true; });
        BuildCommandBuffers();
    }
    ImGui::SameLine();
    if (overlay->button("None"))
    {
        std::for_each(mScene.mNodes.begin(), mScene.mNodes.end(), [](LeoRenderer::Node* node) { node->visible = false; });
        BuildCommandBuffers();
    }
    ImGui::NewLine();

    // POI: Create a list of glTF nodes for visibility toggle
    ImGui::BeginChild("#Nodelist", ImVec2(200.0f * overlay->scale, 340.0f * overlay->scale), false);
    for (auto& node : mScene.mNodes)
    {
        if (overlay->checkBox(node->mName.c_str(), &node->visible))
        {
            BuildCommandBuffers();
        }
    }
    ImGui::EndChild();
}

LeoRenderer::VulkanRenderer* renderer;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (renderer != nullptr)
	{
		renderer->HandleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	for (int32_t i = 0; i < __argc; i++) { LeoRenderer::VulkanRenderer::args.push_back(__argv[i]); };
	renderer = new LeoRenderer::VulkanRenderer();
	renderer->InitVulkan();
	renderer->SetupWindow(hInstance, WndProc);
	renderer->Prepare();
	renderer->RenderLoop();
	delete(renderer);
	return 0;
}