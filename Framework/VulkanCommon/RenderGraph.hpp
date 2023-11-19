#pragma once

#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "VKRenderPass.hpp"
#include "VKImage.hpp"
#include "VKCommandBuffer.hpp"

namespace LeoVK
{
    struct RenderGraphNode
    {
        std::string Name;
        NativeRenderPass NativePass;
        std::unique_ptr<RenderPass> PassCustom;
        std::vector<std::string> UsedAttachments;
        std::function<void(CommandBuffer&, const ResolveInfo&)> PipelineBarrierCallback;
        DescriptorBinding Descriptors;
    };

    class RenderGraph
    {
    private:
        using PresentCallback = std::function<void(CommandBuffer&, const Image&, const Image&)>;
        using CreateCallback = std::function<void(CommandBuffer&)>;

    public:
        RenderGraph(std::vector<RenderGraphNode> nodes, std::unordered_map<std::string, Image> attachments, const std::string& outputName, PresentCallback onPresent, CreateCallback onCreate);
        ~RenderGraph();
        RenderGraph(RenderGraph&&) = default;
        RenderGraph& operator=(RenderGraph&& other) = delete;

        void ExecuteRenderGraphNode(RenderGraphNode& node, CommandBuffer& commandBuffer, ResolveInfo& resolve);
        void Execute(CommandBuffer& commandBuffer);
        void Present(CommandBuffer& commandBuffer, const Image& presentImage);
        const RenderGraphNode& GetNodeByName(const std::string& name) const;
        RenderGraphNode& GetNodeByName(const std::string& name);
        const Image& GetAttachmentByName(const std::string& name) const;

        template<typename T>
        T& GetRenderPassByName(const std::string& name)
        {
            auto& node = this->GetNodeByName(name);
            assert(dynamic_cast<T*>(node.PassCustom.get()) != nullptr);
            return *static_cast<T*>(node.PassCustom.get());
        }

        template<typename T>
        const T& GetRenderPassByName(const std::string& name) const
        {
            const auto& node = this->GetNodeByName(name);
            assert(dynamic_cast<const T*>(node.PassCustom.get()) != nullptr);
            return *static_cast<const T*>(node.PassCustom.get());
        }

    private:
        void InitializeOnFirstFrame(CommandBuffer& commandBuffer);

    private:
        std::vector<RenderGraphNode> mNodes;
        std::unordered_map<std::string, Image> mAttachments;
        std::string mOutputName;
        PresentCallback mOnPresent;
        CreateCallback mOnCreate;
    };
}