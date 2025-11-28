#pragma once

#include "stdafx.h"
#include "VulkanContext.h"
#include "Renderer.h"

class Window
{

protected:
    SDL_Window* _window = nullptr;
    std::shared_ptr<VulkanContext> _ctx = nullptr;
    std::unique_ptr<Renderer> _renderer = nullptr;

    static bool eventCallback(void *userdata, SDL_Event *event);

    // Event handlers
    void onWindowResized(int width, int height);
    void onMouseMotion(float x, float y);
    void onMouseButtonDown(int button, float x, float y);
    void onMouseButtonUp(int button, float x, float y);
    void onMouseWheel(float x, float y);
    void onKeyDown(int key, int scancode, int modifiers);

private:
    bool _isMouseDown = false;
    int  _lastMouseX = 0;
    int  _lastMouseY = 0;

public:
    Window();
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool initialize(const std::string& title, const uint16_t width, const uint16_t height);
    void startRenderingLoop();
};