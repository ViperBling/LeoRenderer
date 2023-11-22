#include "VKDescriptorBinding.hpp"
#include "VKContext.hpp"

namespace LeoVK
{
    Sampler EmptySampler;

    ImageUsage::Bits UniformTypeToImageUsage(UniformType type)
    {
        switch (type)
        {
        case UniformType::SAMPLER:
            return ImageUsage::UNKNOWN;
        case UniformType::COMBINED_IMAGE_SAMPLER:
            return ImageUsage::SHADER_READ;
        case UniformType::SAMPLED_IMAGE:
            return ImageUsage::SHADER_READ;
        case UniformType::STORAGE_IMAGE:
            return ImageUsage::STORAGE;
        case UniformType::UNIFORM_TEXEL_BUFFER:
            return ImageUsage::UNKNOWN;
        case UniformType::STORAGE_TEXEL_BUFFER:
            return ImageUsage::UNKNOWN;
        case UniformType::UNIFORM_BUFFER:
            return ImageUsage::UNKNOWN;
        case UniformType::STORAGE_BUFFER:
            return ImageUsage::UNKNOWN;
        case UniformType::UNIFORM_BUFFER_DYNAMIC:
            return ImageUsage::UNKNOWN;
        case UniformType::STORAGE_BUFFER_DYNAMIC:
            return ImageUsage::UNKNOWN;
        case UniformType::INPUT_ATTACHMENT:
            return ImageUsage::INPUT_ATTACHMENT;
        case UniformType::INLINE_UNIFORM_BLOCK_EXT:
            return ImageUsage::UNKNOWN;
        case UniformType::ACCELERATION_STRUCTURE_KHR:
            return ImageUsage::UNKNOWN;
        default:
            return ImageUsage::UNKNOWN;
        }
    }

    BufferUsage::Bits UniformTypeToBufferUsage(UniformType type)
    {
        switch (type)
        {
        case UniformType::SAMPLER:
            return BufferUsage::UNKNOWN;
        case UniformType::COMBINED_IMAGE_SAMPLER:
            return BufferUsage::UNKNOWN;
        case UniformType::SAMPLED_IMAGE:
            return BufferUsage::UNKNOWN;
        case UniformType::STORAGE_IMAGE:
            return BufferUsage::UNKNOWN;
        case UniformType::UNIFORM_TEXEL_BUFFER:
            return BufferUsage::UNIFORM_TEXEL_BUFFER;
        case UniformType::STORAGE_TEXEL_BUFFER:
            return BufferUsage::STORAGE_TEXEL_BUFFER;
        case UniformType::UNIFORM_BUFFER:
            return BufferUsage::UNIFORM_BUFFER;
        case UniformType::STORAGE_BUFFER:
            return BufferUsage::STORAGE_BUFFER;
        case UniformType::UNIFORM_BUFFER_DYNAMIC:
            return BufferUsage::UNIFORM_BUFFER;
        case UniformType::STORAGE_BUFFER_DYNAMIC:
            return BufferUsage::STORAGE_BUFFER;
        case UniformType::INPUT_ATTACHMENT:
            return BufferUsage::UNKNOWN;
        case UniformType::INLINE_UNIFORM_BLOCK_EXT:
            return BufferUsage::UNIFORM_BUFFER;
        case UniformType::ACCELERATION_STRUCTURE_KHR:
            return BufferUsage::ACCELERATION_STRUCTURE_STORAGE;
        default:
            return BufferUsage::UNKNOWN;
        }
    }

    bool IsBufferType(UniformType type)
    {
        switch (type)
        {
        case UniformType::UNIFORM_TEXEL_BUFFER:
        case UniformType::STORAGE_TEXEL_BUFFER:
        case UniformType::UNIFORM_BUFFER:
        case UniformType::STORAGE_BUFFER:
        case UniformType::UNIFORM_BUFFER_DYNAMIC:
        case UniformType::STORAGE_BUFFER_DYNAMIC:
        case UniformType::INLINE_UNIFORM_BLOCK_EXT:
            return true;
        default:
            return false;
        }
    }

    // ====================================== ResolveInfo ====================================== //

    void ResolveInfo::Resolve(const std::string &name, const Buffer &buffer)
    {
        assert(this->mBufferResolves.find(name) == this->mBufferResolves.end());
        this->mBufferResolves[name] = {buffer};
    }

    void ResolveInfo::Resolve(const std::string &name, const Image &image)
    {
        assert(this->mImageResolves.find(name) == this->mImageResolves.end());
        this->mImageResolves[name] = {image};
    }

    void ResolveInfo::Resolve(const std::string &name, ArrayView<const BufferReference> buffers)
    {
        assert(this->mBufferResolves.find(name) == this->mBufferResolves.end());
        for (const auto &buffer : buffers)
        {
            this->mBufferResolves[name].push_back(buffer);
        }
    }

    void ResolveInfo::Resolve(const std::string &name, ArrayView<const Buffer> buffers)
    {
        assert(this->mBufferResolves.find(name) == this->mBufferResolves.end());
        for (const auto &buffer : buffers)
        {
            this->mBufferResolves[name].push_back(buffer);
        }
    }

    void ResolveInfo::Resolve(const std::string &name, ArrayView<const Image> images)
    {
        assert(this->mImageResolves.find(name) == this->mImageResolves.end());
        for (const auto &image : images)
        {
            this->mImageResolves[name].push_back(image);
        }
    }

    void ResolveInfo::Resolve(const std::string &name, ArrayView<const ImageReference> images)
    {
        assert(this->mImageResolves.find(name) == this->mImageResolves.end());
        for (const auto &image : images)
        {
            this->mImageResolves[name].push_back(image);
        }
    }

    // ====================================== DescriptorBinding ====================================== //

    DescriptorBinding &DescriptorBinding::Bind(uint32_t binding, const std::string &name, UniformType type)
    {
        if (UniformTypeToBufferUsage(type) == BufferUsage::UNKNOWN) // fall back to image
            return this->Bind(binding, name, EmptySampler, type, ImageView::NATIVE);

        this->mBufferToResolve.push_back(BufferToResolve{
            name,
            binding,
            type,
            UniformTypeToBufferUsage(type),
        });
        return *this;
    }

    DescriptorBinding &DescriptorBinding::Bind(uint32_t binding, const std::string &name, UniformType type, ImageView view)
    {
        return this->Bind(binding, name, EmptySampler, type, view);
    }

    DescriptorBinding &DescriptorBinding::Bind(uint32_t binding, const std::string &name, const Sampler &sampler, UniformType type)
    {
        return this->Bind(binding, name, sampler, type, ImageView::NATIVE);
    }

    DescriptorBinding &DescriptorBinding::Bind(uint32_t binding, const std::string &name, const Sampler &sampler, UniformType type, ImageView view)
    {
        this->mImageToResolve.push_back(ImageToResolve{
            name,
            binding,
            type,
            UniformTypeToImageUsage(type),
            view,
            std::addressof(sampler),
        });
        return *this;
    }

    DescriptorBinding &DescriptorBinding::Bind(uint32_t binding, const Sampler &sampler, UniformType type)
    {
        this->mSamplerToResolve.push_back(SamplerToResolve{
            std::addressof(sampler),
            binding,
            type,
        });
        return *this;
    }

    void DescriptorBinding::Resolve(const ResolveInfo &resolveInfo)
    {
        this->mImageWriteInfos.clear();
        this->mBufferWirteInfos.clear();
        this->mDescWrites.clear();

        for (const auto& imageToResolve : this->mImageToResolve)
        {
            auto& images = resolveInfo.GetImages().at(imageToResolve.Name);
            size_t index = 0;
            if ((bool)imageToResolve.SamplerHandle->GetNativeSampler())
            {
                for (const auto& image : images)
                {
                    index = this->AllocateBinding(image.get(), *imageToResolve.SamplerHandle, imageToResolve.View, imageToResolve.Type);
                }
            }
            else
            {
                for (const auto& image : images)
                {
                    index = this->AllocateBinding(image.get(), imageToResolve.View, imageToResolve.Type);
                }
            }
            this->mDescWrites.push_back({
                imageToResolve.Type,
                imageToResolve.Binding,
                static_cast<uint32_t>(index + 1 - images.size()),
                static_cast<uint32_t>(images.size()),
            });
        }

        for (const auto& bufferToResolve : this->mBufferToResolve)
		{
			auto& buffers = resolveInfo.GetBuffers().at(bufferToResolve.Name);
			size_t index = 0;
			for (const auto& buffer : buffers)
            {
                index = this->AllocateBinding(buffer.get(), bufferToResolve.Type);
            }

			this->mDescWrites.push_back({
				bufferToResolve.Type,
				bufferToResolve.Binding,
				uint32_t(index + 1 - buffers.size()),
				uint32_t(buffers.size())
			});
		}

		for (const auto& samplerToResolve : this->mSamplerToResolve)
		{
			size_t index = this->AllocateBinding(*samplerToResolve.SamplerHandle);

			this->mDescWrites.push_back({
				samplerToResolve.Type,
				samplerToResolve.Binding,
				static_cast<uint32_t>(index),
				static_cast<uint32_t>(1)
			});
		}
    }

    void DescriptorBinding::Write(const vk::DescriptorSet &descriptorSet)
    {
        if (this->mOptions == ResolveOptions::ALREADY_RESOLVED) return;
        if (this->mOptions == ResolveOptions::RESOLVE_ONCE)
        {
            this->mOptions = ResolveOptions::ALREADY_RESOLVED;
        }

        std::vector<vk::WriteDescriptorSet> descWrites;
        std::vector<vk::DescriptorBufferInfo> descBufferInfos;
        std::vector<vk::DescriptorImageInfo> descImageInfos;
        descWrites.reserve(this->mDescWrites.size());
        descBufferInfos.reserve(this->mBufferWirteInfos.size());
        descImageInfos.reserve(this->mImageWriteInfos.size());

        // 绑定Buffer和Image到到管线上
        for (const auto& bufferInfo : this->mBufferWirteInfos)
        {
            descBufferInfos.push_back(vk::DescriptorBufferInfo{
                bufferInfo.Handle->GetNativeBuffer(),
                0,
                bufferInfo.Handle->GetSize(),
            });
        }

        for (const auto& imageInfo : this->mImageWriteInfos)
        {
            descImageInfos.push_back(vk::DescriptorImageInfo{
                imageInfo.SamplerHandle != nullptr ? imageInfo.SamplerHandle->GetNativeSampler() : nullptr,
                imageInfo.Handle != nullptr ? imageInfo.Handle->GetNativeView(imageInfo.View) : nullptr,
                ImageUsageToImageLayout(imageInfo.Usage),
            });
        }

        for (const auto& write : this->mDescWrites)
        {
            auto& writeDescSet = descWrites.emplace_back();
            writeDescSet.setDstSet(descriptorSet);
            writeDescSet.setDstBinding(write.Binding);
            writeDescSet.setDescriptorType(ToNative(write.Type));
            writeDescSet.setDescriptorCount(write.Count);

            if (IsBufferType(write.Type))
            {
                writeDescSet.setPBufferInfo(descBufferInfos.data() + write.FirstIndex);
            }
            else
            {
                writeDescSet.setPImageInfo(descImageInfos.data() + write.FirstIndex);
            }
        }
        GetCurrentVulkanContext().GetDevice().updateDescriptorSets(descWrites, {});
    }

    size_t DescriptorBinding::AllocateBinding(const Buffer &buffer, UniformType type)
    {
        this->mBufferWirteInfos.push_back(BufferWriteInfo{
            std::addressof(buffer),
            UniformTypeToBufferUsage(type),
        });
        return this->mBufferWirteInfos.size() - 1;
    }

    size_t DescriptorBinding::AllocateBinding(const Image &image, ImageView view, UniformType type)
    {
        this->mImageWriteInfos.push_back(ImageWriteInfo{
            std::addressof(image),
            UniformTypeToImageUsage(type),
            view,
            {},
        });
        return this->mImageWriteInfos.size() - 1;
    }

    size_t DescriptorBinding::AllocateBinding(const Image &image, const Sampler &sampler, ImageView view, UniformType type)
    {
        this->mImageWriteInfos.push_back(ImageWriteInfo{
            std::addressof(image),
            UniformTypeToImageUsage(type),
            view,
            std::addressof(sampler),
        });
        return this->mImageWriteInfos.size() - 1;
    }

    size_t DescriptorBinding::AllocateBinding(const Sampler &sampler)
    {
        this->mImageWriteInfos.push_back(ImageWriteInfo{
            nullptr,
            ImageUsage::UNKNOWN,
            {},
            std::addressof(sampler),
        });
        return this->mImageWriteInfos.size() - 1;
    }
}