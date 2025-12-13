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
    // Clean up the renderer
    if (_renderer) {
        _renderer = nullptr;
    }

    // Clean up the Vulkan context
    _ctx = nullptr;

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
    _renderer->initialize();

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

        if(!isPaused) _renderer->drawFrame(); // Call the renderer's drawFrame method

        SDL_Delay(16); // Simulate a frame delay (60 FPS)
    }
}

// Static function to handle SDL events
bool Window::eventCallback(void *userdata, SDL_Event *event)
{
    Window* self = static_cast<Window*>(userdata);

    // Pass the event to the GUI for processing
    // if(self->_renderer->getGUI()->handleEvents(event)) {
    //     if (self->_renderer->getGUI()->isCapturingEvent()) {
    //         return false; // Event is captured by GUI
    //     }
    // }   
        
    // Access members of the window class
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
        self->onWindowResized(event->window.data1, event->window.data2);
        return false;
    }

    // Mouse motion event
    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        self->onMouseMotion(event->motion.x, event->motion.y);
        return false;
    }

    // Mouse down event
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        self->onMouseButtonDown(event->button.button, event->button.x, event->button.y);
        return false;
    }

    // Mouse up event
    if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
        self->onMouseButtonUp(event->button.button, event->button.x, event->button.y);
        return false;
    }

    // Mouse wheel event
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        self->onMouseWheel(event->wheel.x, event->wheel.y);
        return false;
    }

    // Keyboard down event
    if (event->type == SDL_EVENT_KEY_DOWN) {
        self->onKeyDown(event->key.key, event->key.scancode, event->key.mod);
    }
    
    // Return true to process the event, false to drop it
    return true;
}

void Window::onWindowResized(int width, int height)
{
    // Handle window resize event
    //_renderer->informFramebufferResized();
}

void Window::onMouseMotion(float x, float y)
{
    // Handle mouse motion event
    if (_isMouseDown) {
        int deltaX = x - _lastMouseX;
        int deltaY = y - _lastMouseY;

        _renderer->handleMouseDrag(deltaX, deltaY);
    }

    _lastMouseX = x;
    _lastMouseY = y;
}

void Window::onMouseButtonDown(int button, float x, float y)
{
    // Handle mouse button down event
    if (button == SDL_BUTTON_LEFT) {
        _isMouseDown = true;

        _renderer->handleMouseClick(x, y); // Call the renderer's mouse click handler
    }
}

void Window::onMouseButtonUp(int button, float x, float y)
{
    // Handle mouse button up event
    if (button == SDL_BUTTON_LEFT) {
        _isMouseDown = false;
    }
}

void Window::onMouseWheel(float x, float y)
{
    // Handle mouse wheel event
    _renderer->handleMouseWheel(y);
}

void Window::onKeyDown(int key, int scancode, int modifiers)
{
    // Handle key down event
    _renderer->handleKeyDown(key, scancode, modifiers);
}