#include "RenderGraph.hpp"
#include "VKContext.hpp"
#include "VKCommandBuffer.hpp"

namespace LeoVK
{
    RenderGraph::RenderGraph(std::vector<RenderGraphNode> nodes, std::unordered_map<std::string, Image> attachments, const std::string &outputName, PresentCallback onPresent, CreateCallback onCreate)
        : mNodes(std::move(nodes))
        , mAttachments(std::move(attachments))
        , mOutputName(outputName)
        , mOnPresent(std::move(onPresent))
        , mOnCreate(std::move(onCreate))
    {
    }

    RenderGraph::~RenderGraph()
    {
        auto& vulkan = GetCurrentVulkanContext();
        auto& device = vulkan.GetDevice();
        device.waitIdle();

        for (const auto& node : this->mNodes)
        {
            auto& pass = node.NativePass;
            if ((bool)pass.Pipeline) device.destroyPipeline(pass.Pipeline);
            if ((bool)pass.PipelineLayout) device.destroyPipelineLayout(pass.PipelineLayout);
            if ((bool)pass.Framebuffer) device.destroyFramebuffer(pass.Framebuffer);
            if ((bool)pass.RenderPassHandle) device.destroyRenderPass(pass.RenderPassHandle);
        }
    }

    void RenderGraph::ExecuteRenderGraphNode(RenderGraphNode &node, CommandBuffer &commandBuffer, ResolveInfo &resolve)
    {
        RenderPassState state { *this, commandBuffer, node.NativePass };

        node.PassCustom->ResolveResources(resolve);
        node.Descriptors.Resolve(resolve);
        node.Descriptors.Write(node.NativePass.DescriptorSet);

        node.PassCustom->BeforeRender(state);
        node.PipelineBarrierCallback(commandBuffer, resolve);

        commandBuffer.BeginPass(node.NativePass);
        node.PassCustom->OnRender(state);
        commandBuffer.EndPass(node.NativePass);

        node.PassCustom->AfterRender(state);
    }

    void RenderGraph::Execute(CommandBuffer &commandBuffer)
    {
        this->InitializeOnFirstFrame(commandBuffer);

        ResolveInfo resolve;
        for (const auto& [attachmentName, attachment] : this->mAttachments)
        {
            resolve.Resolve(attachmentName, attachment);
        }
        for (auto& node : this->mNodes)
        {
            this->ExecuteRenderGraphNode(node, commandBuffer, resolve);
        }
    }

    void RenderGraph::Present(CommandBuffer &commandBuffer, const Image &presentImage)
    {
        this->mOnPresent(commandBuffer, this->mAttachments.at(this->mOutputName), presentImage);
    }

    RenderGraphNode &RenderGraph::GetNodeByName(const std::string &name)
    {
        auto it = std::find_if(this->mNodes.begin(), this->mNodes.end(), [&name](const RenderGraphNode& node) { return node.Name == name; });
        assert(it != this->mNodes.end());
        return *it;
    }
    
    const RenderGraphNode &RenderGraph::GetNodeByName(const std::string &name) const
    {
        auto it = std::find_if(this->mNodes.begin(), this->mNodes.end(), [&name](const RenderGraphNode& node) { return node.Name == name; });
        assert(it != this->mNodes.end());
        return *it;
    }

    const Image &RenderGraph::GetAttachmentByName(const std::string &name) const
    {
        return this->mAttachments.at(name);
    }

    void RenderGraph::InitializeOnFirstFrame(CommandBuffer &commandBuffer)
    {
        if ((bool)this->mOnCreate)
        {
            this->mOnCreate(commandBuffer);
            this->mOnCreate = {};
        }
    }
}