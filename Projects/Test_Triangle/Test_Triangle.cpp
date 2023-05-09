#include "Test_Triangle.h"

// Set to "true" to enable Vulkan's validation layers (see vulkandebug.cpp for details)
#define ENABLE_VALIDATION false
// Set to "true" to use staging buffers for uploading vertex and index data to device local memory
// See "prepareVertices" for details on what's staging and on why to use it
#define USE_STAGING true


class TestTriangle : public VulkanExampleBase
{
public:
    TestTriangle() : VulkanExampleBase(ENABLE_VALIDATION) {
        title = "Test Triangle";
        settings.overlay = false;       // 不使用UI
        camera.type = Camera::CameraType::lookat;
        camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
        camera.setRotation(glm::vec3(0.0f));
        camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 512.0f);
    }

    ~TestTriangle() override
    {
        vkDestroyPipeline(device, pipeline, nullptr);

        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, vertices.buffer, nullptr);
        vkFreeMemory(device, vertices.memory, nullptr);

        vkDestroyBuffer(device, indices.buffer, nullptr);
        vkFreeMemory(device, indices.memory, nullptr);

        vkDestroyBuffer(device, uniformBuffer.buffer, nullptr);
        vkFreeMemory(device, uniformBuffer.memory, nullptr);

        vkDestroySemaphore(device, presentCompleteSemaphore, nullptr);
        vkDestroySemaphore(device, renderCompleteSemaphore, nullptr);

        for (auto& fence : queueCompleteFences)
        {
            vkDestroyFence(device, fence, nullptr);
        }
    }

public:
    void prepare() override
    {
        VulkanExampleBase::prepare();
        prepareSyncPrimtives();
        prepareVertices(USE_STAGING);
        prepareUniformBuffers();
        setupDescriptorSetLayout();
        preparePipelines();
        setupDescriptorPool();
        setupDescriptorSet();
        buildCommandBuffers();
        prepared = true;
    }

    void render() override
    {
        if (!prepared) return;
        draw();
    }

    void viewChanged() override
    {
        updateUniformBuffers();
    }

    // 返回和要求的显存属性匹配的显存类型索引
    uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties)
    {
        for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
        {
            if ((typeBits & 1) == 1)
            {
                if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
            }
            typeBits >>= 1;
        }
        throw std::runtime_error("Could not find a suitable memory type!");
    }

    // 创建同步变量
    void prepareSyncPrimtives()
    {
        // Semaphores，用来纠正命令顺序
        VkSemaphoreCreateInfo semaphoreCI{};
        semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreCI.pNext = nullptr;

        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &presentCompleteSemaphore));
        VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCI, nullptr, &renderCompleteSemaphore));

        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // 在Signal State创建，这样就不必在Command Buffer在第一次渲染时等待
        fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        queueCompleteFences.resize(drawCmdBuffers.size());
        for (auto & fence : queueCompleteFences)
        {
            VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &fence));
        }
    }

    // 从Command Pool中获取新的Command Buffer
    // 如果begin为True，当前的Command Buffer可以开始加入命令
    VkCommandBuffer getCommandBuffer(bool begin)
    {
        VkCommandBuffer cmdBuffer;

        VkCommandBufferAllocateInfo cmdBufferAI{};
        cmdBufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAI.commandPool = cmdPool;
        cmdBufferAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferAI.commandBufferCount = 1;

        VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufferAI, &cmdBuffer));

        if (begin)
        {
            VkCommandBufferBeginInfo cmdBufferBI = vks::initializers::commandBufferBeginInfo();
            VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBI));
        }
        return cmdBuffer;
    }

    // Command Buffer记录完成开始提交，使用Fence确保Command Buffer删除前已执行完成
    void flushCommandBuffer(VkCommandBuffer commandBuffer)
    {
        assert(commandBuffer != VK_NULL_HANDLE);

        VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // 创建Fence
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCI.flags = 0;
        VkFence fence;
        VK_CHECK_RESULT(vkCreateFence(device, &fenceCI, nullptr, &fence));

        // 提交到队列
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
        // 等待Fence通知Command Buffer执行完成
        VK_CHECK_RESULT(vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

        vkDestroyFence(device, fence, nullptr);
        vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
    }

    // 为每个Frame Buffer构建Command Buffer
    void buildCommandBuffers() override
    {
        VkCommandBufferBeginInfo cmdBufferBI{};
        cmdBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferBI.pNext = nullptr;

        // 分别为颜色和深度设置颜色绑定，所以要为它们两个设置Clear Value
        VkClearValue clearValues[2];
        clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassBI{};
        renderPassBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBI.pNext = nullptr;
        renderPassBI.renderPass = renderPass;
        renderPassBI.renderArea.offset.x = 0;
        renderPassBI.renderArea.offset.y = 0;
        renderPassBI.renderArea.extent.width = width;
        renderPassBI.renderArea.extent.height = height;
        renderPassBI.clearValueCount = 2;
        renderPassBI.pClearValues = clearValues;

        for (int32_t i = 0; i < drawCmdBuffers.size(); i++)
        {
            renderPassBI.framebuffer = frameBuffers[i];

            VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufferBI));
            // 开启基类中设置的第一个Subpass
            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

            // 设置Viewport
            VkViewport viewport{};
            viewport.height = (float)height;
            viewport.width = (float)width;
            viewport.minDepth = (float) 0.0f;
            viewport.maxDepth = (float) 1.0f;
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            // Update dynamic scissor state
            VkRect2D scissor = {};
            scissor.extent.width = width;
            scissor.extent.height = height;
            scissor.offset.x = 0;
            scissor.offset.y = 0;
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            // 绑定和Shader对应的描述符集
            vkCmdBindDescriptorSets(
                drawCmdBuffers[i],
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout, 0, 1,
                &descriptorSet, 0, nullptr);

            // 绑定管线
            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            // 绑定Vertex Buffer
            VkDeviceSize offsets[1] = { 0 };
            vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertices.buffer, offsets);

            // 绑定Index Buffer
            vkCmdBindIndexBuffer(drawCmdBuffers[i], indices.buffer, 0, VK_INDEX_TYPE_UINT32);

            // 绘制
            vkCmdDrawIndexed(drawCmdBuffers[i], indices.count, 1, 0, 0, 1);

            vkCmdEndRenderPass(drawCmdBuffers[i]);
            // Render Pass结束后会添加一个隐式的Barrier以便Frame Buffer的颜色绑定转换到VK_IMAGE_LAYOUT_PRESENT_SRC_KHR，从而可以在窗口显示
            VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
        }
    }

    void draw()
    {
        VkResult acquire = swapChain.acquireNextImage(presentCompleteSemaphore, &currentBuffer);
        if (!((acquire == VK_SUCCESS) || (acquire == VK_SUBOPTIMAL_KHR))) VK_CHECK_RESULT(acquire);

        VK_CHECK_RESULT(vkWaitForFences(device, 1, &queueCompleteFences[currentBuffer], VK_TRUE, UINT64_MAX));
        VK_CHECK_RESULT(vkResetFences(device, 1, &queueCompleteFences[currentBuffer]));

        // Pipeline Stage在哪个队列提交时会等待
        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        // Submit Info指明了Command Buffer队列批次
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pWaitDstStageMask = &waitStageMask;               // 指向Semaphore会等待的管线阶段的指针
        submitInfo.waitSemaphoreCount = 1;                           // 一个Wait信号量
        submitInfo.signalSemaphoreCount = 1;                         // 一个Signal信号量
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer]; // 执行此批次的Command Buffer
        submitInfo.commandBufferCount = 1;                           // 当前只有一个Command Buffer

        submitInfo.pWaitSemaphores = &presentCompleteSemaphore;      // 等待命令缓冲开始执行的信号量
        submitInfo.pSignalSemaphores = &renderCompleteSemaphore;     // 标记命令缓冲执行完成的信号量

        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, queueCompleteFences[currentBuffer]));
        // 将当前Frame Buffer呈现到Swap Chain，通过信号量确保只有命令执行完成才会呈现
        VkResult present = swapChain.queuePresent(queue, currentBuffer, renderCompleteSemaphore);
        if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR)))
        {
            VK_CHECK_RESULT(present);
        }
    }

    // 准备VertexBuffer和IndexBuffer
    void prepareVertices(bool useStagingBuffers)
    {
        const std::vector<Vertex> vertexBuffer =
        {
            { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
            { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
            { {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
        };
        uint32_t vertexBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(Vertex);

        const std::vector<uint32_t> indexBuffer = { 0, 1, 2 };
        indices.count = static_cast<uint32_t>(indexBuffer.size());
        uint32_t indexBufferSize = indices.count * sizeof(uint32_t);

        VkMemoryAllocateInfo memoryAI{};
        memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memoryRequirements;

        void * data;

        if (useStagingBuffers)
        {
            // 类似顶点和索引这种静态数据应该存储在显存中以便GPU更快地访问，为了实现这一点，需要通过StagingBuffer
            // 1. 创建一个CPU（host）可见的Buffer，可以被映射
            // 2. 把数据拷贝到上面创建的Buffer中
            // 3. 在显存中创建一个大小相同的Buffer
            // 4. 用CommandBuffer把数据从CPU端Buffer中拷贝到显存的Buffer中
            // 5. 删除CPU端Buffer，使用GPU的Buffer渲染
            StagingBuffers stagingBuffers{};

            // ==================================== VertexBuffer ==================================== //
            VkBufferCreateInfo vertexBufferCI{};
            vertexBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertexBufferCI.size = vertexBufferSize;
            vertexBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;            // Buffer用作拷贝源
            // 1. 创建一个CPU端可见的Buffer，把顶点数据拷贝进去
            VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferCI, nullptr, &stagingBuffers.vertices.buffer));
            vkGetBufferMemoryRequirements(device, stagingBuffers.vertices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            // 获取一块CPU可见的内存存放数据，要求一致性，这样当解除映射后GPU立即可见写入的内容
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(
                memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &stagingBuffers.vertices.memory));

            // 2. 映射并拷贝数据
            VK_CHECK_RESULT(vkMapMemory(device, stagingBuffers.vertices.memory, 0, vertexBufferSize, 0, &data));
            memcpy(data, vertexBuffer.data(), vertexBufferSize);
            vkUnmapMemory(device, stagingBuffers.vertices.memory);
            VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0));

            // 3. 创建一个GPU可见的Buffer，然后把上面Staging Buffer的内容拷贝进去
            vertexBufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferCI, nullptr, &vertices.buffer));
            vkGetBufferMemoryRequirements(device, vertices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &vertices.memory));
            VK_CHECK_RESULT(vkBindBufferMemory(device, vertices.buffer, vertices.memory, 0));

            // ==================================== IndexBuffer ==================================== //
            VkBufferCreateInfo indexBufferCI{};
            indexBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indexBufferCI.size = indexBufferSize;
            indexBufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            // 1. 创建一个CPU端可见的Buffer，把顶点数据拷贝进去
            VK_CHECK_RESULT(vkCreateBuffer(device, &indexBufferCI, nullptr, &stagingBuffers.indices.buffer));
            vkGetBufferMemoryRequirements(device, stagingBuffers.indices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(
                memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &stagingBuffers.indices.memory));

            // 2. 映射并拷贝数据
            VK_CHECK_RESULT(vkMapMemory(device, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &data))
            memcpy(data, indexBuffer.data(), indexBufferSize);
            vkUnmapMemory(device, stagingBuffers.indices.memory);
            VK_CHECK_RESULT(vkBindBufferMemory(device, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0));

            // 3. 创建一个GPU可见的Buffer，然后把上面Staging Buffer的内容拷贝进去
            indexBufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            VK_CHECK_RESULT(vkCreateBuffer(device, &indexBufferCI, nullptr, &indices.buffer));
            vkGetBufferMemoryRequirements(device, indices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &indices.memory));
            VK_CHECK_RESULT(vkBindBufferMemory(device, indices.buffer, indices.memory, 0));

            // 开始创建Command Buffer，并提交
            VkCommandBuffer cmdCopy = getCommandBuffer(true);
            // 设置拷贝范围
            VkBufferCopy copyRegion{};
            // 4. Vertex Buffer拷贝
            copyRegion.size = vertexBufferSize;
            vkCmdCopyBuffer(cmdCopy, stagingBuffers.vertices.buffer, vertices.buffer, 1, &copyRegion);
            // 4. Index Buffer拷贝
            copyRegion.size = indexBufferSize;
            vkCmdCopyBuffer(cmdCopy, stagingBuffers.indices.buffer, indices.buffer, 1, &copyRegion);
            // 提交执行
            flushCommandBuffer(cmdCopy);
            // 销毁临时资源
            vkDestroyBuffer(device, stagingBuffers.vertices.buffer, nullptr);
            vkDestroyBuffer(device, stagingBuffers.indices.buffer, nullptr);
            vkFreeMemory(device, stagingBuffers.vertices.memory, nullptr);
            vkFreeMemory(device, stagingBuffers.indices.memory, nullptr);
        }
        else
        {
            // 不使用Staging Buffer的方式，直接创建一个CPU端可见的Buffer然后用于渲染。不建议这样做，会降低性能
            // ==================================== VertexBuffer ==================================== //
            VkBufferCreateInfo vertexBufferCI{};
            vertexBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertexBufferCI.size = vertexBufferSize;
            vertexBufferCI.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

            // 拷贝顶点数据
            VK_CHECK_RESULT(vkCreateBuffer(device, &vertexBufferCI, nullptr, &vertices.buffer));
            vkGetBufferMemoryRequirements(device, vertices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            // VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT is host visible memory, and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT makes sure writes are directly visible
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(
                memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &vertices.memory));
            VK_CHECK_RESULT(vkMapMemory(device, vertices.memory, 0, memoryAI.allocationSize, 0, &data));
            memcpy(data, vertexBuffer.data(), vertexBufferSize);
            vkUnmapMemory(device, vertices.memory);
            VK_CHECK_RESULT(vkBindBufferMemory(device, vertices.buffer, vertices.memory, 0));

            /// ==================================== IndexBuffer ==================================== //
            VkBufferCreateInfo indexBufferCI{};
            indexBufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indexBufferCI.size = indexBufferSize;
            indexBufferCI.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

            // 拷贝顶点数据
            VK_CHECK_RESULT(vkCreateBuffer(device, &indexBufferCI, nullptr, &indices.buffer));
            vkGetBufferMemoryRequirements(device, indices.buffer, &memoryRequirements);
            memoryAI.allocationSize = memoryRequirements.size;
            memoryAI.memoryTypeIndex = getMemoryTypeIndex(
                memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &indices.memory));
            VK_CHECK_RESULT(vkMapMemory(device, indices.memory, 0, memoryAI.allocationSize, 0, &data));
            memcpy(data, indexBuffer.data(), indexBufferSize);
            vkUnmapMemory(device, indices.memory);
            VK_CHECK_RESULT(vkBindBufferMemory(device, indices.buffer, indices.memory, 0));
        }
    }

    void setupDescriptorPool()
    {
        // 需要告诉Vulkan用的Descriptor的数量
        VkDescriptorPoolSize typeCounts[1];
        // 这里我们只用了一个，用来描述UniformBuffer
        typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        typeCounts[0].descriptorCount = 1;
        // 要添加其他的描述符类型可以按照如下方法
        // E.g. for two combined image samplers :
        // typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // typeCounts[1].descriptorCount = 2;

        // 创建描述符池
        VkDescriptorPoolCreateInfo descriptorPoolCI{};
        descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCI.pNext = nullptr;
        descriptorPoolCI.poolSizeCount = 1;
        descriptorPoolCI.pPoolSizes = typeCounts;
        descriptorPoolCI.maxSets = 1;

        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorPool));
    }

    void setupDescriptorSetLayout()
    {
        // 连接不同管线阶段到描述符，具有唯一性
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.descriptorCount = 1;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        layoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = {};
        descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutCI.pNext = nullptr;
        descriptorLayoutCI.bindingCount = 1;
        descriptorLayoutCI.pBindings = &layoutBinding;

        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

        // 基于描述符布局创建管线布局
        VkPipelineLayoutCreateInfo pipelineLayoutCI = {};
        pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCI.pNext = nullptr;
        pipelineLayoutCI.setLayoutCount = 1;
        pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;

        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));
    }

    void setupDescriptorSet()
    {
        // 从描述符池中分配一个描述符集
        VkDescriptorSetAllocateInfo descriptorSetAI{};
        descriptorSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAI.descriptorPool = descriptorPool;
        descriptorSetAI.descriptorSetCount = 1;
        descriptorSetAI.pSetLayouts = &descriptorSetLayout;

        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAI, &descriptorSet));

        // 更新着色器绑定的描述符集，每个绑定点必须要有一个对应的描述符集
        VkWriteDescriptorSet writeDescriptorSet{};
        // 绑定到0号索引，也就是着色器中的uniform buffer
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSet.pBufferInfo = &uniformBuffer.descriptor;
        // 绑定0号点
        writeDescriptorSet.dstBinding = 0;
        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
    }

    // 创建depth和stencil Buffer绑定
    void setupDepthStencil() override
    {
        // 创建Depth Stencil图像用于Attachment
        VkImageCreateInfo imageCI = {};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = depthFormat;
        // Use example's height and width
        imageCI.extent = { width, height, 1 };
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));

        // 给创建的图像分配显存并绑定
        VkMemoryAllocateInfo memoryAI = {};
        memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device, depthStencil.image, &memoryRequirements);
        memoryAI.allocationSize = memoryRequirements.size;
        memoryAI.memoryTypeIndex = getMemoryTypeIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device, &memoryAI, nullptr, &depthStencil.mem));
        VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

        // 为图像创建视图
        // 图像不能被Vulkan直接访问，但可以通过一个subresourceRange描述的视图访问，这样可以让一个图像有不同的视图
        VkImageViewCreateInfo depthStencilViewCI = {};
        depthStencilViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthStencilViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilViewCI.format = depthFormat;
        depthStencilViewCI.subresourceRange = {};
        depthStencilViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT)
        if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
            depthStencilViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        depthStencilViewCI.subresourceRange.baseMipLevel = 0;
        depthStencilViewCI.subresourceRange.levelCount = 1;
        depthStencilViewCI.subresourceRange.baseArrayLayer = 0;
        depthStencilViewCI.subresourceRange.layerCount = 1;
        depthStencilViewCI.image = depthStencil.image;
        VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilViewCI, nullptr, &depthStencil.view));
    }

    void setupFrameBuffer() override
    {
        // Create a frame buffer for every image in the swapchain
        frameBuffers.resize(swapChain.imageCount);
        for (size_t i = 0; i < frameBuffers.size(); i++)
        {
            std::array<VkImageView, 2> attachments{};
            attachments[0] = swapChain.buffers[i].view; // Color attachment is the view of the swapchain image
            attachments[1] = depthStencil.view;         // Depth/Stencil attachment is the same for all frame buffers

            VkFramebufferCreateInfo frameBufferCI = {};
            frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // All frame buffers use the same renderpass setup
            frameBufferCI.renderPass = renderPass;
            frameBufferCI.attachmentCount = static_cast<uint32_t>(attachments.size());
            frameBufferCI.pAttachments = attachments.data();
            frameBufferCI.width = width;
            frameBufferCI.height = height;
            frameBufferCI.layers = 1;
            // Create the framebuffer
            VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCI, nullptr, &frameBuffers[i]));
        }
    }

    // RenderPass是Vulkan中新的概念，它描述了渲染中用到的attachments，也可能包括有attachments dependecies的subpass。
    // 这让驱动可以提前知道渲染的轮廓以便于优化，尤其是在Tiled-Based Renderer（包含多个Subpass）中
    // 使用Sub pass dependencies还会带来隐式的资源转换，所以就不用专门添加Barrier了
    void setupRenderPass() override
    {
        // 这里只用到一个RenderPass，它包含一个sub pass
        // renderpass中用到的RenderTarget描述符
        std::array<VkAttachmentDescription, 2> attachments{};
        // Color attachment
        attachments[0].format = swapChain.colorFormat;                                  // 使用SwapChain的格式
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;                                 // 不用MSAA
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                            // 在RenderPass开始的时候清理这个Attachments（RT）
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;                          // 在渲染结束后保留Attachment上的内容用于显示
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                 // 不使用Stencil，所以不用管
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;               // 和上一条一样
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                       // RenderPass开始时的布局，不用管，设置为UNDEFINED
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;                   // RenderPass结束时的Attachments转换的布局，因为要显示，所以为PRESENT_KHR

        // Depth attachment
        attachments[1].format = depthFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                           // 在开始时清理Attachments
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;                     // RenderPass结束后不需要Depth
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;                // No stencil
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;              // No Stencil
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;                      // RenderPass开始时的布局，不用管，设置为UNDEFINED
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // 转换成Depth/Stencil

        // 设置Attachments引用
        VkAttachmentReference colorReference{};
        colorReference.attachment = 0;                                                  // Attachment 0是颜色
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;               // 在Subpass中，Attachment布局用于颜色

        VkAttachmentReference depthReference{};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;       // 用作Depth

        // 设置一个Sub Pass引用
        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;                                    // Subpass使用一个Color Attachment
        subpassDescription.pColorAttachments = &colorReference;                         // Color Attachment分配槽位0
        subpassDescription.pDepthStencilAttachment = &depthReference;                   // Depth Attachment分配槽位1
        subpassDescription.inputAttachmentCount = 0;                                    // Input Attachment可以用来采样上一个Subpass的内容
        subpassDescription.pInputAttachments = nullptr;
        subpassDescription.preserveAttachmentCount = 0;
        subpassDescription.pPreserveAttachments = nullptr;
        subpassDescription.pResolveAttachments = nullptr;

        // 设置Subpass依赖
        // 会导致Attachment的隐式资源转换
        // 真正有用的布局是在Attachment Reference中定义的，会一直不变
        // 每个Subpass依赖会在起始subpass和目标subpass之间引起内存和执行依赖，通过srcStageMask，dstStageMask，srcAccessMask和dstAccessMask描述
        std::array<VkSubpassDependency, 2> dependencies{};
        // 对Attachment执行从渲染结束到开始的转换
        // Depth attachment
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;
        // Color attachment
        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].dstSubpass = 0;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].srcAccessMask = 0;
        dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[1].dependencyFlags = 0;

        VkRenderPassCreateInfo renderPassCI{};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCI.attachmentCount = static_cast<uint32_t>(attachments.size());       // RenderPass中用到的Attachments数量
        renderPassCI.pAttachments = attachments.data();                                 // RenderPass用到的Attachments描述
        renderPassCI.subpassCount = 1;                                                  // 当前RenderPass包含的Subpass数量
        renderPassCI.pSubpasses = &subpassDescription;                                  // Subpass描述
        renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassCI.pDependencies = dependencies.data();

        VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderPass));
    }

    // Vulkan只能加载SPIRV格式的Shader，所以要先对Shader进行编译
    VkShaderModule loadSPIRVShader(std::string filename)
    {
        std::ifstream fs(filename, std::ios::binary | std::ios::in | std::ios::ate);
        if (!fs.is_open())
        {
            throw std::runtime_error("Failed to open file!" + filename);
        }

        size_t shaderSize = fs.tellg();
        std::vector<char> shaderCode(shaderSize);

        fs.seekg(0, std::ios::beg);
        fs.read(shaderCode.data(), (long long)shaderSize);
        fs.close();

        // 创建一个ShaderModule用于管线创建
        VkShaderModuleCreateInfo shaderModuleCI{};
        shaderModuleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCI.codeSize = shaderSize;
        shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule;
        VK_CHECK_RESULT(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));

        return shaderModule;
    }

    void preparePipelines()
    {
        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCI.layout = pipelineLayout;
        pipelineCI.renderPass = renderPass;

        // 构建管线的不同阶段
        // Input Assembly阶段描述图元如何组织，例如三角形、线等
        VkPipelineInputAssemblyStateCreateInfo  inputAssemblyStateCI{};
        inputAssemblyStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // 光栅化
        VkPipelineRasterizationStateCreateInfo rasterizationStateCI{};
        rasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
        rasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizationStateCI.depthClampEnable = VK_FALSE;
        rasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
        rasterizationStateCI.depthBiasEnable = VK_FALSE;
        rasterizationStateCI.lineWidth = 1.0f;

        // 混合，需要为每个Color Attachment启用混合阶段，即使不需要混合
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState[1];
        colorBlendAttachmentState[0].colorWriteMask = 0xf;
        colorBlendAttachmentState[0].blendEnable = VK_FALSE;
        VkPipelineColorBlendStateCreateInfo colorBlendStateCI{};
        colorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCI.attachmentCount = 1;
        colorBlendStateCI.pAttachments = colorBlendAttachmentState;

        // Viewport，会被Dynamic State修改
        VkPipelineViewportStateCreateInfo viewportStateCI{};
        viewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCI.viewportCount = 1;
        viewportStateCI.scissorCount = 1;

        // 启用动态阶段，大部分阶段都是写死的，但是有一些是可以通过Command Buffer动态改变的
        // 要启用它需要先指定要改变的阶段，稍后会在Command Buffer中设置，这里我们为了让窗口变化，加入Viewport和Scissor
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicStateCI{};
        dynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateCI.pDynamicStates = dynamicStates.data();

        // 启用深度和模板
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = {};
        depthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCI.depthTestEnable = VK_TRUE;
        depthStencilStateCI.depthWriteEnable = VK_TRUE;
        depthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
        depthStencilStateCI.back.failOp = VK_STENCIL_OP_KEEP;
        depthStencilStateCI.back.passOp = VK_STENCIL_OP_KEEP;
        depthStencilStateCI.back.compareOp = VK_COMPARE_OP_ALWAYS;
        depthStencilStateCI.stencilTestEnable = VK_FALSE;
        depthStencilStateCI.front = depthStencilStateCI.back;

        // MSAA不启用
        VkPipelineMultisampleStateCreateInfo msaaStateCI{};
        msaaStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaaStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        msaaStateCI.pSampleMask = nullptr;

        // Vertex输入，指定顶点输入参数
        VkVertexInputBindingDescription vsiBindCI{};
        vsiBindCI.binding = 0;
        vsiBindCI.stride = sizeof(Vertex);
        vsiBindCI.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        // 输入绑定说明了Shader属性的位置和存储布局
        std::array<VkVertexInputAttributeDescription, 2> vsiAttributes{};
        // 匹配Shader的如下布局
        // layout (location = 0) in vec3 inPos
        // layout (location = 1) in vec3 inColor;
        // Attribute location 0: Position
        vsiAttributes[0].binding = 0;
        vsiAttributes[0].location = 0;
        // Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
        vsiAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        vsiAttributes[0].offset = offsetof(Vertex, position);
        // Attribute location 1: Color
        vsiAttributes[1].binding = 0;
        vsiAttributes[1].location = 1;
        // Color attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
        vsiAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        vsiAttributes[1].offset = offsetof(Vertex, color);

        // 用于创建管线的顶点输入状态
        VkPipelineVertexInputStateCreateInfo vsiStateCI{};
        vsiStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vsiStateCI.vertexBindingDescriptionCount = 1;
        vsiStateCI.pVertexBindingDescriptions = &vsiBindCI;
        vsiStateCI.vertexAttributeDescriptionCount = 2;
        vsiStateCI.pVertexAttributeDescriptions = vsiAttributes.data();

        // Shaders
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

        // Vertex shader
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // Set pipeline stage for this shader
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        // Load binary SPIR-V shader
        shaderStages[0].module = loadSPIRVShader(getAssetPath() + "Shaders/GLSL/TestTriangle/TestTriangleGLSL.vert.spv");
        // Main entry point for the shader
        shaderStages[0].pName = "main";
        assert(shaderStages[0].module != VK_NULL_HANDLE);

        // Fragment shader
        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // Set pipeline stage for this shader
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        // Load binary SPIR-V shader
        shaderStages[1].module = loadSPIRVShader(getAssetPath() + "Shaders/GLSL/TestTriangle/TestTriangleGLSL.frag.spv");
        // Main entry point for the shader
        shaderStages[1].pName = "main";
        assert(shaderStages[1].module != VK_NULL_HANDLE);

        // Set pipeline shader stage info
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();

        // Assign the pipeline states to the pipeline creation info structure
        pipelineCI.pVertexInputState = &vsiStateCI;
        pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
        pipelineCI.pRasterizationState = &rasterizationStateCI;
        pipelineCI.pColorBlendState = &colorBlendStateCI;
        pipelineCI.pMultisampleState = &msaaStateCI;
        pipelineCI.pViewportState = &viewportStateCI;
        pipelineCI.pDepthStencilState = &depthStencilStateCI;
        pipelineCI.pDynamicState = &dynamicStateCI;

        // Create rendering pipeline using the specified states
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

        // Shader modules are no longer needed once the graphics pipeline has been created
        vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
        vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
    }

    void prepareUniformBuffers()
    {
        VkMemoryRequirements memoryReqs;

        // Vertex shader uniform buffer block
        VkBufferCreateInfo bufferInfo = {};
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;
        allocInfo.allocationSize = 0;
        allocInfo.memoryTypeIndex = 0;

        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(ubo);
        // This buffer will be used as a uniform buffer
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffer.buffer));
        // 拿到存储需求
        vkGetBufferMemoryRequirements(device, uniformBuffer.buffer, &memoryReqs);
        allocInfo.allocationSize = memoryReqs.size;
        allocInfo.memoryTypeIndex = getMemoryTypeIndex(
            memoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        // Allocate memory for the uniform buffer
        VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &(uniformBuffer.memory)));
        // Bind memory to buffer
        VK_CHECK_RESULT(vkBindBufferMemory(device, uniformBuffer.buffer, uniformBuffer.memory, 0));

        // Store information in the uniform's descriptor that is used by the descriptor set
        uniformBuffer.descriptor.buffer = uniformBuffer.buffer;
        uniformBuffer.descriptor.offset = 0;
        uniformBuffer.descriptor.range = sizeof(ubo);

        updateUniformBuffers();
    }

    void updateUniformBuffers()
    {
        // Pass matrices to the shaders
        ubo.projectionMat = camera.matrices.perspective;
        ubo.viewMat = camera.matrices.view;
        ubo.modelMat = glm::mat4(1.0f);

        // Map uniform buffer and update it
        uint8_t *pData;
        VK_CHECK_RESULT(vkMapMemory(device, uniformBuffer.memory, 0, sizeof(ubo), 0, (void **)&pData));
        memcpy(pData, &ubo, sizeof(ubo));
        // Unmap after data has been copied
        // Note: Since we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
        vkUnmapMemory(device, uniformBuffer.memory);
    }

public:
    VertexBuffer vertices{};
    IndexBuffer indices{};
    UniformBuffer uniformBuffer{};
    UniformBufferObject ubo{};

    // pipeline layout用于管线访问描述符集
    // 它定义了管线所用到的着色器阶段和着色器资源之间的接口
    // 只要接口匹配，pipeline layout可以在多个管线之间共享
    VkPipelineLayout pipelineLayout{};
    // 管线也就是PSO，用来包含所有影响管线的状态
    VkPipeline pipeline{};

    // 描述符集布局描述了着色器绑定布局，也是一个接口，可以在多个不同的描述符集之间共享
    VkDescriptorSetLayout descriptorSetLayout{};
    // 描述符集存储了绑定在着色器上的资源
    VkDescriptorSet descriptorSet{};

    VkSemaphore presentCompleteSemaphore{};
    VkSemaphore renderCompleteSemaphore{};

    std::vector<VkFence> queueCompleteFences{};
};

TestTriangle* testTriangle;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (testTriangle != nullptr)
    {
        testTriangle->handleMessages(hWnd, uMsg, wParam, lParam);
    }
    return (DefWindowProc(hWnd, uMsg, wParam, lParam));
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    for (size_t i = 0; i < __argc; i++) { TestTriangle::args.push_back(__argv[i]); };
    testTriangle = new TestTriangle();
    testTriangle->initVulkan();
    testTriangle->setupWindow(hInstance, WndProc);
    testTriangle->prepare();
    testTriangle->renderLoop();
    delete(testTriangle);
    return 0;
}