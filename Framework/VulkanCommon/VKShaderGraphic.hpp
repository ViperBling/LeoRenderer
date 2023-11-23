#pragma once

#include "VKShader.hpp"
#include "Utilities/AssetLoader.hpp"

namespace LeoVK
{
    class GraphicShader : public Shader
    {
    public:
        GraphicShader() = default;
        GraphicShader(const ShaderData& vertex, const ShaderData& fragment);
        ~GraphicShader();

        void Init(const ShaderData& vertex, const ShaderData& fragment);

        GraphicShader(const GraphicShader& other) = delete;
        GraphicShader(GraphicShader&& other) noexcept;
        GraphicShader& operator=(const GraphicShader& other) = delete;
        GraphicShader& operator=(GraphicShader&& other) noexcept;


        ArrayView<const TypeSPIRV> GetInputAttributes() const override;
        ArrayView<const ShaderUniforms> GetShaderUniforms() const override;
        virtual const vk::ShaderModule& GetNativeShaderModule(ShaderType type) const override;

    private:
        void Destroy();

    private:
        vk::ShaderModule mVertexShader;
        vk::ShaderModule mPixelShader;
        std::vector<ShaderUniforms> mShaderUniforms;
        std::vector<TypeSPIRV> mInputAttributes;
    };
}