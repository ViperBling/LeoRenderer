#include "VKShaderCompute.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    ArrayView<const TypeSPIRV> ComputeShader::GetInputAttributes() const
    {
        return { };
    }

    ArrayView<const ShaderUniforms> ComputeShader::GetShaderUniforms() const
    {
        return this->mShaderUniforms;
    }

    ComputeShader::ComputeShader(const ShaderData &computeData)
    {
        this->Init(computeData);
    }

    ComputeShader::~ComputeShader()
    {
        this->Destroy();
    }

    void ComputeShader::Init(const ShaderData &computeData)
    {
        auto& vulkan = GetCurrentVulkanContext();

        vk::ShaderModuleCreateInfo shaderCI {};
        shaderCI.setCode(computeData.ByteCode);
        this->mComputeShader = vulkan.GetDevice().createShaderModule(shaderCI);

        assert(computeData.UniformDescSets.size() < 2);
        this->mShaderUniforms.push_back(ShaderUniforms { computeData.UniformDescSets[0], ShaderType::COMPUTE  });
    }

    ComputeShader::ComputeShader(ComputeShader &&other) noexcept
    {
        this->mComputeShader = other.mComputeShader;
        this->mShaderUniforms = std::move(other.mShaderUniforms);
        
        other.mComputeShader = vk::ShaderModule();
    }

    ComputeShader &ComputeShader::operator=(ComputeShader &&other) noexcept
    {
        this->Destroy();

        this->mComputeShader = other.mComputeShader;
        this->mShaderUniforms = std::move(other.mShaderUniforms);

        other.mComputeShader = vk::ShaderModule();

        return *this;
    }

    ArrayView<const TypeSPIRV> ComputeShader::GetInputAttributes() const
    {
        return ArrayView<const TypeSPIRV>();
    }

    ArrayView<const ShaderUniforms> ComputeShader::GetShaderUniforms() const
    {
        return ArrayView<const ShaderUniforms>();
    }

    const vk::ShaderModule &ComputeShader::GetNativeShaderModule(ShaderType type) const
    {
        assert(type == ShaderType::COMPUTE);
        return this->mComputeShader;
    }

    void ComputeShader::Destroy()
    {
        auto& vulkan = GetCurrentVulkanContext();
        auto& device = vulkan.GetDevice();
        if ((bool)this->mComputeShader) device.destroyShaderModule(this->mComputeShader);

        this->mComputeShader = vk::ShaderModule();
    }
}