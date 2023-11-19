#include "GLFWindow.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace LeoVK
{
    const WindowSurface& CreateVulkanSurface(GLFWwindow* window, const VulkanContext& context);

    std::vector<const char *> Window::GetRequiredExtensions() const
    {
        uint32_t extensionCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        return std::vector<const char*>(extensions, extensions + extensionCount);
    }

    Window::Window(const WindowCreateOptions &options)
    {
        if (glfwInit() != GLFW_TRUE)
        {
            options.ErrorCallback("Failed to initialize GLFW");
            return;
        }
        if (glfwVulkanSupported() != GLFW_TRUE)
        {
            options.ErrorCallback("Vulkan is not supported in glfw context");
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, options.TileBar);
        glfwWindowHint(GLFW_RESIZABLE, options.Resizeable);
        glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, options.TransparentFramebuffer);

        this->mHwnd = glfwCreateWindow((int)options.Size.x, (int)options.Size.y, options.Title, nullptr, nullptr);
        if (this->mHwnd == nullptr)
        {
            options.ErrorCallback("Failed to create window");
            return;
        }

        glfwSetWindowPos(this->mHwnd, (int)options.Position.x, (int)options.Position.y);
        glfwSetWindowUserPointer(this->mHwnd, (void*)this);
        glfwSetWindowSizeCallback(this->mHwnd, [](GLFWwindow* handle, int width, int height)
        {
                auto& window = *(Window*)glfwGetWindowUserPointer(handle);
                if (window.mOnResize) window.mOnResize(window, Vector2((float)width, (float)height));
        });
        glfwSetKeyCallback(this->mHwnd, [](GLFWwindow* handle, int key, int scancode, int action, int mods)
        {
            auto& window = *(Window*)glfwGetWindowUserPointer(handle);
            if (window.mOnKeyChanged) window.mOnKeyChanged(window, (KeyCode)key, action == GLFW_PRESS);
        });
        glfwSetMouseButtonCallback(this->mHwnd, [](GLFWwindow* handle, int button, int action, int mods)
        {
            auto& window = *(Window*)glfwGetWindowUserPointer(handle);
            if (window.mOnMouseChanged) window.mOnMouseChanged(window, (MouseButton)button, action == GLFW_PRESS);
        });
    }

    Window::Window(Window &&other) noexcept
    {
        this->mHwnd = other.mHwnd;
        this->mOnResize = std::move(other.mOnResize);
        other.mHwnd = nullptr;
        glfwSetWindowUserPointer(this->mHwnd, (void*)this);
    }

    Window &Window::operator=(Window &&other) noexcept
    {
        this->mHwnd = other.mHwnd;
        this->mOnResize = std::move(other.mOnResize);
        other.mHwnd = nullptr;
        glfwSetWindowUserPointer(this->mHwnd, (void*)this);
    }

    Window::~Window()
    {
        if (this->mHwnd != nullptr)
        {
            glfwDestroyWindow(this->mHwnd);
            this->mHwnd = nullptr;
        }
    }

    void Window::PollEvents() const
    {
        glfwPollEvents();
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(this->mHwnd);
    }

    void Window::Close()
    {
        glfwSetWindowShouldClose(this->mHwnd, true);
    }

    void Window::SetSize(Vector2 size)
    {
        glfwSetWindowSize(this->mHwnd, (int)size.x, (int)size.y);
    }

    Vector2 Window::GetSize() const
    {
        int width = 0, height = 0;
        glfwGetWindowSize(this->mHwnd, &width, &height);
        return Vector2((float)width, (float)height);
    }

    void Window::SetPosition(Vector2 position)
    {
        glfwSetWindowPos(this->mHwnd, (int)position.x, (int)position.y);
    }

    Vector2 Window::GetPosition() const
    {
        int x = 0, y = 0;
        glfwGetWindowPos(this->mHwnd, &x, &y);
        return Vector2((float)x, (float)y);
    }

    void Window::SetTitle(const char *title)
    {
        glfwSetWindowTitle(this->mHwnd, title);
    }

    Vector2 Window::GetCursorPosition() const
    {
        double x = 0, y = 0;
        glfwGetCursorPos(this->mHwnd, &x, &y);
        return Vector2((float)x, (float)y);
    }

    void Window::SetCursorPosition(Vector2 position)
    {
        glfwSetCursorPos(this->mHwnd, (int)position.x, (int)position.y);
    }

    CursorMode Window::GetCursorMode() const
    {
        return (CursorMode)glfwGetInputMode(this->mHwnd, GLFW_CURSOR);
    }

    void Window::SetCursorMode(CursorMode mode)
    {
        glfwSetInputMode(this->mHwnd, GLFW_CURSOR, (int)mode);
    }

    bool Window::IsKeyPressed(KeyCode key) const
    {
        return glfwGetKey(this->mHwnd, (int)key) == GLFW_PRESS;
    }

    bool Window::IsKeyReleased(KeyCode key) const
    {
        return glfwGetKey(this->mHwnd, (int)key) == GLFW_RELEASE;
    }

    bool Window::IsMousePressed(MouseButton button) const
    {
        return glfwGetMouseButton(this->mHwnd, (int)button) == GLFW_PRESS;
    }

    bool Window::IsMouseReleased(MouseButton button) const
    {
        return glfwGetMouseButton(this->mHwnd, (int)button) == GLFW_RELEASE;
    }

    float Window::GetTimeSinceCreation() const
    {
        return (float)glfwGetTime();
    }

    void Window::SetTimeSinceCreation(float time)
    {
        return glfwSetTime((float)time);
    }

    void Window::OnResize(std::function<void(Window &, Vector2)> callback)
    {
        this->mOnResize = std::move(callback);
    }

    void Window::OnKeyChanged(std::function<void(Window &, KeyCode, bool)> callback)
    {
        this->mOnKeyChanged = std::move(callback);
    }

    void Window::OnMouseChanged(std::function<void(Window &, MouseButton, bool)> callback)
    {
        this->mOnMouseChanged = std::move(callback);
    }

    const WindowSurface &Window::CreateWindowSurface(const VulkanContext &context)
    {
        return CreateVulkanSurface(this->mHwnd, context);
    }

    void Window::SetContext(GLFWwindow *window)
    {
        this->mHwnd = window;
        glfwMakeContextCurrent(window);
    }
}