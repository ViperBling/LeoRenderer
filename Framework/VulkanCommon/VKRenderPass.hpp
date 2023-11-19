#pragma once

#include "VKCommandBuffer.hpp"
#include "VKPipeline.hpp"

namespace LeoVK
{
    class RenderGraph;

    struct NativeRenderPass
    {
        vk::RenderPass              RenderPassHandle;
        vk::DescriptorSet           DescriptorSet;
        vk::Framebuffer             Framebuffer;
        vk::Pipeline                Pipeline;
        vk::PipelineLayout          PipelineLayout;
        vk::PipelineBindPoint       PipelineType = { };
        vk::Rect2D                  RenderArea = { };
        std::vector<vk::ClearValue> ClearValues;
    };

    struct RenderPassState
    {
        RenderGraph& Graph;
        CommandBuffer& CommandBuffer;
        const NativeRenderPass& RenderPass;
        const Image& GetAttachment(const std::string& name);
    };

    using PipelineState = Pipeline&;
    using ResolveState = ResolveInfo&;

    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

        virtual void SetupPipeline(PipelineState state) { }
        virtual void ResolveResources(ResolveState resolve) { }

        virtual void BeforeRender(RenderPassState state) { }
        virtual void OnRender(RenderPassState state) { }
        virtual void AfterRender(RenderPassState state) { }
    };
}