#include "Window.h"

#include "stdafx.h"
#include "Renderer.h"


Window::Window()
{
    // Constructor implementation (if needed)
}


Window::~Window()
{
    spdlog::info("Window is getting destroyed...");
    SDL_RemoveEventWatch(eventCallback, this);

    // Clean up renderer
    _renderer.reset();

    // Clean up the Vulkan context
    _ctx.reset();

    // Cleanup SDL resources
    SDL_DestroyWindow(_window);
    SDL_Quit();
}


bool Window::initialize(const std::string& title, const uint16_t width, const uint16_t height)
{
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        const char* error = SDL_GetError();
        spdlog::error("SDL_Init Error: {}", (error[0] ? error : "Unknown error"));
        return false;
    }

    // Create the window
    _window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY );
    if (!_window) {
        const char* error = SDL_GetError();
        spdlog::error("SDL_CreateWindow Error: {}", (error[0] ? error : "Unknown error"));
        return false;
    }

    // Create Vulkan context
    spdlog::info("Creating Vulkan context...");
    _ctx = std::make_shared<VulkanContext>(_window);

    // Create the Vulkan renderer
    _renderer = std::make_unique<Renderer>(_ctx);

    // Always on top
    //SDL_SetWindowAlwaysOnTop(_window, true);

    // Event Handling
    SDL_AddEventWatch(eventCallback, this); // Add the event callback

    return true;
}


void Window::startRenderingLoop()
{
    //SDL_GL_SetSwapInterval(0);

    // Window event loop
    SDL_Event event;
    bool isRunning = true;
    bool isPaused = false;

    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    isRunning = false;
                    break;
                case SDL_EVENT_WINDOW_MINIMIZED:
                    isPaused = true; // Pause rendering when the window is minimized
                    break;
                case SDL_EVENT_WINDOW_RESTORED:
                    isPaused = false; // Resume rendering when the window is restored
                    break;
            }
        }

        if(!isPaused) _renderer->render(); // Call the renderer's render method
    }
}


// Static function to handle SDL events
bool Window::eventCallback(void *userdata, SDL_Event *event)
{
    Window* self = static_cast<Window*>(userdata);

    // Window resize is handled at the window level
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        // swapchain recreation is triggered by Vulkan itself on the next frame
        return false;
    }

    // All input events are forwarded to the renderer, which decides whether
    // ImGui or the scene should handle them
    if (!self->_renderer) return false;
    self->_renderer->handleEvent(event);

    return true;
}
