#pragma once

#include "VKRenderPass.hpp"
#include "VKImGuiContext.hpp"

namespace LeoVK
{
    class ImGuiRenderPass : public RenderPass
    {
    public:
        ImGuiRenderPass(const std::string& outImageName)
            : ImGuiRenderPass(outImageName, AttachmentState::LOAD_COLOR) {}
        ImGuiRenderPass(const std::string& outImageName, AttachmentState onLoad)
            : mOutputImageName(outImageName)
            , mOnLoad(onLoad) {}
        
        virtual void SetupPipeline(PipelineState pipeline) override
        {
            pipeline.AddOutputAttachment(mOutputImageName, mOnLoad);
        }
        virtual void OnRender(RenderPassState state) override
        {
            ImGuiVulkanContext::RenderFrame(state.CommandBuffer.GetNativeCmdBuffer());
        }

    private:
        std::string mOutputImageName;
        AttachmentState mOnLoad;
    };
}