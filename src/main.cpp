#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>

import std;
import occt_lab.vulkan_window;

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
