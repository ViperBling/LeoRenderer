#include "Test_GLTFLoader.h"

VulkanGLTFLoader::~VulkanGLTFLoader()
{
    for (auto node : nodes) delete node;

    vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
    vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
    vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
    vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);

    for (Image image : images)
    {
        vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
        vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
        vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
        vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
    }
}

void VulkanGLTFLoader::LoadImages(tinygltf::Model& input)
{
    images.resize(input.images.size());
    for (size_t i = 0; i < input.images.size(); i++)
    {
        tinygltf::Image& glTFImage = input.images[i];
        unsigned char* buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        bool deleteBuffer = false;
        // 把RGB图像转换成RGBA，大多数设备的Vulkan不支持RGB
        if (glTFImage.component == 3)
        {
            bufferSize = glTFImage.width * glTFImage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &glTFImage.image[0];

            for (size_t j = 0; j < glTFImage.width * glTFImage.height; j++)
            {
                memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else
        {
            buffer = &glTFImage.image[0];
            bufferSize = glTFImage.image.size();
        }
        // 加载材质
        images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
        if (deleteBuffer)
        {
            delete[] buffer;
        }
    }
}

void VulkanGLTFLoader::LoadTextures(tinygltf::Model &input)
{
    textures.resize(input.textures.size());
    for (size_t i = 0; i < input.textures.size(); i++)
    {
        textures[i].imageIndex = input.textures[i].source;
    }
}

void VulkanGLTFLoader::LoadMaterials(tinygltf::Model &input)
{
    materials.resize(input.materials.size());
    for (size_t i = 0; i < input.materials.size(); i++)
    {
        tinygltf::Material glTFMaterial = input.materials[i];
        // BaseColor
        if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end())
        {
            materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
        }
        if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
            materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
        }
    }
}

void VulkanGLTFLoader::LoadNode(
    const tinygltf::Node& inputNode, const tinygltf::Model &input, Node *parent,
    std::vector<uint32_t> &indexBuffer, std::vector<Vertex>& vertexBuffer)
{
    Node* node = new Node;
    node->matrix = glm::mat4(1.0f);
    node->parent = parent;

    if (inputNode.translation.size() == 3) node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
    if (inputNode.rotation.size() == 4)
    {
        glm::quat quat = glm::make_quat(inputNode.rotation.data());
        node->matrix *= glm::mat4(quat);
    }
    if (inputNode.scale.size() == 3) node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
    if (inputNode.matrix.size() == 16) node->matrix = glm::make_mat4x4(inputNode.matrix.data());

    // 加载子节点
    if (!inputNode.children.empty())
    {
        for (auto & child : inputNode.children)
        {
            LoadNode(input.nodes[child], input, node, indexBuffer, vertexBuffer);
        }
    }
    // 如果节点包括MeshData，那么就从Buffer中获取顶点和索引
    if (inputNode.mesh > -1)
    {
        const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
        for (auto & meshPrimitives : mesh.primitives)
        {
            const tinygltf::Primitive& glTFPrimitive = meshPrimitives;
            auto firstIndex = static_cast<uint32_t>(indexBuffer.size());
            auto vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            uint32_t indexCount = 0;

            // Vertices
            {
                const float* positionBuffer{};
                const float* normalsBuffer{};
                const float* texCoordsBuffer{};
                size_t vertexCount = 0;

                // Get buffer data for vertex positions
                if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }
                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }
                // Get buffer data for vertex texture coordinates
                // glTF supports multiple sets, we only load the first one
                if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }
                // Append data to model's vertex buffer
                for (size_t v = 0; v < vertexCount; v++)
                {
                    Vertex vert{};
                    vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                    vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                    vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                    vert.color = glm::vec3(1.0f);
                    vertexBuffer.push_back(vert);
                }
            }
            // Indices
            {
                const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
                const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                indexCount += static_cast<uint32_t>(accessor.count);

                // glTF supports different component types of indices
                switch (accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        auto buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        auto buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        auto buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                }
            }
            Primitive primitive{};
            primitive.firstIndex = firstIndex;
            primitive.indexCount = indexCount;
            primitive.materialIndex = glTFPrimitive.material;
            node->mesh.primitives.push_back(primitive);
        }
    }

    if (parent)
    {
        parent->children.push_back(node);
    }
    else
    {
        nodes.push_back(node);
    }
}

void VulkanGLTFLoader::DrawNode(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, Node *node)
{
    if (!node->mesh.primitives.empty())
    {
        // 通过vkCmdPushConstant来传递变换矩阵
        // 遍历node的层次结构，应用所有的变换
        glm::mat4 nodeMatrix = node->matrix;
        Node* currentParent = node->parent;
        while (currentParent)
        {
            nodeMatrix = currentParent->matrix * nodeMatrix;
            currentParent = currentParent->parent;
        }
        vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
        for (Primitive & primitive : node->mesh.primitives)
        {
            if (primitive.indexCount > 0)
            {
                Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
                // 绑定描述符
                vkCmdBindDescriptorSets(
                    cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    pipelineLayout, 1, 1,
                    &images[texture.imageIndex].descriptorSet, 0,nullptr);
                vkCmdDrawIndexed(cmdBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
            }
        }
    }
    for (auto & child : node->children)
    {
        DrawNode(cmdBuffer, pipelineLayout, child);
    }
}

void VulkanGLTFLoader::Draw(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
    VkDeviceSize offsets[1]{};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    for (auto & node : nodes)
    {
        DrawNode(cmdBuffer, pipelineLayout, node);
    }
}


TestGLTFLoader::TestGLTFLoader() : VulkanExampleBase(ENABLE_VALIDATION)
{
    title = "GLTFLoader";
    camera.type = Camera::CameraType::lookat;
    camera.flipY = true;
    camera.setPosition(glm::vec3(0.0f, -0.1f, -1.0f));
    camera.setRotation(glm::vec3(0.0f, 45.0f, 0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
}

TestGLTFLoader::~TestGLTFLoader()
{
    vkDestroyPipeline(device, pipelines.solid, nullptr);
    if (pipelines.wireFrame != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelines.wireFrame, nullptr);
    }

    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);

    shaderData.buffer.destroy();
}

void TestGLTFLoader::LoadGLTFFile(const std::string filename)
{
    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF glTFContext;
    std::string error, warning;

    this->device = device;

    bool fileLoaded = glTFContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

    // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
    glTFModel.vulkanDevice = vulkanDevice;
    glTFModel.copyQueue = queue;

    std::vector<uint32_t> indexBuffer;
    std::vector<Vertex> vertexBuffer;

    if (fileLoaded)
    {
        glTFModel.LoadImages(glTFInput);
        glTFModel.LoadMaterials(glTFInput);
        glTFModel.LoadTextures(glTFInput);
        const tinygltf::Scene& scene = glTFInput.scenes[0];
        for (auto & n : scene.nodes)
        {
            const tinygltf::Node node = glTFInput.nodes[n];
            glTFModel.LoadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
        }
    }
    else
    {
        vks::tools::exitFatal("Could not open the glTF file.\n\nThe file is part of the additional asset pack.\n\nRun \"download_assets.py\" in the repository root to download the latest version.", -1);
        return;
    }

    size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
    size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
    glTFModel.indices.count = static_cast<int32_t>(indexBuffer.size());

    StagingBuffer vertexStaging{}, indexStaging{};

    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBufferSize,
        &vertexStaging.buffer,
        &vertexStaging.memory,
        vertexBuffer.data()));
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,
        &indexStaging.buffer,
        &indexStaging.memory,
        indexBuffer.data()));

    // Device Local Buffer
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        &glTFModel.vertices.buffer,
        &glTFModel.vertices.memory));
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        &glTFModel.indices.buffer,
        &glTFModel.indices.memory));

    // 拷贝数据，从Staging Buffer到Device Buffer
    VkCommandBuffer copyCmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion{};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmdBuffer, vertexStaging.buffer, glTFModel.vertices.buffer, 1, &copyRegion);

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmdBuffer, indexStaging.buffer, glTFModel.indices.buffer, 1, &copyRegion);

    vulkanDevice->flushCommandBuffer(copyCmdBuffer, queue, true);

    // Free
    vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
    vkFreeMemory(device, vertexStaging.memory, nullptr);
    vkDestroyBuffer(device, indexStaging.buffer, nullptr);
    vkFreeMemory(device, indexStaging.memory, nullptr);
}

void TestGLTFLoader::LoadAssets()
{
    LoadGLTFFile(getAssetPath() + "Models/BusterDrone/busterDrone.gltf");
}

void TestGLTFLoader::getEnabledFeatures()
{
    if (deviceFeatures.fillModeNonSolid) enabledFeatures.fillModeNonSolid = VK_TRUE;
}

void TestGLTFLoader::buildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufferBI = vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBI = vks::initializers::renderPassBeginInfo();
    renderPassBI.renderPass = renderPass;
    renderPassBI.renderArea.offset.x = 0;
    renderPassBI.renderArea.offset.y = 0;
    renderPassBI.renderArea.extent.width = width;
    renderPassBI.renderArea.extent.height = height;
    renderPassBI.clearValueCount = 2;
    renderPassBI.pClearValues = clearValues;

    const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

    for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
    {
        renderPassBI.framebuffer = frameBuffers[i];

        VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufferBI));
        vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
        vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
        // Bind scene matrices descriptor to set 0
        vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireFrame ? pipelines.wireFrame : pipelines.solid);
        glTFModel.Draw(drawCmdBuffers[i], pipelineLayout);
        drawUI(drawCmdBuffers[i]);
        vkCmdEndRenderPass(drawCmdBuffers[i]);
        VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
    }
}

void TestGLTFLoader::SetupDescriptors()
{
    // 这里分别为变换矩阵和Material使用了不同的描述符

    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFModel.images.size()))
    };

    const uint32_t maxSetCount = static_cast<uint32_t>(glTFModel.images.size()) + 1;
    VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));

    // 矩阵描述符集布局
    VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));
    // 图片描述符集布局
    setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));
    // 管线要使用上面两个布局
    std::array<VkDescriptorSetLayout, 2> setLayouts = {descriptorSetLayouts.matrices, descriptorSetLayouts.textures};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // 用pushconstant来完成矩阵传输
    VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 矩阵描述符集创建
    VkDescriptorSetAllocateInfo matDescriptorSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &matDescriptorSetAI, &descriptorSet));
    VkWriteDescriptorSet  matWriteDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
    vkUpdateDescriptorSets(device, 1, &matWriteDescriptorSet, 0, nullptr);
    // 图片描述符集创建
    for (auto & image : glTFModel.images)
    {
        const VkDescriptorSetAllocateInfo imgDescriptorSetAI = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &imgDescriptorSetAI, &image.descriptorSet));
        VkWriteDescriptorSet imgWriteDescriptorSet = vks::initializers::writeDescriptorSet(image.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &image.texture.descriptor);
        vkUpdateDescriptorSets(device, 1, &imgWriteDescriptorSet, 0, nullptr);
    }
}

void TestGLTFLoader::PreparePipelines()
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

    const std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates.data(), static_cast<uint32_t>(dynamicStates.size()), 0);

    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
        vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
        vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),	// Location 0: Position
        vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)), // Location 1: Normal
        vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, uv)),	    // Location 2: Texture coordinates
        vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)),	// Location 3: Color
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
    vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        loadShader(getShadersPath() + "TestGLTFLoader/TestGLTFLoaderGLSL.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        loadShader(getShadersPath() + "TestGLTFLoader/TestGLTFLoaderGLSL.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
    pipelineCI.pVertexInputState = &vertexInputStateCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState = &colorBlendStateCI;
    pipelineCI.pMultisampleState = &multisampleStateCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pDepthStencilState = &depthStencilStateCI;
    pipelineCI.pDynamicState = &dynamicStateCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    // Solid rendering pipeline
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));

    // Wire frame rendering pipeline
    if (deviceFeatures.fillModeNonSolid) {
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
        rasterizationStateCI.lineWidth = 1.0f;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireFrame));
    }
}

void TestGLTFLoader::PrepareUniformBuffers()
{
    // Vertex shader uniform buffer block
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &shaderData.buffer,
        sizeof(shaderData.values)));

    // Map persistent
    VK_CHECK_RESULT(shaderData.buffer.map());

    UpdateUniformBuffers();
}

void TestGLTFLoader::UpdateUniformBuffers()
{
    shaderData.values.projectMat = camera.matrices.perspective;
    shaderData.values.viewMat = camera.matrices.view;
    shaderData.values.viewPos = camera.viewPos;
    memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
}

void TestGLTFLoader::prepare()
{
    VulkanExampleBase::prepare();
    LoadAssets();
    PrepareUniformBuffers();
    SetupDescriptors();
    PreparePipelines();
    buildCommandBuffers();
    prepared = true;
}

void TestGLTFLoader::render()
{
    renderFrame();
    if (camera.updated) {
        UpdateUniformBuffers();
    }
}

void TestGLTFLoader::viewChanged()
{
    UpdateUniformBuffers();
}

void TestGLTFLoader::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    if (overlay->header("Settings")) {
        if (overlay->checkBox("WireFrame", &wireFrame)) {
            buildCommandBuffers();
        }
    }
}

TestGLTFLoader * testGLTFLoader;
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (testGLTFLoader != nullptr)
	{
        testGLTFLoader->handleMessages(hWnd, uMsg, wParam, lParam);
	}
	return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	for (int32_t i = 0; i < __argc; i++)
    {
        TestGLTFLoader::args.push_back(__argv[i]);
    };

	testGLTFLoader = new TestGLTFLoader();
	testGLTFLoader->initVulkan();
	testGLTFLoader->setupWindow(hInstance, WndProc);
	testGLTFLoader->prepare();
	testGLTFLoader->renderLoop();
	delete(testGLTFLoader);

	return 0;
}
