#include "VKUIOverlay.hpp"

namespace LeoVK
{
    UIOverlay::UIOverlay()
    {
        // Init ImGui
        ImGui::CreateContext();
        // Color scheme
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.36f, 0.83f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.36f, 0.83f, 0.8f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.36f, 0.83f, 0.8f, 0.1f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.36f, 0.83f, 0.8f, 0.4f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.36f, 0.83f, 0.5f, 0.4f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.83f, 0.8f, 0.4f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.36f, 0.83f, 0.8f, 0.4f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.36f, 0.83f, 0.8f, 0.8f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.83f, 0.8f, 0.4f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.83f, 0.8f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.36f, 0.83f, 0.8f, 0.4f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.36f, 0.83f, 0.8f, 0.6f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.83f, 0.8f, 0.8f);
        // Dimensions
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = mScale;
    }

    UIOverlay::~UIOverlay()
    {
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
    }

    void UIOverlay::PreparePipeline(
        const VkPipelineCache pipelineCache,
        const VkRenderPass renderPass,
        const VkFormat colorFormat,
        const VkFormat depthFormat)
    {
        // Pipeline layout
        // Push constants for UI rendering parameters
        VkPushConstantRange pushConstantRange = LeoVK::Init::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstantBlock), 0);
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = LeoVK::Init::PipelineLayoutCreateInfo(&mDescSetLayout, 1);
        pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        VK_CHECK(vkCreatePipelineLayout(mpDevice->mLogicalDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout));

        // Setup graphics pipeline for UI rendering
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

        VkPipelineRasterizationStateCreateInfo rasterizationState =
            LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Enable blending
        VkPipelineColorBlendAttachmentState blendAttachmentState{};
        blendAttachmentState.blendEnable = VK_TRUE;
        blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlendState =
            LeoVK::Init::PipelineCBStateCreateInfo(1, &blendAttachmentState);

        VkPipelineDepthStencilStateCreateInfo depthStencilState =
            LeoVK::Init::PipelineDSStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);

        VkPipelineViewportStateCreateInfo viewportState =
            LeoVK::Init::PipelineVPStateCreateInfo(1, 1, 0);

        VkPipelineMultisampleStateCreateInfo multisampleState =
            LeoVK::Init::PipelineMSStateCreateInfo(mMSAA);

        std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState =
            LeoVK::Init::PipelineDYStateCreateInfo(dynamicStateEnables);

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = LeoVK::Init::PipelineCreateInfo(mPipelineLayout, renderPass);

        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState = &colorBlendState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(mShaders.size());
        pipelineCreateInfo.pStages = mShaders.data();
        pipelineCreateInfo.subpass = mSubpass;

#if defined(VK_KHR_dynamic_rendering)
        // SRS - if we are using dynamic rendering (i.e. renderPass null), must define color, depth and stencil attachments at pipeline create time
        VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
        if (renderPass == VK_NULL_HANDLE) {
            pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            pipelineRenderingCreateInfo.colorAttachmentCount = 1;
            pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
            pipelineRenderingCreateInfo.depthAttachmentFormat = depthFormat;
            pipelineRenderingCreateInfo.stencilAttachmentFormat = depthFormat;
            pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
        }
#endif

        // Vertex bindings an attributes based on ImGui vertex definition
        std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            LeoVK::Init::VIBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
        };
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            LeoVK::Init::VIAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
            LeoVK::Init::VIAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
            LeoVK::Init::VIAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
        };
        VkPipelineVertexInputStateCreateInfo viState = LeoVK::Init::PipelineVIStateCreateInfo();
        viState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
        viState.pVertexBindingDescriptions = vertexInputBindings.data();
        viState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
        viState.pVertexAttributeDescriptions = vertexInputAttributes.data();

        pipelineCreateInfo.pVertexInputState = &viState;

        VK_CHECK(vkCreateGraphicsPipelines(mpDevice->mLogicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeline));
    }

    void UIOverlay::PrepareResources()
    {
        ImGuiIO& io = ImGui::GetIO();

        // 创建字体纹理
        unsigned char* fontData;
        int texW, texH;
        const std::string filename = GetAssetsPath() + "Roboto-Medium.ttf";
        io.Fonts->AddFontFromFileTTF(filename.c_str(), 16.0f * mScale);
        io.Fonts->GetTexDataAsRGBA32(&fontData, &texW, &texH);

        VkDeviceSize uploadSize = texW * texH * 4 * sizeof(char);
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(mScale);

        VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageCI.extent = { (uint32_t)texW, (uint32_t)texH, 1 };
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(mpDevice->mLogicalDevice, &imageCI, nullptr, &mFontImage))

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(mpDevice->mLogicalDevice, mFontImage, &memReqs);
        VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = mpDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(mpDevice->mLogicalDevice, &memAllocInfo, nullptr, &mFontMemory))
        VK_CHECK(vkBindImageMemory(mpDevice->mLogicalDevice, mFontImage, mFontMemory, 0))

        // Image view
        VkImageViewCreateInfo viewCI = LeoVK::Init::ImageViewCreateInfo();
        viewCI.image = mFontImage;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.levelCount = 1;
        viewCI.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(mpDevice->mLogicalDevice, &viewCI, nullptr, &mFontView))

        LeoVK::Buffer stagingBuffer;

        VK_CHECK(mpDevice->CreateBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            uploadSize))

        VK_CHECK(stagingBuffer.Map())
        memcpy(stagingBuffer.mpMapped, fontData, uploadSize);
        stagingBuffer.UnMap();

        VkCommandBuffer copyCmd = mpDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Prepare for transfer
        LeoVK::VKTools::SetImageLayout(
            copyCmd, mFontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width = texW;
        bufferCopyRegion.imageExtent.height = texH;
        bufferCopyRegion.imageExtent.depth = 1;

        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer.mBuffer,
            mFontImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion
        );

        // Prepare for shader read
        LeoVK::VKTools::SetImageLayout(
            copyCmd,
            mFontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        mpDevice->FlushCommandBuffer(copyCmd, mQueue, true);

        stagingBuffer.Destroy();

        // Font texture Sampler
        VkSamplerCreateInfo samplerInfo = LeoVK::Init::SamplerCreateInfo();
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(mpDevice->mLogicalDevice, &samplerInfo, nullptr, &mSampler))

        // Descriptor pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            LeoVK::Init::DescPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = LeoVK::Init::DescPoolCreateInfo(poolSizes, 2);
        VK_CHECK(vkCreateDescriptorPool(mpDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &mDescPool));

        // Descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            LeoVK::Init::DescSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = LeoVK::Init::DescSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK(vkCreateDescriptorSetLayout(mpDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescSetLayout));

        // Descriptor set
        VkDescriptorSetAllocateInfo allocInfo = LeoVK::Init::DescSetAllocateInfo(mDescPool, &mDescSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mpDevice->mLogicalDevice, &allocInfo, &mDescSet));
        VkDescriptorImageInfo fontDescriptor = LeoVK::Init::DescImageInfo(
            mSampler,
            mFontView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            LeoVK::Init::WriteDescriptorSet(mDescSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
        };
        vkUpdateDescriptorSets(mpDevice->mLogicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }

    bool UIOverlay::Update()
    {
        ImDrawData* imDrawData = ImGui::GetDrawData();
        bool updateCmdBuffers = false;

        if (!imDrawData) return false;

        VkDeviceSize vbSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
        VkDeviceSize ibSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        // Update buffers only if vertex or index count has been changed compared to current buffer size
        if ((vbSize == 0) || (ibSize == 0))
        {
            return false;
        }

        // VB
        if ((mVertexBuffer.mBuffer == VK_NULL_HANDLE) || (mVertexCount != imDrawData->TotalVtxCount))
        {
            mVertexBuffer.UnMap();
            mVertexBuffer.Destroy();
            VK_CHECK(mpDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mVertexBuffer, vbSize));
            mVertexCount = imDrawData->TotalVtxCount;
            mVertexBuffer.UnMap();
            mVertexBuffer.Map();
            updateCmdBuffers = true;
        }
        // IB
        if ((mIndexBuffer.mBuffer == VK_NULL_HANDLE) || (mIndexCount != imDrawData->TotalVtxCount))
        {
            mIndexBuffer.UnMap();
            mIndexBuffer.Destroy();
            VK_CHECK(mpDevice->CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &mIndexBuffer, ibSize));
            mIndexCount = imDrawData->TotalIdxCount;
            mIndexBuffer.UnMap();
            mIndexBuffer.Map();
            updateCmdBuffers = true;
        }

        // Upload Data to GPU
        auto vtxDst = (ImDrawVert*)mVertexBuffer.mpMapped;
        auto idxDst = (ImDrawIdx*)mIndexBuffer.mpMapped;

        for (int n = 0; n < imDrawData->CmdListsCount; n++)
        {
            const ImDrawList* cmdList = imDrawData->CmdLists[n];
            memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }
        mVertexBuffer.Flush();
        mIndexBuffer.Flush();

        return updateCmdBuffers;
    }

    void UIOverlay::Draw(const VkCommandBuffer cmdBuffer)
    {
        ImDrawData* imDrawData = ImGui::GetDrawData();
        int32_t vertexOffset = 0;
        int32_t indexOffset = 0;

        if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) return;

        ImGuiIO& io = ImGui::GetIO();

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescSet, 0, nullptr);

        mPushConstantBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
        mPushConstantBlock.translate = glm::vec2(-1.0f);
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantBlock), &mPushConstantBlock);

        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &mVertexBuffer.mBuffer, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, mIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT16);

        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
        {
            const ImDrawList* cmdList = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmdList->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];
                VkRect2D scissorRect;
                scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissorRect);
                vkCmdDrawIndexed(cmdBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmdList->VtxBuffer.Size;
        }
    }

    void UIOverlay::Resize(uint32_t width, uint32_t height)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2((float)width, (float)height);
    }

    void UIOverlay::FreeResources()
    {
        mVertexBuffer.Destroy();
        mIndexBuffer.Destroy();
        vkDestroyImageView(mpDevice->mLogicalDevice, mFontView, nullptr);
        vkDestroyImage(mpDevice->mLogicalDevice, mFontImage, nullptr);
        vkFreeMemory(mpDevice->mLogicalDevice, mFontMemory, nullptr);
        vkDestroySampler(mpDevice->mLogicalDevice, mSampler, nullptr);
        vkDestroyDescriptorSetLayout(mpDevice->mLogicalDevice, mDescSetLayout, nullptr);
        vkDestroyDescriptorPool(mpDevice->mLogicalDevice, mDescPool, nullptr);
        vkDestroyPipelineLayout(mpDevice->mLogicalDevice, mPipelineLayout, nullptr);
        vkDestroyPipeline(mpDevice->mLogicalDevice, mPipeline, nullptr);
    }

    bool UIOverlay::Header(const char *caption)
    {
        return ImGui::CollapsingHeader(caption, ImGuiTreeNodeFlags_DefaultOpen);
    }

    bool UIOverlay::CheckBox(const char *caption, bool *value)
    {
        bool res = ImGui::Checkbox(caption, value);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::CheckBox(const char *caption, int32_t *value)
    {
        bool val = (*value == 1);
        bool res = ImGui::Checkbox(caption, &val);
        *value = val;
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::RadioButton(const char *caption, bool value)
    {
        bool res = ImGui::RadioButton(caption, value);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::InputFloat(const char *caption, float *value, float step, uint32_t precision)
    {
        bool res = ImGui::InputFloat(caption, value, step, step * 10.0f, precision);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::SliderFloat(const char *caption, float *value, float min, float max)
    {
        bool res = ImGui::SliderFloat(caption, value, min, max);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::SliderInt(const char *caption, int32_t *value, int32_t min, int32_t max)
    {
        bool res = ImGui::SliderInt(caption, value, min, max);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::ComboBox(const char *caption, int32_t *itemIndex, std::vector<std::string> items)
    {
        if (items.empty())
        {
            return false;
        }

        std::vector<const char*> charItems;
        charItems.reserve(items.size());
        for (const auto & item : items)
        {
            charItems.push_back(item.c_str());
        }
        auto itemCount = static_cast<uint32_t>(charItems.size());
        bool res = ImGui::Combo(caption, itemIndex, &charItems[0], itemCount, itemCount);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::Button(const char *caption)
    {
        bool res = ImGui::Button(caption);
        if (res) mbUpdated = true;

        return res;
    }

    bool UIOverlay::ColorPicker(const char *caption, float *color)
    {
        bool res = ImGui::ColorEdit4(caption, color, ImGuiColorEditFlags_NoInputs);
        if (res) mbUpdated = true;

        return res;
    }

    void UIOverlay::Text(const char *formatStr, ...)
    {
        va_list args;
            va_start(args, formatStr);
        ImGui::TextV(formatStr, args);
            va_end(args);
    }
}
