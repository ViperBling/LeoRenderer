//
// Created by Administrator on 2023/6/13.
//

#include "PBRRenderer.h"

PBRRenderer::PBRRenderer() : VulkanFramework(ENABLE_VALIDATION)
{
    title = "PBRRenderer";

}

PBRRenderer::~PBRRenderer()
{


}

void PBRRenderer::GetEnabledFeatures()
{
    VulkanFramework::GetEnabledFeatures();
}

void PBRRenderer::RenderNode(LeoRenderer::Node *node, uint32_t cbIndex, LeoRenderer::Material::AlphaMode alphaMode)
{

}

void PBRRenderer::LoadScene(std::string)
{

}

void PBRRenderer::LoadEnvironment(std::string)
{

}

void PBRRenderer::LoadAssets()
{

}

void PBRRenderer::SetupNodeDescriptorSet(LeoRenderer::Node *node)
{

}

void PBRRenderer::SetDescriptors()
{

}

void PBRRenderer::PreparePipelines()
{

}

void PBRRenderer::GenerateBRDFLUT()
{

}

void PBRRenderer::GenerateCubeMaps()
{

}

void PBRRenderer::PrepareUniformBuffers()
{

}

void PBRRenderer::UpdateUniformBuffers()
{

}

void PBRRenderer::UpdateParams()
{

}

void PBRRenderer::BuildCommandBuffers()
{
    VulkanFramework::BuildCommandBuffers();
}

void PBRRenderer::Prepare()
{
    VulkanFramework::Prepare();
}

void PBRRenderer::Render()
{

}

void PBRRenderer::ViewChanged()
{
    VulkanFramework::ViewChanged();
}

void PBRRenderer::WindowResized()
{
    VulkanFramework::WindowResized();
}

void PBRRenderer::OnUpdateUIOverlay(vks::UIOverlay *overlay)
{
    VulkanFramework::OnUpdateUIOverlay(overlay);
}

void PBRRenderer::FileDropped(std::string &filename)
{
    VulkanFramework::FileDropped(filename);
}
