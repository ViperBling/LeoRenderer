#pragma once

#include <filesystem>
#include <iostream>
#include <map>

#include "Utilities/GLFWindow.hpp"
#include "Utilities/AssetLoader.hpp"
#include "VulkanCommon/VKContext.hpp"
#include "VulkanCommon/VKImGuiContext.hpp"
#include "VulkanCommon/VKImGuiRenderpass.hpp"
#include "VulkanCommon/RenderGraphBuilder.hpp"
#include "VulkanCommon/VKShaderGraphic.hpp"

using namespace LeoVK;

void VulkanInfoCallback(const std::string& message)
{
    std::cout << "[INFO Vulkan]: " << message << std::endl;
}