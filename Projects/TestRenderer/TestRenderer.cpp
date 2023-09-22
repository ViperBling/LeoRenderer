#include "TestRenderer.hpp"

TestRenderer::TestRenderer() : VKRendererBase(ENABLE_MSAA, ENABLE_VALIDATION)
{
    mTitle = "Test Render";
    mCamera.mType = CameraType::LookAt;
    mCamera.SetPosition(glm::vec3(0.0f, 0.0f, -10.0f));
    mCamera.SetRotation(glm::vec3(-75.0f, 72.0f, 0.0f));
    mCamera.SetPerspective(60.0f, (float)mWidth / (float)mHeight, 0.1f, 256.0f);
    mEnabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    mEnabledDeviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    mEnabledDeviceExtensions.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
	mEnabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);
	mEnabledDeviceExtensions.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
	mEnabledDeviceExtensions.push_back(VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME);
}

TestRenderer::~TestRenderer()
{
    if (mDevice)
    {
        vkDestroyPipeline(mDevice, mPipeline, nullptr);
        vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
        vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
        mUniformBuffer.Destroy();
    }
}

void TestRenderer::GetEnabledFeatures()
{
    // Enable anisotropic filtering if supported
	if (mDeviceFeatures.samplerAnisotropy) 
    {
		mDeviceFeatures.samplerAnisotropy = VK_TRUE;
	};

	mDynamicRenderingFeaturesKHR.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	mDynamicRenderingFeaturesKHR.dynamicRendering = VK_TRUE;

	mpDeviceCreatepNexChain = &mDynamicRenderingFeaturesKHR;
}

void TestRenderer::SetupRenderPass()
{
    mRenderPass = VK_NULL_HANDLE;
}

void TestRenderer::BuildCommandBuffers()
{

}

void TestRenderer::Prepare()
{

}

void TestRenderer::Render()
{

}

void TestRenderer::ViewChanged()
{

}

void TestRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{

}

void TestRenderer::SetupDescriptorPool()
{

}

void TestRenderer::SetupDescriptorSet()
{

}

void TestRenderer::SetupDescriptorSetLayout()
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

void TestRenderer::LoadAssets()
{
    
}

void TestRenderer::Draw()
{

}