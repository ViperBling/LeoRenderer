#include "TestRenderer.hpp"

TestRenderer::TestRenderer() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "Test Renderer";
    mWidth = 1280;
    mHeight = 720;
    mCamera.mType = CameraType::LookAt;
    mCamera.mbFlipY = true;
    mCamera.SetPosition(glm::vec3(0.0f, -0.1f, -1.0f));
    mCamera.SetRotation(glm::vec3(0.0f, 45.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);
}

TestRenderer::~TestRenderer()
{

}

void TestRenderer::GetEnabledFeatures()
{
    VKRendererBase::GetEnabledFeatures();
}

void TestRenderer::BuildCommandBuffers()
{
    VKRendererBase::BuildCommandBuffers();
}

void TestRenderer::Render()
{

}

void TestRenderer::ViewChanged()
{
    VKRendererBase::ViewChanged();
}

void TestRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    VKRendererBase::OnUpdateUIOverlay(overlay);
}

void TestRenderer::LoadScene(std::string filename)
{

}

void TestRenderer::LoadAssets()
{

}

void TestRenderer::SetupDescriptors()
{

}

void TestRenderer::PreparePipelines()
{

}

void TestRenderer::PrepareUniformBuffers()
{

}

void TestRenderer::UpdateUniformBuffers()
{

}

void TestRenderer::PreRender()
{

}
