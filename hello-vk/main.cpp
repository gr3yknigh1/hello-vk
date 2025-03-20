#include "stdafx.h"

#include "defer.h"

bool global_should_stop = false;


static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, 
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data, 
    void* user_data);

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
    // VK Physical device:
    //
    uint32_t vk_physical_devices_count = 0;
    SDL_assert(vkEnumeratePhysicalDevices(vk_instance, &vk_physical_devices_count, nullptr) == VK_SUCCESS);
    SDL_assert(vk_physical_devices_count);

    std::vector<VkPhysicalDevice> vk_physical_devices(vk_physical_devices_count);
    SDL_assert(vkEnumeratePhysicalDevices(
        vk_instance, &vk_physical_devices_count, vk_physical_devices.data()) == VK_SUCCESS);

    //
    // VK Picking physical device:
    //
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    for (auto &device : vk_physical_devices) {
        VkPhysicalDeviceProperties device_properties = {};
        vkGetPhysicalDeviceProperties(device, &device_properties);

        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(device, &device_features);

        // NOTE(gr3yknigh1): I do not want to implement GPU picking right now [2025/03/20]
        vk_physical_device = device;
        break;
#if 0
        if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && device_features.geometryShader) {
            break;
        }
#endif
    }

    SDL_assert(vk_physical_device);

    //
    // Picking Queue family:
    // TODO(gr3yknigh1): Move to Physical Device Picking! [2025/03/20]
    //
    uint32_t vk_queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &vk_queue_family_count, nullptr);

    [[maybe_unused]] std::vector<VkQueueFamilyProperties> vk_queue_families(vk_queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(vk_physical_device, &vk_queue_family_count, vk_queue_families.data());

    int32_t vk_queue_family_index = -1;

    for (int32_t index = 0; index < vk_queue_families.size(); ++index) {
        auto queue_family = vk_queue_families[index];

        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vk_queue_family_index = index;
        }
    }
    
    SDL_assert(vk_queue_family_index != -1);

    VkDevice vk_device = VK_NULL_HANDLE;

    VkDeviceQueueCreateInfo vk_queue_create_info = {};
    vk_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    vk_queue_create_info.queueFamilyIndex = vk_queue_family_index;
    vk_queue_create_info.queueCount = 1;

    float vk_queue_priority = 1.0f;
    vk_queue_create_info.pQueuePriorities = &vk_queue_priority;

    VkPhysicalDeviceFeatures vk_device_features = {}; // TODO(gr3yknigh1): Go back later... [2025/03/20]
    
    VkDeviceCreateInfo vk_device_create_info = {};
    vk_device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vk_device_create_info.pQueueCreateInfos = &vk_queue_create_info;
    vk_device_create_info.queueCreateInfoCount = 1;

    vk_device_create_info.pEnabledFeatures = &vk_device_features;
    vk_device_create_info.enabledExtensionCount = 0;

    if (s_vk_enable_validation_layers) {
        vk_device_create_info.enabledLayerCount = static_cast<uint32_t>(vk_required_validation_layers.size());
        vk_device_create_info.ppEnabledLayerNames = vk_required_validation_layers.data();
    } else {
        vk_device_create_info.enabledLayerCount = 0;
    }

    vk_result = vkCreateDevice(vk_physical_device, &vk_device_create_info, nullptr, &vk_device);
    if (vk_result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "vkCreateDevice() = %s", string_VkResult(vk_result));
        SDL_TriggerBreakpoint();
        return EXIT_FAILURE;
    }
    defer(vkDestroyDevice(vk_device, nullptr));

    VkQueue vk_gfx_queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(vk_device, vk_queue_family_index, 0, &vk_gfx_queue);
    SDL_assert(vk_gfx_queue);

    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    SDL_assert(SDL_Vulkan_CreateSurface(window, vk_instance, &vk_surface));

    VkBool32 vk_is_surface_supported = false;
    SDL_assert(vkGetPhysicalDeviceSurfaceSupportKHR(
        vk_physical_device, vk_queue_family_index, vk_surface, &vk_is_surface_supported) == VK_SUCCESS);
    SDL_assert(vk_is_surface_supported);

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
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: Message: %s", callback_data->pMessage);
    }
    else {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Vulkan: Message(Debug): %s", callback_data->pMessage);
    }
    return VK_FALSE;
}