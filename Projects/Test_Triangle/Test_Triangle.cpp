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

    ~TestTriangle()
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
    virtual void Render()
    {

    }

    virtual void ViewChanged()
    {

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
    void buildCommandBuffers() final
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