#include "VKPipeline.hpp"

namespace LeoVK
{
    void Pipeline::AddOutputAttachment(const std::string &name, ClearColor clear)
    {
        this->AddOutputAttachment(name, clear, OutputAttachment::ALL_LAYERS);
    }

    void Pipeline::AddOutputAttachment(const std::string &name, ClearDepthStencil clear)
    {
        this->AddOutputAttachment(name, clear, OutputAttachment::ALL_LAYERS);
    }

    void Pipeline::AddOutputAttachment(const std::string &name, AttachmentState onLoad)
    {
        this->AddOutputAttachment(name, onLoad, OutputAttachment::ALL_LAYERS);
    }

    void Pipeline::AddOutputAttachment(const std::string &name, ClearColor clear, uint32_t layer)
    {
        this->mOutputAttachments.push_back(OutputAttachment {
            name,
            clear,
            ClearDepthStencil{},
            AttachmentState::CLEAR_COLOR,
            layer
        });
    }

    void Pipeline::AddOutputAttachment(const std::string &name, ClearDepthStencil clear, uint32_t layer)
    {
        this->mOutputAttachments.push_back(OutputAttachment {
            name,
            ClearColor{},
            clear,
            AttachmentState::CLEAR_DEPTH_SPENCIL,
            layer
        });
    }

    void Pipeline::AddOutputAttachment(const std::string &name, AttachmentState onLoad, uint32_t layer)
    {
        this->mOutputAttachments.push_back(OutputAttachment {
            name,
            ClearColor{},
            ClearDepthStencil{},
            onLoad,
            layer
        });
    }

    void Pipeline::AddDependency(const std::string &name, BufferUsage::Bits usage)
    {
        this->mBufferDependencies.push_back(BufferDependency{ name, usage });
    }

    void Pipeline::AddDependency(const std::string& name, ImageUsage::Bits usage)
    {
        this->mImageDependencies.push_back(ImageDependency{ name, usage });
    }

    void Pipeline::DeclareAttachment(const std::string &name, Format format)
    {
        this->DeclareAttachment(name, format, 0, 0, ImageOptions::DEFAULT);
    }

    void Pipeline::DeclareAttachment(const std::string &name, Format format, uint32_t width, uint32_t height)
    {
        this->DeclareAttachment(name, format, width, height, ImageOptions::DEFAULT);
    }

    void Pipeline::DeclareAttachment(const std::string &name, Format format, uint32_t width, uint32_t height, ImageOptions::Value options)
    {
        this->mAttachDeclaration.push_back(AttachmentDeclaration {
            name,
            format,
            width,
            height,
            options
        });
    }
}