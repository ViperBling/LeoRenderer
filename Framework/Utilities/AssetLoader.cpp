#include "AssetLoader.hpp"
#include "VectorMath.hpp"
#include "VulkanCommon/ArrayUtils.hpp"
#include "VulkanCommon/VKContext.hpp"

#include <filesystem>
#include <fstream>

#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <SpirvReflect/spirv_reflect.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

namespace LeoVK
{   
    static bool IsDDSImage(const std::string& filePath)
    {
        std::filesystem::path filename = filePath;
        return filename.extension() == ".dds";
    }

    static bool IsZLIBImage(const std::string& filePath)
    {
        std::filesystem::path filename{ filePath };
        return filename.extension() == ".zlib";
    }

    static Format DDSFormatToImageFormat(tinyddsloader::DDSFile::DXGIFormat format)
    {
        switch (format)
        {
        case tinyddsloader::DDSFile::DXGIFormat::Unknown:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_Float:
            return Format::R32G32B32A32_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_UInt:
            return Format::R32G32B32A32_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32A32_SInt:
            return Format::R32G32B32A32_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_Float:
            return Format::R32G32B32_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_UInt:
            return Format::R32G32B32_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32B32_SInt:
            return Format::R32G32B32_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Float:
            return Format::R16G16B16A16_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UNorm:
            return Format::R16G16B16A16_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_UInt:
            return Format::R16G16B16A16_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SNorm:
            return Format::R16G16B16A16_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_SInt:
            return Format::R16G16B16A16_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32_Float:
            return Format::R32G32_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32_UInt:
            return Format::R32G32_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G32_SInt:
            return Format::R32G32_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32G8X24_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::D32_Float_S8X24_UInt:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R32_Float_X8X24_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::X32_Typeless_G8X24_UInt:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UNorm:
            return Format::A2R10G10B10_UNORM_PACK_32;
        case tinyddsloader::DDSFile::DXGIFormat::R10G10B10A2_UInt:
            return Format::A2R10G10B10_UINT_PACK_32;
        case tinyddsloader::DDSFile::DXGIFormat::R11G11B10_Float:
            return Format::B10G11R11_UFLOAT_PACK_32;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm:
            return Format::R8G8B8A8_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UNorm_SRGB:
            return Format::R8G8B8A8_SRGB;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_UInt:
            return Format::R8G8B8A8_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SNorm:
            return Format::R8G8B8A8_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8B8A8_SInt:
            return Format::R8G8B8A8_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_Float:
            return Format::R16G16_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_UNorm:
            return Format::R16G16_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_UInt:
            return Format::R16G16_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_SNorm:
            return Format::R16G16_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16G16_SInt:
            return Format::R16G16_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::D32_Float:
            return Format::D32_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R32_Float:
            return Format::R32_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::R32_UInt:
            return Format::R32_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R32_SInt:
            return Format::R32_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R24G8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::D24_UNorm_S8_UInt:
            return Format::D24_UNORM_S8_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R24_UNorm_X8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::X24_Typeless_G8_UInt:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_UNorm:
            return Format::R8G8_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_UInt:
            return Format::R8G8_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_SNorm:
            return Format::R8G8_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_SInt:
            return Format::R8G8_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R16_Float:
            return Format::R16_SFLOAT;
        case tinyddsloader::DDSFile::DXGIFormat::D16_UNorm:
            return Format::D16_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16_UNorm:
            return Format::R16_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16_UInt:
            return Format::R16_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R16_SNorm:
            return Format::R16_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R16_SInt:
            return Format::R16_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::R8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R8_UNorm:
            return Format::R8_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8_UInt:
            return Format::R8_UINT;
        case tinyddsloader::DDSFile::DXGIFormat::R8_SNorm:
            return Format::R8_SNORM;
        case tinyddsloader::DDSFile::DXGIFormat::R8_SInt:
            return Format::R8_SINT;
        case tinyddsloader::DDSFile::DXGIFormat::A8_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R1_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R9G9B9E5_SHAREDEXP:
            return Format::E5B9G9R9_UFLOAT_PACK_32;
        case tinyddsloader::DDSFile::DXGIFormat::R8G8_B8G8_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::G8R8_G8B8_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC1_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC1_UNorm_SRGB:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC2_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC2_UNorm_SRGB:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC3_UNorm_SRGB:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC4_SNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC5_SNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::B5G6R5_UNorm:
            return Format::B5G6R5_UNORM_PACK_16;
        case tinyddsloader::DDSFile::DXGIFormat::B5G5R5A1_UNorm:
            return Format::B5G5R5A1_UNORM_PACK_16;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm:
            return Format::B8G8R8A8_UNORM;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::R10G10B10_XR_BIAS_A2_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8A8_UNorm_SRGB:
            return Format::B8G8R8A8_SRGB;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::B8G8R8X8_UNorm_SRGB:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_UF16:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC6H_SF16:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_Typeless:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::BC7_UNorm_SRGB:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::AYUV:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::Y410:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::Y416:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::NV12:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::P010:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::P016:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::YUV420_OPAQUE:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::YUY2:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::Y210:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::Y216:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::NV11:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::AI44:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::IA44:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::P8:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::A8P8:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::B4G4R4A4_UNorm:
            return Format::B4G4R4A4_UNORM_PACK_16;
        case tinyddsloader::DDSFile::DXGIFormat::P208:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::V208:
            return Format::UNDEFINED;
        case tinyddsloader::DDSFile::DXGIFormat::V408:
            return Format::UNDEFINED;
        default:
            return Format::UNDEFINED;
        }
    }

    static ImageData LoadImageUsingDDSLoader(const std::string& filePath)
    {
        ImageData image;
        tinyddsloader::DDSFile dds;
        auto result = dds.Load(filePath.c_str());
        if (result != tinyddsloader::Result::Success)
            return image;

        dds.Flip();
        auto imageData = dds.GetImageData();
        image.Width = imageData->m_height;
        image.Height = imageData->m_height;
        image.ImageFormat = DDSFormatToImageFormat(dds.GetFormat());
        image.ByteData.resize(imageData->m_memSlicePitch);
        std::copy((const uint8_t*)imageData->m_mem, (const uint8_t*)imageData->m_mem + imageData->m_memSlicePitch, image.ByteData.begin());

        for (uint32_t i = 1; i < dds.GetMipCount(); i++)
        {
            auto& mipLevelData = image.MipLevels.emplace_back();
            auto mipImageData = dds.GetImageData(i);
            mipLevelData.resize(mipImageData->m_memSlicePitch);
            std::copy((const uint8_t*)mipImageData->m_mem, (const uint8_t*)mipImageData->m_mem + mipImageData->m_memSlicePitch, mipLevelData.begin());
        }

        return image;
    }

    static std::string ConvertZLIBToDDS(const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.good()) return filePath;

        std::vector<char> compressedData{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()
        };

        int decompressedSize = 0;
        char* decompressedData = stbi_zlib_decode_malloc(compressedData.data(), (int)compressedData.size(), &decompressedSize);

        std::filesystem::path path{ filePath };
        auto ddsFilepath = (path.parent_path() / path.stem()).string() + ".dds";
        std::ofstream ddsFile(ddsFilepath, std::ios::binary);
        ddsFile.write(decompressedData, decompressedSize);
        free(decompressedData);

        return ddsFilepath;
    }

    static ImageData LoadImageUsingZLIBLoader(const std::string& filePath)
    {        
        auto ddsFilepath = ConvertZLIBToDDS(filePath);
        return LoadImageUsingDDSLoader(ddsFilepath);
    }

    static ImageData LoadImageUsingSTBLoader(const std::string& filePath)
    {
        int width = 0, height = 0, channels = 0;
        const uint32_t actualChannels = 4;

        stbi_set_flip_vertically_on_load(true);
        uint8_t* data = stbi_load(filePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        stbi_set_flip_vertically_on_load(false);

        std::vector<uint8_t> vecData;
        vecData.resize(width * height * actualChannels * sizeof(uint8_t));
        std::copy(data, data + vecData.size(), vecData.begin());
        stbi_image_free(data);
        return ImageData{ std::move(vecData), Format::R8G8B8A8_UNORM, (uint32_t)width, (uint32_t)height };
    }

    static std::vector<uint8_t> ExtractCubemapFace(const ImageData& image, size_t faceWidth, size_t faceHeight, size_t channelCount, size_t sliceX, size_t sliceY)
    {
        std::vector<uint8_t> result(image.ByteData.size() / 6);

        for (size_t i = 0; i < faceHeight; i++)
        {
            size_t y = (faceHeight - i - 1) + sliceY * faceHeight;
            size_t x = sliceX * faceWidth;
            size_t bytesInRow = faceWidth * channelCount;

            std::memcpy(result.data() + i * bytesInRow, image.ByteData.data() + (y * image.Width + x) * channelCount, bytesInRow);
        }
        return result;
    };

    static CubemapData CreateCubemapFromSingleImage(const ImageData& image)
    {
        CubemapData cubemapData;
        cubemapData.FaceFormat = image.ImageFormat;
        cubemapData.FaceWidth = image.Width / 4;
        cubemapData.FaceHeight = image.Height / 3;
        assert(cubemapData.FaceWidth == cubemapData.FaceHeight); // single face should be a square
        assert(cubemapData.FaceFormat == Format::R8G8B8A8_UNORM); // support only this format for now
        constexpr size_t ChannelCount = 4;

        cubemapData.Faces[0] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 2, 1);
        cubemapData.Faces[1] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 0, 1);
        cubemapData.Faces[2] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 2);
        cubemapData.Faces[3] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 0);
        cubemapData.Faces[4] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 1, 1);
        cubemapData.Faces[5] = ExtractCubemapFace(image, cubemapData.FaceWidth, cubemapData.FaceHeight, ChannelCount, 3, 1);
        return cubemapData;
        
    }
    
    ImageData ImageLoader::LoadImageFromFile(const std::string &filePath)
    {
        if (IsDDSImage(filePath)) return LoadImageUsingDDSLoader(filePath);
        else if (IsZLIBImage(filePath)) return LoadImageUsingZLIBLoader(filePath);
        else return LoadImageUsingSTBLoader(filePath);
    }

    CubemapData ImageLoader::LoadCubemapFromFile(const std::string &filePath)
    {
        auto imageData = ImageLoader::LoadImageFromFile(filePath);
        return CreateCubemapFromSingleImage(imageData);
    }

    EShLanguage ShaderTypeTable[] = 
    {
        EShLangVertex,
        EShLangTessControl,
        EShLangTessEvaluation,
        EShLangGeometry,
        EShLangFragment,
        EShLangCompute,
        EShLangRayGen,
        EShLangIntersect,
        EShLangAnyHit,
        EShLangClosestHit,
        EShLangMiss,
        EShLangCallable,
        EShLangTaskNV,
        EShLangMeshNV,
    };

    glslang::EShSource ShaderLanguageTable[] = 
    {
        glslang::EShSource::EShSourceGlsl,
        glslang::EShSource::EShSourceHlsl,
    };

    constexpr auto GetResourceLimits()
    {
        TBuiltInResource defaultResources = {};
        defaultResources.maxLights = 32;
        defaultResources.maxClipPlanes = 6;
        defaultResources.maxTextureUnits = 32;
        defaultResources.maxTextureCoords = 32;
        defaultResources.maxVertexAttribs = 64;
        defaultResources.maxVertexUniformComponents = 4096;
        defaultResources.maxVaryingFloats = 64;
        defaultResources.maxVertexTextureImageUnits = 32;
        defaultResources.maxCombinedTextureImageUnits = 80;
        defaultResources.maxTextureImageUnits = 32;
        defaultResources.maxFragmentUniformComponents = 4096;
        defaultResources.maxDrawBuffers = 32;
        defaultResources.maxVertexUniformVectors = 128;
        defaultResources.maxVaryingVectors = 8;
        defaultResources.maxFragmentUniformVectors = 16;
        defaultResources.maxVertexOutputVectors = 16;
        defaultResources.maxFragmentInputVectors = 15;
        defaultResources.minProgramTexelOffset = -8;
        defaultResources.maxProgramTexelOffset = 7;
        defaultResources.maxClipDistances = 8;
        defaultResources.maxComputeWorkGroupCountX = 65535;
        defaultResources.maxComputeWorkGroupCountY = 65535;
        defaultResources.maxComputeWorkGroupCountZ = 65535;
        defaultResources.maxComputeWorkGroupSizeX = 1024;
        defaultResources.maxComputeWorkGroupSizeY = 1024;
        defaultResources.maxComputeWorkGroupSizeZ = 64;
        defaultResources.maxComputeUniformComponents = 1024;
        defaultResources.maxComputeTextureImageUnits = 16;
        defaultResources.maxComputeImageUniforms = 8;
        defaultResources.maxComputeAtomicCounters = 8;
        defaultResources.maxComputeAtomicCounterBuffers = 1;
        defaultResources.maxVaryingComponents = 60;
        defaultResources.maxVertexOutputComponents = 64;
        defaultResources.maxGeometryInputComponents = 64;
        defaultResources.maxGeometryOutputComponents = 128;
        defaultResources.maxFragmentInputComponents = 128;
        defaultResources.maxImageUnits = 8;
        defaultResources.maxCombinedImageUnitsAndFragmentOutputs = 8;
        defaultResources.maxCombinedShaderOutputResources = 8;
        defaultResources.maxImageSamples = 0;
        defaultResources.maxVertexImageUniforms = 0;
        defaultResources.maxTessControlImageUniforms = 0;
        defaultResources.maxTessEvaluationImageUniforms = 0;
        defaultResources.maxGeometryImageUniforms = 0;
        defaultResources.maxFragmentImageUniforms = 8;
        defaultResources.maxCombinedImageUniforms = 8;
        defaultResources.maxGeometryTextureImageUnits = 16;
        defaultResources.maxGeometryOutputVertices = 256;
        defaultResources.maxGeometryTotalOutputComponents = 1024;
        defaultResources.maxGeometryUniformComponents = 1024;
        defaultResources.maxGeometryVaryingComponents = 64;
        defaultResources.maxTessControlInputComponents = 128;
        defaultResources.maxTessControlOutputComponents = 128;
        defaultResources.maxTessControlTextureImageUnits = 16;
        defaultResources.maxTessControlUniformComponents = 1024;
        defaultResources.maxTessControlTotalOutputComponents = 4096;
        defaultResources.maxTessEvaluationInputComponents = 128;
        defaultResources.maxTessEvaluationOutputComponents = 128;
        defaultResources.maxTessEvaluationTextureImageUnits = 16;
        defaultResources.maxTessEvaluationUniformComponents = 1024;
        defaultResources.maxTessPatchComponents = 120;
        defaultResources.maxPatchVertices = 32;
        defaultResources.maxTessGenLevel = 64;
        defaultResources.maxViewports = 16;
        defaultResources.maxVertexAtomicCounters = 0;
        defaultResources.maxTessControlAtomicCounters = 0;
        defaultResources.maxTessEvaluationAtomicCounters = 0;
        defaultResources.maxGeometryAtomicCounters = 0;
        defaultResources.maxFragmentAtomicCounters = 8;
        defaultResources.maxCombinedAtomicCounters = 8;
        defaultResources.maxAtomicCounterBindings = 1;
        defaultResources.maxVertexAtomicCounterBuffers = 0;
        defaultResources.maxTessControlAtomicCounterBuffers = 0;
        defaultResources.maxTessEvaluationAtomicCounterBuffers = 0;
        defaultResources.maxGeometryAtomicCounterBuffers = 0;
        defaultResources.maxFragmentAtomicCounterBuffers = 1;
        defaultResources.maxCombinedAtomicCounterBuffers = 1;
        defaultResources.maxAtomicCounterBufferSize = 16384;
        defaultResources.maxTransformFeedbackBuffers = 4;
        defaultResources.maxTransformFeedbackInterleavedComponents = 64;
        defaultResources.maxCullDistances = 8;
        defaultResources.maxCombinedClipAndCullDistances = 8;
        defaultResources.maxSamples = 4;
        defaultResources.maxMeshOutputVerticesNV = 256;
        defaultResources.maxMeshOutputPrimitivesNV = 512;
        defaultResources.maxMeshWorkGroupSizeX_NV = 32;
        defaultResources.maxMeshWorkGroupSizeY_NV = 1;
        defaultResources.maxMeshWorkGroupSizeZ_NV = 1;
        defaultResources.maxTaskWorkGroupSizeX_NV = 32;
        defaultResources.maxTaskWorkGroupSizeY_NV = 1;
        defaultResources.maxTaskWorkGroupSizeZ_NV = 1;
        defaultResources.maxMeshViewCountNV = 4;
        defaultResources.maxDualSourceDrawBuffersEXT = 1;
        defaultResources.limits.nonInductiveForLoops = 1;
        defaultResources.limits.whileLoops = 1;
        defaultResources.limits.doWhileLoops = 1;
        defaultResources.limits.generalUniformIndexing = 1;
        defaultResources.limits.generalAttributeMatrixVectorIndexing = 1;
        defaultResources.limits.generalVaryingIndexing = 1;
        defaultResources.limits.generalSamplerIndexing = 1;
        defaultResources.limits.generalVariableIndexing = 1;
        defaultResources.limits.generalConstantMatrixVectorIndexing = 1;
        
        return defaultResources;
    }

    TypeSPIRV GetTypeByReflection(const SpvReflectTypeDescription& type)
    {
        Format format = Format::UNDEFINED;
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT)
        {
            if (type.traits.numeric.vector.component_count <  2 || type.traits.numeric.matrix.column_count <  2)
                format = Format::R32_SFLOAT;
            if (type.traits.numeric.vector.component_count == 2 || type.traits.numeric.matrix.column_count == 2)
                format = Format::R32G32_SFLOAT;
            if (type.traits.numeric.vector.component_count == 3 || type.traits.numeric.matrix.column_count == 3)
                format = Format::R32G32B32_SFLOAT;
            if (type.traits.numeric.vector.component_count == 4 || type.traits.numeric.matrix.column_count == 4)
                format = Format::R32G32B32A32_SFLOAT;
        }
        if (type.type_flags & SPV_REFLECT_TYPE_FLAG_INT)
        {
            if (type.traits.numeric.vector.component_count <  2 || type.traits.numeric.matrix.column_count <  2)
                format = type.traits.numeric.scalar.signedness ? Format::R32_SINT : Format::R32_UINT;
            if (type.traits.numeric.vector.component_count == 2 || type.traits.numeric.matrix.column_count == 2)
                format = type.traits.numeric.scalar.signedness ? Format::R32G32_SINT : Format::R32G32_UINT;
            if (type.traits.numeric.vector.component_count == 3 || type.traits.numeric.matrix.column_count == 3)
                format = type.traits.numeric.scalar.signedness ? Format::R32G32B32_SINT : Format::R32G32B32_UINT;
            if (type.traits.numeric.vector.component_count == 4 || type.traits.numeric.matrix.column_count == 4)
                format = type.traits.numeric.scalar.signedness ? Format::R32G32B32A32_SINT : Format::R32G32B32A32_UINT;
        }
        assert(format != Format::UNDEFINED);

        int32_t byteSize = type.traits.numeric.scalar.width / 8;
        int32_t componentCount = 1;

        if (type.traits.numeric.vector.component_count > 0)
            byteSize *= type.traits.numeric.vector.component_count;
        else if (type.traits.numeric.matrix.row_count > 0)
            byteSize *= type.traits.numeric.matrix.row_count;

        if (type.traits.numeric.matrix.column_count > 0)
            componentCount = type.traits.numeric.matrix.column_count;

        return TypeSPIRV{ format, componentCount, byteSize };
    }

    void RecursiveUniformVisit(std::vector<TypeSPIRV>& uniformVariables, const SpvReflectTypeDescription& type)
    {
        if (type.member_count > 0)
        {
            for (uint32_t i = 0; i < type.member_count; i++)
                RecursiveUniformVisit(uniformVariables, type.members[i]);
        }
        else
        {
            if(type.type_flags & (SPV_REFLECT_TYPE_FLAG_INT | SPV_REFLECT_TYPE_FLAG_FLOAT))
                uniformVariables.push_back(GetTypeByReflection(type));
        }
    }

    ShaderData ShaderLoader::LoadFromSourceFile(const std::string &filePath, ShaderType type, ShaderLanguage language)
    {
        std::ifstream file(filePath);
        std::string source { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        return ShaderLoader::LoadFromSource(source, type, language);
    }

    ShaderData ShaderLoader::LoadFromBinaryFile(const std::string &filePath)
    {
        std::vector<uint32_t> byteCode;
        std::ifstream file(filePath, std::ios_base::binary);
        auto binaryData = std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        byteCode.resize(binaryData.size() / sizeof(uint32_t));
        std::copy((uint32_t*)binaryData.data(), (uint32_t*)binaryData.data() + byteCode.size(), byteCode.begin());
        return ShaderLoader::LoadFromBinary(byteCode);
    }

    ShaderData ShaderLoader::LoadFromBinary(std::vector<uint32_t> byteCode)
    {
        ShaderData result;
        result.ByteCode = std::move(byteCode);

        SpvReflectResult spvRes;
        SpvReflectShaderModule reflectedShaderModule;
        spvRes = spvReflectCreateShaderModule(result.ByteCode.size() * sizeof(uint32_t), (const void*)result.ByteCode.data(), &reflectedShaderModule);
        assert(spvRes == SPV_REFLECT_RESULT_SUCCESS);

        uint32_t inputAttributeCount = 0;
        spvRes = spvReflectEnumerateInputVariables(&reflectedShaderModule, &inputAttributeCount, nullptr);
        assert(spvRes == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectInterfaceVariable*> inputAttributes(inputAttributeCount);
        spvRes = spvReflectEnumerateInputVariables(&reflectedShaderModule, &inputAttributeCount, inputAttributes.data());
        assert(spvRes == SPV_REFLECT_RESULT_SUCCESS);

        // sort in location order
        std::sort(inputAttributes.begin(), inputAttributes.end(), [](const auto& v1, const auto& v2) { return v1->location < v2->location; });
        for (const auto& inputAttri : inputAttributes)
        {
            if (inputAttri->built_in == (SpvBuiltIn) - 1)
            {
                result.InputAttributes.push_back(GetTypeByReflection(*inputAttri->type_description));
            }
        }

        uint32_t descBindingCount = 0;
        spvRes = spvReflectEnumerateDescriptorBindings(&reflectedShaderModule, &descBindingCount, nullptr);
        assert(spvRes == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectDescriptorBinding*> descriptorBindings(descBindingCount);
        spvRes = spvReflectEnumerateDescriptorBindings(&reflectedShaderModule, &descBindingCount, descriptorBindings.data());
        assert(spvRes == SPV_REFLECT_RESULT_SUCCESS);

        for (const auto& descriptorBinding : descriptorBindings)
        {
            if (result.UniformDescSets.size() < (size_t)descriptorBinding->set + 1)
                result.UniformDescSets.resize((size_t)descriptorBinding->set + 1);

            auto& uniformBlock = result.UniformDescSets[descriptorBinding->set];

            std::vector<TypeSPIRV> uniformVariables;
            RecursiveUniformVisit(uniformVariables, *descriptorBinding->type_description);

            uniformBlock.push_back(Uniform 
            { 
                std::move(uniformVariables),
                FromNative((vk::DescriptorType)descriptorBinding->descriptor_type),
                descriptorBinding->binding,
                descriptorBinding->count
            });
        }
        if (result.UniformDescSets.empty()) 
            result.UniformDescSets.emplace_back(); // insert empty descriptor set

        spvReflectDestroyShaderModule(&reflectedShaderModule);

        return result;
    }

    ShaderData ShaderLoader::LoadFromSource(const std::string &code, ShaderType type, ShaderLanguage language)
    {
        const char* rawSource = code.c_str();
        constexpr static auto ResourceLimits = GetResourceLimits();

        glslang::TShader shader(ShaderTypeTable[(int)type]);
        shader.setStrings(&rawSource, 1);
        shader.setEnvInput(ShaderLanguageTable[(size_t)language], ShaderTypeTable[(size_t)type], glslang::EShClient::EShClientVulkan, 460);
        shader.setEnvClient(glslang::EShClient::EShClientVulkan, (glslang::EShTargetClientVersion)GetCurrentVulkanContext().GetAPIVersion());
        shader.setEnvTarget(glslang::EShTargetLanguage::EShTargetSpv, glslang::EShTargetLanguageVersion::EShTargetSpv_1_5);
        bool isParsed = shader.parse(&ResourceLimits, 460, false, EShMessages::EShMsgDefault);
        if (!isParsed) return ShaderData{ };

        glslang::TProgram program;
        program.addShader(&shader);
        bool isLinked = program.link(EShMessages::EShMsgDefault);
        if (!isLinked) return ShaderData{ };

        auto intermediate = program.getIntermediate(ShaderTypeTable[(size_t)type]);
        std::vector<uint32_t> bytecode;
        glslang::GlslangToSpv(*intermediate, bytecode);

        return ShaderLoader::LoadFromBinary(std::move(bytecode));
    }

    static std::string GetAbsolutePathToObjResource(const std::string& objPath, const std::string& relativePath)
    {
        const auto parentPath = std::filesystem::path{ objPath }.parent_path();
        const auto absolutePath = parentPath / relativePath;
        return absolutePath.string();
    }

    static ImageData CreateStubTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        return ImageData{ std::vector{ r, g, b, a }, Format::R8G8B8A8_UNORM, 1, 1 };
    }

    static auto ComputeTangentsBitangents(const tinyobj::mesh_t& mesh, const tinyobj::attrib_t& attrib)
    {
        std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
        tangentsBitangents.resize(attrib.normals.size(), { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } });

        ModelData::Index indexOffset = 0;
        for (size_t faceIndex : mesh.num_face_vertices)
        {
            assert(faceIndex == 3);

            tinyobj::index_t idx1 = mesh.indices[indexOffset + 0];
            tinyobj::index_t idx2 = mesh.indices[indexOffset + 1];
            tinyobj::index_t idx3 = mesh.indices[indexOffset + 2];

            Vector3 position1, position2, position3;
            Vector2 texCoord1, texCoord2, texCoord3;

            position1.x = attrib.vertices[3 * size_t(idx1.vertex_index) + 0];
            position1.y = attrib.vertices[3 * size_t(idx1.vertex_index) + 1];
            position1.z = attrib.vertices[3 * size_t(idx1.vertex_index) + 2];

            position2.x = attrib.vertices[3 * size_t(idx2.vertex_index) + 0];
            position2.y = attrib.vertices[3 * size_t(idx2.vertex_index) + 1];
            position2.z = attrib.vertices[3 * size_t(idx2.vertex_index) + 2];

            position3.x = attrib.vertices[3 * size_t(idx3.vertex_index) + 0];
            position3.y = attrib.vertices[3 * size_t(idx3.vertex_index) + 1];
            position3.z = attrib.vertices[3 * size_t(idx3.vertex_index) + 2];

            texCoord1.x = attrib.texcoords[2 * size_t(idx1.texcoord_index) + 0];
            texCoord1.y = attrib.texcoords[2 * size_t(idx1.texcoord_index) + 1];

            texCoord2.x = attrib.texcoords[2 * size_t(idx2.texcoord_index) + 0];
            texCoord2.y = attrib.texcoords[2 * size_t(idx2.texcoord_index) + 1];

            texCoord3.x = attrib.texcoords[2 * size_t(idx3.texcoord_index) + 0];
            texCoord3.y = attrib.texcoords[2 * size_t(idx3.texcoord_index) + 1];

            auto tangentBitangent = ComputeTangentSpace(
                position1, position2, position3,
                texCoord1, texCoord2, texCoord3
            );

            tangentsBitangents[idx1.vertex_index].first  += tangentBitangent.first;
            tangentsBitangents[idx1.vertex_index].second += tangentBitangent.second;

            tangentsBitangents[idx2.vertex_index].first  += tangentBitangent.first;
            tangentsBitangents[idx2.vertex_index].second += tangentBitangent.second;

            tangentsBitangents[idx3.vertex_index].first  += tangentBitangent.first;
            tangentsBitangents[idx3.vertex_index].second += tangentBitangent.second;

            indexOffset += faceIndex;
        }

        for (auto& [tangent, bitangent] : tangentsBitangents)
        {
            if (tangent != Vector3{ 0.0f, 0.0f, 0.0f }) tangent = Normalize(tangent);
            if (bitangent != Vector3{ 0.0f, 0.0f, 0.0f }) bitangent = Normalize(bitangent);
        }
        return tangentsBitangents;
    }

    static auto ComputeTangentsBitangents(ArrayView<ModelData::Index> indices, ArrayView<const Vector3> positions, ArrayView<const Vector2> texCoords)
    {
        std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
        tangentsBitangents.resize(positions.size(), { Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } });

        assert(indices.size() % 3 == 0);
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            const auto& position1 = positions[indices[i + 0]];
            const auto& position2 = positions[indices[i + 1]];
            const auto& position3 = positions[indices[i + 2]];

            const auto& texCoord1 = texCoords[indices[i + 0]];
            const auto& texCoord2 = texCoords[indices[i + 1]];
            const auto& texCoord3 = texCoords[indices[i + 2]];

            auto tangentBitangent = ComputeTangentSpace(
                position1, position2, position3,
                texCoord1, texCoord2, texCoord3
            );

            tangentsBitangents[indices[i + 0]].first += tangentBitangent.first;
            tangentsBitangents[indices[i + 0]].second += tangentBitangent.second;

            tangentsBitangents[indices[i + 1]].first += tangentBitangent.first;
            tangentsBitangents[indices[i + 1]].second += tangentBitangent.second;

            tangentsBitangents[indices[i + 2]].first += tangentBitangent.first;
            tangentsBitangents[indices[i + 2]].second += tangentBitangent.second;
        }

        for (auto& [tangent, bitangent] : tangentsBitangents)
        {
            if (tangent != Vector3{ 0.0f, 0.0f, 0.0f }) tangent = Normalize(tangent);
            if (bitangent != Vector3{ 0.0f, 0.0f, 0.0f }) bitangent = Normalize(bitangent);
        }
        return tangentsBitangents;
    }

    ModelData ModelLoader::LoadFromObj(const std::string &filePath)
    {
        ModelData result;

        tinyobj::ObjReaderConfig reader_config;
        tinyobj::ObjReader reader;

        if (!reader.ParseFromFile(filePath, reader_config))
        {
            return result;
        }

        auto& attrib = reader.GetAttrib();
        auto& shapes = reader.GetShapes();
        auto& materials = reader.GetMaterials();

        result.Materials.reserve(materials.size());
        for (const auto& material : materials)
        {
            auto& resultMaterial = result.Materials.emplace_back();

            resultMaterial.Name = material.name;

            if (!material.diffuse_texname.empty())
                resultMaterial.AlbedoTexture = ImageLoader::LoadImageFromFile(GetAbsolutePathToObjResource(filePath, material.diffuse_texname));
            else
                resultMaterial.AlbedoTexture = CreateStubTexture(255, 255, 255, 255);

            if (!material.normal_texname.empty())
                resultMaterial.NormalTexture = ImageLoader::LoadImageFromFile(GetAbsolutePathToObjResource(filePath, material.normal_texname));
            else
                resultMaterial.NormalTexture = CreateStubTexture(127, 127, 255, 255);

            // not supported by obj format
            resultMaterial.MetallicRoughnessTexture = CreateStubTexture(0, 255, 0, 255);
        }

        result.Primitives.reserve(shapes.size());
        for (const auto& shape : shapes)
        {
            auto& resultShape = result.Primitives.emplace_back();

            resultShape.Name = shape.name;

            if (!shape.mesh.material_ids.empty())
                resultShape.MaterialIndex = shape.mesh.material_ids.front();

            std::vector<std::pair<Vector3, Vector3>> tangentsBitangents;
            if (!attrib.normals.empty() && !attrib.texcoords.empty())
                tangentsBitangents = ComputeTangentsBitangents(shape.mesh, attrib);

            resultShape.Vertices.reserve(shape.mesh.indices.size());
            resultShape.Indices.reserve(shape.mesh.indices.size());

            ModelData::Index indexOffset = 0;
            for (size_t faceIndex : shape.mesh.num_face_vertices)
            {
                assert(faceIndex == 3);
                for (size_t v = 0; v < faceIndex; v++, indexOffset++)
                {
                    tinyobj::index_t idx = shape.mesh.indices[indexOffset];
                    auto& vertex = resultShape.Vertices.emplace_back();
                    resultShape.Indices.push_back(indexOffset);

                    vertex.Position.x = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
                    vertex.Position.y = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
                    vertex.Position.z = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

                    vertex.TexCoord.x = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
                    vertex.TexCoord.y = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

                    vertex.Normal.x = attrib.normals[3 * size_t(idx.normal_index) + 0];
                    vertex.Normal.y = attrib.normals[3 * size_t(idx.normal_index) + 1];
                    vertex.Normal.z = attrib.normals[3 * size_t(idx.normal_index) + 2];

                    if (!tangentsBitangents.empty())
                    {
                        vertex.Tangent   = tangentsBitangents[idx.vertex_index].first;
                        vertex.Bitangent = tangentsBitangents[idx.vertex_index].second;
                    }
                }
            }
        }

        return result;
    }

    static Format ImageFormatFromGLTFImage(const tinygltf::Image& image)
    {
        if (image.component == 1)
        {
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_BYTE)
                return Format::R8_SNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                return Format::R8_UNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_SHORT)
                return Format::R16_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                return Format::R16_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_INT)
                return Format::R32_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                return Format::R32_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
                return Format::R32_SFLOAT;
        }
        if (image.component == 2)
        {
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_BYTE)
                return Format::R8G8_SNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                return Format::R8G8_UNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_SHORT)
                return Format::R16G16_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                return Format::R16G16_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_INT)
                return Format::R32G32_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                return Format::R32G32_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
                return Format::R32G32_SFLOAT;
        }
        if (image.component == 3)
        {
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_BYTE)
                return Format::R8G8B8_SNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                return Format::R8G8B8_UNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_SHORT)
                return Format::R16G16B16_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                return Format::R16G16B16_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_INT)
                return Format::R32G32B32_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                return Format::R32G32B32_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
                return Format::R32G32B32_SFLOAT;
        }
        if (image.component == 4)
        {
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_BYTE)
                return Format::R8G8B8A8_SNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                return Format::R8G8B8A8_UNORM;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_SHORT)
                return Format::R16G16B16A16_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                return Format::R16G16B16A16_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_INT)
                return Format::R32G32B32A32_SINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                return Format::R32G32B32A32_UINT;
            if (image.pixel_type == TINYGLTF_COMPONENT_TYPE_FLOAT)
                return Format::R32G32B32A32_SFLOAT;
        }
        return Format::UNDEFINED;
    }

    ModelData ModelLoader::LoadFromGLTF(const std::string& filePath)
    {
        ModelData result;

        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string errorMessage, warningMessage;

        bool res = loader.LoadASCIIFromFile(&model, &errorMessage, &warningMessage, filePath);
        if (!res) return result;

        result.Materials.reserve(model.materials.size());
        for (const auto& material : model.materials)
        {
            auto& resultMaterial = result.Materials.emplace_back();
            
            resultMaterial.Name = material.name;
            resultMaterial.RoughnessScale = material.pbrMetallicRoughness.roughnessFactor;
            resultMaterial.MetallicScale = material.pbrMetallicRoughness.metallicFactor;

            if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
            {
                const auto& albedoTexture = model.images[model.textures[material.pbrMetallicRoughness.baseColorTexture.index].source];
                resultMaterial.AlbedoTexture = ImageData{ 
                    albedoTexture.image, 
                    ImageFormatFromGLTFImage(albedoTexture),
                    (uint32_t)albedoTexture.width, 
                    (uint32_t)albedoTexture.height, 
                };
            }
            else
            {
                resultMaterial.AlbedoTexture = CreateStubTexture(255, 255, 255, 255);
            }

            if (material.normalTexture.index != -1)
            {
                const auto& normalTexture = model.images[model.textures[material.normalTexture.index].source];
                resultMaterial.NormalTexture = ImageData{ 
                    normalTexture.image, 
                    ImageFormatFromGLTFImage(normalTexture),
                    (uint32_t)normalTexture.width, 
                    (uint32_t)normalTexture.height, 
                };
            }
            else
            {
                resultMaterial.NormalTexture = CreateStubTexture(127, 127, 255, 255);
            }

            if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
            {
                const auto& metallicRoughnessTexture = model.images[model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index].source];
                resultMaterial.MetallicRoughnessTexture = ImageData{
                    metallicRoughnessTexture.image,
                    ImageFormatFromGLTFImage(metallicRoughnessTexture),
                    (uint32_t)metallicRoughnessTexture.width,
                    (uint32_t)metallicRoughnessTexture.height,
                };
            }
            else
            {
                resultMaterial.MetallicRoughnessTexture = CreateStubTexture(0, 255, 0, 255);
            }

            if (material.emissiveTexture.index != -1)
            {
                const auto& emissiveTexture = model.images[model.textures[material.emissiveTexture.index].source];
                resultMaterial.EmissiveTexture = ImageData{
                    emissiveTexture.image,
                    ImageFormatFromGLTFImage(emissiveTexture),
                    (uint32_t)emissiveTexture.width,
                    (uint32_t)emissiveTexture.height,
                };
            }
            else
            {
                resultMaterial.EmissiveTexture = CreateStubTexture(0, 0, 0, 255);
            }
        }

        for (const auto& mesh : model.meshes)
        {
            result.Primitives.reserve(result.Primitives.size() + mesh.primitives.size());
            for (const auto& primitive : mesh.primitives)
            {
                auto& resultShape = result.Primitives.emplace_back();
                resultShape.Name = "shape_" + std::to_string(result.Primitives.size());
                resultShape.MaterialIndex = (uint32_t)primitive.material;

                const auto& indexAccessor = model.accessors[primitive.indices];
                auto& indexBuffer = model.bufferViews[indexAccessor.bufferView];
                const uint8_t* indexBegin = model.buffers[indexBuffer.buffer].data.data() + indexBuffer.byteOffset;

                const auto& positionAccessor = model.accessors[primitive.attributes.at("POSITION")];
                const auto& texCoordAccessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                const auto& normalAccessor = model.accessors[primitive.attributes.at("NORMAL")];

                const auto& positionBuffer = model.bufferViews[positionAccessor.bufferView];
                const auto& texCoordBuffer = model.bufferViews[texCoordAccessor.bufferView];
                const auto& normalBuffer = model.bufferViews[normalAccessor.bufferView];

                const uint8_t* positionBegin = model.buffers[positionBuffer.buffer].data.data() + positionBuffer.byteOffset;
                const uint8_t* texCoordBegin = model.buffers[texCoordBuffer.buffer].data.data() + texCoordBuffer.byteOffset;
                const uint8_t* normalBegin = model.buffers[normalBuffer.buffer].data.data() + normalBuffer.byteOffset;

                ArrayView<const Vector3> positions((const Vector3*)positionBegin, (const Vector3*)(positionBegin + positionBuffer.byteLength));
                ArrayView<const Vector2> texCoords((const Vector2*)texCoordBegin, (const Vector2*)(texCoordBegin + texCoordBuffer.byteLength));
                ArrayView<const Vector3> normals((const Vector3*)normalBegin, (const Vector3*)(normalBegin + normalBuffer.byteLength));

                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    ArrayView<const uint16_t> indices((const uint16_t*)indexBegin, (const uint16_t*)(indexBegin + indexBuffer.byteLength));
                    resultShape.Indices.resize(indices.size());
                    for (size_t i = 0; i < resultShape.Indices.size(); i++)
                        resultShape.Indices[i] = (ModelData::Index)indices[i];
                }
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    ArrayView<const uint32_t> indices((const uint32_t*)indexBegin, (const uint32_t*)(indexBegin + indexBuffer.byteLength));
                    resultShape.Indices.resize(indices.size());
                    for (size_t i = 0; i < resultShape.Indices.size(); i++)
                        resultShape.Indices[i] = (ModelData::Index)indices[i];
                }

                auto tangentsBitangents = ComputeTangentsBitangents(resultShape.Indices, positions, texCoords);

                resultShape.Vertices.resize(positions.size());
                for (size_t i = 0; i < resultShape.Vertices.size(); i++)
                {
                    auto& vertex = resultShape.Vertices[i];
                    vertex.Position = positions[i];
                    vertex.TexCoord = texCoords[i];
                    vertex.Normal = normals[i];
                    vertex.Tangent = tangentsBitangents[i].first;
                    vertex.Bitangent = tangentsBitangents[i].second;
                }
            }
        }

        return result;
    }

    static bool IsGLTFModel(const std::string& filePath)
    {
        std::filesystem::path filename{ filePath };
        return filename.extension() == ".gltf";
    }

    static bool IsObjModel(const std::string& filePath)
    {
        std::filesystem::path filename{ filePath };
        return filename.extension() == ".obj";
    }

    ModelData ModelLoader::Load(const std::string& filePath)
    {
        if (IsGLTFModel(filePath))
            return ModelLoader::LoadFromGLTF(filePath);
        if (IsObjModel(filePath))
            return ModelLoader::LoadFromObj(filePath);
        assert(false); // unknown model format
        return ModelData{ };
    }
}