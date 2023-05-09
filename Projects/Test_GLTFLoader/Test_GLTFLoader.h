#pragma once

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "tiny_gltf.h"
#include "vulkanexamplebase.h"

#define ENABLE_VALIDATION true

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec3 color;
};

struct VertexBuffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct IndexBuffer
{
    int32_t count;
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct StagingBuffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct Node;

struct Primitive
{
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t materialIndex;
};

struct Mesh
{
    std::vector<Primitive> primitives;
};

struct Node
{
    Node* parent;
    std::vector<Node*> children;
    Mesh mesh;
    glm::mat4 matrix;
    ~Node() {
        for (auto & child : children) {
            delete child;
        }
    }
};

struct Material
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    uint32_t baseColorTextureIndex;
    // uint32_t normalTextureIndex;

};

struct Image
{
    vks::Texture2D texture;
    VkDescriptorSet descriptorSet;
};

struct Texture
{
    int32_t imageIndex;
};

struct ShaderData
{
    vks::Buffer buffer;
    struct Values {
        glm::mat4 projectMat;
        glm::mat4 viewMat;
        glm::vec4 lightPos = glm::vec4(0.0f, 5.0f, -5.0f, 1.0f);
        glm::vec4 viewPos;
    } values;
};

struct Pipelines
{
    VkPipeline solid{};
    VkPipeline wireFrame = VK_NULL_HANDLE;
};

struct DescriptorSetLayouts
{
    VkDescriptorSetLayout matrices;
    VkDescriptorSetLayout textures;
};

class VulkanGLTFLoader
{
public:
    ~VulkanGLTFLoader();
    void LoadImages(tinygltf::Model& input);
    void LoadTextures(tinygltf::Model& input);
    void LoadMaterials(tinygltf::Model& input);
    void LoadNode(
        const tinygltf::Node& inputNode,
        const tinygltf::Model& input,
        Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);

    // Draw a single node including child nodes (if present)
    void DrawNode(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, Node* node);
    // Draw the glTF scene starting at the top-level-nodes
    void Draw(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);

public:

    vks::VulkanDevice* vulkanDevice;
    VkQueue copyQueue;

    VertexBuffer vertices;
    IndexBuffer indices;

    std::vector<Image> images;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    std::vector<Node*> nodes;
};

class TestGLTFLoader : public VulkanExampleBase
{
public:

    TestGLTFLoader();
    ~TestGLTFLoader();

    void LoadGLTFFile(std::string filename);
    void LoadAssets();

    void getEnabledFeatures() override;
    void buildCommandBuffers() override;

    void SetupDescriptors();
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();

    void prepare() override;
    void render() override;
    void viewChanged() override;

    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

public:

    bool wireFrame = false;

    VulkanGLTFLoader glTFModel{};

    VkPipelineLayout pipelineLayout{};
    VkDescriptorSet descriptorSet{};

    ShaderData shaderData{};
    Pipelines pipelines{};
    DescriptorSetLayouts descriptorSetLayouts{};
};