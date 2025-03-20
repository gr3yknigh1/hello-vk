#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <set>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#if defined(_DEBUG)
#pragma comment(lib, "SDL2d.lib")
#else
#pragma comment(lib, "SDL2.lib")
#endif

#pragma comment(lib, "vulkan-1.lib")
