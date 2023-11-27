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

    void EmptyPipelineBarrier(CommandBuffer& cmdBuffer, const ResolveInfo& resolveInfo, const std::unordered_map<std::string, BufferTransition>& bufferTransition, const std::unordered_map<std::string, ImageTransition>& imageTransition)
    {
        vk::PipelineStageFlags pipelineSrcFlags = {};
        vk::PipelineStageFlags pipelineDstFlags = {};

        std::vector<vk::BufferMemoryBarrier> bufferBarriers;
        for (const auto& [bufferName, bufferTransition] : bufferTransition)
        {
            
        }
    }

    RenderGraphBuilder &RenderGraphBuilder::AddRenderPass(const std::string &name, std::unique_ptr<RenderPass> renderPass)
    {
        // TODO: insert return statement here
    }

    RenderGraphBuilder &RenderGraphBuilder::SetOutputName(const std::string &name)
    {
        // TODO: insert return statement here
    }

    std::unique_ptr<RenderGraph> RenderGraphBuilder::Build()
    {
        return std::unique_ptr<RenderGraph>();
    }

    NativeRenderPass RenderGraphBuilder::BuildRenderPass(const RenderPassReference &renderPassReference, const PipelineHashMap &pipelines, const AttachmentHashMap &attachments, const ResourceTransitions &resourceTransitions)
    {
        return NativeRenderPass();
    }

    RenderGraphBuilder::PipelineBarrierCallback RenderGraphBuilder::CreatePipelineBarrierCallback(const std::string &renderPassName, const Pipeline &pipeline, const ResourceTransitions &resourceTransitions)
    {
        return PipelineBarrierCallback();
    }

    RenderGraphBuilder::PresentCallback RenderGraphBuilder::CreatePresentCallback(const std::string &outputName, const ResourceTransitions &transitions)
    {
        return PresentCallback();
    }

    RenderGraphBuilder::CreateCallback RenderGraphBuilder::CreateCreateCallback(const PipelineHashMap &pipelines, const ResourceTransitions &transitions, const AttachmentHashMap &attachments)
    {
        return CreateCallback();
    }

    RenderGraphBuilder::ResourceTransitions RenderGraphBuilder::ResolveResourceTransitions(const PipelineHashMap &pipelines)
    {
        return ResourceTransitions();
    }

    RenderGraphBuilder::AttachmentHashMap RenderGraphBuilder::AllocateAttachments(const PipelineHashMap &pipelines, const ResourceTransitions &transitions)
    {
        return AttachmentHashMap();
    }

    void RenderGraphBuilder::SetupOutputImage(ResourceTransitions &transitions, const std::string &outputImage)
    {
    }
    
    RenderGraphBuilder::PipelineHashMap RenderGraphBuilder::CreatePipelines()
    {
        return PipelineHashMap();
    }

    ImageTransition RenderGraphBuilder::GetOutputImageFinalTransition(const std::string &outputName, const ResourceTransitions &resourceTransitions)
    {
        return ImageTransition();
    }

    std::vector<std::string> RenderGraphBuilder::GetRenderPassAttachmentNames(const std::string &renderPassName, const PipelineHashMap &pipelines)
    {
        return std::vector<std::string>();
    }

    DescriptorBinding RenderGraphBuilder::GetRenderPassDescriptorBinding(const std::string &renderPassName, const PipelineHashMap &pipelines)
    {
        return DescriptorBinding();
    }
}