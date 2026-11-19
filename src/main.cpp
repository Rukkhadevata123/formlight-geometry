#include "vulkan_window.hpp"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <print>

auto main(int, char **) -> int
{
    try
    {
        run_vulkan_window();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &error)
    {
        std::println(stderr, "occt-lab: {}", error.what());
        return EXIT_FAILURE;
    }
}
