#pragma once

#include <iostream>
#include <string>
#include <cassert>
#include <fstream>
#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>
#include "vulkanexamplebase.h"

struct Vertex
{
    float position[3];
    float color[3];
};

// Vertex buffer and attributes
struct VertexBuffer
{
    VkDeviceMemory memory;      // Handle to the device memory for this buffer
    VkBuffer buffer;            // Handle to the Vulkan buffer object that the memory is bound to
};

// Index buffer
struct IndexBuffer
{
    VkDeviceMemory memory;
    VkBuffer buffer;
    uint32_t count;
};

// Uniform buffer
struct UniformBuffer
{
    VkDeviceMemory memory;
    VkBuffer buffer;
    VkDescriptorBufferInfo descriptor;
};

struct UniformBufferObject
{
    glm::mat4 projectionMat;
    glm::mat4 modelMat;
    glm::mat4 viewMat;
};

struct StagingBuffer
{
    VkDeviceMemory memory;
    VkBuffer buffer;
};

struct StagingBuffers
{
    StagingBuffer vertices;
    StagingBuffer indices;
};