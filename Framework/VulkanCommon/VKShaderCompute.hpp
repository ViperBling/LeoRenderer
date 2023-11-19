#pragma once

#include "VKShader.hpp"
#include "Utilities/AssetLoader.hpp"

namespace LeoVK
{
    class ComputeShader : public Shader
    {
    public:
        ComputeShader() = default;
        ComputeShader(const ShaderData& computeData);
        virtual ~ComputeShader();

        void Init(const ShaderData& computeData);

        ComputeShader(const ComputeShader& other) = delete;
        ComputeShader(ComputeShader&& other) noexcept;
        ComputeShader& operator=(const ComputeShader& other) = delete;
        ComputeShader& operator=(ComputeShader&& other) noexcept;

        ArrayView<const TypeSPIRV> GetInputAttributes() const override;
        ArrayView<const ShaderUniforms> GetShaderUniforms() const override;
        virtual const vk::ShaderModule& GetNativeShaderModule(ShaderType type) const override;

    private:
        void Destroy();

    private:
        vk::ShaderModule mComputeShader;
        std::vector<ShaderUniforms> mShaderUniforms;
    };
}