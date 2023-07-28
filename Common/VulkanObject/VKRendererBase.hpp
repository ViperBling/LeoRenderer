#pragma once

#pragma comment(linker, "/subsystem:windows")

#include "ProjectPCH.hpp"

#include "CmdLineParser.hpp"
#include "KeyCodes.hpp"
#include "VKTools.hpp"
#include "VKDebug.hpp"
#include "VKUIOverlay.hpp"
#include "VKSwapChain.hpp"
#include "VKBuffer.hpp"
#include "VKDevice.hpp"
#include "VKTexture.hpp"

#include "VKInitializers.hpp"
#include "Camera.hpp"
#include "Benchmark.hpp"


struct RenderTarget
{
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory memory;
};

struct MultiSampleTarget
{
    RenderTarget color;
    RenderTarget depth;
};

class VKRendererBase
{
public:
    explicit VKRendererBase(bool msaa = false, bool enableValidation = false);
    virtual ~VKRendererBase();
    /** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
    bool InitVulkan();

    void SetupConsole(std::string title);
    void SetupDPIAwareness();
    HWND SetupWindow(HINSTANCE hInstance, WNDPROC wndProc);
    void HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    /** @brief (Virtual) Creates the application wide Vulkan instance */
    virtual VkResult CreateInstance(bool enableValidation);

    /** @brief (Pure virtual) Render function to be implemented by the sample application */
    virtual void Render() = 0;

    /** @brief (Virtual) Called when the camera view has changed */
    virtual void ViewChanged();

    /** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
    virtual void KeyPressed(uint32_t);

    /** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
    virtual void MouseMoved(double x, double y, bool &handled);

    /** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
    virtual void WindowResized();

    /** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
    virtual void BuildCommandBuffers();

    /** @brief (Virtual) Setup default depth and stencil views */
    virtual void SetupDepthStencil();

    /** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
    virtual void SetupFrameBuffer();

    /** @brief (Virtual) Setup a default renderpass */
    virtual void SetupRenderPass();

    /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
    virtual void GetEnabledFeatures();

    /** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
    virtual void GetEnabledExtensions();

    /** @brief Prepares all Vulkan resources and functions required to run the sample */
    virtual void Prepare();

    /** @brief Loads a SPIR-V shader file for the given shader stage */
    VkPipelineShaderStageCreateInfo LoadShader(std::string filename, VkShaderStageFlagBits stage);

    /** @brief Entry point for the main render loop */
    void RenderLoop();

    /** @brief Adds the drawing commands for the ImGui overlay to the given command buffer */
    void DrawUI(VkCommandBuffer commandBuffer);

    /** Prepare the next frame for workload submission by acquiring the next swap chain image */
    void PrepareFrame();

    /** @brief Presents the current image to the swap chain */
    void SubmitFrame();

    /** @brief (Virtual) Default image acquire + submission and command buffer submission function */
    virtual void RenderFrame();

    /** @brief (Virtual) Called when the UI overlay is updating, can be used to add custom elements to the overlay */
    virtual void OnUpdateUIOverlay(LeoVK::UIOverlay *overlay);

    virtual void FileDropped(std::string& filename);
    virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    bool mbPrepared = false;
    bool mbResized = false;
    bool mbViewUpdated = false;
    uint32_t mWidth = 1280;
    uint32_t mHeight = 720;

    LeoVK::UIOverlay mUIOverlay;
    CmdLineParser mCmdLineParser;

    /** @brief Last frame time measured using a high performance timer (if available) */
    float mFrameTimer = 1.0f;

    LeoVK::Benchmark mBenchmark;

    /** @brief Encapsulated physical and logical vulkan device */
    LeoVK::VulkanDevice* mpVulkanDevice;

    /** @brief Example settings that can be changed e.g. by command line arguments */
    struct Settings
    {
        /** @brief Activates validation layers (and message output) when set to true */
        bool validation = false;
        /** @brief Set to true if fullscreen mode has been requested via command line */
        bool fullscreen = false;
        /** @brief Set to true if v-sync will be forced for the swapchain */
        bool vsync = false;
        /** @brief Enable UI overlay */
        bool overlay = true;
        bool multiSampling = true;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_4_BIT;;
    } mSettings;

    VkClearColorValue mDefaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

    static std::vector<const char*> mArgs;

    // Defines a frame rate independent timer value clamped from -1.0...1.0
    // For use in animations, rotations, etc.
    float mTimer = 0.0f;
    // Multiplier for speeding up (or slowing down) the global timer
    float mTimerSpeed = 0.25f;
    bool mbPaused = false;

    Camera mCamera;
    glm::vec2 mMousePos;

    std::string mTitle = "Vulkan Renderer";
    std::string mName = "vulkanRenderer";
    uint32_t mAPIVersion = VK_API_VERSION_1_3;

    RenderTarget mDepthStencil;

    struct
    {
        bool left = false;
        bool right = false;
        bool middle = false;
    } mMouseButtons;

    // OS specific
    HWND mHwnd;
    HINSTANCE mHInstance;

protected:
    // Returns the path to the root of the glsl or hlsl shader directory.
    std::string GetShadersPath() const;

protected:
    // Frame counter to display fps
    uint32_t mFrameCounter = 0;
    uint32_t mLastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> mLastTimestamp, mTPrevEnd;

    // Vulkan instance, stores all per-application states
    VkInstance mInstance;
    std::vector<std::string> mSupportedInstanceExtensions;

    VkPhysicalDevice mPhysicalDevice;
    VkPhysicalDeviceProperties mDeviceProps;
    VkPhysicalDeviceFeatures mDeviceFeatures;
    VkPhysicalDeviceMemoryProperties mDeviceMemoryProps;
    /** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
    VkPhysicalDeviceFeatures mEnabledFeatures{};
    /** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
    std::vector<const char*> mEnabledDeviceExtensions;
    std::vector<const char*> mEnabledInstanceExtensions;
    /** @brief Optional pNext structure for passing extension structures to device creation */
    void* mpDeviceCreatepNexChain = nullptr;
    /** @brief Logical device, application's view of the physical device (GPU) */
    VkDevice mDevice;
    // Handle to the device graphics queue that command buffers are submitted to
    VkQueue mQueue;
    // Depth buffer format (selected during Vulkan initialization)
    VkFormat mDepthFormat;
    // Command buffer pool
    VkCommandPool mCmdPool;
    /** @brief Pipeline stages used to wait at for graphics queue submissions */
    VkPipelineStageFlags mSubmitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // Contains command buffers and semaphores to be presented to the queue
    VkSubmitInfo mSubmitInfo;
    // Command buffers used for rendering
    std::vector<VkCommandBuffer> mDrawCmdBuffers;
    // Global render pass for frame buffer writes
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer> mFrameBuffers;
    // Active frame buffer index
    uint32_t mCurrentBuffer = 0;
    // Descriptor set pool
    VkDescriptorPool mDescPool = VK_NULL_HANDLE;
    // List of shader modules created (stored for cleanup)
    std::vector<VkShaderModule> mShaderModules;
    // Pipeline cache object
    VkPipelineCache mPipelineCache;
    // Wraps the swap chain to present images (framebuffers) to the windowing system
    VulkanSwapChain mSwapChain;
    // Synchronization semaphores
    struct
    {
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    } mSemaphores;
    std::vector<VkFence> mWaitFences;

private:
    std::string getWindowTitle();
    void windowResizing();
    void handleMouseMove(int32_t x, int32_t y);
    void nextFrame();
    void updateOverlay();
    void createPipelineCache();
    void createCommandPool();
    void createSynchronizationPrimitives();
    void initSwapChain();
    void setupSwapChain();
    void createCommandBuffers();
    void destroyCommandBuffers();
    void setupRenderTarget(VkImage* image, VkImageView* imageView, VkDeviceMemory* memory, bool isDepth);

private:
    uint32_t mDstWidth;
    uint32_t mDstHeight;
    bool mbResizing = false;
    std::string mShaderDir = "GLSL";
    MultiSampleTarget mMSTarget;
};