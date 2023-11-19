#pragma once

#include <vector>
#include <string>
#include <array>
#include <optional>
#include <unordered_map>

#include "RenderGraph.hpp"
#include "VKShader.hpp"
#include "VKDescriptorBinding.hpp"

namespace LeoVK
{
    struct ImageTransition
    {
        ImageUsage::Bits InitialUsage;
        ImageUsage::Bits FinalUsage;
    };

    struct BufferTransition
    {
        BufferUsage::Bits InitialUsage;
        BufferUsage::Bits FinalUsage;
    };

    class RenderGraphBuilder
    {
    private:
        struct RenderPassReference
        {
            std::string Name;
            std::unique_ptr<RenderPass> Pass;
        };

        using RenderPassName = std::string;

        template<typename ResourceType, typename TransitionType>
        struct ResourceTypeTransitions
        {
            using TransitionHashMap = std::unordered_map<ResourceType, TransitionType>;
            using PerRenderPassTransitionHashMap = std::unordered_map<RenderPassName, TransitionHashMap>;
            using UsageMask = uint32_t;

            PerRenderPassTransitionHashMap Transitions;
            std::unordered_map<ResourceType, UsageMask> TotalUsages;
            std::unordered_map<ResourceType, RenderPassName> FirstUsages;
            std::unordered_map<ResourceType, RenderPassName> LastUsages;
        };

        struct ResourceTransitions
        {
            ResourceTypeTransitions<std::string, BufferTransition> Buffers;
            ResourceTypeTransitions<std::string, ImageTransition> Images;
        };

        using AttachmentHashMap = std::unordered_map<std::string, Image>;
        using PipelineHashMap = std::unordered_map<RenderPassName, Pipeline>;
        using PipelineBarrierCallback = std::function<void(CommandBuffer&, const ResolveInfo&)>;
        using PresentCallback = std::function<void(CommandBuffer&, const Image&, const Image&)>;
        using CreateCallback = std::function<void(CommandBuffer&)>;

    public:
        RenderGraphBuilder& AddRenderPass(const std::string& name, std::unique_ptr<RenderPass> renderPass);
        RenderGraphBuilder& SetOutputName(const std::string& name);
        std::unique_ptr<RenderGraph> Build();

    private:
        NativeRenderPass BuildRenderPass(const RenderPassReference& renderPassReference, const PipelineHashMap& pipelines, const AttachmentHashMap& attachments, const ResourceTransitions& resourceTransitions);
        PipelineBarrierCallback CreatePipelineBarrierCallback(const std::string& renderPassName, const Pipeline& pipeline, const ResourceTransitions& resourceTransitions);
        PresentCallback CreatePresentCallback(const std::string& outputName, const ResourceTransitions& transitions);
        CreateCallback CreateCreateCallback(const PipelineHashMap& pipelines, const ResourceTransitions& transitions, const AttachmentHashMap& attachments);
        ResourceTransitions ResolveResourceTransitions(const PipelineHashMap& pipelines);
        AttachmentHashMap AllocateAttachments(const PipelineHashMap& pipelines, const ResourceTransitions& transitions);
        void SetupOutputImage(ResourceTransitions& transitions, const std::string& outputImage);
        PipelineHashMap CreatePipelines();
        ImageTransition GetOutputImageFinalTransition(const std::string& outputName, const ResourceTransitions& resourceTransitions);
        std::vector<std::string> GetRenderPassAttachmentNames(const std::string& renderPassName, const PipelineHashMap& pipelines);
        DescriptorBinding GetRenderPassDescriptorBinding(const std::string& renderPassName, const PipelineHashMap& pipelines);

    private:
        std::vector<RenderPassReference> mRenderPassRefs;
        std::string mOutputName;
    };
}