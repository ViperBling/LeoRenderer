#include "VKShaderGraphic.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    GraphicShader::GraphicShader(const ShaderData &vertex, const ShaderData &fragment)
    {
        this->Init(vertex, fragment);
    }

    GraphicShader::~GraphicShader()
    {
        this->Destroy();
    }

    void GraphicShader::Init(const ShaderData &vertex, const ShaderData &fragment)
    {
        auto& vulkan = GetCurrentVulkanContext();

        vk::ShaderModuleCreateInfo vsCI {};
        vsCI.setCode(vertex.ByteCode);
        this->mVertexShader = vulkan.GetDevice().createShaderModule(vsCI);

        vk::ShaderModuleCreateInfo psCI {};
        psCI.setCode(fragment.ByteCode);
        this->mPixelShader = vulkan.GetDevice().createShaderModule(psCI);

        this->mInputAttributes = vertex.InputAttributes;
        assert(vertex.UniformDescSets.size() < 2);
        assert(fragment.UniformDescSets.size() < 2);
        this->mShaderUniforms.push_back(ShaderUniforms { vertex.UniformDescSets[0], ShaderType::VERTEX });
        this->mShaderUniforms.push_back(ShaderUniforms { fragment.UniformDescSets[0], ShaderType::FRAGMENT  });
    }

    GraphicShader::GraphicShader(GraphicShader &&other) noexcept
    {
        this->mVertexShader = other.mVertexShader;
        this->mPixelShader = other.mPixelShader;
        this->mShaderUniforms = std::move(other.mShaderUniforms);
        this->mInputAttributes = std::move(other.mInputAttributes);

        other.mVertexShader = vk::ShaderModule();
        other.mPixelShader = vk::ShaderModule();
    }

    GraphicShader &GraphicShader::operator=(GraphicShader &&other) noexcept
    {
        this->Destroy();

        this->mVertexShader = other.mVertexShader;
        this->mPixelShader = other.mPixelShader;
        this->mShaderUniforms = std::move(other.mShaderUniforms);
        this->mInputAttributes = std::move(other.mInputAttributes);

        other.mVertexShader = vk::ShaderModule();
        other.mPixelShader = vk::ShaderModule();

        return *this;
    }

    ArrayView<const TypeSPIRV> GraphicShader::GetInputAttributes() const
    {
        return this->mInputAttributes;
    }

    ArrayView<const ShaderUniforms> GraphicShader::GetShaderUniforms() const
    {
        return this->mShaderUniforms;
    }

    const vk::ShaderModule &GraphicShader::GetNativeShaderModule(ShaderType type) const
    {
        switch (type)
        {
        case ShaderType::VERTEX:
            return this->mVertexShader;
        case ShaderType::FRAGMENT:
            return this->mPixelShader;
        default:
            assert(false);
            return this->mVertexShader;
        }
    }

    void GraphicShader::Destroy()
    {
        auto& vulkan = GetCurrentVulkanContext();
        auto& device = vulkan.GetDevice();

        if ((bool)this->mVertexShader) device.destroyShaderModule(this->mVertexShader);
        if ((bool)this->mPixelShader) device.destroyShaderModule(this->mPixelShader);

        this->mVertexShader = vk::ShaderModule();
        this->mPixelShader = vk::ShaderModule();
    }
} // namespace LeoVK
