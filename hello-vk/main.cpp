#include "stdafx.h"

#if defined(_DEBUG)
#pragma comment(lib, "SDL2d.lib")
#else
#pragma comment(lib, "SDL2.lib")
#endif

#pragma comment(lib, "vulkan-1.lib")

bool global_should_stop = false;

int
main(void) {

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "E: SDL_Init() = %s\n", SDL_GetError());
        return 0;
    }

    SDL_version v = {};
    SDL_GetVersion(&v);
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION, "SDL version %d.%d.%d\n", v.major,
        v.minor, v.patch);


    SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("Hello VK", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "E: SDL_CreateWindow() = %s\n", SDL_GetError());
        return 0;
    }
    

    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    while (!global_should_stop) {

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                global_should_stop = true;
            }

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                global_should_stop = true;
            }
        }


        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
