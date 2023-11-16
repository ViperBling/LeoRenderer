#pragma once

#include "ArrayUtils.hpp"

namespace LeoVK
{
    class Shader
    {
    public:
        virtual ~Shader() = default;

        virtual ArrayView<const TypeSPIRV> GetInputTypes() const = 0;
        virtual ArrayView<const ShaderUniforms> GetShaderUniforms() const = 0;
        virtual const vk::ShaderModule& GetNativeShaderModule(ShaderType type) const = 0;
    }
}