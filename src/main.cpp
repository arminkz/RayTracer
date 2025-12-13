#include "stdafx.h"
#include "Window.h"


int main(int argc, char* argv[]) {

    // Parse command line arguments for verbosity
    spdlog::level::level_enum log_level = spdlog::level::info; // default level
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-vvv") {
            log_level = spdlog::level::trace;
            spdlog::info("Verbosity level set to TRACE");
        } else if (arg == "-vv") {
            log_level = spdlog::level::debug;
            spdlog::info("Verbosity level set to DEBUG");
        }
    }
    spdlog::set_level(log_level);


    spdlog::info("Starting Vulkan RayTracer v0.1");
    //Create a window
    try{
        Window window;
        if (!window.initialize("Vulkan RayTracer v0.1 (by @arminkz)", 1920, 1080)) return EXIT_FAILURE;

        // Start the rendering loop
        window.startRenderingLoop();
        
    } catch (const std::exception& e) {
        spdlog::error("{}", e.what());
        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}