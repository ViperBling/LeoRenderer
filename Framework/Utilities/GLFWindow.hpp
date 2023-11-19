#pragma once

#include <vector>
#include <string>
#include <functional>

#include "VectorMath.hpp"
#include "KeyCodes.hpp"

struct GLFWwindow;

namespace LeoVK
{
    inline void DefaultWindowCallback(const std::string&) { }

    class VulkanContext;
    struct WindowSurface;

    struct WindowCreateOptions
    {
        bool TransparentFramebuffer = false;
        bool Resizeable = true;
        bool TileBar = true;
        std::function<void(const std::string&)> ErrorCallback = DefaultWindowCallback;
        Vector2 Size{ 800.0f, 600.0f };
        Vector2 Position{ 0.0f, 0.0f };
        const char* Title = "LeoRenderer";
    };

    class Window
    {
    public:
        GLFWwindow* GetNativeHandle() const { return this->mHwnd; }
        std::vector<const char*> GetRequiredExtensions() const;

        Window(const WindowCreateOptions& options);
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&& other) noexcept;
        Window& operator=(Window&& other) noexcept;
        ~Window();

        void PollEvents() const;
        bool ShouldClose() const;
        void Close();

        void SetSize(Vector2 size);
        Vector2 GetSize() const;
        void SetPosition(Vector2 position);
        Vector2 GetPosition() const;
        void SetTitle(const char* title);

        Vector2 GetCursorPosition() const;
        void SetCursorPosition(Vector2 position);

        CursorMode GetCursorMode() const;
        void SetCursorMode(CursorMode mode);

        bool IsKeyPressed(KeyCode key) const;
        bool IsKeyReleased(KeyCode key) const;

        bool IsMousePressed(MouseButton button) const;
        bool IsMouseReleased(MouseButton button) const;

        float GetTimeSinceCreation() const;
        void SetTimeSinceCreation(float time);

        void OnResize(std::function<void(Window&, Vector2)> callback);
        void OnKeyChanged(std::function<void(Window&, KeyCode, bool)> callback);
        void OnMouseChanged(std::function<void(Window&, MouseButton, bool)> callback);

        const WindowSurface& CreateWindowSurface(const VulkanContext& context);

        void SetContext(GLFWwindow* window);

    private:
        GLFWwindow* mHwnd = nullptr;
        std::function<void(Window&, Vector2)> mOnResize;
        std::function<void(Window&, KeyCode, bool)> mOnKeyChanged;
        std::function<void(Window&, MouseButton, bool)> mOnMouseChanged;
    };
}