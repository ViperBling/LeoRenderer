#pragma once

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <array>

#include <assimp/Importer.hpp>

#include "VulkanCommon/VKShaderReflection.hpp"
#include "VectorMath.hpp"

namespace LeoVK
{
    struct ImageData
    {
        std::vector<uint8_t> ByteData;
        Format ImageFormat = Format::UNDEFINED;
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<std::vector<uint8_t>> MipLevels;
    };

    struct CubemapData
    {
        std::array<std::vector<uint8_t>, 6> Faces;
        Format FaceFormat = Format::UNDEFINED;
        uint32_t FaceWidth = 0;
        uint32_t FaceHeight = 0;
    };

    struct ShaderData
    {
        using ByteCodeSPIRV = std::vector<uint32_t>;
        using Attributes = std::vector<TypeSPIRV>;
        using UniformBlock = std::vector<Uniform>;
        using Uniforms = std::vector<UniformBlock>;

        ByteCodeSPIRV ByteCode;
        Attributes InputAttributes;
        Uniforms UniformDescSets;
    };

    struct ModelData
    {
        struct Vertex
        {
            Vector3 Position{ 0.0f, 0.0f, 0.0f };
            Vector2 TexCoord{ 0.0f, 0.0f };
            Vector3 Normal{ 0.0f, 0.0f, 0.0f };
            Vector3 Tangent{ 0.0f, 0.0f, 0.0f };
            Vector3 Bitangent{ 0.0f, 0.0f, 0.0f };
        };

        struct Material
        {
            std::string Name;
            ImageData AlbedoTexture;
            ImageData NormalTexture;
            ImageData EmissiveTexture;
            ImageData MetallicRoughnessTexture;
            Vector3 EmissiveFactor = { 1.0f, 1.0f, 1.0f };
            float RoughnessScale = 1.0f;
            float MetallicScale = 1.0f;
        };

        using Index = uint32_t;

        struct Primitive
        {
            std::string Name;
            std::vector<Vertex> Vertices;
            std::vector<Index> Indices;
            uint32_t MaterialIndex = -1;
        };

        std::vector<Primitive> Primitives;
        std::vector<Material> Materials;
    };

    class ImageLoader
    {
    public:
        static ImageData LoadImageFromFile(const std::string& filePath);
        static CubemapData LoadCubemapFromFile(const std::string& filePath);
    };

    class ShaderLoader
    {
    public:
        static ShaderData LoadFromSourceFile(const std::string& filePath, ShaderType type, ShaderLanguage language);
        static ShaderData LoadFromBinaryFile(const std::string& filePath);
        static ShaderData LoadFromBinary(std::vector<uint32_t> byteCode);
        static ShaderData LoadFromSource(const std::string& code, ShaderType type, ShaderLanguage language);
    };

    class ModelLader
    {
    public:
        static ModelData LoadFromObj(const std::string& filePath);
        static ModelData LoadFromGLTF(const std::string& filePath);
        static ModelData Load(const std::string& filepath);
    };

} // namespace LeoVK
