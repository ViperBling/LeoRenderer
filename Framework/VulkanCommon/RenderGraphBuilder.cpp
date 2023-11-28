#include "RenderGraphBuilder.hpp"
#include "VKCommandBuffer.hpp"
#include "VKContext.hpp"
#include "VKShaderGraphic.hpp"
#include "VKShaderCompute.hpp"

namespace LeoVK
{
    vk::VertexInputRate VertexBindingRateToVertexInputRate(VertexBinding::Rate rate)
    {
        switch (rate)
        {
        case VertexBinding::Rate::PER_VERTEX:
            return vk::VertexInputRate::eVertex;
        case VertexBinding::Rate::PER_INSTANCE:
            return vk::VertexInputRate::eInstance;
        default:
            assert(false);
            return vk::VertexInputRate::eVertex;
        }
    }

    vk::ShaderStageFlags PipelineTypeToShaderStages(vk::PipelineBindPoint type)
    {
        switch (type)
        {
        case vk::PipelineBindPoint::eGraphics:
            return vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
        case vk::PipelineBindPoint::eCompute:
            return vk::ShaderStageFlagBits::eCompute;
        default:
            assert(false);
            return vk::ShaderStageFlags {};
        }
    }

    vk::AttachmentLoadOp AttachmentStateToLoadOp(AttachmentState state)
    {
        switch (state)
        {
        case AttachmentState::DISCARD_COLOR:
            return vk::AttachmentLoadOp::eDontCare;
        case AttachmentState::DISCARD_DEPTH_SPENCIL:
            return vk::AttachmentLoadOp::eDontCare;
        case AttachmentState::LOAD_COLOR:
            return vk::AttachmentLoadOp::eLoad;
        case AttachmentState::LOAD_DEPTH_STENCIL:
            return vk::AttachmentLoadOp::eLoad;
        case AttachmentState::CLEAR_COLOR:
            return vk::AttachmentLoadOp::eClear;
        case AttachmentState::CLEAR_DEPTH_SPENCIL:
            return vk::AttachmentLoadOp::eClear;
        default:
            assert(false);
            return vk::AttachmentLoadOp::eDontCare;
        }
    }

    ImageUsage::Bits AttachmentStateToImageUsage(AttachmentState state)
    {
        switch (state)
        {
        case AttachmentState::DISCARD_COLOR:
            return ImageUsage::COLOR_ATTACHMENT;
        case AttachmentState::DISCARD_DEPTH_SPENCIL:
            return ImageUsage::DEPTH_STENCIL_ATTACHMENT;
        case AttachmentState::LOAD_COLOR:
            return ImageUsage::COLOR_ATTACHMENT;
        case AttachmentState::LOAD_DEPTH_STENCIL:
            return ImageUsage::DEPTH_STENCIL_ATTACHMENT;
        case AttachmentState::CLEAR_COLOR:
            return ImageUsage::COLOR_ATTACHMENT;
        case AttachmentState::CLEAR_DEPTH_SPENCIL:
            return ImageUsage::DEPTH_STENCIL_ATTACHMENT;
        default:
            assert(false);
            return ImageUsage::COLOR_ATTACHMENT;
        }
    }

    vk::PipelineStageFlags BufferUsageToPipelineStage(BufferUsage::Bits layout)
    {
        switch (layout)
        {
        case BufferUsage::UNKNOWN:
            return vk::PipelineStageFlagBits::eTopOfPipe;
        case BufferUsage::TRANSFER_SOURCE:
            return vk::PipelineStageFlagBits::eTransfer;
        case BufferUsage::TRANSFER_DESTINATION:
            return vk::PipelineStageFlagBits::eTransfer;
        case BufferUsage::UNIFORM_TEXEL_BUFFER:
            return vk::PipelineStageFlagBits::eVertexShader; // TODO: from other shader stages?
        case BufferUsage::STORAGE_TEXEL_BUFFER:
            return vk::PipelineStageFlagBits::eComputeShader;
        case BufferUsage::UNIFORM_BUFFER:
            return vk::PipelineStageFlagBits::eVertexShader;
        case BufferUsage::STORAGE_BUFFER:
            return vk::PipelineStageFlagBits::eComputeShader;
        case BufferUsage::INDEX_BUFFER:
            return vk::PipelineStageFlagBits::eVertexInput;
        case BufferUsage::VERTEX_BUFFER:
            return vk::PipelineStageFlagBits::eVertexInput;
        case BufferUsage::INDIRECT_BUFFER:
            return vk::PipelineStageFlagBits::eDrawIndirect;
        case BufferUsage::SHADER_DEVICE_ADDRESS:
            return vk::PipelineStageFlagBits::eFragmentShader; // TODO: what should be here?
        case BufferUsage::TRANSFORM_FEEDBACK_BUFFER:
            return vk::PipelineStageFlagBits::eTransformFeedbackEXT;
        case BufferUsage::TRANSFORM_FEEDBACK_COUNTER_BUFFER:
            return vk::PipelineStageFlagBits::eTransformFeedbackEXT;
        case BufferUsage::CONDITIONAL_RENDERING:
            return vk::PipelineStageFlagBits::eConditionalRenderingEXT;
        case BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_READONLY:
            return vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR;
        case BufferUsage::ACCELERATION_STRUCTURE_STORAGE:
            return vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR; // TODO: what should be here?
        case BufferUsage::SHADER_BINDING_TABLE:
            return vk::PipelineStageFlagBits::eFragmentShader; // TODO: what should be here?
        default:
            assert(false);
            return vk::PipelineStageFlags{ };
        }
    }

    vk::AccessFlags BufferUsageToAccessFlags(BufferUsage::Bits layout)
    {
        switch (layout)
        {
        case BufferUsage::UNKNOWN:
            return vk::AccessFlags{ };
        case BufferUsage::TRANSFER_SOURCE:
            return vk::AccessFlagBits::eTransferRead;
        case BufferUsage::TRANSFER_DESTINATION:
            return vk::AccessFlagBits::eTransferWrite;
        case BufferUsage::UNIFORM_TEXEL_BUFFER:
            return vk::AccessFlagBits::eShaderRead;
        case BufferUsage::STORAGE_TEXEL_BUFFER:
            return vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
        case BufferUsage::UNIFORM_BUFFER:
            return vk::AccessFlagBits::eShaderRead;
        case BufferUsage::STORAGE_BUFFER:
            return vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
        case BufferUsage::INDEX_BUFFER:
            return vk::AccessFlagBits::eIndexRead;
        case BufferUsage::VERTEX_BUFFER:
            return vk::AccessFlagBits::eVertexAttributeRead;
        case BufferUsage::INDIRECT_BUFFER:
            return vk::AccessFlagBits::eIndirectCommandRead;
        case BufferUsage::SHADER_DEVICE_ADDRESS:
            return vk::AccessFlagBits::eShaderRead;
        case BufferUsage::TRANSFORM_FEEDBACK_BUFFER:
            return vk::AccessFlagBits::eTransformFeedbackWriteEXT;
        case BufferUsage::TRANSFORM_FEEDBACK_COUNTER_BUFFER:
            return vk::AccessFlagBits::eTransformFeedbackCounterWriteEXT;
        case BufferUsage::CONDITIONAL_RENDERING:
            return vk::AccessFlagBits::eConditionalRenderingReadEXT;
        case BufferUsage::ACCELERATION_STRUCTURE_BUILD_INPUT_READONLY:
            return vk::AccessFlagBits::eAccelerationStructureReadKHR;
        case BufferUsage::ACCELERATION_STRUCTURE_STORAGE:
            return vk::AccessFlagBits::eAccelerationStructureReadKHR;
        case BufferUsage::SHADER_BINDING_TABLE:
            return vk::AccessFlagBits::eShaderRead;
        default:
            assert(false);
            return vk::AccessFlags{ };
        }
    }

    bool HasImageWriteDependency(ImageUsage::Bits usage)
    {
        switch (usage)
        {
        case ImageUsage::TRANSFER_DESTINATION:
        case ImageUsage::STORAGE:
        case ImageUsage::COLOR_ATTACHMENT:
        case ImageUsage::DEPTH_STENCIL_ATTACHMENT:
        case ImageUsage::FRAGMENT_SHADING_RATE_ATTACHMENT:
            return true;
        default:
            return false;
        }
    }

    bool HasBufferWriteDependency(BufferUsage::Bits usage)
    {
        switch (usage)
        {
        case BufferUsage::TRANSFER_DESTINATION:
        case BufferUsage::UNIFORM_TEXEL_BUFFER:
        case BufferUsage::STORAGE_TEXEL_BUFFER:
        case BufferUsage::STORAGE_BUFFER:
        case BufferUsage::TRANSFORM_FEEDBACK_BUFFER:
        case BufferUsage::TRANSFORM_FEEDBACK_COUNTER_BUFFER:
        case BufferUsage::ACCELERATION_STRUCTURE_STORAGE:
            return true;
        default:
            return false;
        }
    }

    vk::ImageMemoryBarrier CreateImageMemoryBarrier(vk::Image image, ImageUsage::Bits oldUsage, ImageUsage::Bits newUsage, Format format, uint32_t mipLevelCount, uint32_t layerCount)
    {
        vk::ImageSubresourceRange subresourceRange;
        subresourceRange.setAspectMask(ImageFormatToImageAspect(format));
        subresourceRange.setBaseArrayLayer(0);
        subresourceRange.setBaseMipLevel(0);
        subresourceRange.setLayerCount(layerCount);
        subresourceRange.setLevelCount(mipLevelCount);

        vk::ImageMemoryBarrier imageBarrier;
        imageBarrier.setImage(image);
        imageBarrier.setOldLayout(ImageUsageToImageLayout(oldUsage));
        imageBarrier.setNewLayout(ImageUsageToImageLayout(newUsage));
        imageBarrier.setSrcAccessMask(ImageUsageToAccessFlags(oldUsage));
        imageBarrier.setDstAccessMask(ImageUsageToAccessFlags(newUsage));
        imageBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        imageBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        imageBarrier.setSubresourceRange(subresourceRange);

        return imageBarrier;
    }

    vk::BufferMemoryBarrier CreateBufferMemoryBarrier(vk::Buffer buffer, BufferUsage::Bits oldUsage, BufferUsage::Bits newUsage)
    {
        vk::BufferMemoryBarrier bufferBarrier;
        bufferBarrier.setBuffer(buffer);
        bufferBarrier.setSrcAccessMask(BufferUsageToAccessFlags(oldUsage));
        bufferBarrier.setDstAccessMask(BufferUsageToAccessFlags(newUsage));
        bufferBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        bufferBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        bufferBarrier.setSize(VK_WHOLE_SIZE);
        bufferBarrier.setOffset(0);

        return bufferBarrier;
    }

    void EmitPipelineBarrier(CommandBuffer& cmdBuffer, const ResolveInfo& resolveInfo, const std::unordered_map<std::string, BufferTransition>& bufferTransitions, const std::unordered_map<std::string, ImageTransition>& imageTransitions)
    {
        vk::PipelineStageFlags pipelineSrcFlags = {};
        vk::PipelineStageFlags pipelineDstFlags = {};

        std::vector<vk::BufferMemoryBarrier> bufferBarriers;
        for (const auto& [bufferName, bufferTransition] : bufferTransitions)
        {
            if (!HasBufferWriteDependency(bufferTransition.InitialUsage)) continue;

            pipelineSrcFlags |= BufferUsageToPipelineStage(bufferTransition.InitialUsage);
            pipelineDstFlags |= BufferUsageToPipelineStage(bufferTransition.FinalUsage);

            auto buffers = resolveInfo.GetBuffers().at(bufferName);
            for (const auto& buffer : buffers)
            {
                bufferBarriers.push_back(CreateBufferMemoryBarrier(buffer.get().GetNativeBuffer(), bufferTransition.InitialUsage, bufferTransition.FinalUsage));
            }
        }
        std::vector<vk::ImageMemoryBarrier> imageBarriers;
        for (const auto& [imageName, imageTransition] : imageTransitions)
        {
            if (imageTransition.InitialUsage == imageTransition.FinalUsage && !HasImageWriteDependency(imageTransition.InitialUsage)) continue;

            pipelineSrcFlags |= ImageUsageToPipelineStage(imageTransition.InitialUsage);
            pipelineDstFlags |= ImageUsageToPipelineStage(imageTransition.FinalUsage);

            auto images = resolveInfo.GetImages().at(imageName);
            for (const auto& image : images)
            {
                imageBarriers.push_back(CreateImageMemoryBarrier(image.get().GetNativeImage(), imageTransition.InitialUsage, imageTransition.FinalUsage, image.get().GetFormat(), image.get().GetMipLevelCount(), image.get().GetLayerCount()));
            }
        }

        if (bufferBarriers.empty() && imageBarriers.empty()) return;

        cmdBuffer.GetNativeCmdBuffer().pipelineBarrier(pipelineSrcFlags, pipelineDstFlags, {}, {}, bufferBarriers, imageBarriers);
    }

    static void DefaultPresentCallback(CommandBuffer&, const Image&, const Image&) { }

    static vk::Pipeline CreateComputePipeline(const Shader& shader, const vk::PipelineLayout& layout)
    {
        vk::PipelineShaderStageCreateInfo ssCI;
        ssCI.setStage(ToNative(ShaderType::COMPUTE));
        ssCI.setModule(shader.GetNativeShaderModule(ShaderType::COMPUTE));
        ssCI.setPName("main");

        vk::ComputePipelineCreateInfo pipelineCI {};
        pipelineCI.setStage(ssCI);
        pipelineCI.setLayout(layout);
        pipelineCI.setBasePipelineHandle(vk::Pipeline{});
        pipelineCI.setBasePipelineIndex(0);

        return GetCurrentVulkanContext().GetDevice().createComputePipeline(vk::PipelineCache{}, pipelineCI).value;
    }

    static vk::Pipeline CreateGraphicPipeline(const Shader& shader, const vk::PipelineLayout& layout, ArrayView<const VertexBinding> vertexBindings, const vk::RenderPass& renderPass)
    {
        std::array ssCIs = {
            vk::PipelineShaderStageCreateInfo {
                vk::PipelineShaderStageCreateFlags {},
                ToNative(ShaderType::VERTEX),
                shader.GetNativeShaderModule(ShaderType::VERTEX),
                "main"
            },
            vk::PipelineShaderStageCreateInfo {
                vk::PipelineShaderStageCreateFlags {},
                ToNative(ShaderType::FRAGMENT),
                shader.GetNativeShaderModule(ShaderType::FRAGMENT),
                "main"
            }
        };

        auto vertexAttributes = shader.GetInputAttributes();

        std::vector<vk::VertexInputBindingDescription> vtBindingDescs;
        std::vector<vk::VertexInputAttributeDescription> vtAttributeDescs;
        uint32_t vtBindingID = 0, vtBindingOffset = 0;
        uint32_t attributeLocationID = 0, attributeLocationOffset = 0;

        for (const auto& attribute : vertexAttributes)
        {
            for (size_t i = 0; i < attribute.ComponentCount; i++)
            {
                vtAttributeDescs.push_back(
                    vk::VertexInputAttributeDescription {
                        attributeLocationID,
                        vtBindingID,
                        ToNative(attribute.LayoutFormat),
                        attributeLocationOffset
                    }
                );
                attributeLocationID++;
                vtBindingOffset += attribute.ByteSize / attribute.ComponentCount;
            }
            if (attributeLocationID == attributeLocationOffset + vertexBindings[vtBindingID].BindingRange)
            {
                vtBindingDescs.push_back(
                    vk::VertexInputBindingDescription {
                        vtBindingID,
                        vtBindingOffset,
                        VertexBindingRateToVertexInputRate(vertexBindings[vtBindingID].InputRate)
                    }
                );
                vtBindingID++;
                attributeLocationOffset = attributeLocationID;
                vtBindingOffset = 0;    // vertex binding offset is local to binding
            }
        }
        // move everything else to last vertex buffer
        if (attributeLocationID != attributeLocationOffset)
        {
            vtBindingDescs.push_back(
                vk::VertexInputBindingDescription {
                    vtBindingID,
                    vtBindingOffset,
                    VertexBindingRateToVertexInputRate(vertexBindings[vtBindingID].InputRate)
                }
            );
        }

        vk::PipelineVertexInputStateCreateInfo viStateCI {};
        viStateCI.setVertexBindingDescriptions(vtBindingDescs);
        viStateCI.setVertexAttributeDescriptions(vtAttributeDescs);

        vk::PipelineInputAssemblyStateCreateInfo iaStateCI {};
        iaStateCI.setPrimitiveRestartEnable(false);
        iaStateCI.setTopology(vk::PrimitiveTopology::eTriangleList);

        vk::PipelineViewportStateCreateInfo vpStateCI {};
        vpStateCI.setViewportCount(1);
        vpStateCI.setScissorCount(1);

        vk::PipelineRasterizationStateCreateInfo rsStateCI {};
        rsStateCI.setPolygonMode(vk::PolygonMode::eFill);
        rsStateCI.setCullMode(vk::CullModeFlagBits::eBack);
        rsStateCI.setFrontFace(vk::FrontFace::eCounterClockwise);
        rsStateCI.setLineWidth(1.0f);

        vk::PipelineMultisampleStateCreateInfo msStateCI {};
        msStateCI.setRasterizationSamples(vk::SampleCountFlagBits::e1);
        msStateCI.setMinSampleShading(1.0f);

        vk::PipelineColorBlendAttachmentState cbAttachState {};
        cbAttachState.setBlendEnable(true);
        cbAttachState.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
        cbAttachState.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
        cbAttachState.setColorBlendOp(vk::BlendOp::eAdd);
        cbAttachState.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        cbAttachState.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        cbAttachState.setAlphaBlendOp(vk::BlendOp::eAdd);
        cbAttachState.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

        vk::PipelineColorBlendStateCreateInfo cbStateCI {};
        cbStateCI.setLogicOpEnable(false);
        cbStateCI.setLogicOp(vk::LogicOp::eCopy);
        cbStateCI.setAttachments(cbAttachState);
        cbStateCI.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

        std::array dyStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        vk::PipelineDepthStencilStateCreateInfo dsStateCI {};
        dsStateCI.setStencilTestEnable(false);
        dsStateCI.setDepthTestEnable(true);
        dsStateCI.setDepthWriteEnable(true);
        dsStateCI.setDepthBoundsTestEnable(false);
        dsStateCI.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
        dsStateCI.setMinDepthBounds(0.0f);
        dsStateCI.setMaxDepthBounds(1.0f);

        vk::PipelineDynamicStateCreateInfo dyStateCI {};
        dyStateCI.setDynamicStates(dyStates);

        vk::GraphicsPipelineCreateInfo pipelineCI {};
        pipelineCI.setStages(ssCIs);
        pipelineCI.setPVertexInputState(&viStateCI);
        pipelineCI.setPInputAssemblyState(&iaStateCI);
        pipelineCI.setPViewportState(&vpStateCI);
        pipelineCI.setPRasterizationState(&rsStateCI);
        pipelineCI.setPMultisampleState(&msStateCI);
        pipelineCI.setPDepthStencilState(&dsStateCI);
        pipelineCI.setPColorBlendState(&cbStateCI);
        pipelineCI.setPDynamicState(&dyStateCI);
        pipelineCI.setLayout(layout);
        pipelineCI.setRenderPass(renderPass);
        pipelineCI.setSubpass(0);
        pipelineCI.setBasePipelineHandle(vk::Pipeline{});
        pipelineCI.setBasePipelineIndex(0);

        return GetCurrentVulkanContext().GetDevice().createGraphicsPipeline(vk::PipelineCache{}, pipelineCI).value;
    }

    static vk::PipelineLayout CreatePipelineLayout(const vk::DescriptorSetLayout& descSetLayout, vk::PipelineBindPoint pipelineType)
    {
        vk::PushConstantRange pushConstRange;
        pushConstRange.setStageFlags(PipelineTypeToShaderStages(pipelineType));
        pushConstRange.setOffset(0);
        pushConstRange.setSize(128);

        vk::PipelineLayoutCreateInfo layoutCI {};
        layoutCI.setSetLayouts(descSetLayout);
        layoutCI.setPushConstantRanges(pushConstRange);

        return GetCurrentVulkanContext().GetDevice().createPipelineLayout(layoutCI);
    }

    RenderGraphBuilder &RenderGraphBuilder::AddRenderPass(const std::string &name, std::unique_ptr<RenderPass> renderPass)
    {
        this->mRenderPassRefs.push_back({ name, std::move(renderPass) });
        return *this;
    }

    RenderGraphBuilder &RenderGraphBuilder::SetOutputName(const std::string &name)
    {
        this->mOutputName = name;
        return *this;
    }

    std::unique_ptr<RenderGraph> RenderGraphBuilder::Build()
    {
        PipelineHashMap pipelines = this->CreatePipelines();
        ResourceTransitions resourceTransitions = this->ResolveResourceTransitions(pipelines);
        if (!this->mOutputName.empty())
        {
            this->SetupOutputImage(resourceTransitions, this->mOutputName);
        }
        AttachmentHashMap attachments = this->AllocateAttachments(pipelines, resourceTransitions);

        std::vector<RenderGraphNode> nodes;

        for (auto& renderPassRef : this->mRenderPassRefs)
        {
            auto renderPass = this->BuildRenderPass(renderPassRef, pipelines, attachments, resourceTransitions);
            nodes.push_back(RenderGraphNode {
                renderPassRef.Name,
                renderPass,
                std::move(renderPassRef.Pass),
                this->GetRenderPassAttachmentNames(renderPassRef.Name, pipelines),
                this->CreatePipelineBarrierCallback(renderPassRef.Name, pipelines.at(renderPassRef.Name), resourceTransitions),
                this->GetRenderPassDescriptorBinding(renderPassRef.Name, pipelines)
            });
        }

        auto OnCreate = this->CreateCreateCallback(pipelines, resourceTransitions, attachments);
        auto OnPresent = !this->mOutputName.empty() ? this->CreatePresentCallback(this->mOutputName, resourceTransitions) : DefaultPresentCallback;

        return std::make_unique<RenderGraph>(std::move(nodes), std::move(attachments), std::move(this->mOutputName),  std::move(OnPresent), std::move(OnCreate));
    }

    NativeRenderPass RenderGraphBuilder::BuildRenderPass(const RenderPassReference &renderPassReference, const PipelineHashMap &pipelines, const AttachmentHashMap &attachments, const ResourceTransitions &resourceTransitions)
    {
        NativeRenderPass nativePass;

        std::vector<vk::AttachmentDescription> attachDescs;
        std::vector<vk::AttachmentReference> attachRefs;
        std::vector<vk::ImageView> attachViews;

        uint32_t renderAreaWidth = 0, renderAreaHeight = 0;

        auto& pass = pipelines.at(renderPassReference.Name);
        auto& renderPassAttachments = pass.GetOutputAttachments();
        auto& imageTransitions = resourceTransitions.Images.Transitions.at(renderPassReference.Name);

        if (!renderPassAttachments.empty())
        {
            vk::AttachmentReference dsAttachmentRef;
            for (size_t attachmentIndex = 0; attachmentIndex < renderPassAttachments.size(); attachmentIndex++)
            {
                const auto& attachment = renderPassAttachments[attachmentIndex];
                const auto& imageRef = attachments.at(attachment.Name);
                const auto& attachmentTransition = imageTransitions.at(attachment.Name);

                if (renderAreaWidth == 0 && renderAreaHeight == 0)
                {
                    renderAreaWidth = std::max(renderAreaWidth, (uint32_t)imageRef.GetWidth());
                    renderAreaHeight = std::max(renderAreaHeight, (uint32_t)imageRef.GetHeight());
                }
                vk::AttachmentDescription attachmentDesc;
                attachmentDesc.setFormat(ToNative(imageRef.GetFormat()));
                attachmentDesc.setSamples(vk::SampleCountFlagBits::e1);
                attachmentDesc.setLoadOp(AttachmentStateToLoadOp(attachment.OnLoad));
                attachmentDesc.setStoreOp(vk::AttachmentStoreOp::eStore);
                attachmentDesc.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
                attachmentDesc.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
                attachmentDesc.setInitialLayout(ImageUsageToImageLayout(attachmentTransition.InitialUsage));
                attachmentDesc.setFinalLayout(ImageUsageToImageLayout(attachmentTransition.FinalUsage));

                attachDescs.push_back(std::move(attachmentDesc));
                if (attachment.Layer == Pipeline::OutputAttachment::ALL_LAYERS)
                {
                    attachViews.push_back(imageRef.GetNativeView(ImageView::NATIVE));
                }
                else
                {
                    attachViews.push_back(imageRef.GetNativeView(ImageView::NATIVE, attachment.Layer));
                }

                vk::AttachmentReference attachmentRef;
                attachmentRef.setAttachment(attachmentIndex);
                attachmentRef.setLayout(ImageUsageToImageLayout(attachmentTransition.FinalUsage));

                if (attachmentTransition.FinalUsage == ImageUsage::DEPTH_STENCIL_ATTACHMENT)
                {
                    dsAttachmentRef = std::move(attachmentRef);
                    nativePass.ClearValues.push_back(vk::ClearDepthStencilValue {
                        attachment.DepthSpencilClear.Depth,
                        attachment.DepthSpencilClear.Stencil
                    });
                }
                else
                {
                    attachRefs.push_back(std::move(attachmentRef));
                    nativePass.ClearValues.push_back(vk::ClearColorValue {
                        std::array { attachment.ColorClear.R, attachment.ColorClear.G, attachment.ColorClear.B, attachment.ColorClear.A }
                    });
                }
            }

            vk::SubpassDescription subpassDesc;
            subpassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
            subpassDesc.setColorAttachments(attachRefs);
            subpassDesc.setPDepthStencilAttachment(dsAttachmentRef != vk::AttachmentReference {} ? std::addressof(dsAttachmentRef) : nullptr);

            std::array subpassDependencies = {
                vk::SubpassDependency {
                    VK_SUBPASS_EXTERNAL,
                    0,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::AccessFlagBits::eMemoryRead,
                    vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::DependencyFlagBits::eByRegion
                },
                vk::SubpassDependency {
                    0,
                    VK_SUBPASS_EXTERNAL,
                    vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                    vk::PipelineStageFlagBits::eBottomOfPipe,
                    vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                    vk::AccessFlagBits::eMemoryRead,
                    vk::DependencyFlagBits::eByRegion
                }
            };

            vk::RenderPassCreateInfo renderPassCI {};
            renderPassCI.setAttachments(attachDescs);
            renderPassCI.setSubpasses(subpassDesc);
            renderPassCI.setDependencies(subpassDependencies);

            vk::RenderPassMultiviewCreateInfo passMultiviewCI {};
            if (!renderPassAttachments.empty())
            {
                auto& layerAttachment = renderPassAttachments.front();
                uint32_t layerCount = attachments.at(layerAttachment.Name).GetLayerCount();

                if (layerCount > 1 && layerAttachment.Layer == Pipeline::OutputAttachment::ALL_LAYERS)
                {
                    uint32_t viewMask = (1u << layerCount) - 1;
                    passMultiviewCI.setSubpassCount(1);
                    passMultiviewCI.setViewMasks(viewMask);
                    passMultiviewCI.setCorrelationMasks(viewMask);
                    renderPassCI.setPNext(&passMultiviewCI);
                }
            }
            nativePass.RenderPassHandle = GetCurrentVulkanContext().GetDevice().createRenderPass(renderPassCI);

            vk::FramebufferCreateInfo fbCI {};
            fbCI.setRenderPass(nativePass.RenderPassHandle);
            fbCI.setAttachments(attachViews);
            fbCI.setWidth(renderAreaWidth);
            fbCI.setHeight(renderAreaHeight);
            fbCI.setLayers(1);

            nativePass.Framebuffer = GetCurrentVulkanContext().GetDevice().createFramebuffer(fbCI);
            nativePass.RenderArea = vk::Rect2D { vk::Offset2D { 0, 0 }, vk::Extent2D { renderAreaWidth, renderAreaHeight } };
        }

        if (dynamic_cast<GraphicShader*>(pass.mShader.get()) != nullptr)
        {
            nativePass.PipelineType = vk::PipelineBindPoint::eGraphics;
        }
        else if (dynamic_cast<ComputeShader*>(pass.mShader.get()) != nullptr)
        {
            nativePass.PipelineType = vk::PipelineBindPoint::eCompute;
        }

        if ((bool)pass.mShader)
        {
            auto desc = GetCurrentVulkanContext().GetDescriptorCache().GetDescriptor(pass.mShader->GetShaderUniforms());
            nativePass.DescriptorSet = desc.DescSet;
            nativePass.PipelineLayout = CreatePipelineLayout(desc.DescSetLayout, nativePass.PipelineType);

            if (nativePass.PipelineType == vk::PipelineBindPoint::eGraphics)
            {
                nativePass.Pipeline = CreateGraphicPipeline(*pass.mShader, nativePass.PipelineLayout, pass.mVertexBindings, nativePass.RenderPassHandle);
            }
            if (nativePass.PipelineType == vk::PipelineBindPoint::eCompute)
            {
                nativePass.Pipeline = CreateComputePipeline(*pass.mShader, nativePass.PipelineLayout);
            }
        }
        return nativePass;
    }

    RenderGraphBuilder::PipelineBarrierCallback RenderGraphBuilder::CreatePipelineBarrierCallback(const std::string &renderPassName, const Pipeline &pipeline, const ResourceTransitions &resourceTransitions)
    {
        auto& bufferTransitions = resourceTransitions.Buffers.Transitions.at(renderPassName);
        auto& imageTransitions = resourceTransitions.Images.Transitions.at(renderPassName);

        return [bufferTransitions, imageTransitions](CommandBuffer& commandBuffer, const ResolveInfo& resolveInfo)
        {
            EmitPipelineBarrier(commandBuffer, resolveInfo, bufferTransitions, imageTransitions);
        };
    }

    RenderGraphBuilder::PresentCallback RenderGraphBuilder::CreatePresentCallback(const std::string &outputName, const ResourceTransitions &transitions)
    {
        auto outImageTransition = this->GetOutputImageFinalTransition(this->mOutputName, transitions);
        return [outImageTransition](CommandBuffer& commandBuffer, const Image& outImage, const Image& presentImage)
        {
            commandBuffer.BlitImage(outImage, outImageTransition.InitialUsage, presentImage, ImageUsage::UNKNOWN, BlitFilter::LINEAR);
            auto subresourceRange = GetDefaultImageSubresourceRange(outImage);
            if (outImageTransition.FinalUsage != ImageUsage::TRANSFER_SOURCE)
            {
                commandBuffer.GetNativeCmdBuffer().pipelineBarrier(
                    vk::PipelineStageFlagBits::eTransfer,
                    ImageUsageToPipelineStage(outImageTransition.FinalUsage),
                    {},
                    {},
                    {},
                    vk::ImageMemoryBarrier {
                        vk::AccessFlagBits::eTransferRead,
                        ImageUsageToAccessFlags(outImageTransition.FinalUsage),
                        vk::ImageLayout::eTransferSrcOptimal,
                        ImageUsageToImageLayout(outImageTransition.FinalUsage),
                        VK_QUEUE_FAMILY_IGNORED,
                        VK_QUEUE_FAMILY_IGNORED,
                        outImage.GetNativeImage(),
                        subresourceRange
                    }
                );
            }
        };
    }

    RenderGraphBuilder::CreateCallback RenderGraphBuilder::CreateCreateCallback(const PipelineHashMap &pipelines, const ResourceTransitions &transitions, const AttachmentHashMap &attachments)
    {
        ResolveInfo resolveInfo;
        std::unordered_map<std::string, ImageTransition> attachTransitions;
        // 遍历管线中绑定的renderpass
        for (const auto& [renderPassName, pipeline] : pipelines)
        {
            for (const auto& attachment : pipeline.GetOutputAttachments())
            {
                auto& attachTransition = transitions.Images.Transitions.at(renderPassName).at(attachment.Name);
                if (transitions.Images.FirstUsages.at(attachment.Name) == renderPassName)
                {
                    attachTransitions[attachment.Name] = ImageTransition { ImageUsage::UNKNOWN, attachTransition.InitialUsage };
                    resolveInfo.Resolve(attachment.Name, attachments.at(attachment.Name));
                }
            }
        }
        return [resolve = std::move(resolveInfo), transitions = std::move(attachTransitions)](CommandBuffer& commandBuffer)
        {
            EmitPipelineBarrier(commandBuffer, resolve, {}, transitions);
        };
    }

    RenderGraphBuilder::ResourceTransitions RenderGraphBuilder::ResolveResourceTransitions(const PipelineHashMap &pipelines)
    {
        ResourceTransitions resTransition;
        std::unordered_map<std::string, BufferUsage::Bits> lastBufferUsages;
        std::unordered_map<std::string, ImageUsage::Bits> lastImageUsages;

        for (const auto& renderPassRef : this->mRenderPassRefs)
        {
            auto& pipeline = pipelines.at(renderPassRef.Name);
            auto& bufferTransitions = resTransition.Buffers.Transitions[renderPassRef.Name];
            auto& imageTransitions = resTransition.Images.Transitions[renderPassRef.Name];

            for (const auto& bufferDep : pipeline.GetBufferDependencies())
            {
                if (lastBufferUsages.find(bufferDep.Name) == lastBufferUsages.end())
                {
                    resTransition.Buffers.FirstUsages[bufferDep.Name] = renderPassRef.Name;
                    lastBufferUsages[bufferDep.Name] = BufferUsage::UNKNOWN;
                }
                auto lastBufferUsage = lastBufferUsages.at(bufferDep.Name);
                bufferTransitions[bufferDep.Name].InitialUsage = lastBufferUsage;
                bufferTransitions[bufferDep.Name].FinalUsage = bufferDep.Usage;
                resTransition.Buffers.TotalUsages[bufferDep.Name] |= bufferDep.Usage;
                lastBufferUsages[bufferDep.Name] = bufferDep.Usage;
                resTransition.Buffers.LastUsages[bufferDep.Name] = renderPassRef.Name;
            }

            for (const auto& imageDep : pipeline.GetImageDependencies())
            {
                if (lastImageUsages.find(imageDep.Name) == lastImageUsages.end())
                {
                    resTransition.Images.FirstUsages[imageDep.Name] = renderPassRef.Name;
                    lastImageUsages[imageDep.Name] = ImageUsage::UNKNOWN;
                }
                auto lastImageUsage = lastImageUsages.at(imageDep.Name);
                imageTransitions[imageDep.Name].InitialUsage = lastImageUsage;
                imageTransitions[imageDep.Name].FinalUsage = imageDep.Usage;
                resTransition.Images.TotalUsages[imageDep.Name] |= imageDep.Usage;
                lastImageUsages[imageDep.Name] = imageDep.Usage;
                resTransition.Images.LastUsages[imageDep.Name] = renderPassRef.Name;
            }

            for (const auto& attachmentDep : pipeline.GetOutputAttachments())
            {
                auto attachDepUsage = AttachmentStateToImageUsage(attachmentDep.OnLoad);

                if (lastImageUsages.find(attachmentDep.Name) == lastImageUsages.end())
                {
                    resTransition.Images.FirstUsages[attachmentDep.Name] = renderPassRef.Name;
                    lastImageUsages[attachmentDep.Name] = ImageUsage::UNKNOWN;
                }
                auto lastImageUsage = lastImageUsages.at(attachmentDep.Name);
                imageTransitions[attachmentDep.Name].InitialUsage = lastImageUsage;
                imageTransitions[attachmentDep.Name].FinalUsage = attachDepUsage;
                resTransition.Images.TotalUsages[attachmentDep.Name] |= attachDepUsage;
                lastImageUsages[attachmentDep.Name] = attachDepUsage;
                resTransition.Images.LastUsages[attachmentDep.Name] = renderPassRef.Name;
            }
        }

        for (const auto& [bufferNativeHandle, renderPassName] : resTransition.Buffers.FirstUsages)
        {
            auto& firstBufferTransition = resTransition.Buffers.Transitions[renderPassName][bufferNativeHandle];
            firstBufferTransition.InitialUsage = lastBufferUsages[bufferNativeHandle];
        }
        for (const auto& [imageNativeHandle, renderPassName] : resTransition.Images.FirstUsages)
        {
            auto& firstImageTransition = resTransition.Images.Transitions[renderPassName][imageNativeHandle];
            firstImageTransition.InitialUsage = lastImageUsages[imageNativeHandle];
        }
        return resTransition;
    }

    RenderGraphBuilder::AttachmentHashMap RenderGraphBuilder::AllocateAttachments(const PipelineHashMap &pipelines, const ResourceTransitions &transitions)
    {
        AttachmentHashMap attachments;

        auto [surfaceWidth, surfaceHeight] = GetCurrentVulkanContext().GetSurfaceExtent();

        for (const auto& [renderPassName, pipeline] : pipelines)
        {
            auto& attachmentDeclarations = pipeline.GetAttachmentDeclarations();
            for (const auto& attachment : attachmentDeclarations)
            {
                auto attachmentUsage = transitions.Images.TotalUsages.at(attachment.Name);

                attachments.emplace(attachment.Name, Image(
                    attachment.Width == 0 ? surfaceWidth : attachment.Width,
                    attachment.Height == 0 ? surfaceHeight : attachment.Height,
                    attachment.ImageFormat,
                    attachmentUsage,
                    MemoryUsage::GPU_ONLY,
                    attachment.Options
                ));
            }
        }
        return attachments;
    }

    void RenderGraphBuilder::SetupOutputImage(ResourceTransitions &transitions, const std::string &outputImage)
    {
        transitions.Images.TotalUsages.at(outputImage) |= ImageUsage::TRANSFER_SOURCE;
    }
    
    RenderGraphBuilder::PipelineHashMap RenderGraphBuilder::CreatePipelines()
    {
        PipelineHashMap pipelines;
        for (const auto& renderPassReference : this->mRenderPassRefs)
        {  
            auto& pipeline = pipelines[renderPassReference.Name];
            renderPassReference.Pass->SetupPipeline(pipeline);

            for (const auto& boundBuffer : pipeline.mDescBindings.GetBoundBuffers())
            {
                pipeline.AddDependency(boundBuffer.Name, boundBuffer.Usage);
            }
            for (const auto& boundImage : pipeline.mDescBindings.GetBoundImages())
            {
                pipeline.AddDependency(boundImage.Name, boundImage.Usage);
            }
        }
        return pipelines;
    }

    ImageTransition RenderGraphBuilder::GetOutputImageFinalTransition(const std::string &outputName, const ResourceTransitions &resourceTransitions)
    {
        const auto& firstRenderPassName = resourceTransitions.Images.FirstUsages.at(outputName);
        const auto& lastRenderPassName = resourceTransitions.Images.LastUsages.at(outputName);

        ImageTransition transition;
        transition.InitialUsage = resourceTransitions.Images.Transitions.at(lastRenderPassName).at(outputName).FinalUsage;
        transition.FinalUsage = resourceTransitions.Images.Transitions.at(firstRenderPassName).at(outputName).InitialUsage;
        return transition;
    }

    std::vector<std::string> RenderGraphBuilder::GetRenderPassAttachmentNames(const std::string &renderPassName, const PipelineHashMap &pipelines)
    {
        std::vector<std::string> attachmentNames;
        auto& outputAttachments = pipelines.at(renderPassName).GetOutputAttachments();

        attachmentNames.reserve(outputAttachments.size());
        for (const auto& outputAttachment : outputAttachments)
            attachmentNames.push_back(outputAttachment.Name);

        return attachmentNames;
    }

    DescriptorBinding RenderGraphBuilder::GetRenderPassDescriptorBinding(const std::string &renderPassName, const PipelineHashMap &pipelines)
    {
        return pipelines.at(renderPassName).mDescBindings;
    }
}