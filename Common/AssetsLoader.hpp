#pragma once

#include "ProjectPCH.hpp"

#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "VKDevice.hpp"
#include "VKTexture.hpp"


#if defined(_WIN32) && defined(ERROR) && defined(TINYGLTF_ENABLE_DRACO)
#undef ERROR
#pragma message ("ERROR constant already defined, undefining")
#endif

// Changing this value here also requires changing it in the vertex shader
#define MAX_NUM_JOINTS 128u

namespace LeoVK
{
    enum FileLoadingFlags {
		None = 0x00000000,
		PreTransformVertices = 0x00000001,
		PreMultiplyVertexColors = 0x00000002,
		FlipY = 0x00000004,
		DontLoadImages = 0x00000008
	};

    struct BoundingBox
    {
        glm::vec3 mMin;
        glm::vec3 mMax;
        bool mbValid = false;
        BoundingBox();
        BoundingBox(glm::vec3 min, glm::vec3 max);
        BoundingBox GetAABB(glm::mat4 mat);
    };

    struct Material
    {
        enum AlphaMode
        {
            BINDIMAGES,
            ALPHA_MODE_OPAQUE,
            ALPHA_MODE_MASK,
            ALPHA_MODE_BLEND
        };

        AlphaMode   mAlphaMode = ALPHA_MODE_OPAQUE;
        float       mAlphaCutoff = 1.0f;
        bool        mbDoubleSided = false;
        float       mMetallicFactor = 1.0f;
        float       mRoughnessFactor = 1.0f;
        glm::vec4   mEmissiveFactor = glm::vec4(1.0f);
        glm::vec4   mBaseColorFactor = glm::vec4(1.0f);
        Texture*    mpBaseColorTexture = nullptr;
        Texture*    mpMetallicRoughnessTexture = nullptr;
        Texture*    mpNormalTexture = nullptr;
        Texture*    mpOcclusionTexture = nullptr;
        Texture*    mpEmissiveTexture = nullptr;

        struct TexCoordSets
        {
            uint8_t mBaseColor = 0;
            uint8_t mMetallicRoughness = 0;
            uint8_t mSpecularGlossiness = 0;
            uint8_t mNormal = 0;
            uint8_t mOcclusion = 0;
            uint8_t mEmissive = 0;
        } mTexCoordSets;
        struct Extension
        {
            Texture*    mpSpecularGlossinessTexture = nullptr;
            Texture*    mpDiffuseTexture = nullptr;
            glm::vec4   mDiffuseFactor = glm::vec4(1.0f);
            glm::vec3   mSpecularFactor = glm::vec3(0.0f);
        } mExtension;
        struct PBRWorkFlows
        {
            bool mbMetallicRoughness = true;
            bool mbSpecularGlossiness = false;
        } mPBRWorkFlows;
        VkDescriptorSet mDescriptorSet = VK_NULL_HANDLE;
        int mIndex = 0;
        bool mbUnlit = false;
        float mEmissiveStrength = 1.0f;
    };

    struct Dimensions
    {
        glm::vec3 mMin = glm::vec3(FLT_MAX);
        glm::vec3 mMax = glm::vec3(-FLT_MAX);
    };

    class Primitive
    {
    public:
        Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material) :
            mFirstIndex(firstIndex),
            mIndexCount(indexCount),
            mVertexCount(vertexCount),
            mMaterial(material) { mbHasIndices = indexCount > 0; };
        void SetBoundingBox(glm::vec3 min, glm::vec3 max);

        uint32_t mFirstIndex;
        uint32_t mIndexCount;
        uint32_t mFirstVertex;
        uint32_t mVertexCount{};
        bool mbHasIndices;
        BoundingBox mBBox;
        Material& mMaterial;
    };

    class Mesh
    {
    public:
        Mesh(LeoVK::VulkanDevice* device, glm::mat4 matrix);
        virtual ~Mesh();
        void SetBoundingBox(glm::vec3 min, glm::vec3 max);

    public:
        struct UniformBuffer
        {
            VkBuffer                mBuffer;
            VkDeviceMemory          mMemory;
            VkDescriptorBufferInfo  mDescriptor;
            VkDescriptorSet         mDescriptorSet{};
            void*                   mpMapped;
        } mUniformBuffer;

        struct UniformBlock
        {
            glm::mat4 mMatrix;
            glm::mat4 mJointMatrix[64]{};
            float     mJointCount{};
        } mUniformBlock;

        LeoVK::VulkanDevice*    mpDevice;
        std::vector<Primitive*> mPrimitives;
        BoundingBox             mBBox;
        BoundingBox             mAABB;
        std::string             mName;
    };

    struct Node;

    struct Skin
    {
        std::string             mName;
        Node*                   mpSkeletonRoot{};
        std::vector<glm::mat4>  mInverseBindMatrices;
        std::vector<Node*>      mJoints;
    };

    class Node
    {
    public:
        glm::mat4 LocalMatrix();        // 根据平移、旋转、缩放计算本地矩阵
        glm::mat4 GetMatrix();          // 根据父节点计算自身的目前的矩阵，因为可能有关联的变换
        void Update();
        virtual ~Node();

    public:
        Node*               mpParent;
        uint32_t            mIndex;
        std::vector<Node*>  mChildren;
        glm::mat4           mMatrix;
        std::string         mName;
        Mesh*               mpMesh;
        Skin*               mpSkin;
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
        Node*    mpNode;
        uint32_t mSamplerIndex;
    };

    struct AnimationSampler
    {
        enum InterpolationType
        {
            LINEAR, STEP, CUBICSPLINE
        };
        InterpolationType       mInterpolation;
        std::vector<float>      mInputs;
        std::vector<glm::vec4>  mOutputsVec4;
    };

    struct Animation
    {
        std::string                     mName;
        std::vector<AnimationSampler>   mSamplers;
        std::vector<AnimationChannel>   mChannels;
        float                           mStart = std::numeric_limits<float>::max();
        float                           mEnd = std::numeric_limits<float>::min();
    };

    struct Vertex
    {
        glm::vec3 mPos;
        glm::vec3 mNormal;
        glm::vec2 mUV0;
        glm::vec2 mUV1;
        glm::vec4 mColor;
        glm::vec4 mJoint0;
        glm::vec4 mWeight0;
        glm::vec4 mTangent;
    };

    struct Vertices
    {
        VkBuffer        mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory  mMemory;
    };
    struct Indices
    {
        VkBuffer        mBuffer = VK_NULL_HANDLE;
        VkDeviceMemory  mMemory;
    };

    struct LoaderInfo
    {
        uint32_t*   mpIndexBuffer{};
        Vertex*     mpVertexBuffer{};
        size_t      mIndexPos = 0;
        size_t      mVertexPos = 0;
    };

    class GLTFScene
    {
    public:
        void Destroy(VkDevice device);
        void LoadNode(LeoVK::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalScale);
        void GetNodeProperty(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
        void LoadSkins(tinygltf::Model& gltfModel);
        void LoadTextures(tinygltf::Model& gltfModel, LeoVK::VulkanDevice* device, VkQueue transferQueue);
        VkSamplerAddressMode GetVkWrapMode(int32_t wrapMode);
        VkFilter GetVkFilterMode(int32_t filterMode);
        VkDescriptorImageInfo GetTextureDescriptor(const size_t index);
        void LoadTextureSamplers(tinygltf::Model& gltfModel);
        void LoadMaterials(tinygltf::Model& gltfModel);
        void LoadAnimations(tinygltf::Model& gltfModel);
        void LoadFromFile(const std::string& filename, LeoVK::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags = FileLoadingFlags::None, float scale = 1.0f);
        void DrawNode(Node* node, VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1, Material::AlphaMode renderFlag = Material::ALPHA_MODE_OPAQUE);
        void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1, Material::AlphaMode renderFlag = Material::ALPHA_MODE_OPAQUE);
        void CalculateBoundingBox(Node* node, Node* parent);
        void GetSceneDimensions();
        void UpdateAnimation(uint32_t index, float time);
        Node* FindNode(Node* parent, uint32_t index);
        Node* NodeFromIndex(uint32_t index);

    public:
        LeoVK::VulkanDevice* mpDevice{};
        VkDescriptorPool     mDescPool{};

        Vertices    mVertices;
        Indices     mIndices;
        glm::mat4   mAABB;

        std::vector<Node*>          mNodes;
        std::vector<Node*>          mLinearNodes;
        std::vector<Skin*>          mSkins;
        std::vector<Texture>        mTextures;
        std::vector<TextureSampler> mTexSamplers;
        std::vector<Material>       mMaterials;
        std::vector<Animation>      mAnimations;
        std::vector<std::string>    mExtensions;

        Dimensions mDimensions;
    };
}



