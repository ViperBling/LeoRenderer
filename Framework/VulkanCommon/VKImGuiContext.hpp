#pragma once

#include <imgui.h>
#include <vulkan/vulkan.hpp>

namespace LeoVK
{
    class VulkanContext;
    class Window;
    class Image;
    class RenderPass;

    class ImGuiVulkanContext
    {
    public:
        static void Init(const Window& window, const vk::RenderPass& renderPass);
        static void Destroy();
        static void StartFrame();
        static void RenderFrame(const vk::CommandBuffer& commandBuffer);
        static ImTextureID GetTextureId(const Image& image);
        static ImTextureID GetTextureId(const vk::ImageView& view);
        static void EndFrame();
    };
}