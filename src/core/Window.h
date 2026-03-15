#pragma once

#include "stdafx.h"
#include "vulkan/VulkanContext.h"
#include "core/FramePresenter.h"

class Window
{

protected:
    SDL_Window* _window = nullptr;
    std::shared_ptr<VulkanContext> _ctx = nullptr;
    std::unique_ptr<FramePresenter> _fp = nullptr;

    static bool eventCallback(void *userdata, SDL_Event *event);

public:
    Window();
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool initialize(const std::string& title, const uint16_t width, const uint16_t height);
    void startRenderingLoop();
};
