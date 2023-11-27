#pragma once

#include "VKShader.hpp"
#include "VKDescriptorBinding.hpp"
#include "VKCommandBuffer.hpp"

namespace LeoVK
{
    enum class AttachmentState
    {
        DISCARD_COLOR = 0,
        DISCARD_DEPTH_SPENCIL,
        LOAD_COLOR,
        LOAD_DEPTH_STENCIL,
        CLEAR_COLOR,
        CLEAR_DEPTH_SPENCIL,
    };

    class Pipeline
    {
    public:
        void AddOutputAttachment(const std::string& name, ClearColor clear);
        void AddOutputAttachment(const std::string& name, ClearDepthStencil clear);
        void AddOutputAttachment(const std::string& name, AttachmentState onLoad);
        void AddOutputAttachment(const std::string& name, ClearColor clear, uint32_t layer);
        void AddOutputAttachment(const std::string& name, ClearDepthStencil clear, uint32_t layer);
        void AddOutputAttachment(const std::string& name, AttachmentState onLoad, uint32_t layer);

        void AddDependency(const std::string& name, BufferUsage::Bits usage);
        void AddDependency(const std::string& name, ImageUsage::Bits usage);

        void DeclareAttachment(const std::string& name, Format format);
        void DeclareAttachment(const std::string& name, Format format, uint32_t width, uint32_t height);
        void DeclareAttachment(const std::string& name, Format format, uint32_t width, uint32_t height, ImageOptions::Value options);

        const auto& GetBufferDependencies() const { return this->mBufferDependencies; }
        const auto& GetImageDependencies() const { return this->mImageDependencies; }
        const auto& GetAttachmentDeclarations() const { return this->mAttachDeclaration; }
        const auto& GetOutputAttachments() const { return this->mOutputAttachments; }

    public:
        struct BufferDeclaration
        {
            std::string Name;
        };

        struct ImageDeclaration
        {
            std::string Name;
        };

        struct ImageDependency
        {
            std::string Name;
            ImageUsage::Bits Usage;
        };

        struct BufferDependency
        {
            std::string Name;
            BufferUsage::Bits Usage;
        };

        struct AttachmentDeclaration
        {
            std::string Name;
            Format ImageFormat;
            uint32_t Width;
            uint32_t Height;
            ImageOptions::Value Options;
        };

        struct OutputAttachment
        {
            constexpr static uint32_t ALL_LAYERS = uint32_t(-1);

            std::string Name;
            ClearColor ColorClear;
            ClearDepthStencil DepthSpencilClear;
            AttachmentState OnLoad;
            uint32_t Layer;
        };

        std::shared_ptr<Shader> mShader;
        std::vector<VertexBinding> mVertexBindings;
        std::vector<DescriptorBinding> mDescBindings;

    private:
        std::vector<BufferDependency> mBufferDependencies;
        std::vector<ImageDependency> mImageDependencies;

        std::vector<AttachmentDeclaration> mAttachDeclaration;
        std::vector<OutputAttachment> mOutputAttachments;
    };
}