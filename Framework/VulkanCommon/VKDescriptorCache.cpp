#include "VKDescriptorCache.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    void DescriptorCache::Init()
    {
        auto& vulkan = GetCurrentVulkanContext();

        std::array descPoolSize = {
            vk::DescriptorPoolSize { vk::DescriptorType::eSampler,              1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eCombinedImageSampler, 1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eSampledImage,         1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eStorageImage,         1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eUniformTexelBuffer,   1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eStorageTexelBuffer,   1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eUniformBuffer,        1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eStorageBuffer,        1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eUniformBufferDynamic, 1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eStorageBufferDynamic, 1024 },
            vk::DescriptorPoolSize { vk::DescriptorType::eInputAttachment,      1024 },
        };
        vk::DescriptorPoolCreateInfo descPoolCI {};
        descPoolCI
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind)
            .setPoolSizes(descPoolSize)
            .setMaxSets(2048 * static_cast<uint32_t>(descPoolSize.size()));
        
        this->mDescPool = vulkan.GetDevice().createDescriptorPool(descPoolCI);
    }

    void DescriptorCache::Destroy()
    {
        auto& vulkan = GetCurrentVulkanContext();
        if ((bool)this->mDescPool) vulkan.GetDevice().destroyDescriptorPool(this->mDescPool);

        for (auto& desc : this->mDescriptor)
        {
            this->DestroyDescriptorSetLayout(desc.DescSetLayout);
        }
        this->mDescriptor.clear();
    }

    DescriptorCache::Descriptor DescriptorCache::GetDescriptor(ArrayView<const ShaderUniforms> specification)
    {
        auto descSetLayout = this->CreateDescriptorSetLayout(specification);
        auto descSet = this->AllocateDescriptorSet(descSetLayout);
        return this->mDescriptor.emplace_back(Descriptor { descSetLayout, descSet });
    }

    vk::DescriptorSetLayout DescriptorCache::CreateDescriptorSetLayout(ArrayView<const ShaderUniforms> specification)
    {
        auto& vulkan = GetCurrentVulkanContext();

        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings;
        std::vector<vk::DescriptorBindingFlags> bindingFlags;
        size_t totalUniformCount = 0;
        for (const auto& uniformPerStage : specification)
        {
            totalUniformCount += uniformPerStage.Uniforms.size();
        }
        layoutBindings.reserve(totalUniformCount);
        bindingFlags.reserve(totalUniformCount);

        for (const auto& uniforPerStage : specification)
        {
            for (const auto& uniform : uniforPerStage.Uniforms)
            {
                auto layoutIt = std::find_if(layoutBindings.begin(), layoutBindings.end(), [&uniform](const auto& layout) {
                    return layout.binding == uniform.Binding;
                });

                if (layoutIt != layoutBindings.end())
                {
                    assert(layoutIt->descriptorType == ToNative(uniform.Type));
                    assert(layoutIt->descriptorCount == uniform.Count);
                    layoutIt->stageFlags |= ToNative(uniforPerStage.ShaderStage);
                    continue;
                }
                layoutBindings.push_back(vk::DescriptorSetLayoutBinding {
                    uniform.Binding,
                    ToNative(uniform.Type),
                    uniform.Count,
                    ToNative(uniforPerStage.ShaderStage),
                });

                vk::DescriptorBindingFlags descBindingFlags = {};
                descBindingFlags |= vk::DescriptorBindingFlagBits::eUpdateAfterBind;
                if (uniform.Count > 1)
                {
                    descBindingFlags |= vk::DescriptorBindingFlagBits::ePartiallyBound;
                }
                bindingFlags.push_back(descBindingFlags);
            }
        }
        vk::DescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCI {};
        bindingFlagsCI.setBindingFlags(bindingFlags);

        vk::DescriptorSetLayoutCreateInfo descSetLayoutCI {};
        descSetLayoutCI.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
        descSetLayoutCI.setBindings(layoutBindings);
        descSetLayoutCI.setPNext(&bindingFlagsCI);

        return vulkan.GetDevice().createDescriptorSetLayout(descSetLayoutCI);
    }

    vk::DescriptorSet DescriptorCache::AllocateDescriptorSet(vk::DescriptorSetLayout layout)
    {
        auto& vulkan = GetCurrentVulkanContext();

        vk::DescriptorSetAllocateInfo descSetAI {};
        descSetAI.setDescriptorPool(this->mDescPool);
        descSetAI.setSetLayouts(layout);

        return vulkan.GetDevice().allocateDescriptorSets(descSetAI).front();
    }

    void DescriptorCache::DestroyDescriptorSetLayout(vk::DescriptorSetLayout layout)
    {
        GetCurrentVulkanContext().GetDevice().destroyDescriptorSetLayout(layout);
    }

    void DescriptorCache::FreeDescriptorSet(vk::DescriptorSet set)
    {
        GetCurrentVulkanContext().GetDevice().freeDescriptorSets(this->mDescPool, set);
    }
}