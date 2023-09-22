#include "VKRendererBase.hpp"

std::vector<const char*> VKRendererBase::mArgs;

// Public
VKRendererBase::VKRendererBase(bool msaa, bool enableValidation)
{
    // Check for a valid asset path
    struct stat info{};
    if (stat(GetAssetsPath().c_str(), &info) != 0)
    {
        std::string msg = "Could not locate asset path in \"" + GetAssetsPath() + "\" !";
        MessageBoxA(nullptr, msg.c_str(), "Fatal error", MB_OK | MB_ICONERROR);
        exit(-1);
    }

    mSettings.multiSampling = msaa;
    mSettings.sampleCount = msaa ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    mSettings.validation = enableValidation;
    mUIOverlay.mMSAA = mSettings.sampleCount;

    // Command line arguments
    mCmdLineParser.Add("help", { "--help" }, 0, "Show help");
    mCmdLineParser.Add("validation", { "-v", "--validation" }, 0, "Enable validation layers");
    mCmdLineParser.Add("vsync", { "-vs", "--vsync" }, 0, "Enable V-Sync");
    mCmdLineParser.Add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
    mCmdLineParser.Add("width", { "-w", "--width" }, 1, "Set window width");
    mCmdLineParser.Add("height", { "-h", "--height" }, 1, "Set window height");
    mCmdLineParser.Add("shaders", { "-s", "--shaders" }, 1, "Select shader type to use (glsl or hlsl)");
    mCmdLineParser.Add("gpuSelection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    mCmdLineParser.Add("gpuList", { "-gl", "--listGPUs" }, 0, "Display a list of available Vulkan devices");
    mCmdLineParser.Add("benchmark", { "-b", "--benchmark" }, 0, "Run example in benchmark mode");
    mCmdLineParser.Add("benchmarkWarmup", { "-bw", "--benchWarmup" }, 1, "Set warmup time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkRuntime", { "-br", "--benchRuntime" }, 1, "Set duration time for benchmark mode in seconds");
    mCmdLineParser.Add("benchmarkResultFile", { "-bf", "--benchFilename" }, 1, "Set file name for benchmark results");
    mCmdLineParser.Add("benchmarkResultFrames", { "-bt", "--benchFrameTimes" }, 0, "Save frame times to benchmark results file");
    mCmdLineParser.Add("benchmarkFrames", { "-bfs", "--benchmarkFrames" }, 1, "Only render the given number of frames");

    mCmdLineParser.Parse(mArgs);
    if (mCmdLineParser.IsSet("help")) 
    {

        SetupConsole("Debug Console");
        mCmdLineParser.PrintHelp();
        std::cin.get();
        exit(0);
    }
    if (mCmdLineParser.IsSet("validation")) mSettings.validation = true;
    
    if (mCmdLineParser.IsSet("vsync")) mSettings.vsync = true;

    if (mCmdLineParser.IsSet("height")) mHeight = mCmdLineParser.GetValueAsInt("height", (int)mHeight);

    if (mCmdLineParser.IsSet("width")) mWidth = mCmdLineParser.GetValueAsInt("width", (int)mWidth);

    if (mCmdLineParser.IsSet("fullscreen")) mSettings.fullscreen = true;

    if (mCmdLineParser.IsSet("shaders"))
    {
        std::string value = mCmdLineParser.GetValueAsString("shaders", "glsl");

        if ((value != "glsl") && (value != "hlsl")) std::cerr << "Shader type must be one of 'glsl' or 'hlsl'\n";
        else mShaderDir = value;
    }

    if (mCmdLineParser.IsSet("benchmark"))
    {
        mBenchmark.mbActive = true;
        LeoVK::VKTools::bErrorModeSilent = true;
    }

    if (mCmdLineParser.IsSet("benchmarkwarmup")) {
        mBenchmark.mWarmup = mCmdLineParser.GetValueAsInt("benchmarkwarmup", mBenchmark.mWarmup);
    }
    if (mCmdLineParser.IsSet("benchmarkruntime")) {
        mBenchmark.mDuration = mCmdLineParser.GetValueAsInt("benchmarkruntime", mBenchmark.mDuration);
    }
    if (mCmdLineParser.IsSet("benchmarkresultfile")) {
        mBenchmark.mFilename = mCmdLineParser.GetValueAsString("benchmarkresultfile", mBenchmark.mFilename);
    }
    if (mCmdLineParser.IsSet("benchmarkresultframes")) {
        mBenchmark.mbOutputFrameTime = true;
    }
    if (mCmdLineParser.IsSet("benchmarkframes")) {
        mBenchmark.mOutputFrames = mCmdLineParser.GetValueAsInt("benchmarkframes", mBenchmark.mOutputFrames);
    }

    // Enable console if validation is active, debug message callback will output to it
    if (this->mSettings.validation)
    {
        SetupConsole("Debug Console");
    }
    SetupDPIAwareness();
}

VKRendererBase::~VKRendererBase()
{
    // Clean up Vulkan resources
    mSwapChain.Cleanup();
    if (mDescPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(mDevice, mDescPool, nullptr);
    }
    destroyCommandBuffers();
    if (mRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    }
    for (auto & mFrameBuffer : mFrameBuffers)
    {
        vkDestroyFramebuffer(mDevice, mFrameBuffer, nullptr);
    }

    for (auto& shaderModule : mShaderModules)
    {
        vkDestroyShaderModule(mDevice, shaderModule, nullptr);
    }
    vkDestroyImageView(mDevice, mDepthStencil.imageView, nullptr);
    vkDestroyImage(mDevice, mDepthStencil.image, nullptr);
    vkFreeMemory(mDevice, mDepthStencil.memory, nullptr);

    vkDestroyPipelineCache(mDevice, mPipelineCache, nullptr);
    vkDestroyCommandPool(mDevice, mCmdPool, nullptr);

    if (mSettings.multiSampling)
    {
        vkDestroyImage(mDevice, mMSTarget.color.image, nullptr);
        vkDestroyImageView(mDevice, mMSTarget.color.imageView, nullptr);
        vkFreeMemory(mDevice, mMSTarget.color.memory, nullptr);
        vkDestroyImage(mDevice, mMSTarget.depth.image, nullptr);
        vkDestroyImageView(mDevice, mMSTarget.depth.imageView, nullptr);
        vkFreeMemory(mDevice, mMSTarget.depth.memory, nullptr);
    }

    vkDestroySemaphore(mDevice, mSemaphores.presentComplete, nullptr);
    vkDestroySemaphore(mDevice, mSemaphores.renderComplete, nullptr);
    for (auto& fence : mWaitFences)
    {
        vkDestroyFence(mDevice, fence, nullptr);
    }

    if (mSettings.overlay) {
        mUIOverlay.FreeResources();
    }

    delete mpVulkanDevice;

    if (mSettings.validation)
    {
        LeoVK::Debug::FreeDebugCallback(mInstance);
    }
    vkDestroyInstance(mInstance, nullptr);
}

bool VKRendererBase::InitVulkan()
{
    VkResult result;

    // Instance
    result = CreateInstance(mSettings.validation);
    if (result)
    {
        LeoVK::VKTools::ExitFatal("Could not create Vulkan instance : \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }
    if (mSettings.validation) LeoVK::Debug::SetupDebugging(mInstance);

    // Physical Device
    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &gpuCount, nullptr))
    if (gpuCount == 0)
    {
        LeoVK::VKTools::ExitFatal("No suitable vulkan device! ", -1);
        return false;
    }
    // Enumerate Device
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    result = vkEnumeratePhysicalDevices(mInstance, &gpuCount, physicalDevices.data());
    if (result)
    {
        LeoVK::VKTools::ExitFatal("Could not enumerate physical devices : \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }

    // Select GPU
    uint32_t selectedDevice = 0;
    if (mCmdLineParser.IsSet("gpuSelection"))
    {
        uint32_t index = mCmdLineParser.GetValueAsInt("gpuSelection", 0);
        if (index > gpuCount - 1)
        {
            std::cerr << "Selected device index " << index << " is out of range, reverting to device 0 (use -listGPUs to show available Vulkan devices)" << "\n";
        }
        else
        {
            selectedDevice = index;
        }
    }
    if (mCmdLineParser.IsSet("gpuList"))
    {
        std::cout << "Available Vulkan Device" << "\n";
        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDeviceProperties deviceProps;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
            std::cout << "Device [" << i << "] : " << deviceProps.deviceName << std::endl;
            std::cout << " Type: " << LeoVK::VKTools::PhysicalDeviceTypeString(deviceProps.deviceType) << "\n";
            std::cout << " API: " << (deviceProps.apiVersion >> 22) << "." << ((deviceProps.apiVersion >> 12) & 0x3ff) << "." << (deviceProps.apiVersion & 0xfff) << "\n";
        }
    }

    mPhysicalDevice = physicalDevices[selectedDevice];

    // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(mPhysicalDevice, &mDeviceProps);
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &mDeviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mDeviceMemoryProps);

    // 子类可重写此方法以设置一些必要的Feature
    GetEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    mpVulkanDevice = new LeoVK::VulkanDevice(mPhysicalDevice);

    // 子类可以启用从物理设备中获取的一系列扩展
    GetEnabledExtensions();

    result = mpVulkanDevice->CreateLogicalDevice(mEnabledFeatures, mEnabledDeviceExtensions, mpDeviceCreatepNexChain);
    if (result != VK_SUCCESS)
    {
        LeoVK::VKTools::ExitFatal("Could not create Vulkan device: \n" + LeoVK::VKTools::ErrorString(result), result);
        return false;
    }

    mDevice = mpVulkanDevice->mLogicalDevice;

    vkGetDeviceQueue(mDevice, mpVulkanDevice->mQueueFamilyIndices.graphics, 0, &mQueue);

    // Find a suitable Depth format
    VkBool32 validDepthFormat = LeoVK::VKTools::GetSupportedDepthFormat(mPhysicalDevice, &mDepthFormat);
    assert(validDepthFormat);

    mSwapChain.Connect(mInstance, mPhysicalDevice, mDevice);

    // Sync Objects
    VkSemaphoreCreateInfo semaphoreCI = LeoVK::Init::SemaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCI, nullptr, &mSemaphores.presentComplete))
    VK_CHECK(vkCreateSemaphore(mDevice, &semaphoreCI, nullptr, &mSemaphores.renderComplete))

    // Set up submit info struct
    mSubmitInfo = LeoVK::Init::SubmitInfo();
    mSubmitInfo.pWaitDstStageMask = &mSubmitPipelineStages;
    mSubmitInfo.waitSemaphoreCount = 1;
    mSubmitInfo.pWaitSemaphores = &mSemaphores.presentComplete;
    mSubmitInfo.signalSemaphoreCount = 1;
    mSubmitInfo.pSignalSemaphores = &mSemaphores.renderComplete;

    return true;
}

void VKRendererBase::SetupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    SetConsoleTitleA(title.c_str());
}

void VKRendererBase::SetupDPIAwareness()
{
    typedef HRESULT *(__stdcall *SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore)
    {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if (setProcessDpiAwareness != nullptr)
        {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VKRendererBase::SetupWindow(HINSTANCE hInstance, WNDPROC wndProc)
{
    this->mHInstance = hInstance;

    WNDCLASSEXA wndClass;

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = nullptr;
    wndClass.lpszClassName = mName.c_str();
    wndClass.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassExA(&wndClass))
    {
        std::cout << "Could not register window class!\n";
        fflush(stdout);
        exit(1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (mSettings.fullscreen)
    {
        if ((mWidth != (uint32_t)screenWidth) && (mHeight != (uint32_t)screenHeight))
        {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth  = mWidth;
            dmScreenSettings.dmPelsHeight = mHeight;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                if (MessageBoxA(nullptr, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
                {
                    mSettings.fullscreen = false;
                }
                else
                {
                    return nullptr;
                }
            }
            screenWidth = (int)mWidth;
            screenHeight = (int)mHeight;
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;

    if (mSettings.fullscreen)
    {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect;
    windowRect.left = 0L;
    windowRect.top = 0L;
    windowRect.right = mSettings.fullscreen ? (long)screenWidth : (long)mWidth;
    windowRect.bottom = mSettings.fullscreen ? (long)screenHeight : (long)mHeight;

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    mHwnd = CreateWindowExA(
        0,
        mName.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!mSettings.fullscreen)
    {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(mHwnd, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    if (!mHwnd)
    {
        printf("Could not create window!\n");
        fflush(stdout);
        return nullptr;
    }

    ShowWindow(mHwnd, SW_SHOW);
    SetForegroundWindow(mHwnd);
    SetFocus(mHwnd);

    return mHwnd;
}

void VKRendererBase::HandleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CLOSE:
            mbPrepared = false;
            DestroyWindow(hWnd);
            PostQuitMessage(0);
            break;
        case WM_PAINT:
            ValidateRect(mHwnd, nullptr);
            break;
        case WM_KEYDOWN:
            switch (wParam)
            {
                case KEY_P:
                    mbPaused = !mbPaused;
                    break;
                case KEY_F1:
                    mUIOverlay.mbVisible = !mUIOverlay.mbVisible;
                    mUIOverlay.mbUpdated = true;
                    break;
                case KEY_ESCAPE:
                    PostQuitMessage(0);
                    break;
                default:
                    break;
            }

            if (mCamera.mType == FirstPerson)
            {
                switch (wParam)
                {
                    case KEY_W:
                        mCamera.mKeys.mUp = true;
                        break;
                    case KEY_S:
                        mCamera.mKeys.mDown = true;
                        break;
                    case KEY_A:
                        mCamera.mKeys.mLeft = true;
                        break;
                    case KEY_D:
                        mCamera.mKeys.mRight = true;
                        break;
                    default:
                        break;
                }
            }

            KeyPressed((uint32_t)wParam);
            break;
        case WM_KEYUP:
            if (mCamera.mType == FirstPerson)
            {
                switch (wParam)
                {
                    case KEY_W:
                        mCamera.mKeys.mUp = false;
                        break;
                    case KEY_S:
                        mCamera.mKeys.mDown = false;
                        break;
                    case KEY_A:
                        mCamera.mKeys.mLeft = false;
                        break;
                    case KEY_D:
                        mCamera.mKeys.mRight = false;
                        break;
                    default:
                        break;
                }
            }
            break;
        case WM_LBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.left = true;
            break;
        case WM_RBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.right = true;
            break;
        case WM_MBUTTONDOWN:
            mMousePos = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
            mMouseButtons.middle = true;
            break;
        case WM_LBUTTONUP:
            mMouseButtons.left = false;
            break;
        case WM_RBUTTONUP:
            mMouseButtons.right = false;
            break;
        case WM_MBUTTONUP:
            mMouseButtons.middle = false;
            break;
        case WM_MOUSEWHEEL:
        {
            short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            mCamera.Translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
            mbViewUpdated = true;
            break;
        }
        case WM_MOUSEMOVE:
        {
            handleMouseMove(LOWORD(lParam), HIWORD(lParam));
            break;
        }
        case WM_SIZE:
            if ((mbPrepared) && (wParam != SIZE_MINIMIZED))
            {
                if ((mbResizing) || ((wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED)))
                {
                    mDstWidth = LOWORD(lParam);
                    mDstHeight = HIWORD(lParam);
                    windowResizing();
                }
            }
            break;
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
            minMaxInfo->ptMinTrackSize.x = 64;
            minMaxInfo->ptMinTrackSize.y = 64;
            break;
        }
        case WM_ENTERSIZEMOVE:
            mbResizing = true;
            break;
        case WM_EXITSIZEMOVE:
            mbResizing = false;
            break;
        case WM_DROPFILES:
        {
            std::string fname;
            HDROP hDrop = reinterpret_cast<HDROP>(wParam);
            // extract files here
            char filename[MAX_PATH];
            uint32_t count = DragQueryFileA(hDrop, -1, nullptr, 0);
            for (uint32_t i = 0; i < count; ++i)
            {
                if (DragQueryFileA(hDrop, i, filename, MAX_PATH))
                {
                    fname = filename;
                }
                break;
            }
            DragFinish(hDrop);
            FileDropped(fname);
            break;
        }
    }

    OnHandleMessage(hWnd, uMsg, wParam, lParam);
}

VkResult VKRendererBase::CreateInstance(bool enableValidation)
{
    mSettings.validation = enableValidation;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = mName.c_str();
    appInfo.pEngineName = mName.c_str();
    appInfo.apiVersion = mAPIVersion;

    std::vector<const char*> instExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    instExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

    // Get extensions supported by the instance and store for later use
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties extension : extensions) mSupportedInstanceExtensions.emplace_back(extension.extensionName);
        }
    }

    // Enabled requested instance extensions
    if (!mEnabledInstanceExtensions.empty())
    {
        for (const char * enabledExtension : mEnabledInstanceExtensions)
        {
            // Output message if requested extension is not available
            if (std::find(mSupportedInstanceExtensions.begin(), mSupportedInstanceExtensions.end(), enabledExtension) == mSupportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instExtensions.push_back(enabledExtension);
        }
    }
    VkInstanceCreateInfo instanceCI {};
    instanceCI.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCI.pNext = nullptr;
    instanceCI.pApplicationInfo = &appInfo;

    if (!instExtensions.empty())
    {
        if (mSettings.validation)
        {
            instExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// SRS - Dependency when VK_EXT_DEBUG_MARKER is enabled
            instExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCI.enabledExtensionCount = (uint32_t)instExtensions.size();
        instanceCI.ppEnabledExtensionNames = instExtensions.data();
    }

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    // Note that on Android this layer requires at least NDK r20
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (mSettings.validation)
    {
        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties)
        {
            if (strcmp(layer.layerName, validationLayerName) == 0)
            {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCI.ppEnabledLayerNames = &validationLayerName;
            instanceCI.enabledLayerCount = 1;
        } else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    return vkCreateInstance(&instanceCI, nullptr, &mInstance);
}

void VKRendererBase::ViewChanged()
{

}

void VKRendererBase::KeyPressed(uint32_t)
{

}

void VKRendererBase::MouseMoved(double x, double y, bool &handled)
{

}

void VKRendererBase::WindowResized()
{

}

void VKRendererBase::BuildCommandBuffers()
{

}

void VKRendererBase::SetupDepthStencil()
{
    VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = mDepthFormat;
    imageCI.extent = { mWidth, mHeight, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VK_CHECK(vkCreateImage(mDevice, &imageCI, nullptr, &mDepthStencil.image))
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(mDevice, mDepthStencil.image, &memReqs);

    VkMemoryAllocateInfo memAI = LeoVK::Init::MemoryAllocateInfo();
    memAI.allocationSize = memReqs.size;
    memAI.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(mDevice, &memAI, nullptr, &mDepthStencil.memory))
    VK_CHECK(vkBindImageMemory(mDevice, mDepthStencil.image, mDepthStencil.memory, 0))

    VkImageViewCreateInfo depthViewCI = LeoVK::Init::ImageViewCreateInfo();
    depthViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewCI.image = mDepthStencil.image;
    depthViewCI.format = mDepthFormat;
    depthViewCI.subresourceRange.baseMipLevel = 0;
    depthViewCI.subresourceRange.levelCount = 1;
    depthViewCI.subresourceRange.baseArrayLayer = 0;
    depthViewCI.subresourceRange.layerCount = 1;
    depthViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (mDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        depthViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    VK_CHECK(vkCreateImageView(mDevice, &depthViewCI, nullptr, &mDepthStencil.imageView))
}

void VKRendererBase::SetupFrameBuffer()
{
    if (mSettings.multiSampling)
    {
        setupRenderTarget(&mMSTarget.color.image, &mMSTarget.color.imageView, &mMSTarget.color.memory, false);
        setupRenderTarget(&mMSTarget.depth.image, &mMSTarget.depth.imageView, &mMSTarget.depth.memory, true);
    }
    VkImageView attachments[4];
    if (mSettings.multiSampling)
    {
        attachments[0] = mMSTarget.color.imageView;
        attachments[2] = mMSTarget.depth.imageView;
        attachments[3] = mDepthStencil.imageView;
    }
    else
    {
        attachments[1] = mDepthStencil.imageView;
    }

    VkFramebufferCreateInfo frameBufferCI{};
    frameBufferCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCI.pNext = nullptr;
    frameBufferCI.renderPass = mRenderPass;
    frameBufferCI.attachmentCount = mSettings.multiSampling ? 4 : 2;
    frameBufferCI.pAttachments = attachments;
    frameBufferCI.width = mWidth;
    frameBufferCI.height = mHeight;
    frameBufferCI.layers = 1;

    // Create frame buffers for every swap chain image
    mFrameBuffers.resize(mSwapChain.mImageCount);
    for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
    {
        if (mSettings.multiSampling)
        {
            attachments[1] = mSwapChain.mBuffers[i].mView;
        }
        else
        {
            attachments[0] = mSwapChain.mBuffers[i].mView;
        }
        VK_CHECK(vkCreateFramebuffer(mDevice, &frameBufferCI, nullptr, &mFrameBuffers[i]));
    }
}

void VKRendererBase::SetupRenderPass()
{
    if (mSettings.multiSampling)
    {
        std::array<VkAttachmentDescription, 4> attachments{};

        // MSAA Attachment render to
        attachments[0].format = mSwapChain.mFormat;
        attachments[0].samples = mSettings.sampleCount;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // 用于显示的Attachment，同时也是MSAA的Resolve结果
        attachments[1].format = mSwapChain.mFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // MSAA的Depth Stencil，用于Resolve
        attachments[2].format = mDepthFormat;
        attachments[2].samples = mSettings.sampleCount;
        attachments[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Depth resolve attachment
        attachments[3].format = mDepthFormat;
        attachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 2;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference resolveRef{};
        resolveRef.attachment = 1;
        resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc{};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorRef;
        subpassDesc.pResolveAttachments = &resolveRef;
        subpassDesc.pDepthStencilAttachment = &depthRef;

        std::array<VkSubpassDependency, 2> subpassDep{};
        subpassDep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        subpassDep[0].dstSubpass = 0;
        subpassDep[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDep[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDep[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDep[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDep[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        subpassDep[1].srcSubpass = 0;
        subpassDep[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        subpassDep[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpassDep[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        subpassDep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        subpassDep[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        subpassDep[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassCI = LeoVK::Init::RenderPassCreateInfo();
        renderPassCI.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassCI.pAttachments = attachments.data();
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpassDesc;
        renderPassCI.dependencyCount = static_cast<uint32_t>(subpassDep.size());
        renderPassCI.pDependencies = subpassDep.data();

        VK_CHECK(vkCreateRenderPass(mDevice, &renderPassCI, nullptr, &mRenderPass))
    }
    else
    {
        std::array<VkAttachmentDescription, 2> attachments = {};
        // Color attachment
        attachments[0].format = mSwapChain.mFormat;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // Depth attachment
        attachments[1].format = mDepthFormat;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc{};
        subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorReference;
        subpassDesc.pDepthStencilAttachment = &depthReference;
        subpassDesc.inputAttachmentCount = 0;
        subpassDesc.pInputAttachments = nullptr;
        subpassDesc.preserveAttachmentCount = 0;
        subpassDesc.pPreserveAttachments = nullptr;
        subpassDesc.pResolveAttachments = nullptr;

        // Subpass dependencies for layout transitions
        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        dependencies[0].dependencyFlags = 0;

        dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].dstSubpass = 0;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].srcAccessMask = 0;
        dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
        dependencies[1].dependencyFlags = 0;

        VkRenderPassCreateInfo renderPassCI = LeoVK::Init::RenderPassCreateInfo();
        renderPassCI.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassCI.pAttachments = attachments.data();
        renderPassCI.subpassCount = 1;
        renderPassCI.pSubpasses = &subpassDesc;
        renderPassCI.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassCI.pDependencies = dependencies.data();

        VK_CHECK(vkCreateRenderPass(mDevice, &renderPassCI, nullptr, &mRenderPass));
    }
}

void VKRendererBase::GetEnabledFeatures()
{

}

void VKRendererBase::GetEnabledExtensions()
{

}

void VKRendererBase::Prepare()
{
    if (mpVulkanDevice->mbEnableDebugMarkers) LeoVK::DebugMarker::Setup(mDevice);

    initSwapChain();
    createCommandPool();
    setupSwapChain();
    createCommandBuffers();
    createSynchronizationPrimitives();
    SetupDepthStencil();
    SetupRenderPass();
    createPipelineCache();
    SetupFrameBuffer();

    mSettings.overlay = mSettings.overlay && (!mBenchmark.mbActive);
    if (mSettings.overlay)
    {
        mUIOverlay.mpDevice = mpVulkanDevice;
        mUIOverlay.mQueue = mQueue;
        mUIOverlay.mShaders = {
            LoadShader(GetShadersPath() + "Base/UIOverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            LoadShader(GetShadersPath() + "Base/UIOverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT),
        };
        mUIOverlay.PrepareResources();
        mUIOverlay.PreparePipeline(mPipelineCache, mRenderPass, mSwapChain.mFormat, mDepthFormat);
    }
}

VkPipelineShaderStageCreateInfo VKRendererBase::LoadShader(std::string filename, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStage {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
    shaderStage.module = LeoVK::VKTools::LoadShader(filename.c_str(), mDevice);
    shaderStage.pName = "main";

    assert(shaderStage.module != VK_NULL_HANDLE);
    mShaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void VKRendererBase::RenderLoop()
{
    if (mBenchmark.mbActive)
    {
        mBenchmark.Run([=] { Render(); }, mpVulkanDevice->mProperties);
        vkDeviceWaitIdle(mDevice);
        if (!mBenchmark.mFilename.empty()) mBenchmark.SaveResults();
        return;
    }

    mDstWidth = mWidth;
    mDstHeight = mHeight;
    mLastTimestamp = std::chrono::high_resolution_clock::now();
    mTPrevEnd = mLastTimestamp;

    MSG msg;
    bool quitMessageReceived = false;
    while (!quitMessageReceived)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                quitMessageReceived = true;
                break;
            }
        }
        if (mbPrepared && !IsIconic(mHwnd))
        {
            nextFrame();
        }
    }
    // Flush device to make sure all resources can be freed
    if (mDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(mDevice);
    }
}

void VKRendererBase::DrawUI(VkCommandBuffer commandBuffer)
{
    if (mSettings.overlay && mUIOverlay.mbVisible)
    {
        const VkViewport viewport = LeoVK::Init::Viewport((float)mWidth, (float)mHeight, 0.0f, 1.0f);
        const VkRect2D scissor = LeoVK::Init::Rect2D((int)mWidth, (int)mHeight, 0, 0);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        mUIOverlay.Draw(commandBuffer);
    }
}

void VKRendererBase::PrepareFrame()
{
    // Acquire the next image from the swap chain
    VkResult result = mSwapChain.AcquireNextImage(mSemaphores.presentComplete, &mCurrentBuffer);

    // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE)
    // SRS - If no longer optimal (VK_SUBOPTIMAL_KHR), wait until submitFrame() in case number of swapchain images will change on resize
    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            windowResizing();
        }
        return;
    }
    else
    {
        VK_CHECK(result);
    }
}

void VKRendererBase::SubmitFrame()
{
    VkResult result = mSwapChain.QueuePresent(mQueue, mCurrentBuffer, mSemaphores.renderComplete);
    // Recreate the swapchain if it's no longer compatible with the surface (OUT_OF_DATE) or no longer optimal for presentation (SUBOPTIMAL)

    if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR))
    {
        windowResizing();
        if (result == VK_ERROR_OUT_OF_DATE_KHR) return;
    }
    else
    {
        VK_CHECK(result);
    }
    VK_CHECK(vkQueueWaitIdle(mQueue));
}

void VKRendererBase::RenderFrame()
{
    VKRendererBase::PrepareFrame();
    mSubmitInfo.commandBufferCount = 1;
    mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[mCurrentBuffer];
    VK_CHECK(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));
    VKRendererBase::SubmitFrame();
}

void VKRendererBase::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{

}

void VKRendererBase::FileDropped(std::string &filename)
{

}

void VKRendererBase::OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

}

// Protected
std::string VKRendererBase::GetShadersPath() const
{
    return GetAssetsPath() + "Shaders/" + mShaderDir + "/";
}

// Private
std::string VKRendererBase::getWindowTitle()
{
    std::string device(mDeviceProps.deviceName);
    std::string windowTitle;
    windowTitle = mTitle + " - " + device;
    if (!mSettings.overlay)
    {
        windowTitle += " - " + std::to_string(mFrameCounter) + " fps";
    }
    return windowTitle;
}

void VKRendererBase::windowResizing()
{
    if (!mbPrepared) return;
    mbPrepared = false;

    vkDeviceWaitIdle(mDevice);
    mWidth = mDstWidth;
    mHeight = mDstHeight;
    setupSwapChain();

    if (mSettings.multiSampling)
    {
        vkDestroyImageView(mDevice, mMSTarget.color.imageView, nullptr);
        vkDestroyImage(mDevice, mMSTarget.color.image, nullptr);
        vkFreeMemory(mDevice, mMSTarget.color.memory, nullptr);
        vkDestroyImageView(mDevice, mMSTarget.depth.imageView, nullptr);
        vkDestroyImage(mDevice, mMSTarget.depth.image, nullptr);
        vkFreeMemory(mDevice, mMSTarget.depth.memory, nullptr);
    }
    vkDestroyImageView(mDevice, mDepthStencil.imageView, nullptr);
    vkDestroyImage(mDevice, mDepthStencil.image, nullptr);
    vkFreeMemory(mDevice, mDepthStencil.memory, nullptr);
    SetupDepthStencil();
    for (auto & mFrameBuffer : mFrameBuffers)
    {
        vkDestroyFramebuffer(mDevice, mFrameBuffer, nullptr);
    }
    SetupFrameBuffer();

    if (mSettings.overlay) mUIOverlay.Resize(mWidth, mHeight);

    destroyCommandBuffers();
    createCommandBuffers();
    BuildCommandBuffers();

    for (auto & fence : mWaitFences) vkDestroyFence(mDevice, fence, nullptr);

    createSynchronizationPrimitives();

    vkDeviceWaitIdle(mDevice);

    mCamera.UpdateAspectRatio((float)mWidth / (float)mHeight);
    WindowResized();
    ViewChanged();

    mbPrepared = true;
}

void VKRendererBase::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mMousePos.x - x;
    int32_t dy = (int32_t)mMousePos.y - y;

    bool handled = false;

    if (mSettings.overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        handled = io.WantCaptureMouse && mUIOverlay.mbVisible;
    }
    MouseMoved((float)x, (float)y, handled);

    if (handled)
    {
        mMousePos = glm::vec2((float)x, (float)y);
        return;
    }

    if (mMouseButtons.left)
    {
        mCamera.Rotate(glm::vec3((float)dy * mCamera.mRotationSpeed, -(float)dx * mCamera.mRotationSpeed, 0.0f));
        mbViewUpdated = true;
    }
    if (mMouseButtons.right)
    {
        mCamera.Translate(glm::vec3(-0.0f, 0.0f, (float)dy * .005f));
        mbViewUpdated = true;
    }
    if (mMouseButtons.middle)
    {
        mCamera.Translate(glm::vec3(-(float)dx * 0.005f, -(float)dy * 0.005f, 0.0f));
        mbViewUpdated = true;
    }
    mMousePos = glm::vec2((float)x, (float)y);
}

void VKRendererBase::nextFrame()
{
    auto tStart = std::chrono::high_resolution_clock::now();
    if (mbViewUpdated)
    {
        mbViewUpdated = false;
        ViewChanged();
    }

    Render();
    mFrameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();

    mFrameTimer = (float)tDiff / 1000.0f;
    mCamera.Update(mFrameTimer);
    if (mCamera.Moving())
    {
        mbViewUpdated = true;
    }
    // Convert to clamped timer value
    if (!mbPaused)
    {
        mTimer += mTimerSpeed * mFrameTimer;
        if (mTimer > 1.0)
        {
            mTimer -= 1.0f;
        }
    }
    float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - mLastTimestamp).count());
    if (fpsTimer > 1000.0f)
    {
        mLastFPS = static_cast<uint32_t>((float)mFrameCounter * (1000.0f / fpsTimer));

        if (!mSettings.overlay)	{
            std::string windowTitle = getWindowTitle();
            SetWindowTextA(mHwnd, windowTitle.c_str());
        }
        mFrameCounter = 0;
        mLastTimestamp = tEnd;
    }
    mTPrevEnd = tEnd;

    // TODO: Cap UI overlay update rates
    updateOverlay();
}

void VKRendererBase::updateOverlay()
{
    if (!mSettings.overlay) return;

    ImGuiIO& io = ImGui::GetIO();

    io.DisplaySize = ImVec2((float)mWidth, (float)mHeight);
    io.DeltaTime = mFrameTimer;

    io.MousePos = ImVec2(mMousePos.x, mMousePos.y);
    io.MouseDown[0] = mMouseButtons.left && mUIOverlay.mbVisible;
    io.MouseDown[1] = mMouseButtons.right && mUIOverlay.mbVisible;
    io.MouseDown[2] = mMouseButtons.middle && mUIOverlay.mbVisible;

    ImGui::NewFrame();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10 * mUIOverlay.mScale, 10 * mUIOverlay.mScale));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Vulkan Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextUnformatted(mTitle.c_str());
    ImGui::TextUnformatted(mDeviceProps.deviceName);
    ImGui::Text("%.2f ms/frame (%.1d fps)", (1000.0f / (float)mLastFPS), mLastFPS);

    ImGui::PushItemWidth(110.0f * mUIOverlay.mScale);
    OnUpdateUIOverlay(&mUIOverlay);
    ImGui::PopItemWidth();

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();

    if (mUIOverlay.Update() || mUIOverlay.mbUpdated)
    {
        BuildCommandBuffers();
        mUIOverlay.mbUpdated = false;
    }
}

void VKRendererBase::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCI {};
    pipelineCacheCI.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK(vkCreatePipelineCache(mDevice, &pipelineCacheCI, nullptr, &mPipelineCache));
}

void VKRendererBase::createCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolCI = LeoVK::Init::CmdPoolCreateInfo();
    cmdPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolCI.queueFamilyIndex = mSwapChain.mQueueNodeIndex;
    cmdPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(mDevice, &cmdPoolCI, nullptr, &mCmdPool))
}

void VKRendererBase::createSynchronizationPrimitives()
{
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCI = LeoVK::Init::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    mWaitFences.resize(mDrawCmdBuffers.size());
    for (auto& fence : mWaitFences)
    {
        VK_CHECK(vkCreateFence(mDevice, &fenceCI, nullptr, &fence));
    }
}

void VKRendererBase::initSwapChain()
{
    mSwapChain.InitSurface(mHInstance, mHwnd);
}

void VKRendererBase::setupSwapChain()
{
    mSwapChain.Create(&mWidth, &mHeight, mSettings.vsync, mSettings.fullscreen);
}

void VKRendererBase::createCommandBuffers()
{
    mDrawCmdBuffers.resize(mSwapChain.mImageCount);

    VkCommandBufferAllocateInfo cmdBufferAI = LeoVK::Init::CmdBufferAllocateInfo(
        mCmdPool,
        VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        static_cast<uint32_t>(mDrawCmdBuffers.size())
        );
    VK_CHECK(vkAllocateCommandBuffers(mDevice, &cmdBufferAI, mDrawCmdBuffers.data()))
}

void VKRendererBase::destroyCommandBuffers()
{
    vkFreeCommandBuffers(mDevice, mCmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}

void VKRendererBase::setupRenderTarget(VkImage *image, VkImageView *imageView, VkDeviceMemory *memory, bool isDepth)
{
    VkImageCreateInfo imageCI = LeoVK::Init::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = isDepth ? mDepthFormat : mSwapChain.mFormat;
    imageCI.extent.width = mWidth;
    imageCI.extent.height = mHeight;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.samples = mSettings.sampleCount;
    imageCI.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                    (isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(vkCreateImage(
        mDevice, &imageCI, nullptr,
        isDepth ? &mMSTarget.depth.image : &mMSTarget.color.image))

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(mDevice, isDepth ? mMSTarget.depth.image : mMSTarget.color.image, &memReqs);

    VkBool32 lazyMemTypePresent;
    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memReqs.size;
    memoryAI.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, &lazyMemTypePresent);

    if (!lazyMemTypePresent)
    {
        memoryAI.memoryTypeIndex = mpVulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    if (isDepth)
    {
        VK_CHECK(vkAllocateMemory(mDevice, &memoryAI, nullptr, &mMSTarget.depth.memory));
        vkBindImageMemory(mDevice, mMSTarget.depth.image, mMSTarget.depth.memory, 0);
    }
    else
    {
        VK_CHECK(vkAllocateMemory(mDevice, &memoryAI, nullptr, &mMSTarget.color.memory));
        vkBindImageMemory(mDevice, mMSTarget.color.image, mMSTarget.color.memory, 0);
    }

    // Create image view for the MSAA target
    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = isDepth ? mMSTarget.depth.image : mMSTarget.color.image;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = isDepth ? mDepthFormat : mSwapChain.mFormat;
    imageViewCI.components.r = VK_COMPONENT_SWIZZLE_R;
    imageViewCI.components.g = VK_COMPONENT_SWIZZLE_G;
    imageViewCI.components.b = VK_COMPONENT_SWIZZLE_B;
    imageViewCI.components.a = VK_COMPONENT_SWIZZLE_A;
    imageViewCI.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(
        mDevice, &imageViewCI, nullptr,
        isDepth ? &mMSTarget.depth.imageView : &mMSTarget.color.imageView));
}
