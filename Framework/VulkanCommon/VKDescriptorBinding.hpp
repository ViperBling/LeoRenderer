#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "VKBuffer.hpp"
#include "VKImage.hpp"
#include "VKSampler.hpp"
#include "VKShaderReflection.hpp"
#include "ArrayUtils.hpp"

namespace LeoVK
{
    class ResolveInfo
    {
    public:
        void Resolve(const std::string &name, const Buffer &buffer);
        void Resolve(const std::string &name, ArrayView<const Buffer> buffers);
        void Resolve(const std::string &name, ArrayView<const BufferReference> buffers);

        void Resolve(const std::string &name, const Image &image);
        void Resolve(const std::string &name, ArrayView<const Image> images);
        void Resolve(const std::string &name, ArrayView<const ImageReference> images);

        const auto &GetBuffers() const { return this->mBufferResolves; }
        const auto &GetImages() const { return this->mImageResolves; }

    private:
        std::unordered_map<std::string, std::vector<BufferReference>> mBufferResolves;
        std::unordered_map<std::string, std::vector<ImageReference>> mImageResolves;
    };

    enum class ResolveOptions
    {
        RESOLVE_EACH_FRAME,
		RESOLVE_ONCE,
		ALREADY_RESOLVED,
    };

    class DescriptorBinding
    {
    public:
		DescriptorBinding& Bind(uint32_t binding, const std::string& name, UniformType type);
		DescriptorBinding& Bind(uint32_t binding, const std::string& name, UniformType type, ImageView view);
		DescriptorBinding& Bind(uint32_t binding, const std::string& name, const Sampler& sampler, UniformType type);
		DescriptorBinding& Bind(uint32_t binding, const std::string& name, const Sampler& sampler, UniformType type, ImageView view);

		DescriptorBinding& Bind(uint32_t binding, const Sampler& sampler, UniformType type);

		void SetOptions(ResolveOptions options) { this->options = options; }

		void Resolve(const ResolveInfo& resolveInfo);

		void Write(const vk::DescriptorSet& descriptorSet);
		const auto& GetBoundBuffers() const { return this->mBufferToResolve; }
		const auto& GetBoundImages() const { return this->mImageToResolve; }

    private:
        size_t AllocateBinding(const Buffer& buffer, UniformType type);
		size_t AllocateBinding(const Image& image, ImageView view, UniformType type);
		size_t AllocateBinding(const Image& image, const Sampler& sampler, ImageView view, UniformType type);
		size_t AllocateBinding(const Sampler& sampler);

    private:
        struct DescriptorWriteInfo
		{
			UniformType Type;
			uint32_t Binding;
			uint32_t FirstIndex;
			uint32_t Count;
		};

		struct BufferWriteInfo
		{
			const Buffer* Handle;
			BufferUsage::Bits Usage;
		};

		struct ImageWriteInfo
		{
			const Image* Handle;
			ImageUsage::Bits Usage;
			ImageView View;
			const Sampler* SamplerHandle;
		};

		struct ImageToResolve
		{
			std::string Name;
			uint32_t Binding;
			UniformType Type;
			ImageUsage::Bits Usage;
			ImageView View;
			const Sampler* SamplerHandle;
		};

		struct BufferToResolve
		{
			std::string Name;
			uint32_t Binding;
			UniformType Type;
			BufferUsage::Bits Usage;
		};

		struct SamplerToResolve
		{
			const Sampler* SamplerHandle;
			uint32_t Binding;
			UniformType Type;
		};

		std::vector<DescriptorWriteInfo> mDescWrites;
		std::vector<BufferWriteInfo> mBufferWirteInfos;
		std::vector<ImageWriteInfo> mImageWriteInfos;
		std::vector<BufferToResolve> mBufferToResolve;
		std::vector<ImageToResolve> mImageToResolve;
		std::vector<SamplerToResolve> mSamplerToResolve;

		ResolveOptions options = ResolveOptions::RESOLVE_EACH_FRAME;
    };
}