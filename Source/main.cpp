#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

//This macro is used so we can use {.name1 = ... , .name2 = ...} in constructors for vulkan classes
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_raii.hpp>

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <iostream>


#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif



class VulkanTutorialApp {
public:
    void run() {
        init();
        mainLoop();
        cleanup();
    }

private:

    SDL_Window* _window = nullptr;
    vk::raii::Context  _context;
    vk::raii::Instance _instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;

    void init() {
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        throw std::runtime_error("Failed to init SDL");

        if (!SDL_Vulkan_LoadLibrary(nullptr))
            throw std::runtime_error("Failed to load vulkan library");

        _window = SDL_CreateWindow("Vulkan Window", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
        if (_window == NULL) {
            SDL_Log("SDL_CreateWindow: %s", SDL_GetError());
            return;
        }

        initVulkanInstance();
        setupDebugMessenger();

        // JSON demo
        nlohmann::json config = {
            {"window", {
                {"width", 800},
                {"height", 600}
            }}
        };
        std::cout << "Config JSON: " << config.dump(2) << std::endl;

        // glm demo
        glm::vec3 a(1.0f, 0.0f, 0.0f);
        glm::vec3 b(0.0f, 1.0f, 0.0f);
        glm::vec3 c = glm::cross(a, b);
        std::cout << "Cross product: (" << c.x << ", " << c.y << ", " << c.z << ")\n";

    }

    void initVulkanInstance()
    {
        constexpr vk::ApplicationInfo appInfo{ .pApplicationName = "Vulkan Tutorial",
            .applicationVersion = VK_MAKE_VERSION( 1, 0, 0 ),
            .pEngineName        = "No Engine",
            .engineVersion      = VK_MAKE_VERSION( 1, 0, 0 ),
            .apiVersion         = vk::ApiVersion14 };

        auto requiredLayers = getRequiredLayers();
        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = _context.enumerateInstanceLayerProperties();
        for (auto const& requiredLayer : requiredLayers)
        {
            if (std::ranges::none_of(layerProperties,
                                     [requiredLayer](auto const& layerProperty)
                                     { return strcmp(layerProperty.layerName, requiredLayer) == 0; }))
            {
                throw std::runtime_error("Required layer not supported: " + std::string(requiredLayer));
            }
        }

        // Get the required extensions.
        auto requiredExtensions = getRequiredExtensions();
        // Check if the required extensions are supported by the Vulkan implementation.
        auto extensionProperties = _context.enumerateInstanceExtensionProperties();
        for (auto const & requiredExtension : requiredExtensions)
        {
            if (std::ranges::none_of(extensionProperties,
                                     [requiredExtension](auto const& extensionProperty)
                                     { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; }))
            {
                throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
            }
        }

         vk::InstanceCreateInfo createInfo{
        .pApplicationInfo        = &appInfo,
        .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
        .ppEnabledLayerNames     = requiredLayers.data(),
        .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
        .ppEnabledExtensionNames = requiredExtensions.data() };
        
        _instance = vk::raii::Instance(_context, createInfo);
    }

    void mainLoop() {
         // Main loop
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                }
            }
        }
    }

    void cleanup() {
        SDL_Log("SDL3 shutdown");
        SDL_DestroyWindow(_window);

        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }

    void pickPhysicalDevice() {

    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
        vk::DebugUtilsMessageTypeFlagsEXT    messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{
            .messageSeverity = severityFlags,
            .messageType = messageTypeFlags,
            .pfnUserCallback = &debugCallback
            };
        _debugMessenger = _instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    const std::vector<const char*> getRequiredExtensions()
    {
        uint32_t countInstanceExtensions;
        const char * const *instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&countInstanceExtensions);

        std::vector<const char*> extensions(instanceExtensions, instanceExtensions + countInstanceExtensions);
        if (enableValidationLayers) {
            extensions.push_back(vk::EXTDebugUtilsExtensionName );
        }

        return extensions;
    }

    const std::vector<const char*> getRequiredLayers()
    {
        const std::vector validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

        // Get the required layers
        std::vector<char const*> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }
        return requiredLayers;
    }

    //Copied from Vulkan official tutorial 
    //https://github.com/KhronosGroup/Vulkan-Tutorial/blob/main/attachments/02_validation_layers.cpp#L138
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
        }
        return vk::False;
    }
};

int main(int argc, char* argv[]) {
    VulkanTutorialApp app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
