#include "VKSampler.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    vk::Filter FilterToNative(Sampler::Filter filter)
    {
        switch (filter)
        {
        case Sampler::Filter::NEAREST:
            return vk::Filter::eNearest;
        case Sampler::Filter::LINEAR:
            return vk::Filter::eLinear;
        default:
            assert(false);
            return vk::Filter::eNearest;
        }
    }

    vk::SamplerMipmapMode MipmapToNative(Sampler::Filter filter)
    {
        switch (filter)
        {
        case Sampler::Filter::NEAREST:
            return vk::SamplerMipmapMode::eNearest;
        case Sampler::Filter::LINEAR:
            return vk::SamplerMipmapMode::eLinear;
        default:
            assert(false);
            return vk::SamplerMipmapMode::eNearest;
        }
    }

    vk::SamplerAddressMode AddressToNative(Sampler::AddressMode address)
    {
        switch (address)
        {
        case Sampler::AddressMode::REPEAT:
            return vk::SamplerAddressMode::eRepeat;
        case Sampler::AddressMode::MIRRORED_REPEAT:
            return vk::SamplerAddressMode::eMirroredRepeat;
        case Sampler::AddressMode::CLAMP_TO_EDGE:
            return vk::SamplerAddressMode::eClampToEdge;
        case Sampler::AddressMode::CLAMP_TO_BORDER:
            return vk::SamplerAddressMode::eClampToBorder;
        default:
            assert(false);
            return vk::SamplerAddressMode::eClampToEdge;
        }
    }

    Sampler::~Sampler()
    {
        this->Destroy();
    }

    Sampler::Sampler(Sampler &&other) noexcept
    {
        this->mSampler = other.mSampler;
        other.mSampler = vk::Sampler();
    }

    Sampler &Sampler::operator=(Sampler &&other) noexcept
    {
        this->Destroy();
        this->mSampler = other.mSampler;
        other.mSampler = vk::Sampler();
        return *this;
    }

    Sampler::Sampler(MinFilter minFilter, MagFilter magFilter, AddressMode uvwAddress, MipFilter mipFilter)
    {
        this->Init(minFilter, magFilter, uvwAddress, mipFilter);
    }

    void Sampler::Init(MinFilter minFilter, MagFilter magFilter, AddressMode uvwAddress, MipFilter mipFilter)
    {
        vk::SamplerCreateInfo samplerCI {};
        samplerCI.setMinFilter(FilterToNative(minFilter));
        samplerCI.setMagFilter(FilterToNative(magFilter));
        samplerCI.setAddressModeU(AddressToNative(uvwAddress));
        samplerCI.setAddressModeV(AddressToNative(uvwAddress));
        samplerCI.setAddressModeW(AddressToNative(uvwAddress));
        samplerCI.setMipmapMode(MipmapToNative(mipFilter));
        samplerCI.setMipLodBias(0.0f);
        samplerCI.setMinLod(0.0f);
        samplerCI.setMaxLod(0.0f);

        this->mSampler = GetCurrentVulkanContext().GetDevice().createSampler(samplerCI);
    }

    const vk::Sampler &Sampler::GetNativeSampler() const
    {
        return this->mSampler;
    }

    void Sampler::Destroy()
    {
        if ((bool)this->mSampler)
        {
            GetCurrentVulkanContext().GetDevice().destroySampler(this->mSampler);
            this->mSampler = vk::Sampler();
        }
    }
}