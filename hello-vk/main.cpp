#include "stdafx.h"

#include "defer.h"

bool global_should_stop = false;


static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, 
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void* user_data);

///
/// @brief Searches for indices of queue which supports `present` and `graphics` from queue families of physical device.
///
/// @note For full support both of indices should has non-null values.
///
std::pair<std::optional<uint32_t>, std::optional<uint32_t>> vk_find_queue_family_indices(VkPhysicalDevice device, VkSurfaceKHR surface);

VkPhysicalDevice vk_pick_physical_device(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& required_device_extensions);

VkDevice vk_make_logical_device(
    VkPhysicalDevice physical_device, 
    const std::vector<const char *> &required_validation_layers, const std::vector<const char *> &required_device_extensions,
    uint32_t graphics_queue_index, uint32_t present_queue_index, bool enable_validation_layers);

VkExtent2D vk_pick_swap_extent(VkSurfaceCapabilitiesKHR *capabilities, int width, int height);

VkResult vk_make_swapchain(
    VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format,
    uint32_t graphics_queue_index, uint32_t present_queue_index, VkPresentModeKHR present_mode,
    VkSurfaceCapabilitiesKHR* capabilities, VkExtent2D extent, VkSwapchainKHR* result);

int
main(void) {
    using namespace std::literals;

    //
    // SDL initialization:
    //
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init() = %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    defer(SDL_Quit());

    SDL_version v = {};
    SDL_GetVersion(&v);
    SDL_LogInfo(
        SDL_LOG_CATEGORY_APPLICATION, "SDL version %d.%d.%d\n", v.major,
        v.minor, v.patch);


    SDL_WindowFlags window_flags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("Hello VK", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (window == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow() = %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }
    defer(SDL_DestroyWindow(window));

    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);

    //
    // Vulkan Initialization:
    //
    VkResult vk_result = VK_SUCCESS;
    VkInstance vk_instance = {};

    VkApplicationInfo vk_app_info = {};
    vk_app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vk_app_info.pApplicationName = "Hello VK";
    vk_app_info.applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 0);
    vk_app_info.pEngineName = "Hello VK";
    vk_app_info.engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 0);
    vk_app_info.apiVersion = VK_API_VERSION_1_0;

    //
    // VK Extensions:
    //
    unsigned int vk_extensions_count = 0;
    SDL_assert(SDL_Vulkan_GetInstanceExtensions(window, &vk_extensions_count, nullptr));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: extensions supported = %d", vk_extensions_count);
    
    std::vector<const char*> vk_extensions(vk_extensions_count);
    SDL_assert(SDL_Vulkan_GetInstanceExtensions(window, &vk_extensions_count, vk_extensions.data()));

    //
    // VK Extension Properties:
    //
    uint32_t vk_extensions_properties_count = 0;
    SDL_assert(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensions_properties_count, nullptr) == VK_SUCCESS);

    std::vector<VkExtensionProperties> vk_extensions_properties(vk_extensions_properties_count);
    SDL_assert(vkEnumerateInstanceExtensionProperties(nullptr, &vk_extensions_properties_count, vk_extensions_properties.data()) == VK_SUCCESS);

    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: extensions properties:");
    for (auto &extension_properties : vk_extensions_properties) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "    %s", extension_properties.extensionName);
    }

    //
    // VK Validation Layers:
    //
    const std::vector<const char *> vk_required_validation_layers = {
        "VK_LAYER_KHRONOS_validation",
    };
    constexpr static bool s_vk_enable_validation_layers = _DEBUG ? true : false;

    uint32_t vk_available_layers_count = 0;
    SDL_assert(vkEnumerateInstanceLayerProperties(&vk_available_layers_count, nullptr) == VK_SUCCESS);

    std::vector<VkLayerProperties> vk_available_layers(vk_available_layers_count);
    SDL_assert(vkEnumerateInstanceLayerProperties(&vk_available_layers_count, vk_available_layers.data()) == VK_SUCCESS);

    if (s_vk_enable_validation_layers) {
        for (auto &required_layer : vk_required_validation_layers) {
            bool found = false;
            
            std::string_view required_layer_view(required_layer);
            for (auto &available_layer : vk_available_layers) {
                if (required_layer_view == available_layer.layerName) {
                    found = true;
                    break;
                }
            }

            SDL_assert(found);
        }
    }

    //
    // VK Instance creation:
    //
    VkInstanceCreateInfo vk_create_info = {};
    vk_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vk_create_info.pApplicationInfo = &vk_app_info;

    VkDebugUtilsMessengerCreateInfoEXT vk_debug_create_info = {};
    vk_debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vk_debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vk_debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vk_debug_create_info.pfnUserCallback = vk_debug_callback;
    vk_debug_create_info.pUserData = nullptr;

    if constexpr (s_vk_enable_validation_layers) {
        vk_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk_create_info.enabledExtensionCount = static_cast<uint32_t>(vk_extensions.size());
    vk_create_info.ppEnabledExtensionNames = vk_extensions.data();

    if constexpr (s_vk_enable_validation_layers) {
        vk_create_info.enabledLayerCount = static_cast<uint32_t>(vk_required_validation_layers.size());
        vk_create_info.ppEnabledLayerNames = vk_required_validation_layers.data();

        vk_create_info.pNext = &vk_debug_create_info;
    } else {
        vk_create_info.enabledLayerCount = 0;

        vk_create_info.pNext = nullptr;
    }

    vk_result = vkCreateInstance(&vk_create_info, nullptr, &vk_instance);
    if (vk_result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkCreateInstance() = %s", string_VkResult(vk_result));
        
        SDL_TriggerBreakpoint();
        return EXIT_FAILURE;
    }
    defer(vkDestroyInstance(vk_instance, nullptr));

    //
    // VK Debug Messenger:
    //
    VkDebugUtilsMessengerEXT vk_debug_messeger = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext = nullptr;
    defer(if constexpr (s_vk_enable_validation_layers) { 
        vk_destroy_debug_utils_messenger_ext(vk_instance, vk_debug_messeger, nullptr); });

    if constexpr (s_vk_enable_validation_layers) {
        auto vk_create_debug_utils_messenger_ext = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT"));
        SDL_assert(vk_create_debug_utils_messenger_ext);

        if (vk_create_debug_utils_messenger_ext) {
            SDL_assert(vk_create_debug_utils_messenger_ext(vk_instance, &vk_debug_create_info, nullptr, &vk_debug_messeger) == VK_SUCCESS);
        }

        vk_destroy_debug_utils_messenger_ext = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugUtilsMessengerEXT"));
        SDL_assert(vk_destroy_debug_utils_messenger_ext);
    }

    //
    // VK: surface creation.
    //
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    SDL_assert(SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface));
    defer(vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr));

    //
    // VK: physical device.
    //
    std::vector<const char *> vk_required_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDevice vk_physical_device = vk_pick_physical_device(vk_instance, vk_surface, vk_required_device_extensions);  
    SDL_assert(vk_physical_device);
    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    // TODO(gr3yknigh1): Need cleanup? [2025/03/20]

    //
    // VK: picking queue family.
    // TODO(gr3yknigh1): Move to Physical Device Picking! [2025/03/20]
    //
    auto [vk_graphics_queue_index, vk_present_queue_index] = 
        vk_find_queue_family_indices(vk_physical_device, vk_surface);

    SDL_assert(vk_graphics_queue_index.has_value());
    SDL_assert(vk_present_queue_index.has_value());

    //
    // VK: logical device.
    //
    VkDevice vk_device = vk_make_logical_device(
        vk_physical_device, vk_required_validation_layers, vk_required_device_extensions, vk_graphics_queue_index.value(),
        vk_present_queue_index.value(), s_vk_enable_validation_layers);
    defer(vkDestroyDevice(vk_device, nullptr));

    VkQueue vk_graphics_queue = VK_NULL_HANDLE, vk_present_queue = VK_NULL_HANDLE;

    vkGetDeviceQueue(vk_device, vk_graphics_queue_index.value(), 0, &vk_graphics_queue);
    SDL_assert(vk_graphics_queue);

    vkGetDeviceQueue(vk_device, vk_present_queue_index.value(), 0, &vk_present_queue);
    SDL_assert(vk_present_queue);

    //
    // VK: searching for surface format.
    // NOTE(gr3yknigh1): Duplication on physical device picking [2025/03/20]
    //
    uint32_t vk_surface_formats_count = 0;
    SDL_assert(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &vk_surface_formats_count, nullptr) == VK_SUCCESS);

    std::vector<VkSurfaceFormatKHR> vk_surface_formats(vk_surface_formats_count);
    SDL_assert(vkGetPhysicalDeviceSurfaceFormatsKHR(
        vk_physical_device, vk_surface, &vk_surface_formats_count, vk_surface_formats.data()) == VK_SUCCESS);

    std::optional<VkSurfaceFormatKHR> vk_surface_format = std::nullopt;
    for (auto &format : vk_surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            vk_surface_format = format;
        }
    }
    SDL_assert(vk_surface_format.has_value());

    //
    // VK: searching for present mode.
    //

    uint32_t vk_present_modes_count = 0;
    SDL_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk_physical_device, vk_surface, &vk_present_modes_count, nullptr) == VK_SUCCESS);

    std::vector<VkPresentModeKHR> vk_present_modes(vk_present_modes_count);
    SDL_assert(vkGetPhysicalDeviceSurfacePresentModesKHR(
        vk_physical_device, vk_surface, &vk_present_modes_count, vk_present_modes.data()) == VK_SUCCESS);

    using namespace std;

    optional<VkPresentModeKHR> vk_present_mode = nullopt;
    for (auto &present_mode : vk_present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            vk_present_mode = present_mode;
        }
    }
    if (!vk_present_mode) {
        vk_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    //
    // VK: swap extent.
    //
    VkSurfaceCapabilitiesKHR vk_capabilities = {};
    SDL_assert(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_physical_device, vk_surface, &vk_capabilities) == VK_SUCCESS);

    VkExtent2D vk_extent_2d = vk_pick_swap_extent(&vk_capabilities, window_width, window_height);

    VkSwapchainKHR vk_swapchain = VK_NULL_HANDLE;
    SDL_assert(vk_make_swapchain(
        vk_device, vk_surface, vk_surface_format.value(), 
        vk_graphics_queue_index.value(), vk_present_queue_index.value(),
        vk_present_mode.value(), &vk_capabilities, vk_extent_2d, &vk_swapchain) == VK_SUCCESS);
    defer(vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr));

    uint32_t vk_swapchain_images_count = 0;
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_images_count, nullptr);
    std::vector<VkImage> vk_swapchain_images(vk_swapchain_images_count);
    vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &vk_swapchain_images_count, vk_swapchain_images.data());

    std::vector<VkImageView> vk_swapchain_image_views(vk_swapchain_images.size());
    for (size_t index = 0; index < vk_swapchain_images.size(); index++) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = vk_swapchain_images[index];

        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = vk_surface_format.value().format;

        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        SDL_assert(vkCreateImageView(vk_device, &create_info, nullptr, &vk_swapchain_image_views[index]) == VK_SUCCESS);
    }
    defer(for (auto& view : vk_swapchain_image_views) vkDestroyImageView(vk_device, view, nullptr));



    //
    // Main loop:
    //
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

    return EXIT_SUCCESS;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    if (message_severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: %s", callback_data->pMessage);
    }
    else {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: %s", callback_data->pMessage);
    }
    return VK_FALSE;
}

std::pair<std::optional<uint32_t>, std::optional<uint32_t>> 
vk_find_queue_family_indices(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    std::optional<uint32_t> graphics_queue_index = std::nullopt;
    std::optional<uint32_t> present_queue_index = std::nullopt;

    for (uint32_t index = 0; index < queue_families.size(); ++index) {
        auto queue_family = queue_families[index];

        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_index = index;
            continue; // TODO(gr3yknigh1): Gain more information about Queue picking. Made this because validators complained about same queue index from same family [2025/03/20]
        }
        
        VkBool32 is_surface_supported = false;
        SDL_assert(vkGetPhysicalDeviceSurfaceSupportKHR(
            device, index, surface, &is_surface_supported) == VK_SUCCESS);
        SDL_assert(is_surface_supported);

        if (is_surface_supported) {
            present_queue_index = index;
        }

        if (graphics_queue_index.has_value() && present_queue_index.has_value()) {
            break;
        }
    }

    return { graphics_queue_index, present_queue_index };
}

VkPhysicalDevice 
vk_pick_physical_device(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char *> &required_device_extensions)
{
    using namespace std::literals;

    std::vector<std::string_view> required_device_extensions_views(required_device_extensions.begin(), required_device_extensions.end());

    VkPhysicalDevice result = VK_NULL_HANDLE;

    uint32_t devices_count = 0;
    SDL_assert(vkEnumeratePhysicalDevices(instance, &devices_count, nullptr) == VK_SUCCESS);
    SDL_assert(devices_count);

    std::vector<VkPhysicalDevice> devices(devices_count);
    SDL_assert(vkEnumeratePhysicalDevices(instance, &devices_count, devices.data()) == VK_SUCCESS);

    for (auto &device : devices) {
        [[maybe_unused]] VkPhysicalDeviceProperties device_properties = {};
        vkGetPhysicalDeviceProperties(device, &device_properties);

        [[maybe_unused]] VkPhysicalDeviceFeatures device_features = {};
        vkGetPhysicalDeviceFeatures(device, &device_features);

        auto [graphics_index, present_index] = vk_find_queue_family_indices(device, surface);

        if (!graphics_index.has_value() || !present_index.has_value()) {
            continue;
        }

        uint32_t extension_count = 0;
        SDL_assert(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr) == VK_SUCCESS);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        SDL_assert(vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data()) == VK_SUCCESS);

        uint32_t extension_matches_count = 0;
        for (const auto &extension : available_extensions) {
            for (const auto &required_extension : required_device_extensions_views) {
                if (required_extension == extension.extensionName) {
                    extension_matches_count++;
                    break;
                }
            }
        }

        bool all_required_extensions_supported = extension_matches_count == required_device_extensions_views.size();

        if (all_required_extensions_supported) {
            uint32_t surface_formats_count = 0;
            SDL_assert(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &surface_formats_count, nullptr) == VK_SUCCESS);

            uint32_t present_mode_count = 0;
            SDL_assert(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &present_mode_count, nullptr) == VK_SUCCESS);

            if (surface_formats_count > 0 && present_mode_count > 0) {
                result = device;
                break;
            }
        }

#if 0
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && device_features.geometryShader) {
            break;
        }
#endif
    }

    return result;
}

VkDevice 
vk_make_logical_device(
    VkPhysicalDevice physical_device, const std::vector<const char*>& required_validation_layers, const std::vector<const char*>& required_device_extensions,
    uint32_t graphics_queue_index, uint32_t present_queue_index, bool enable_validation_layers)
{
    VkDevice device = VK_NULL_HANDLE;
    float queue_priority = 1.0f;

    std::array<VkDeviceQueueCreateInfo, 2> queue_create_infos = {};

    queue_create_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_infos[0].queueFamilyIndex = graphics_queue_index;
    queue_create_infos[0].queueCount = 1;
    queue_create_infos[0].pQueuePriorities = &queue_priority;

    queue_create_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_infos[1].queueFamilyIndex = present_queue_index; 
    queue_create_infos[1].queueCount = 1;
    queue_create_infos[1].pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {}; // TODO(gr3yknigh1): Go back later... [2025/03/20]
    
    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos    = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    
    device_create_info.pEnabledFeatures     = &device_features;
    
    device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());

    if (enable_validation_layers) {
        device_create_info.enabledLayerCount = static_cast<uint32_t>(required_validation_layers.size());
        device_create_info.ppEnabledLayerNames = required_validation_layers.data();
    } else {
        device_create_info.enabledLayerCount = 0;
    }

    VkResult result = vkCreateDevice(physical_device, &device_create_info, nullptr, &device);
    if (result != VK_SUCCESS) {
        // TODO(gr3yknigh1): Find another way to report error from here [2025/03/20]
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkCreateDevice() = %s", string_VkResult(result));
        SDL_TriggerBreakpoint();
        return VK_NULL_HANDLE;
    }

    return device;
}


VkExtent2D
vk_pick_swap_extent(VkSurfaceCapabilitiesKHR *capabilities, int width, int height)
{
    if (capabilities->currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities->currentExtent;
    }

    VkExtent2D result = {};
    result.width = std::clamp(static_cast<uint32_t>(width), capabilities->minImageExtent.width, capabilities->maxImageExtent.width);
    result.height = std::clamp(static_cast<uint32_t>(height), capabilities->minImageExtent.height, capabilities->maxImageExtent.height);

    return result;
}

VkResult 
vk_make_swapchain(
    VkDevice device, VkSurfaceKHR surface, VkSurfaceFormatKHR surface_format, 
    uint32_t graphics_queue_index, uint32_t present_queue_index, VkPresentModeKHR present_mode,
    VkSurfaceCapabilitiesKHR *capabilities, VkExtent2D extent, VkSwapchainKHR *result)
{

    uint32_t image_count = capabilities->minImageCount + 1;
    if (capabilities->maxImageCount > 0 && image_count > capabilities->maxImageCount) {
        image_count = capabilities->maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // TODO(gr3yknigh1): Remove duplication in creation of logical device [2025/03/21]
    std::array<uint32_t, 2> queue_family_indices = { graphics_queue_index, present_queue_index };

    if (graphics_queue_index != present_queue_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = queue_family_indices.size();
        create_info.pQueueFamilyIndices = queue_family_indices.data();
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0; // Optional
        create_info.pQueueFamilyIndices = nullptr; // Optional
    }

    create_info.preTransform = capabilities->currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    
    return vkCreateSwapchainKHR(device, &create_info, nullptr, result);
}
