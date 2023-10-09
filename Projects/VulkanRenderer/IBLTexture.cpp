#include "VulkanRenderer.hpp"

void VulkanRenderer::GenerateBRDFLUT()
{
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
    const int32_t dim = 512;

    //  Image
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
    VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &mTextures.mLUTBRDF.mImage));
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(mDevice, mTextures.mLUTBRDF.mImage, &memReqs);
    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &mTextures.mLUTBRDF.mDeviceMemory));
    VK_CHECK(vkBindImageMemory(mDevice, mTextures.mLUTBRDF.mImage, mTextures.mLUTBRDF.mDeviceMemory, 0));

    // View
    VkImageViewCreateInfo imageViewCI = LeoVK::Init::ImageViewCreateInfo();
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = format;
    imageViewCI.subresourceRange = {};
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.image = mTextures.mLUTBRDF.mImage;
    VK_CHECK(vkCreateImageView(mDevice, &imageViewCI, nullptr, &mTextures.mLUTBRDF.mView));

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
    samplerCI.maxAnisotropy = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK(vkCreateSampler(mDevice, &samplerCI, nullptr, &mTextures.mLUTBRDF.mSampler));

    VkAttachmentDescription attachDesc {};
    attachDesc.format = format;
    attachDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attachDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDesc {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorReference;

    std::array<VkSubpassDependency, 2> dependencies;
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

    VkRenderPassCreateInfo renderPassCI = LeoVK::Init::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attachDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDesc;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(mDevice, &renderPassCI, nullptr, &renderPass));

    VkFramebufferCreateInfo frameBufferCI = LeoVK::Init::FrameBufferCreateInfo();
    frameBufferCI.renderPass = renderPass;
    frameBufferCI.attachmentCount = 1;
    frameBufferCI.pAttachments = &mTextures.mLUTBRDF.mView;
    frameBufferCI.width = dim;
    frameBufferCI.height = dim;
    frameBufferCI.layers = 1;

    VkFramebuffer frameBuffer;
    VK_CHECK(vkCreateFramebuffer(mDevice, &frameBufferCI, nullptr, &frameBuffer));

    // Desriptors
    VkDescriptorSetLayout descSetLayout;
    VkDescriptorSetLayoutCreateInfo descSetLayoutCI {};
    descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descSetLayoutCI, nullptr, &descSetLayout));

    // Pipeline layout
    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo();
    pipelineLayoutCI.pSetLayouts = &descSetLayout;
    VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo iaStateCI = LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rsStateCI = LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState cbAttachState = LeoVK::Init::PipelineCBAState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo cbStateCI = LeoVK::Init::PipelineCBStateCreateInfo(1, &cbAttachState);
    VkPipelineDepthStencilStateCreateInfo dsStateCI = LeoVK::Init::PipelineDSStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo vpStateCI = LeoVK::Init::PipelineVPStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo msStateCI = LeoVK::Init::PipelineMSStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dyStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyStateCI = LeoVK::Init::PipelineDYStateCreateInfo(dyStateEnables.data(), static_cast<uint32_t>(dyStateEnables.size()), 0);
    VkPipelineVertexInputStateCreateInfo viStateCI = LeoVK::Init::PipelineVIStateCreateInfo();

    std::array<VkPipelineShaderStageCreateInfo, 2> ssCI;

    VkGraphicsPipelineCreateInfo pipelineCI = LeoVK::Init::PipelineCreateInfo(pipelineLayout, renderPass);
    pipelineCI.pInputAssemblyState = &iaStateCI;
    pipelineCI.pVertexInputState = &viStateCI;
    pipelineCI.pRasterizationState = &rsStateCI;
    pipelineCI.pColorBlendState = &cbStateCI;
    pipelineCI.pMultisampleState = &msStateCI;
    pipelineCI.pViewportState = &vpStateCI;
    pipelineCI.pDepthStencilState = &dsStateCI;
    pipelineCI.pDynamicState = &dyStateCI;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = ssCI.data();

    ssCI = {
        LoadShader(GetShadersPath() + "VulkanRenderer/GenerateBRDFLUT.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
        LoadShader(GetShadersPath() + "VulkanRenderer/GenerateBRDFLUT.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // for (auto s : ssCI) vkDestroyShaderModule(mDevice, s.module, nullptr);

    // ======================= Start Rendering ======================= //
    VkClearValue clearValues[1];
    clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    VkRenderPassBeginInfo renderPassBI = LeoVK::Init::RenderPassBeginInfo();
    renderPassBI.renderPass = renderPass;
    renderPassBI.renderArea.extent.width = dim;
    renderPassBI.renderArea.extent.height = dim;
    renderPassBI.clearValueCount = 1;
    renderPassBI.pClearValues = clearValues;
    renderPassBI.framebuffer = frameBuffer;

    VkCommandBuffer cmdBuffer = mpVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBI, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport {};
    viewport.width = (float)dim;
    viewport.height = (float)dim;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor {};
    scissor.extent = {dim, dim};

    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuffer);
    mpVulkanDevice->FlushCommandBuffer(cmdBuffer, mQueue);

    vkQueueWaitIdle(mQueue);

    vkDestroyPipeline(mDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(mDevice, renderPass, nullptr);
    vkDestroyFramebuffer(mDevice, frameBuffer, nullptr);
    vkDestroyDescriptorSetLayout(mDevice, descSetLayout, nullptr);

    mTextures.mLUTBRDF.mDescriptor.imageView = mTextures.mLUTBRDF.mView;
    mTextures.mLUTBRDF.mDescriptor.sampler = mTextures.mLUTBRDF.mSampler;
    mTextures.mLUTBRDF.mDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    mTextures.mLUTBRDF.mpDevice = mpVulkanDevice;

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
}

void VulkanRenderer::GenerateCubeMaps()
{
    enum Target { IRRADIANCE = 0, PREFILTEREDENV = 1 };

    for (uint32_t target = 0; target < PREFILTEREDENV + 1; target++) 
    {
        LeoVK::TextureCube cubemap;

        auto tStart = std::chrono::high_resolution_clock::now();

        VkFormat format;
        int32_t dim;

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
        };

        const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

        // Create target cubemap
        {
        	// Image
        	VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
        	imageCI.imageType = VK_IMAGE_TYPE_2D;
        	imageCI.format = format;
        	imageCI.extent.width = dim;
        	imageCI.extent.height = dim;
        	imageCI.extent.depth = 1;
        	imageCI.mipLevels = numMips;
        	imageCI.arrayLayers = 6;
        	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        	imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        	imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        	VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &cubemap.mImage));
        	VkMemoryRequirements memReqs;
        	vkGetImageMemoryRequirements(mDevice, cubemap.mImage, &memReqs);
        	VkMemoryAllocateInfo memAllocInfo = LeoVK::Init::MemoryAllocateInfo();
        	memAllocInfo.allocationSize = memReqs.size;
        	memAllocInfo.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        	VK_CHECK(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &cubemap.mDeviceMemory));
        	VK_CHECK(vkBindImageMemory(mDevice, cubemap.mImage, cubemap.mDeviceMemory, 0));

        	// View
        	VkImageViewCreateInfo viewCI = LeoVK::Init::ImageViewCreateInfo();
        	viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        	viewCI.format = format;
        	viewCI.subresourceRange = {};
        	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        	viewCI.subresourceRange.levelCount = numMips;
        	viewCI.subresourceRange.layerCount = 6;
        	viewCI.image = cubemap.mImage;
        	VK_CHECK(vkCreateImageView(mDevice, &viewCI, nullptr, &cubemap.mView));

        	// Sampler
        	VkSamplerCreateInfo samplerCI = LeoVK::Init::SamplerCreateInfo();
        	samplerCI.magFilter = VK_FILTER_LINEAR;
        	samplerCI.minFilter = VK_FILTER_LINEAR;
        	samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        	samplerCI.minLod = 0.0f;
        	samplerCI.maxLod = static_cast<float>(numMips);
        	samplerCI.maxAnisotropy = 1.0f;
        	samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        	VK_CHECK(vkCreateSampler(mDevice, &samplerCI, nullptr, &cubemap.mSampler));
        }

        // FB, Att, RP, Pipe, etc.
        VkAttachmentDescription attDesc{};
        // Color attachment
        attDesc.format = format;
        attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpassDescription{};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = 1;
        subpassDescription.pColorAttachments = &colorReference;

        // Use subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> dependencies;
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

        // Renderpass
        VkRenderPassCreateInfo renderPassCI{};
        renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCI.attachmentCount = 1;
        renderPassCI.pAttachments = &attDesc;
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpassDescription;
        renderPassCI.dependencyCount = 2;
        renderPassCI.pDependencies = dependencies.data();
        VkRenderPass renderPass;
        VK_CHECK(vkCreateRenderPass(mDevice, &renderPassCI, nullptr, &renderPass));

        struct Offscreen 
        {
        	VkImage image;
        	VkImageView view;
        	VkDeviceMemory memory;
        	VkFramebuffer framebuffer;
        } offscreen;

        // Create offscreen framebuffer
        {
        	// Image
        	VkImageCreateInfo imageCI{};
        	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        	imageCI.imageType = VK_IMAGE_TYPE_2D;
        	imageCI.format = format;
        	imageCI.extent.width = dim;
        	imageCI.extent.height = dim;
        	imageCI.extent.depth = 1;
        	imageCI.mipLevels = 1;
        	imageCI.arrayLayers = 1;
        	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        	imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        	imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        	VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &offscreen.image));
        	VkMemoryRequirements memReqs;
        	vkGetImageMemoryRequirements(mDevice, offscreen.image, &memReqs);
        	VkMemoryAllocateInfo memAllocInfo{};
        	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        	memAllocInfo.allocationSize = memReqs.size;
        	memAllocInfo.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        	VK_CHECK(vkAllocateMemory(mDevice, &memAllocInfo, nullptr, &offscreen.memory));
        	VK_CHECK(vkBindImageMemory(mDevice, offscreen.image, offscreen.memory, 0));

        	// View
        	VkImageViewCreateInfo viewCI{};
        	viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        	viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        	viewCI.format = format;
        	viewCI.flags = 0;
        	viewCI.subresourceRange = {};
        	viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        	viewCI.subresourceRange.baseMipLevel = 0;
        	viewCI.subresourceRange.levelCount = 1;
        	viewCI.subresourceRange.baseArrayLayer = 0;
        	viewCI.subresourceRange.layerCount = 1;
        	viewCI.image = offscreen.image;
        	VK_CHECK(vkCreateImageView(mDevice, &viewCI, nullptr, &offscreen.view));

        	// Framebuffer
        	VkFramebufferCreateInfo framebufferCI{};
        	framebufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        	framebufferCI.renderPass = renderPass;
        	framebufferCI.attachmentCount = 1;
        	framebufferCI.pAttachments = &offscreen.view;
        	framebufferCI.width = dim;
        	framebufferCI.height = dim;
        	framebufferCI.layers = 1;
        	VK_CHECK(vkCreateFramebuffer(mDevice, &framebufferCI, nullptr, &offscreen.framebuffer));

        	VkCommandBuffer layoutCmd = mpVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        	VkImageMemoryBarrier imageMemoryBarrier{};
        	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        	imageMemoryBarrier.image = offscreen.image;
        	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        	imageMemoryBarrier.srcAccessMask = 0;
        	imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        	imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        	vkCmdPipelineBarrier(layoutCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        	mpVulkanDevice->FlushCommandBuffer(layoutCmd, mQueue, true);
        }

        // Descriptors
        VkDescriptorSetLayout descSetLayout;
        VkDescriptorSetLayoutBinding setLayoutBinding = { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
        VkDescriptorSetLayoutCreateInfo descSetLayoutCI = LeoVK::Init::DescSetLayoutCreateInfo(&setLayoutBinding, 1);
        VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descSetLayoutCI, nullptr, &descSetLayout));

        // Descriptor Pool
        VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
        VkDescriptorPoolCreateInfo descPoolCI = LeoVK::Init::DescPoolCreateInfo(1, &poolSize, 2);
        VkDescriptorPool descPool;
        VK_CHECK(vkCreateDescriptorPool(mDevice, &descPoolCI, nullptr, &descPool));

        // Descriptor sets
        VkDescriptorSet descSet;
        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = LeoVK::Init::DescSetAllocateInfo(descPool, &descSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(mDevice, &descriptorSetAllocInfo, &descSet));
        VkWriteDescriptorSet writeDescSet = LeoVK::Init::WriteDescriptorSet(descSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &mTextures.mEnvCube.mDescriptor);
        vkUpdateDescriptorSets(mDevice, 1, &writeDescSet, 0, nullptr);

        struct PushBlockIrradiance 
        {
        	glm::mat4 mvp;
        	float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
        	float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
        } pushBlockIrradiance;

        struct PushBlockPrefilterEnv 
        {
        	glm::mat4 mvp;
        	float roughness;
        	uint32_t numSamples = 32u;
        } pushBlockPrefilterEnv;

        // Pipeline layout
        VkPipelineLayout pipelinelayout;
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        switch (target) 
        {
        	case IRRADIANCE:
        		pushConstantRange.size = sizeof(PushBlockIrradiance);
        		break;
        	case PREFILTEREDENV:
        		pushConstantRange.size = sizeof(PushBlockPrefilterEnv);
        		break;
        };

        VkPipelineLayoutCreateInfo pipelineLayoutCI = LeoVK::Init::PipelineLayoutCreateInfo();
        pipelineLayoutCI.setLayoutCount = 1;
        pipelineLayoutCI.pSetLayouts = &descSetLayout;
        pipelineLayoutCI.pushConstantRangeCount = 1;
        pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
        VK_CHECK(vkCreatePipelineLayout(mDevice, &pipelineLayoutCI, nullptr, &pipelinelayout));

        VkPipelineInputAssemblyStateCreateInfo iaStateCI = LeoVK::Init::PipelineIAStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineRasterizationStateCreateInfo rsStateCI = LeoVK::Init::PipelineRSStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        VkPipelineColorBlendAttachmentState cbAttachState = LeoVK::Init::PipelineCBAState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
        VkPipelineColorBlendStateCreateInfo cbStateCI = LeoVK::Init::PipelineCBStateCreateInfo(1, &cbAttachState);
        VkPipelineDepthStencilStateCreateInfo dsStateCI = LeoVK::Init::PipelineDSStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
        VkPipelineViewportStateCreateInfo vpStateCI = LeoVK::Init::PipelineVPStateCreateInfo(1, 1);
        VkPipelineMultisampleStateCreateInfo msStateCI = LeoVK::Init::PipelineMSStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        std::vector<VkDynamicState> dyStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyStateCI = LeoVK::Init::PipelineDYStateCreateInfo(dyStateEnables.data(), static_cast<uint32_t>(dyStateEnables.size()), 0);

        const std::vector<VkVertexInputBindingDescription> viBindings = {
            LeoVK::Init::VIBindingDescription(0, sizeof(LeoVK::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
        };
        const std::vector<VkVertexInputAttributeDescription> viAttributes = {
            LeoVK::Init::VIAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LeoVK::Vertex, mPos)),
        };
        VkPipelineVertexInputStateCreateInfo viStateCI = LeoVK::Init::PipelineVIStateCreateInfo(viBindings, viAttributes);

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCI.layout = pipelinelayout;
        pipelineCI.renderPass = renderPass;
        pipelineCI.pInputAssemblyState = &iaStateCI;
        pipelineCI.pVertexInputState = &viStateCI;
        pipelineCI.pRasterizationState = &rsStateCI;
        pipelineCI.pColorBlendState = &cbStateCI;
        pipelineCI.pMultisampleState = &msStateCI;
        pipelineCI.pViewportState = &vpStateCI;
        pipelineCI.pDepthStencilState = &dsStateCI;
        pipelineCI.pDynamicState = &dyStateCI;
        pipelineCI.stageCount = 2;
        pipelineCI.pStages = shaderStages.data();
        pipelineCI.renderPass = renderPass;

        shaderStages[0] = LoadShader(GetShadersPath() + "Base/FilterCube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        switch (target) 
        {
            case IRRADIANCE:
                shaderStages[1] = LoadShader(GetShadersPath() + "Base/IrradianceCube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
                break;
            case PREFILTEREDENV:
                shaderStages[1] = LoadShader(GetShadersPath() + "Base/PrefilterEnvMap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
                break;
        };

        VkPipeline pipeline;
        VK_CHECK(vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCI, nullptr, &pipeline));
        // for (auto ss : shaderStages) 
        // {
        // 	vkDestroyShaderModule(mDevice, ss.module, nullptr);
        // }

        // Render cubemap
        VkClearValue clearValues[1];
        clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

        VkRenderPassBeginInfo renderPassBeginInfo = LeoVK::Init::RenderPassBeginInfo();
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.framebuffer = offscreen.framebuffer;
        renderPassBeginInfo.renderArea.extent.width = dim;
        renderPassBeginInfo.renderArea.extent.height = dim;
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = clearValues;

        std::vector<glm::mat4> matrices = 
        {
        	glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        	glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        	glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        	glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        	glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
        	glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        };

        VkCommandBuffer cmdBuffer = mpVulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

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
        	mpVulkanDevice->BeginCommandBuffer(cmdBuffer);
        	VkImageMemoryBarrier imageMemoryBarrier{};
        	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        	imageMemoryBarrier.image = cubemap.mImage;
        	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        	imageMemoryBarrier.srcAccessMask = 0;
        	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        	imageMemoryBarrier.subresourceRange = subresourceRange;
        	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        	mpVulkanDevice->FlushCommandBuffer(cmdBuffer, mQueue, false);
        }

        for (uint32_t m = 0; m < numMips; m++) 
        {
            for (uint32_t f = 0; f < 6; f++) 
            {
                mpVulkanDevice->BeginCommandBuffer(cmdBuffer);

                viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
                viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
                vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
                vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

                // Render scene from cube face's point of view
                vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

                // Pass parameters for current pass using a push constant block
                switch (target) 
                {
                    case IRRADIANCE:
                        pushBlockIrradiance.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
                        vkCmdPushConstants(cmdBuffer, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlockIrradiance), &pushBlockIrradiance);
                        break;
                    case PREFILTEREDENV:
                        pushBlockPrefilterEnv.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];
                        pushBlockPrefilterEnv.roughness = (float)m / (float)(numMips - 1);
                        vkCmdPushConstants(cmdBuffer, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlockPrefilterEnv), &pushBlockPrefilterEnv);
                        break;
                };

                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descSet, 0, nullptr);

                VkDeviceSize offsets[1] = { 0 };

                mScenes.mSkybox.Draw(cmdBuffer);

                vkCmdEndRenderPass(cmdBuffer);

                VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                subresourceRange.baseMipLevel = 0;
                subresourceRange.levelCount = numMips;
                subresourceRange.layerCount = 6;

                {
                    VkImageMemoryBarrier imageMemoryBarrier{};
                    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    imageMemoryBarrier.image = offscreen.image;
                    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
                }

                // Copy region for transfer from framebuffer to cube face
                VkImageCopy copyRegion{};

                copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.srcSubresource.baseArrayLayer = 0;
                copyRegion.srcSubresource.mipLevel = 0;
                copyRegion.srcSubresource.layerCount = 1;
                copyRegion.srcOffset = { 0, 0, 0 };

                copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.dstSubresource.baseArrayLayer = f;
                copyRegion.dstSubresource.mipLevel = m;
                copyRegion.dstSubresource.layerCount = 1;
                copyRegion.dstOffset = { 0, 0, 0 };

                copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
                copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
                copyRegion.extent.depth = 1;

                vkCmdCopyImage(
                    cmdBuffer,
                    offscreen.image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    cubemap.mImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);

                {
                    VkImageMemoryBarrier imageMemoryBarrier{};
                    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    imageMemoryBarrier.image = offscreen.image;
                    imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                    imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
                }

                mpVulkanDevice->FlushCommandBuffer(cmdBuffer, mQueue, false);
            }
        }
        {
            mpVulkanDevice->BeginCommandBuffer(cmdBuffer);
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.image = cubemap.mImage;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            mpVulkanDevice->FlushCommandBuffer(cmdBuffer, mQueue, false);
        }


        vkDestroyRenderPass(mDevice, renderPass, nullptr);
        vkDestroyFramebuffer(mDevice, offscreen.framebuffer, nullptr);
        vkFreeMemory(mDevice, offscreen.memory, nullptr);
        vkDestroyImageView(mDevice, offscreen.view, nullptr);
        vkDestroyImage(mDevice, offscreen.image, nullptr);
        vkDestroyDescriptorPool(mDevice, descPool, nullptr);
        vkDestroyDescriptorSetLayout(mDevice, descSetLayout, nullptr);
        vkDestroyPipeline(mDevice, pipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, pipelinelayout, nullptr);
        
        cubemap.mDescriptor.imageView = cubemap.mView;
        cubemap.mDescriptor.sampler = cubemap.mSampler;
        cubemap.mDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        cubemap.mpDevice = mpVulkanDevice;
        
        switch (target) 
        {
        	case IRRADIANCE:
        		mTextures.mIrradianceCube = cubemap;
        		break;
        	case PREFILTEREDENV:
        		mTextures.mPreFilteredCube = cubemap;
        		mUBOParams.mPrefilteredCubeMipLevels = static_cast<float>(numMips);
        		break;
        };
        
        auto tEnd = std::chrono::high_resolution_clock::now();
        auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
        std::cout << "Generating cube map with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
    }
}