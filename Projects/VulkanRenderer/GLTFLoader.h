#pragma once

#include "ProjectPCH.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "tiny_gltf.h"

namespace LeoRenderer
{
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
        void FromGLTFImage(tinygltf::Image& gltfImage, std::string path, vks::VulkanDevice* device, VkQueue copyQueue);

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
        explicit Material(vks::VulkanDevice* device) : mDevice(device) {};
        void CreateDescSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);

        enum AlphaMode {
            ALPHAMODE_OPAQUE,
            ALPHAMODE_MASK,
            ALPHAMODE_BLEND };

        vks::VulkanDevice*  mDevice = nullptr;
        AlphaMode           mAlphaMode = ALPHAMODE_OPAQUE;
        float               mAlphaCutoff = 1.0f;
        float               mMetallicFactor = 1.0f;
        float               mRoughnessFactor = 1.0f;
        glm::vec4           mBaseColorFactor = glm::vec4(1.0f);
        Texture*            mBaseColorTexture = nullptr;
        Texture*            mMetallicRoughnessTexture = nullptr;
        Texture*            mNormalTexture = nullptr;
        Texture*            mOcclusionTexture = nullptr;
        Texture*            mEmissiveTexture = nullptr;
        Texture*            mSpecularGlossinessTexture = nullptr;
        Texture*            mDiffuseTexture = nullptr;
        VkDescriptorSet     mDescriptorSet{};
    };

    struct Dimensions
    {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
        glm::vec3 size{};
        glm::vec3 center{};
        float radius{};
    };

    struct Primitive
    {
        Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) :
            mFirstIndex(firstIndex),
            mIndexCount(indexCount),
            mMaterial(material) {};
        void SetDimensions(glm::vec3 min, glm::vec3 max);

        Dimensions mDimensions;
        uint32_t mFirstIndex;
        uint32_t mIndexCount;
        uint32_t mFirstVertex{};
        uint32_t mVertexCount{};
        Material& mMaterial;
    };

    struct Mesh
    {
        Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
        ~Mesh();

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
    };

    struct AnimationChannel
    {
        enum PathType
        {
            TRANSLATION, ROTATION, SCALE
        };
        PathType mPath;
        Node* mNode;
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
        static VkVertexInputBindingDescription InputBindingDescription(uint32_t binding);
        static VkVertexInputAttributeDescription InputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
        static std::vector<VkVertexInputAttributeDescription> InputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent>& components);
        /** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
        static VkPipelineVertexInputStateCreateInfo* GetPipelineVertexInputState(const std::vector<VertexComponent>& components);

        glm::vec3 mPos;
        glm::vec3 mNormal;
        glm::vec2 mUV;
        glm::vec4 mColor;
        glm::vec4 mJoint0;
        glm::vec4 mWeight0;
        glm::vec4 mTangent;
        static VkVertexInputBindingDescription mVertexInputBindingDescription;
        static std::vector<VkVertexInputAttributeDescription> mVertexInputAttributeDescriptions;
        static VkPipelineVertexInputStateCreateInfo mPipelineVertexInputStateCreateInfo;
    };

    struct Vertices
    {
        uint32_t count;
        VkBuffer buffer;
        VkDeviceMemory memory;
    };
    struct Indices
    {
        uint32_t count;
        VkBuffer buffer;
        VkDeviceMemory memory;
    };

    enum FileLoadingFlags
    {
        None = 0x00000000,
        PreTransformVertices = 0x00000001,
        PreMultiplyVertexColors = 0x00000002,
        FlipY = 0x00000004,
        DontLoadImages = 0x00000008
    };

    enum RenderFlags
    {
        BindImages = 0x00000001,
        RenderOpaqueNodes = 0x00000002,
        RenderAlphaMaskedNodes = 0x00000004,
        RenderAlphaBlendedNodes = 0x00000008
    };

    class GLTFModel
    {
    public:
        GLTFModel() {};
        ~GLTFModel();
        void LoadNode(LeoRenderer::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale);
        void LoadSkins(tinygltf::Model& gltfModel);
        void LoadImages(tinygltf::Model& gltfModel, vks::VulkanDevice* device, VkQueue transferQueue);
        void LoadMaterials(tinygltf::Model& gltfModel);
        void LoadAnimations(tinygltf::Model& gltfModel);
        void LoadFromFile(std::string& filename, vks::VulkanDevice* device, VkQueue transferQueue, uint32_t fileLoadingFlags = LeoRenderer::FileLoadingFlags::None, float scale = 1.0f);
        void BindBuffers(VkCommandBuffer commandBuffer);
        void DrawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
        void Draw(VkCommandBuffer commandBuffer, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
        void GetNodeDimensions(Node* node, glm::vec3& min, glm::vec3& max);
        void GetSceneDimensions();
        void UpdateAnimation(uint32_t index, float time);
        Node* FindNode(Node* parent, uint32_t index);
        Node* NodeFromIndex(uint32_t index);
        void PrepareNodeDescriptor(LeoRenderer::Node* node, VkDescriptorSetLayout descriptorSetLayout);

    public:
        vks::VulkanDevice* m_pDevice;
        VkDescriptorPool mDescPool;

        Vertices mVertices;
        Indices mIndices;

        std::vector<Node*> mNodes;
        std::vector<Node*> mLinearNodes;
        std::vector<Skin*> mSkins;
        std::vector<Texture> mTextures;
        std::vector<Material> mMaterials;
        std::vector<Animation> mAnimations;

        Dimensions mDimensions;

        bool m_bMetallicWorkFlow = true;
        bool m_bBufferBound = true;
        std::string mPath;

    private:
        LeoRenderer::Texture* GetTexture(uint32_t index);
        void CreateEmptyTexture(VkQueue transferQueue);

    private:
        LeoRenderer::Texture mEmptyTexture;
    };
}



