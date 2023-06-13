#pragma once

#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cstddef>

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanFramework.h"
#include "GLTFLoader.h"
#include "VulkanUIOverlay.h"

#include <ktx.h>
#include <ktxvulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>