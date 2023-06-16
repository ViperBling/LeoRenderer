#pragma once

#include "ProjectPCH.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "tiny_gltf.h"

namespace LeoRenderer
{
    struct BoundingBox
    {
        glm::vec3 mMin;
        glm::vec3 mMax;
        bool mbValid = false;
        BoundingBox();
        BoundingBox(glm::vec3 min, glm::vec3 max);
        BoundingBox GetAABB(glm::mat4 mat);
    };

    struct TextureSampler
    {
        VkFilter mMagFilter;
        VkFilter mMinFilter;
        VkSamplerAddressMode mAddressModeU;
        VkSamplerAddressMode mAddressModeV;
        VkSamplerAddressMode mAddressModeW;
    };

    enum DescriptorBindingFlags
    {
        ImageBaseColor = 0x00000001,
        ImageNormalMap = 0x00000002
    };

    extern VkDescriptorSetLayout descriptorSetLayoutImage;
    extern VkDescriptorSetLayout descriptorSetLayoutUBO;
    extern VkMemoryPropertyFlags memoryPropertyFlags;
    extern uint32_t              descriptorBindingFlags;

    struct Texture
    {
        void UpdateDescriptor();
        void OnDestroy();
        void FromGLTFImage(tinygltf::Image& gltfImage, TextureSampler texSampler, vks::VulkanDevice* device, VkQueue copyQueue);

        vks::VulkanDevice*      mDevice = nullptr;
        VkImage                 mImage{};
        VkImageLayout           mImageLayout{};
        VkDeviceMemory          mDeviceMemory{};
        VkImageView             mImageView{};
        uint32_t                mWidth{}, mHeight{};
        uint32_t                mMipLevels{};
        uint32_t                mLayerCount{};
        VkDescriptorImageInfo   mDescriptor{};
        VkSampler               mSampler{};
    };

    struct Material
    {
        enum AlphaMode
        {
            ALPHA_MODE_OPAQUE,
            ALPHA_MODE_MASK,
            ALPHA_MODE_BLEND
        };

        AlphaMode           mAlphaMode = ALPHA_MODE_OPAQUE;
        float               mAlphaCutoff = 1.0f;
        bool                m_bDoubleSided = false;
        float               mMetallicFactor = 1.0f;
        float               mRoughnessFactor = 1.0f;
        glm::vec4           mEmissiveFactor = glm::vec4(1.0f);
        glm::vec4           mBaseColorFactor = glm::vec4(1.0f);
        Texture*            mBaseColorTexture = nullptr;
        Texture*            mMetallicRoughnessTexture = nullptr;
        Texture*            mNormalTexture = nullptr;
        Texture*            mOcclusionTexture = nullptr;
        Texture*            mEmissiveTexture = nullptr;

        struct TexCoordSet
        {
            uint8_t mBaseColor = 0;
            uint8_t mMetallicRoughness = 0;
            uint8_t mSpecularGlossiness = 0;
            uint8_t mNormal = 0;
            uint8_t mOcclusion = 0;
            uint8_t mEmissive = 0;
        } mTexCoordSet;
        struct Extension
        {
            Texture*    mSpecularGlossinessTexture = nullptr;
            Texture*    mDiffuseTexture = nullptr;
            glm::vec4   mDiffuseFactor = glm::vec4(1.0f);
            glm::vec4   mSpecularFactor = glm::vec4(0.0f);
        };
        struct PBRWorkFlows
        {
            bool mbMetallicRoughness = true;
            bool mbSpecularGlossiness = false;
        };
        VkDescriptorSet mDescriptorSet{};
    };

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
    };

    struct Primitive
    {
        Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) :
            mFirstIndex(firstIndex),
            mIndexCount(indexCount),
            mMaterial(material) {};
        void SetBoundingBox(glm::vec3 min, glm::vec3 max);

        uint32_t mFirstIndex;
        uint32_t mIndexCount;
        uint32_t mVertexCount{};
        BoundingBox mBBox;
        Material& mMaterial;
    };

    struct Mesh
    {
        Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
        ~Mesh();
        void SetBoundingBox(glm::vec3 min, glm::vec3 max);

        struct UniformBuffer {
            VkBuffer                buffer;
            VkDeviceMemory          memory;
            VkDescriptorBufferInfo  descriptor;
            VkDescriptorSet         descriptorSet{};
            void*                   mapped;
        } mUniformBuffer;

        struct UniformBlock {
            glm::mat4 matrix;
            glm::mat4 jointMatrix[64]{};
            float jointCount{};
        } mUniformBlock;

        vks::VulkanDevice*      mDevice;
        std::vector<Primitive*> mPrimitives;
        std::string             mName;
    };

    struct Node;

    struct Skin
    {
        std::string             mName;
        Node*                   mSkeletonRoot{};
        std::vector<glm::mat4>  mInverseBindMatrices;
        std::vector<Node*>      mJoints;
    };

    struct Node
    {
        glm::mat4 LocalMatrix();        // 根据平移、旋转、缩放计算本地矩阵
        glm::mat4 GetMatrix();          // 根据父节点计算自身的目前的矩阵，因为可能有关联的变换
        void Update();
        ~Node();

        Node*               mParent;
        uint32_t            mIndex;
        std::vector<Node*>  mChildren;
        glm::mat4           mMatrix;
        std::string         mName;
        Mesh*               mMesh;
        Skin*               mSkin;
        int32_t             mSkinIndex = -1;
        glm::vec3           mTranslation{};
        glm::vec3           mScale{ 1.0f };
        glm::quat           mRotation{};
        BoundingBox         mBVH;
        BoundingBox         mAABB;
    };

    struct AnimationChannel
    {
        enum PathType
        {
            TRANSLATION, ROTATION, SCALE
        };
        PathType mPath;
        Node*    mNode;
        uint32_t mSamplerIndex;
    };

    struct AnimationSampler
    {
        enum InterpolationType
        {
            LINEAR, STEP, CUBICSPLINE
        };
        InterpolationType mInterpolation;
        std::vector<float> mInputs;
        std::vector<glm::vec4> mOutputsVec4;
    };

    struct Animation
    {
        std::string mName;
        std::vector<AnimationSampler> mSamplers;
        std::vector<AnimationChannel> mChannels;
        float mStart = std::numeric_limits<float>::max();
        float mEnd = std::numeric_limits<float>::min();
    };

    enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

    struct Vertex
    {
        glm::vec3 mPos;
        glm::vec3 mNormal;
        glm::vec2 mUV0;
        glm::vec4 mUV1;
        glm::vec4 mColor;
        glm::vec4 mJoint0;
        glm::vec4 mWeight0;
        glm::vec4 mTangent;
    };

    struct Vertices
    {
        VkBuffer mVBuffer;
        VkDeviceMemory mVMemory;
    };
    struct Indices
    {
        VkBuffer mIBuffer;
        VkDeviceMemory mIMemory;
    };

    struct LoaderInfo
    {
        uint32_t*   mIndexBuffer{};
        Vertex*     mVertexBuffer{};
        size_t      mIndexPos = 0;
        size_t      mVertexPos = 0;
    };

    class GLTFModel
    {
    public:
        GLTFModel() = default;
        ~GLTFModel();
        void LoadNode(LeoRenderer::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalScale);
        void GetNodeProperpty(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
        void LoadSkins(tinygltf::Model& gltfModel);
        void LoadTextures(tinygltf::Model& gltfModel, vks::VulkanDevice* device, VkQueue transferQueue);
        VkSamplerAddressMode GetVkWrapMode(int32_t wrapMode);
        VkFilter GetVkFilterMode(int32_t filterMode);
        void LoadeTextureSamplers(tinygltf::Model& gltfModel);
        void LoadMaterials(tinygltf::Model& gltfModel);
        void LoadAnimations(tinygltf::Model& gltfModel);
        void LoadFromFile(std::string& filename, vks::VulkanDevice* device, VkQueue transferQueue, float scale = 1.0f);
        void DrawNode(Node* node, VkCommandBuffer commandBuffer);
        void Draw(VkCommandBuffer commandBuffer);
        void CalculateBoundingBox(Node* node, Node* parent);
        void GetSceneDimensions();
        void UpdateAnimation(uint32_t index, float time);
        Node* FindNode(Node* parent, uint32_t index);
        Node* NodeFromIndex(uint32_t index);

    public:
        vks::VulkanDevice* m_pDevice;
        VkDescriptorPool mDescPool;

        Vertices    mVertices;
        Indices     mIndices;

        glm::mat4 mAABB;

        std::vector<Node*> mNodes;
        std::vector<Node*> mLinearNodes;
        std::vector<Skin*> mSkins;
        std::vector<Texture> mTextures;
        std::vector<TextureSampler> mTexSamplers;
        std::vector<Material> mMaterials;
        std::vector<Animation> mAnimations;
        std::vector<std::string> mExtensions;

        Dimensions mDimensions;

        bool m_bMetallicWorkFlow = true;
        bool m_bBufferBound = false;
        std::string mPath;
    };
}



