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

void VulkanErrorCallback(const std::string& message)
{
    std::cout << "[ERROR Vulkan]: " << message << std::endl;
}

void WindowErrorCallback(const std::string& message)
{
    std::cerr << "[ERROR Window]: " << message << std::endl;
}

constexpr size_t MaxLightCount = 4;
constexpr size_t MaxMaterialCount = 256;

struct Mesh
{
    struct Material
    {
        uint32_t AlbedoIndex;
        uint32_t NormalIndex;
        uint32_t MetallicRoughnessIndex;
        float RoughnessFactor;
    };
    struct SubMesh
    {
        Buffer VertexBuffer;
        Buffer IndexBuffer;
        uint32_t MaterialIndex;
    };

    std::vector<SubMesh> SubMeshes;
    std::vector<Material> Materials;
    std::vector<Image> Textures;
};

struct CameraUniformData
{
    Matrix4x4 Matrix;
    Vector3 Position;
};

struct ModelUniformData
{
    Matrix4x4 Matrix;
};

struct LightUniformData
{
    Matrix3x3 Rotation;
    Vector3 Position;
    float Width;
    Vector3 Color;
    float Height;
    uint32_t TextureIndex;
    uint32_t Padding[3];
};

struct SharedResources
{
    Buffer CameraUniformBuffer;
    Buffer MeshDataUniformBuffer;
    Buffer MaterialUniformBuffer;
    Buffer LightUniformBuffer;

    Mesh RenderMesh;
    Image LookupLTCMatrix;
    Image LookupLTCAmplitude;
    std::vector<Image> LightTextures;
    CameraUniformData CameraUniform;
    ModelUniformData ModelUniform;
    std::array<LightUniformData, MaxLightCount> LightUniformsArray;
};

void LoadImage(CommandBuffer& cmdBuffer, Image& image, const ImageData& imageData, ImageOptions::Value options)
{
    auto& stageBuffer = GetCurrentVulkanContext().GetCurrentStageBuffer();

    image.Init(
        imageData.Width,
        imageData.Height,
        imageData.ImageFormat,
        ImageUsage::SHADER_READ | ImageUsage::TRANSFER_SOURCE | ImageUsage::TRANSFER_DESTINATION,
        MemoryUsage::GPU_ONLY,
        options
    );
    auto allocation = stageBuffer.Submit(MakeView(imageData.ByteData));
    cmdBuffer.CopyBufferToImage(
        BufferInfo { stageBuffer.GetBuffer(), allocation.Offset },
        ImageInfo { image, ImageUsage::UNKNOWN, 0, 0 }
    );
    
    if (options & ImageOptions::MIPMAPS)
    {
        if (imageData.MipLevels.empty())
        {
            cmdBuffer.GenerateMipLevels(image, ImageUsage::TRANSFER_DESTINATION, BlitFilter::LINEAR);
        }
        else
        {
            uint32_t mipLevel = 1;
            for (const auto& mipData : imageData.MipLevels)
            {
                auto allocation = stageBuffer.Submit(MakeView(mipData));
                cmdBuffer.CopyBufferToImage(
                    BufferInfo { stageBuffer.GetBuffer(), allocation.Offset },
                    ImageInfo { image, ImageUsage::TRANSFER_DESTINATION, mipLevel, 0 }
                );
                mipLevel++;
            }
        }
    }
    cmdBuffer.TransferLayout(image, ImageUsage::TRANSFER_DESTINATION, ImageUsage::SHADER_READ);
}

void LoadImage(Image& image, const std::string& filepath, ImageOptions::Value options)
{
    auto& cmdBuffer = GetCurrentVulkanContext().GetCurrentCommandBuffer();
    auto& stageBuffer = GetCurrentVulkanContext().GetCurrentStageBuffer();
    cmdBuffer.Begin();

    LoadImage(cmdBuffer, image, ImageLoader::LoadImageFromFile(filepath), options);

    stageBuffer.Flush();
    cmdBuffer.End();
    GetCurrentVulkanContext().SubmitCommandsImmediate(cmdBuffer);
    stageBuffer.Reset();
}

void LoadModelGLTF(Mesh& mesh, const std::string& filepath)
{
    auto model = ModelLoader::LoadFromGLTF(filepath);
   
    auto& commandBuffer = GetCurrentVulkanContext().GetCurrentCommandBuffer();
    auto& stageBuffer = GetCurrentVulkanContext().GetCurrentStageBuffer();

    commandBuffer.Begin();

    for (const auto& primitive : model.Primitives)
    {
        auto& submesh = mesh.SubMeshes.emplace_back();
        submesh.VertexBuffer.Init(
            primitive.Vertices.size() * sizeof(ModelData::Vertex),
            BufferUsage::VERTEX_BUFFER | BufferUsage::TRANSFER_DESTINATION, 
            MemoryUsage::GPU_ONLY
        );

        auto vertexAllocation = stageBuffer.Submit(MakeView(primitive.Vertices));
        commandBuffer.CopyBuffer(
            BufferInfo{ stageBuffer.GetBuffer(), vertexAllocation.Offset },
            BufferInfo{ submesh.VertexBuffer, 0 }, 
            vertexAllocation.Size
        );

        submesh.IndexBuffer.Init(
            primitive.Indices.size() * sizeof(ModelData::Index),
            BufferUsage::INDEX_BUFFER | BufferUsage::TRANSFER_DESTINATION,
            MemoryUsage::GPU_ONLY
        );

        auto indexAllocation = stageBuffer.Submit(MakeView(primitive.Indices));
        commandBuffer.CopyBuffer(
            BufferInfo{ stageBuffer.GetBuffer(), indexAllocation.Offset },
            BufferInfo{ submesh.IndexBuffer, 0 },
            indexAllocation.Size
        );

        submesh.MaterialIndex = primitive.MaterialIndex;
    }

    stageBuffer.Flush();
    commandBuffer.End();
    GetCurrentVulkanContext().SubmitCommandsImmediate(commandBuffer);
    stageBuffer.Reset();

    uint32_t textureIndex = 0;
    for (const auto& material : model.Materials)
    {
        commandBuffer.Begin();

        LoadImage(commandBuffer, mesh.Textures.emplace_back(), material.AlbedoTexture, ImageOptions::MIPMAPS);
        LoadImage(commandBuffer, mesh.Textures.emplace_back(), material.NormalTexture, ImageOptions::MIPMAPS);
        LoadImage(commandBuffer, mesh.Textures.emplace_back(), material.MetallicRoughnessTexture, ImageOptions::MIPMAPS);

        constexpr float AppliedRoughnessScale = 0.5f;
        mesh.Materials.push_back(Mesh::Material{ textureIndex, textureIndex + 1, textureIndex + 2, AppliedRoughnessScale * material.RoughnessScale });
        textureIndex += 3;

        stageBuffer.Flush();
        commandBuffer.End();
        GetCurrentVulkanContext().SubmitCommandsImmediate(commandBuffer);
        stageBuffer.Reset();
    }
}

class UniformSubmitRenderPass : public RenderPass
{
public:
    

private:
    SharedResources& mShaderResources;
};

