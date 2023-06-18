#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "GLTFLoader.h"

VkDescriptorSetLayout LeoRenderer::descriptorSetLayoutImage = VK_NULL_HANDLE;
VkDescriptorSetLayout LeoRenderer::descriptorSetLayoutUBO = VK_NULL_HANDLE;
VkMemoryPropertyFlags LeoRenderer::memoryPropertyFlags = 0;
uint32_t LeoRenderer::descriptorBindingFlags = LeoRenderer::DescriptorBindingFlags::ImageBaseColor;

bool loadImageDataFunc(
    tinygltf::Image* image,
    const int imageIndex,
    std::string* error,
    std::string* warning,
    int req_width, int req_height,
    const unsigned char* bytes,
    int size, void* userData)
{
    if (image->uri.find_last_of('.') != std::string::npos)
    {
        if (image->uri.substr(image->uri.find_last_of('.') + 1) == "ktx") return true;
    }

    return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
}

bool loadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
{
    // This function will be used for samples that don't require images to be loaded
    return true;
}

LeoRenderer::BoundingBox::BoundingBox()
{
}

LeoRenderer::BoundingBox::BoundingBox(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max)
{
}

LeoRenderer::BoundingBox LeoRenderer::BoundingBox::GetAABB(glm::mat4 mat)
{
    auto min = glm::vec3(mat[3]);
    glm::vec3 max = min;
    glm::vec3 v0, v1;

    auto right = glm::vec3(mat[0]);
    v0 = right * this->mMin.x;
    v1 = right * this->mMax.x;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    auto up = glm::vec3(mat[1]);
    v0 = up * this->mMin.y;
    v1 = up * this->mMax.y;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    auto back = glm::vec3(mat[2]);
    v0 = back * this->mMin.z;
    v1 = back * this->mMax.z;
    min += glm::min(v0, v1);
    max += glm::max(v0, v1);

    return {min, max};
}

void LeoRenderer::Texture::UpdateDescriptor()
{
    mDescriptor.sampler = mSampler;
    mDescriptor.imageView = mImageView;
    mDescriptor.imageLayout = mImageLayout;
}

void LeoRenderer::Texture::OnDestroy()
{
    if (mDevice)
    {
        vkDestroyImageView(mDevice->logicalDevice, mImageView, nullptr);
        vkDestroyImage(mDevice->logicalDevice, mImage, nullptr);
        vkFreeMemory(mDevice->logicalDevice, mDeviceMemory, nullptr);
        vkDestroySampler(mDevice->logicalDevice, mSampler, nullptr);
    }
}

void LeoRenderer::Texture::FromGLTFImage(
    tinygltf::Image &gltfImage,
    TextureSampler texSampler,
    vks::VulkanDevice *device,
    VkQueue copyQueue)
{
    this->mDevice = device;

    unsigned char* buffer = nullptr;
    VkDeviceSize bufferSize = 0;
    bool deleteBuffer = false;
    if (gltfImage.component == 3) {
        // Most devices don't support RGB only on Vulkan so convert if necessary
        bufferSize = gltfImage.width * gltfImage.height * 4;
        buffer = new unsigned char[bufferSize];
        unsigned char* rgba = buffer;
        unsigned char* rgb = &gltfImage.image[0];
        for (size_t i = 0; i < gltfImage.width * gltfImage.height; ++i) {
            for (int32_t j = 0; j < 3; ++j) {
                rgba[j] = rgb[j];
            }
            rgba += 4;
            rgb += 3;
        }
        deleteBuffer = true;
    }
    else {
        buffer = &gltfImage.image[0];
        bufferSize = gltfImage.image.size();
    }
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

    VkFormatProperties formatProperties;

    mWidth = gltfImage.width;
    mHeight = gltfImage.height;
    mMipLevels = static_cast<uint32_t>(floor(log2(std::max(mWidth, mHeight))) + 1.0);

    vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);
    assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
    assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

    VkMemoryAllocateInfo memAllocInfo{};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkMemoryRequirements memReqs{};

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK_RESULT(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
    vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    VK_CHECK_RESULT(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    uint8_t* data;
    VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
    memcpy(data, buffer, bufferSize);
    vkUnmapMemory(device->logicalDevice, stagingMemory);

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.mipLevels = mMipLevels;
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = { mWidth, mHeight, 1 };
    imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &mImage));
    vkGetImageMemoryRequirements(device->logicalDevice, mImage, &memReqs);
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mDeviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, mImage, mDeviceMemory, 0));

    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 1;
    {
        // 管线同步屏障，拷贝前
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.image = mImage;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    VkBufferImageCopy bufferCopyRegion = {};
    bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    bufferCopyRegion.imageSubresource.mipLevel = 0;
    bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
    bufferCopyRegion.imageSubresource.layerCount = 1;
    bufferCopyRegion.imageExtent.width = mWidth;
    bufferCopyRegion.imageExtent.height = mHeight;
    bufferCopyRegion.imageExtent.depth = 1;
    // 从StagingBuffer拷贝图片内容到mImage
    vkCmdCopyBufferToImage(copyCmd, stagingBuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
    {
        // 管线同步屏障，拷贝后
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        imageMemoryBarrier.image = mImage;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }

    device->flushCommandBuffer(copyCmd, copyQueue, true);

    vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

    // Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
    VkCommandBuffer blitCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    for (uint32_t i = 1; i < mMipLevels; i++)
    {
        VkImageBlit imageBlit{};

        imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.srcSubresource.layerCount = 1;
        imageBlit.srcSubresource.mipLevel = i - 1;
        imageBlit.srcOffsets[1].x = int32_t(mWidth >> (i - 1));
        imageBlit.srcOffsets[1].y = int32_t(mHeight >> (i - 1));
        imageBlit.srcOffsets[1].z = 1;

        imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageBlit.dstSubresource.layerCount = 1;
        imageBlit.dstSubresource.mipLevel = i;
        imageBlit.dstOffsets[1].x = int32_t(mWidth >> i);
        imageBlit.dstOffsets[1].y = int32_t(mHeight >> i);
        imageBlit.dstOffsets[1].z = 1;

        VkImageSubresourceRange mipSubRange = {};
        mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipSubRange.baseMipLevel = i;
        mipSubRange.levelCount = 1;
        mipSubRange.layerCount = 1;

        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = 0;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = mipSubRange;
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        vkCmdBlitImage(blitCmd, mImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = mImage;
            imageMemoryBarrier.subresourceRange = mipSubRange;
            vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
    }

    subresourceRange.levelCount = mMipLevels;
    mImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    {
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.image = mImage;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
    }
    device->flushCommandBuffer(blitCmd, copyQueue, true);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = texSampler.mMagFilter;
    samplerInfo.minFilter = texSampler.mMinFilter;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = texSampler.mAddressModeU;
    samplerInfo.addressModeV = texSampler.mAddressModeV;
    samplerInfo.addressModeW = texSampler.mAddressModeW;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxLod = (float)mMipLevels;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &mSampler));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = mImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.levelCount = mMipLevels;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &mImageView));

    mDescriptor.sampler = mSampler;
    mDescriptor.imageView = mImageView;
    mDescriptor.imageLayout = mImageLayout;

    if (deleteBuffer) delete[] buffer;
}

void LeoRenderer::Primitive::SetBoundingBox(glm::vec3 min, glm::vec3 max)
{
    mBBox.mMin = min;
    mBBox.mMax = max;
    mBBox.mbValid = true;
}

// ============================== Mesh ==============================
LeoRenderer::Mesh::Mesh(vks::VulkanDevice *device, glm::mat4 matrix)
{
    mDevice = device;
    mUniformBlock.matrix = matrix;
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sizeof(mUniformBlock),
        &mUniformBuffer.buffer,
        &mUniformBuffer.memory,
        &mUniformBlock));
    VK_CHECK_RESULT(vkMapMemory(device->logicalDevice, mUniformBuffer.memory, 0, sizeof(mUniformBlock), 0, &mUniformBuffer.mapped));
    mUniformBuffer.descriptor = {mUniformBuffer.buffer, 0, sizeof(mUniformBlock)};
}

LeoRenderer::Mesh::~Mesh()
{
    vkDestroyBuffer(mDevice->logicalDevice, mUniformBuffer.buffer, nullptr);
    vkFreeMemory(mDevice->logicalDevice, mUniformBuffer.memory, nullptr);
    for (auto primitive : mPrimitives) delete primitive;
}

void LeoRenderer::Mesh::SetBoundingBox(glm::vec3 min, glm::vec3 max)
{
    mBBox.mMin = min;
    mBBox.mMax = max;
    mBBox.mbValid = true;
}

// ============================== Node ==============================
glm::mat4 LeoRenderer::Node::LocalMatrix()
{
    return glm::translate(glm::mat4(1.0f), mTranslation) * glm::mat4(mRotation) * glm::scale(glm::mat4(1.0f), mScale) * mMatrix;
}

glm::mat4 LeoRenderer::Node::GetMatrix()
{
    glm::mat4 localMat = LocalMatrix();
    LeoRenderer::Node* p = mParent;
    while (p)
    {
        localMat = p->LocalMatrix() * localMat;
        p = p->mParent;
    }
    return localMat;
}

void LeoRenderer::Node::Update()
{
    if (mMesh)
    {
        glm::mat localMat = GetMatrix();
        if (mSkin)
        {
            mMesh->mUniformBlock.matrix = localMat;
            // Update joint
            glm::mat4 inverseTransform = glm::inverse(localMat);
            for (size_t i = 0; i < mSkin->mJoints.size(); i++)
            {
                LeoRenderer::Node* joinNode = mSkin->mJoints[i];
                glm::mat4 joinMat = joinNode->GetMatrix() * mSkin->mInverseBindMatrices[i];
                joinMat = inverseTransform * joinMat;
                mMesh->mUniformBlock.jointMatrix[i] = joinMat;
            }
            mMesh->mUniformBlock.jointCount = (float)mSkin->mJoints.size();
            memcpy(mMesh->mUniformBuffer.mapped, &mMesh->mUniformBlock, sizeof(mMesh->mUniformBlock));
        }
        else
        {
            memcpy(mMesh->mUniformBuffer.mapped, &localMat, sizeof(glm::mat4));
        }
    }
    for (auto& child : mChildren) child->Update();
}

LeoRenderer::Node::~Node()
{
    if (mMesh) delete mMesh;
    for (auto& child : mChildren) delete child;
}

// // ============================== Model ==============================

LeoRenderer::GLTFModel::~GLTFModel()
{
    if (mVertices.mVBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_pDevice->logicalDevice, mVertices.mVBuffer, nullptr);
        vkFreeMemory(m_pDevice->logicalDevice, mVertices.mVMemory, nullptr);
        mVertices.mVBuffer = VK_NULL_HANDLE;
    }
    if (mIndices.mIBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_pDevice->logicalDevice, mIndices.mIBuffer, nullptr);
        vkFreeMemory(m_pDevice->logicalDevice, mIndices.mIMemory, nullptr);
        mIndices.mIBuffer = VK_NULL_HANDLE;
    }

    for (auto texture : mTextures) texture.OnDestroy();
    for (auto node : mNodes) delete node;
    for (auto skin : mSkins) delete skin;
}

void LeoRenderer::GLTFModel::OnDestroy()
{
    if (mVertices.mVBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_pDevice->logicalDevice, mVertices.mVBuffer, nullptr);
        vkFreeMemory(m_pDevice->logicalDevice, mVertices.mVMemory, nullptr);
        mVertices.mVBuffer = VK_NULL_HANDLE;
    }
    if (mIndices.mIBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_pDevice->logicalDevice, mIndices.mIBuffer, nullptr);
        vkFreeMemory(m_pDevice->logicalDevice, mIndices.mIMemory, nullptr);
        mIndices.mIBuffer = VK_NULL_HANDLE;
    }

    for (auto texture : mTextures) texture.OnDestroy();
    for (auto node : mNodes) delete node;
    for (auto skin : mSkins) delete skin;
    mTextures.resize(0);
    mTexSamplers.resize(0);
    mMaterials.resize(0);
    mAnimations.resize(0);
    mNodes.resize(0);
    mLinearNodes.resize(0);
    mExtensions.resize(0);
    mSkins.resize(0);
}

void LeoRenderer::GLTFModel::LoadNode(
    LeoRenderer::Node* parent,
    const tinygltf::Node& node,
    uint32_t nodeIndex,
    const tinygltf::Model& model,
    LoaderInfo& loaderInfo,
    float globalScale)
{
    auto newNode = new LeoRenderer::Node{};
    newNode->mIndex = nodeIndex;
    newNode->mParent = parent;
    newNode->mName = node.name;
    newNode->mSkinIndex = node.skin;
    newNode->mMatrix = glm::mat4(1.0f);

    // Generate local node matrix
    auto translation = glm::vec3(0.0f);
    if (node.translation.size() == 3)
    {
        translation = glm::make_vec3(node.translation.data());
        newNode->mTranslation = translation;
    }
    auto rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4)
    {
        glm::quat q = glm::make_quat(node.rotation.data());
        newNode->mRotation = glm::mat4(q);
    }
    auto scale = glm::vec3(1.0f);
    if (node.scale.size() == 3)
    {
        scale = glm::make_vec3(node.scale.data());
        newNode->mScale = scale;
    }
    if (node.matrix.size() == 16)
    {
        newNode->mMatrix = glm::make_mat4x4(node.matrix.data());
        if (globalScale != 1.0f)
        {
            newNode->mMatrix = glm::scale(newNode->mMatrix, glm::vec3(globalScale));
        }
    }

    if (!node.children.empty())
    {
        for (int i : node.children)
        {
            LoadNode(newNode, model.nodes[i], i, model, loaderInfo, globalScale);
        }
    }

    if (node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        Mesh* newMesh = new Mesh(m_pDevice, newNode->mMatrix);
        newMesh->mName = mesh.name;
        for (const auto & primitive : mesh.primitives)
        {
            auto indexStart = static_cast<uint32_t>(loaderInfo.mVertexPos);
            auto vertexStart = static_cast<uint32_t>(loaderInfo.mIndexPos);
            uint32_t indexCount = 0;
            uint32_t vertexCount = 0;
            glm::vec3 posMin{};
            glm::vec3 posMax{};
            bool hasSkin = false;
            bool hasIndices = primitive.indices > -1;

            // Vertices
            {
                const float *bufferPos = nullptr;
                const float *bufferNormals = nullptr;
                const float *bufferTexCoordSet0 = nullptr;
                const float *bufferTexCoordSet1 = nullptr;
                const float *bufferColorSet0 = nullptr;
                const void  *bufferJoints = nullptr;
                const float *bufferWeights = nullptr;
                const float *bufferTangents = nullptr;

                // bs i.e. ByteStride
                int bsPos;
                int bsNormal;
                int bsUV0;
                int bsUV1;
                int bsColor0;
                int bsJoint;
                int bsWeight;
                int bsTangent;

                uint32_t jointComponentType = TINYGLTF_COMPONENT_TYPE_BYTE;

                // Position attribute is required
                assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

                const tinygltf::Accessor &posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                const tinygltf::BufferView &posView = model.bufferViews[posAccessor.bufferView];
                bufferPos = reinterpret_cast<const float *>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
                posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
                vertexCount = static_cast<uint32_t>(posAccessor.count);
                bsPos = posAccessor.ByteStride(posView) ? (int)(posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

                if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &normalAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView &normalView = model.bufferViews[normalAccessor.bufferView];
                    bufferNormals = reinterpret_cast<const float *>(&(model.buffers[normalView.buffer].data[normalAccessor.byteOffset + normalView.byteOffset]));
                    bsNormal = normalAccessor.ByteStride(normalView) ? (int)(normalAccessor.ByteStride(normalView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                }

                // UV
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoordSet0 = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                    bsUV0 = uvAccessor.ByteStride(uvView) ? (int)(uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
                }
                if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_1")->second];
                    const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                    bufferTexCoordSet1 = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                    bsUV1 = uvAccessor.ByteStride(uvView) ? (int)(uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
                }

                // Vertex Color
                if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                    const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
                    // Color buffer are either of type vec3 or vec4
                    bufferColorSet0 = reinterpret_cast<const float*>(&(model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
                    bsColor0 = colorAccessor.ByteStride(colorView) ? (int)(colorAccessor.ByteStride(colorView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                }

                if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
                    const tinygltf::BufferView &tangentView = model.bufferViews[tangentAccessor.bufferView];
                    bufferTangents = reinterpret_cast<const float *>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
                    bsTangent = tangentAccessor.ByteStride(tangentView) ? (int)(tangentAccessor.ByteStride(tangentView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                }

                // Skinning
                // Joints
                if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                    const tinygltf::BufferView &jointView = model.bufferViews[jointAccessor.bufferView];
                    bufferJoints = reinterpret_cast<const uint16_t *>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
                    bsJoint = jointAccessor.ByteStride(jointView) ? (jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(jointComponentType)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                }

                if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                {
                    const tinygltf::Accessor &weightAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                    const tinygltf::BufferView &weightView = model.bufferViews[weightAccessor.bufferView];
                    bufferWeights = reinterpret_cast<const float *>(&(model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
                    bsWeight = weightAccessor.ByteStride(weightView) ? (int)(weightAccessor.ByteStride(weightView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                }

                hasSkin = (bufferJoints && bufferWeights);

                for (size_t v = 0; v < posAccessor.count; v++)
                {
                    Vertex& vert = loaderInfo.mVertexBuffer[loaderInfo.mVertexPos];
                    vert.mPos = glm::vec4(glm::make_vec3(&bufferPos[v * bsPos]), 1.0f);
                    vert.mNormal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * bsNormal]) : glm::vec3(0.0f)));
                    vert.mUV0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * bsUV0]) : glm::vec3(0.0f);
                    vert.mUV1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * bsUV1]) : glm::vec3(0.0f);
                    vert.mColor = bufferColorSet0 ? glm::make_vec4(&bufferColorSet0[v * bsColor0]) : glm::vec4(1.0f);

                    if (hasSkin)
                    {
                        switch (jointComponentType)
                        {
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            {
                                auto *buf = static_cast<const uint16_t*>(bufferJoints);
                                vert.mJoint0 = glm::vec4(glm::make_vec4(&buf[v * bsJoint]));
                                break;
                            }
                            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                            {
                                auto *buf = static_cast<const uint8_t*>(bufferJoints);
                                vert.mJoint0 = glm::vec4(glm::make_vec4(&buf[v * bsJoint]));
                                break;
                            }
                            default:
                                // Not supported by spec
                                std::cerr << "Joint component type " << jointComponentType << " not supported!" << std::endl;
                                break;
                        }
                    }
                    else
                    {
                        vert.mJoint0 = glm::vec4(0.0f);
                    }
                    vert.mTangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * bsTangent])) : glm::vec4(0.0f);
                    if (glm::length(vert.mWeight0) == 0.0f)
                    {
                        vert.mWeight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    }

                    loaderInfo.mVertexPos++;
                }
            }
            // Indices
            if (hasIndices)
            {
                const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
                const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

                indexCount = static_cast<uint32_t>(accessor.count);
                const void* dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                switch (accessor.componentType)
                {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                        auto buf = static_cast<const uint32_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            loaderInfo.mIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                            loaderInfo.mIndexPos++;
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        auto buf = static_cast<const uint16_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            loaderInfo.mIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                            loaderInfo.mIndexPos++;
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        auto buf = static_cast<const uint8_t*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            loaderInfo.mIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                            loaderInfo.mIndexPos++;
                        }
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                }
            }
            auto newPrimitive = new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? mMaterials[primitive.material] : mMaterials.back());
            newPrimitive->SetBoundingBox(posMin, posMax);
            newMesh->mPrimitives.push_back(newPrimitive);
        }
        for (auto p : newMesh->mPrimitives)
        {
            if (p->mBBox.mbValid && !newMesh->mBBox.mbValid)
            {
                newMesh->mBBox = p->mBBox;
                newMesh->mBBox.mbValid = true;
            }
            newMesh->mBBox.mMin = glm::min(newMesh->mBBox.mMin, p->mBBox.mMin);
            newMesh->mBBox.mMax = glm::max(newMesh->mBBox.mMax, p->mBBox.mMax);
        }
        newNode->mMesh = newMesh;
    }
    if (parent)
    {
        parent->mChildren.push_back(newNode);
    }
    else
    {
        mNodes.push_back(newNode);
    }
    mLinearNodes.push_back(newNode);
}

void LeoRenderer::GLTFModel::GetNodeProperty(
    const tinygltf::Node& node,
    const tinygltf::Model& model,
    size_t& vertexCount, size_t& indexCount)
{
    if (!node.children.empty())
    {
        for (auto & child : node.children)
        {
            GetNodeProperty(model.nodes[child], model, vertexCount, indexCount);
        }
    }
    if (node.mesh > -1)
    {
        const tinygltf::Mesh mesh = model.meshes[node.mesh];
        for (auto primitive : mesh.primitives)
        {
            vertexCount += model.accessors[primitive.attributes.find("POSITION")->second].count;
            if (primitive.indices > -1) indexCount += model.accessors[primitive.indices].count;
        }
    }
}

void LeoRenderer::GLTFModel::LoadSkins(tinygltf::Model &gltfModel)
{
    for (tinygltf::Skin &source : gltfModel.skins)
    {
        Skin *newSkin = new Skin{};
        newSkin->mName = source.name;

        // Find skeleton root node
        if (source.skeleton > -1)
        {
            newSkin->mSkeletonRoot = NodeFromIndex(source.skeleton);
        }

        // Find joint nodes
        for (int jointIndex : source.joints)
        {
            Node* node = NodeFromIndex(jointIndex);
            if (node) newSkin->mJoints.push_back(NodeFromIndex(jointIndex));
        }

        // Get inverse bind matrices from buffer
        if (source.inverseBindMatrices > -1)
        {
            const tinygltf::Accessor &accessor = gltfModel.accessors[source.inverseBindMatrices];
            const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
            const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];
            newSkin->mInverseBindMatrices.resize(accessor.count);
            memcpy(newSkin->mInverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
        }
        mSkins.push_back(newSkin);
    }
}

void LeoRenderer::GLTFModel::LoadTextures(
    tinygltf::Model &gltfModel,
    vks::VulkanDevice *device,
    VkQueue transferQueue)
{
    for (auto & tex : gltfModel.textures)
    {
        tinygltf::Image image = gltfModel.images[tex.source];
        LeoRenderer::TextureSampler texSampler{};
        if (tex.sampler == -1)
        {
            // No sampler specified, use a default one
            texSampler.mMagFilter = VK_FILTER_LINEAR;
            texSampler.mMinFilter = VK_FILTER_LINEAR;
            texSampler.mAddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            texSampler.mAddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            texSampler.mAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
        else
        {
            texSampler = mTexSamplers[tex.sampler];
        }
        LeoRenderer::Texture texture;
        texture.FromGLTFImage(image, texSampler, device, transferQueue);
        mTextures.push_back(texture);
    }
}

VkSamplerAddressMode LeoRenderer::GLTFModel::GetVkWrapMode(int32_t wrapMode)
{
    switch (wrapMode)
    {
        case -1:
        case 10497:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case 33071:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case 33648:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        default:
            std::cerr << "Unknown wrap mode: " << wrapMode << std::endl;
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkFilter LeoRenderer::GLTFModel::GetVkFilterMode(int32_t filterMode)
{
    switch (filterMode)
    {
        case -1:
        case 9728:
            return VK_FILTER_NEAREST;
        case 9729:
            return VK_FILTER_LINEAR;
        case 9984:
        case 9985:
            return VK_FILTER_NEAREST;
        case 9986:
        case 9987:
            return VK_FILTER_LINEAR;
        default:
            std::cerr << "Unknown filter mode: " << filterMode << std::endl;
            return VK_FILTER_NEAREST;
    }
}

void LeoRenderer::GLTFModel::LoadTextureSamplers(tinygltf::Model &gltfModel)
{
    for (const tinygltf::Sampler& smpl : gltfModel.samplers)
    {
        LeoRenderer::TextureSampler sampler{};
        sampler.mMinFilter = GetVkFilterMode(smpl.minFilter);
        sampler.mMagFilter = GetVkFilterMode(smpl.magFilter);
        sampler.mAddressModeU = GetVkWrapMode(smpl.wrapS);
        sampler.mAddressModeV = GetVkWrapMode(smpl.wrapT);
        sampler.mAddressModeW = sampler.mAddressModeV;
        mTexSamplers.push_back(sampler);
    }
}

void LeoRenderer::GLTFModel::LoadMaterials(tinygltf::Model &gltfModel)
{
    for (tinygltf::Material &mat : gltfModel.materials)
    {
        LeoRenderer::Material material{};
        material.m_bDoubleSided = mat.doubleSided;

        if (mat.values.find("baseColorTexture") != mat.values.end())
        {
            material.mBaseColorTexture = &mTextures[mat.values["baseColorTexture"].TextureIndex()];
            material.mTexCoordSet.mBaseColor = mat.values["baseColorTexture"].TextureTexCoord();
        }
        // Metallic roughness workflow
        if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
        {
            material.mMetallicRoughnessTexture = &mTextures[mat.values["metallicRoughnessTexture"].TextureIndex()];
            material.mTexCoordSet.mMetallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
        }
        if (mat.values.find("roughnessFactor") != mat.values.end())
        {
            material.mRoughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
        }
        if (mat.values.find("metallicFactor") != mat.values.end())
        {
            material.mMetallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
        }
        if (mat.values.find("baseColorFactor") != mat.values.end())
        {
            material.mBaseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
        }
        if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
        {
            material.mNormalTexture = &mTextures[mat.additionalValues["normalTexture"].TextureIndex()];
            material.mTexCoordSet.mNormal = mat.additionalValues["normalTexture"].TextureTexCoord();
        }
        if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
        {
            material.mEmissiveTexture = &mTextures[mat.additionalValues["emissiveTexture"].TextureIndex()];
            material.mTexCoordSet.mEmissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
        }
        if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end())
        {
            material.mEmissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
        }
        if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
        {
            material.mOcclusionTexture = &mTextures[mat.additionalValues["occlusionTexture"].TextureIndex()];
            material.mTexCoordSet.mOcclusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
        }
        if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
        {
            tinygltf::Parameter param = mat.additionalValues["alphaMode"];
            if (param.string_value == "BLEND")
            {
                material.mAlphaMode = Material::ALPHA_MODE_BLEND;
            }
            if (param.string_value == "MASK")
            {
                material.mAlphaCutoff = 0.5f;
                material.mAlphaMode = Material::ALPHA_MODE_MASK;
            }

        }
        if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
        {
            material.mAlphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
        }

        // Extensions
        if (mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
        {
            auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
            if (ext->second.Has("specularGlossinessTexture"))
            {
                auto index = ext->second.Get("specularGlossinessTexture").Get("index");
                material.mExtension.mSpecularGlossinessTexture = &mTextures[index.Get<int>()];
                auto texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
                material.mTexCoordSet.mSpecularGlossiness = texCoordSet.Get<int>();
                material.mPBRWorkFlows.mbSpecularGlossiness = true;
            }
            if (ext->second.Has("diffuseTexture"))
            {
                auto index = ext->second.Get("diffuseTexture").Get("index");
                material.mExtension.mDiffuseTexture = &mTextures[index.Get<int>()];
            }
            if (ext->second.Has("diffuseFactor"))
            {
                auto factor = ext->second.Get("diffuseFactor");
                for (int i = 0; i < factor.ArrayLen(); i++)
                {
                    auto val = factor.Get(i);
                    material.mExtension.mDiffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                }
            }
            if (ext->second.Has("specularFactor"))
            {
                auto factor = ext->second.Get("specularFactor");
                for (int i = 0; i < factor.ArrayLen(); i++)
                {
                    auto val = factor.Get(i);
                    material.mExtension.mSpecularFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                }
            }
        }
        mMaterials.push_back(material);
    }
    // Push a default material at the end of the list for meshes with no material assigned
    mMaterials.emplace_back();
}

void LeoRenderer::GLTFModel::LoadAnimations(tinygltf::Model &gltfModel)
{
    for (tinygltf::Animation &anim : gltfModel.animations)
    {
        LeoRenderer::Animation animation{};
        animation.mName = anim.name;
        if (anim.name.empty())
        {
            animation.mName = std::to_string(mAnimations.size());
        }

        // Samplers
        for (auto &samp : anim.samplers)
        {
            LeoRenderer::AnimationSampler sampler{};

            if (samp.interpolation == "LINEAR") sampler.mInterpolation = AnimationSampler::InterpolationType::LINEAR;
            if (samp.interpolation == "STEP") sampler.mInterpolation = AnimationSampler::InterpolationType::STEP;
            if (samp.interpolation == "CUBICSPLINE") sampler.mInterpolation = AnimationSampler::InterpolationType::CUBICSPLINE;

            // Read sampler input time values
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[samp.input];
                const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                auto buf = static_cast<const float*>(dataPtr);
                for (size_t index = 0; index < accessor.count; index++)
                {
                    sampler.mInputs.push_back(buf[index]);
                }

                for (auto input : sampler.mInputs)
                {
                    if (input < animation.mStart)
                    {
                        animation.mStart = input;
                    }
                    if (input > animation.mEnd)
                    {
                        animation.mEnd = input;
                    }
                }
            }

            // Read sampler output T/R/S values
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[samp.output];
                const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

                switch (accessor.type)
                {
                    case TINYGLTF_TYPE_VEC3:
                    {
                        auto buf = static_cast<const glm::vec3*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            sampler.mOutputsVec4.emplace_back(buf[index], 0.0f);
                        }
                        break;
                    }
                    case TINYGLTF_TYPE_VEC4:
                    {
                        auto buf = static_cast<const glm::vec4*>(dataPtr);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            sampler.mOutputsVec4.emplace_back(buf[index]);
                        }
                        break;
                    }
                    default:
                    {
                        std::cout << "unknown type" << std::endl;
                        break;
                    }
                }
            }

            animation.mSamplers.push_back(sampler);
        }

        // Channels
        for (auto &source: anim.channels)
        {
            LeoRenderer::AnimationChannel channel{};
            if (source.target_path == "rotation")
            {
                channel.mPath = AnimationChannel::PathType::ROTATION;
            }
            if (source.target_path == "translation")
            {
                channel.mPath = AnimationChannel::PathType::TRANSLATION;
            }
            if (source.target_path == "scale")
            {
                channel.mPath = AnimationChannel::PathType::SCALE;
            }
            if (source.target_path == "weights")
            {
                std::cout << "weights not yet supported, skipping channel" << std::endl;
                continue;
            }
            channel.mSamplerIndex = source.sampler;
            channel.mNode = NodeFromIndex(source.target_node);

            if (!channel.mNode) continue;

            animation.mChannels.push_back(channel);
        }
        mAnimations.push_back(animation);
    }
}

void LeoRenderer::GLTFModel::LoadFromFile(
    std::string& filename,
    vks::VulkanDevice *device,
    VkQueue transferQueue,
    float scale)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfContext;

    std::string error;
    std::string warning;

    m_pDevice = device;

    bool binary = false;
    size_t extPos = filename.rfind('.', filename.length());
    if (extPos != std::string::npos)
    {
        binary = (filename.substr(extPos + 1, filename.length() - extPos) == "glb");
    }

    bool fileLoaded = binary ?
        gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filename) :
        gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);

    LoaderInfo loaderInfo{};
    size_t vertexCount = 0;
    size_t indexCount = 0;

    if (fileLoaded)
    {
        LoadTextureSamplers(gltfModel);
        LoadTextures(gltfModel, m_pDevice, transferQueue);
        LoadMaterials(gltfModel);

        const tinygltf::Scene &scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];
        for (auto i : scene.nodes)
        {
            GetNodeProperty(gltfModel.nodes[i], gltfModel, vertexCount, indexCount);
        }
        loaderInfo.mVertexBuffer = new Vertex[vertexCount];
        loaderInfo.mIndexBuffer = new uint32_t[indexCount];
        for (auto i : scene.nodes)
        {
            const tinygltf::Node node = gltfModel.nodes[i];
            LoadNode(nullptr, node, i, gltfModel, loaderInfo, scale);
        }
        if (!gltfModel.animations.empty())
        {
            LoadAnimations(gltfModel);
        }
        LoadSkins(gltfModel);

        for (auto node : mLinearNodes)
        {
            // Assign skins
            if (node->mSkinIndex > -1)
            {
                node->mSkin = mSkins[node->mSkinIndex];
            }
            // Initial pose
            if (node->mMesh)
            {
                node->Update();
            }
        }
    }
    else
    {
        // TODO: throw
        vks::tools::exitFatal("Could not load glTF file \"" + filename + "\": " + error, -1);
        return;
    }

    mExtensions = gltfModel.extensionsUsed;

    size_t vertexBufferSize = vertexCount * sizeof(Vertex);
    size_t indexBufferSize = indexCount * sizeof(uint32_t);

    assert((vertexBufferSize > 0) && (indexBufferSize > 0));

    struct StagingBuffer
    {
        VkBuffer buffer;
        VkDeviceMemory memory;
    } vertexStaging{}, indexStaging{};

    // Create staging buffers
    // Vertex data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBufferSize,
        &vertexStaging.buffer,
        &vertexStaging.memory,
        loaderInfo.mVertexBuffer))

    // Index data
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBufferSize,
        &indexStaging.buffer,
        &indexStaging.memory,
        loaderInfo.mIndexBuffer));

    // Create device local buffers
    // Vertex buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBufferSize,
        &mVertices.mVBuffer,
        &mVertices.mVMemory));
    // Index buffer
    VK_CHECK_RESULT(device->createBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBufferSize,
        &mIndices.mIBuffer,
        &mIndices.mIMemory));

    // Copy from staging buffers
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferCopy copyRegion{};

    copyRegion.size = vertexBufferSize;
    vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, mVertices.mVBuffer, 1, &copyRegion);

    copyRegion.size = indexBufferSize;
    vkCmdCopyBuffer(copyCmd, indexStaging.buffer, mIndices.mIBuffer, 1, &copyRegion);

    device->flushCommandBuffer(copyCmd, transferQueue, true);

    vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
    vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
    vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

    delete[] loaderInfo.mVertexBuffer;
    delete[] loaderInfo.mIndexBuffer;

    GetSceneDimensions();
}

void LeoRenderer::GLTFModel::DrawNode(LeoRenderer::Node *node, VkCommandBuffer commandBuffer)
{
    if (node->mMesh)
    {
        for (auto primitive : node->mMesh->mPrimitives)
        {
            vkCmdDrawIndexed(commandBuffer, primitive->mIndexCount, 1, primitive->mFirstIndex, 0, 0);
        }
    }
    for (auto& child : node->mChildren) DrawNode(child, commandBuffer);
}

void LeoRenderer::GLTFModel::Draw(VkCommandBuffer commandBuffer)
{

    const VkDeviceSize offsets[1]{};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertices.mVBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mIndices.mIBuffer, 0, VK_INDEX_TYPE_UINT32);

    for (auto& node : mNodes) DrawNode(node, commandBuffer);
}

void LeoRenderer::GLTFModel::CalculateBoundingBox(LeoRenderer::Node *node, LeoRenderer::Node *parent)
{
    BoundingBox parentBVH = parent ? parent->mBVH : BoundingBox(mDimensions.mMin, mDimensions.mMax);

    if (node->mMesh)
    {
        if (node->mMesh->mBBox.mbValid)
        {
            node->mAABB = node->mMesh->mBBox.GetAABB(node->GetMatrix());
            if (node->mChildren.empty())
            {
                node->mBVH.mMin = node->mAABB.mMin;
                node->mBVH.mMax = node->mAABB.mMax;
                node->mBVH.mbValid = true;
            }
        }
    }
    parentBVH.mMin = glm::min(parentBVH.mMin, node->mBVH.mMin);
    parentBVH.mMax = glm::max(parentBVH.mMax, node->mBVH.mMax);

    for (auto & child : node->mChildren) CalculateBoundingBox(child, node);
}

void LeoRenderer::GLTFModel::GetSceneDimensions()
{
    for (auto node : mLinearNodes)
    {
        CalculateBoundingBox(node, nullptr);
    }
    mDimensions.mMin = glm::vec3(FLT_MAX);
    mDimensions.mMax = glm::vec3(-FLT_MAX);

    for (auto node : mLinearNodes)
    {
        if (node->mBVH.mbValid)
        {
            mDimensions.mMin = glm::min(mDimensions.mMin, node->mBVH.mMin);
            mDimensions.mMax = glm::max(mDimensions.mMax, node->mBVH.mMax);
        }
    }

    // Scene AABB
    mAABB = glm::scale(
        glm::mat4(1.0f),
        glm::vec3(
            mDimensions.mMax[0] - mDimensions.mMin[0],
            mDimensions.mMax[1] - mDimensions.mMin[1],
            mDimensions.mMax[2] - mDimensions.mMin[2]));
    mAABB[3][0] = mDimensions.mMin[0];
    mAABB[3][1] = mDimensions.mMin[1];
    mAABB[3][2] = mDimensions.mMin[2];
}

void LeoRenderer::GLTFModel::UpdateAnimation(uint32_t index, float time)
{
    if (mAnimations.empty())
    {
        std::cout << ".glTF does not contain animation." << std::endl;
        return;
    }
    if (index > static_cast<uint32_t>(mAnimations.size()) - 1)
    {
        std::cout << "No animation with index " << index << std::endl;
        return;
    }
    Animation &animation = mAnimations[index];

    bool updated = false;
    for (auto& channel : animation.mChannels)
    {
        LeoRenderer::AnimationSampler &sampler = animation.mSamplers[channel.mSamplerIndex];
        if (sampler.mInputs.size() > sampler.mOutputsVec4.size()) continue;

        for (auto i = 0; i < sampler.mInputs.size() - 1; i++)
        {
            // 如果当前时间在两帧动画之间就进行插值
            if ((time >= sampler.mInputs[i]) && (time <= sampler.mInputs[i + 1]))
            {
                // 插值
                float u = std::max(0.0f, time - sampler.mInputs[i]) / (sampler.mInputs[i + 1] - sampler.mInputs[i]);
                if (u <= 1.0f)
                {
                    switch (channel.mPath)
                    {
                        case LeoRenderer::AnimationChannel::PathType::TRANSLATION:
                        {
                            glm::vec4 trans = glm::mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
                            channel.mNode->mTranslation = glm::vec3(trans);
                            break;
                        }
                        case LeoRenderer::AnimationChannel::PathType::SCALE:
                        {
                            glm::vec4 trans = glm::mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
                            channel.mNode->mScale = glm::vec3(trans);
                            break;
                        }
                        case LeoRenderer::AnimationChannel::PathType::ROTATION:
                        {
                            glm::quat q1;
                            q1.x = sampler.mOutputsVec4[i].x;
                            q1.y = sampler.mOutputsVec4[i].y;
                            q1.z = sampler.mOutputsVec4[i].z;
                            q1.w = sampler.mOutputsVec4[i].w;
                            glm::quat q2;
                            q2.x = sampler.mOutputsVec4[i + 1].x;
                            q2.y = sampler.mOutputsVec4[i + 1].y;
                            q2.z = sampler.mOutputsVec4[i + 1].z;
                            q2.w = sampler.mOutputsVec4[i + 1].w;
                            channel.mNode->mRotation = glm::normalize(glm::slerp(q1, q2, u));
                            break;
                        }
                    }
                    updated = true;
                }
            }
        }
    }
    if (updated)
    {
        for (auto &node : mNodes) node->Update();
    }
}

LeoRenderer::Node *LeoRenderer::GLTFModel::FindNode(LeoRenderer::Node *parent, uint32_t index)
{
    Node* nodeFound = nullptr;
    if (parent->mIndex == index) return parent;
    for (auto& child : parent->mChildren)
    {
        nodeFound = FindNode(child, index);
        if (nodeFound) break;
    }
    return nodeFound;
}

LeoRenderer::Node *LeoRenderer::GLTFModel::NodeFromIndex(uint32_t index)
{
    Node* nodeFound = nullptr;
    for (auto & node : mNodes)
    {
        nodeFound = FindNode(node, index);
        if (nodeFound) break;
    }
    return nodeFound;
}


