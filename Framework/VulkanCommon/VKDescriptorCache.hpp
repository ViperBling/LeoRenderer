#pragma once

#include <vulkan/vulkan.hpp>

#include "VKShaderReflection.hpp"
#include "ArrayUtils.hpp"

namespace LeoVK
{
    class DescriptorCache
    {
    public:
        struct Descriptor
        {
            vk::DescriptorSetLayout DescSetLayout;
            vk::DescriptorSet DescSet;
        };
    public:
        void Init();
        void Destroy();
        const auto& GetDescriptorPool() const { return mDescPool; }
        Descriptor GetDescriptor(ArrayView<const ShaderUniforms> specification);
    
    private:
        vk::DescriptorSetLayout CreateDescriptorSetLayout(ArrayView<const ShaderUniforms> specification);
        vk::DescriptorSet vkAllocateDescriptorSet(vk::DescriptorSetLayout layout);
        void DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout);
        void FreeDescriptorSet(vk::DescriptorSet set);

    private:
        vk::DescriptorPool mDescPool;
        std::vector<Descriptor> mDescSets;
    };
}