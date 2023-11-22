#include "VKRenderPass.hpp"
#include "RenderGraph.hpp"

namespace LeoVK
{
    const Image &RenderPassState::GetAttachment(const std::string &name)
    {
        return this->Graph.GetAttachmentByName(name);
    }
}