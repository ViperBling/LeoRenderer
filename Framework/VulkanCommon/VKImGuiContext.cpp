#include "VKImGuiContext.hpp"
#include "VKContext.hpp"
#include "Utilities/GLFWindow.hpp"
#include "VKSampler.hpp"

#include <ImGui/backends/imgui_impl_vulkan.h>
#include <ImGui/backends/imgui_impl_glfw.h>

namespace LeoVK
{
    void ImGuiVulkanContext::Init(const Window &window, const vk::RenderPass &renderPass)
    {
        auto& vulkan = GetCurrentVulkanContext();

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(window.GetNativeHandle(), true);
        ImGui_ImplVulkan_InitInfo initInfo = {};
        initInfo.Instance = vulkan.GetInstance();
        initInfo.PhysicalDevice = vulkan.GetPhysicalDevice();
        initInfo.Device = vulkan.GetDevice();
        initInfo.QueueFamily = vulkan.GetQueueFamilyIndex();
        initInfo.Queue = vulkan.GetGraphicsQueue();
        initInfo.PipelineCache = vk::PipelineCache{};
        initInfo.Allocator = nullptr;
        initInfo.InFlyFrameCount = vulkan.GetVirtualFrameCount();
        initInfo.ImageCount = vulkan.GetPresentImageCount();
        initInfo.MinImageCount = vulkan.GetPresentImageCount();
        initInfo.CheckVkResultFn = nullptr;
        ImGui_ImplVulkan_Init(&initInfo, renderPass);

        auto cmdBuffer = vulkan.GetCurrentCommandBuffer();
        cmdBuffer.Begin();
        ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer.GetNativeCmdBuffer());
        cmdBuffer.End();

        vulkan.SubmitCommandsImmediate(cmdBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void ImGuiVulkanContext::Destroy()
    {
        GetCurrentVulkanContext().GetDevice().waitIdle();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }

    void ImGuiVulkanContext::StartFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiVulkanContext::RenderFrame(const vk::CommandBuffer &commandBuffer)
    {
        ImGui::Render();
        ImDrawData* drawData = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }

    ImTextureID ImGuiVulkanContext::GetTextureId(const Image &image)
    {
        return (ImTextureID)image.GetNativeView(ImageView::NATIVE);
    }

    ImTextureID ImGuiVulkanContext::GetTextureId(const vk::ImageView &view)
    {
        return (ImTextureID)view;
    }

    void ImGuiVulkanContext::EndFrame()
    {
        ImGui::EndFrame();
    }
}