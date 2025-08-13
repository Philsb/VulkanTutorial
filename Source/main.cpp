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
    vk::raii::PhysicalDevice _physicalDevice = nullptr;
    vk::raii::Device _device = nullptr;

    vk::raii::SurfaceKHR _surface = nullptr;

    /*There could be a posibility that one queue is for graphics and anohter for presenting. 
      Most likely both will be the same. But we need a uniform approach.
      We make a search that prefers a queue that supports both for improved performance.
    */
    vk::raii::Queue _graphicsQueue = nullptr;
    vk::raii::Queue _presentQueue = nullptr;


    vk::raii::SwapchainKHR _swapChain = nullptr;
    std::vector<vk::Image> _swapChainImages;
    vk::Format _swapChainImageFormat = vk::Format::eUndefined;
    vk::Extent2D _swapChainExtent;
    std::vector<vk::raii::ImageView> _swapChainImageViews;

    std::vector<const char*> _requiredDeviceExtensions = {
            vk::KHRSwapchainExtensionName,
            vk::KHRSpirv14ExtensionName,
            vk::KHRSynchronization2ExtensionName,
            vk::KHRCreateRenderpass2ExtensionName
    };

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
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();

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
            .apiVersion         = vk::HeaderVersionComplete };

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

    void createSurface() {
        VkSurfaceKHR surface;
        if (!SDL_Vulkan_CreateSurface(_window, *_instance, nullptr, &surface)) {
            throw std::runtime_error("failed to create window surface!");
        }
        //The RAII handles the destruction of the surface so we dont need to use SDL_Vulkan_DestroySurface
        _surface = vk::raii::SurfaceKHR(_instance, surface);
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

        auto devices = _instance.enumeratePhysicalDevices();
        if (devices.empty()) 
            throw std::runtime_error("failed to find GPUs with Vulkan support!");

        const auto deviceIter = std::ranges::find_if(devices, [&]( const vk::raii::PhysicalDevice& device )
        {
            // Check if the device supports the Vulkan 1.4 API version
            bool supportsVulkanAPI = device.getProperties().apiVersion >= VK_API_VERSION_1_4;

            // Check if any of the queue families support graphics operations
            auto queueFamilies = device.getQueueFamilyProperties();
            bool supportsGraphics =
            std::ranges::any_of( queueFamilies, []( const auto & qfp ) { return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0); } );

            // Check if all required device extensions are available
            auto availableDeviceExtensions = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions =
            std::ranges::all_of( _requiredDeviceExtensions,
                                   [&availableDeviceExtensions]( auto const & requiredDeviceExtension )
                                   {
                                     return std::ranges::any_of( availableDeviceExtensions,
                                                                 [requiredDeviceExtension]( auto const & availableDeviceExtension )
                                                                 { return strcmp( availableDeviceExtension.extensionName, requiredDeviceExtension ) == 0; } );
                                   } );


            auto features = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                                            features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return supportsVulkanAPI && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        } );

        if ( deviceIter != devices.end() )
        {
            _physicalDevice = *deviceIter;
        }
        else
        {
            throw std::runtime_error( "failed to find a suitable GPU!" );
        }
    }

    void createLogicalDevice() {
        // find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = _physicalDevice.getQueueFamilyProperties();

        uint32_t graphicsQueueIndex = UINT32_MAX;
        uint32_t presentQueueIndex = UINT32_MAX;

        //Finds queues that support graphics and/or surface. It stops when it finds both
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            const bool supportsGraphics = (queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
            const bool supportsSurface = _physicalDevice.getSurfaceSupportKHR(qfpIndex, *_surface);

            if (supportsGraphics)
                graphicsQueueIndex = static_cast<uint32_t>(qfpIndex);

            if (supportsSurface)
                presentQueueIndex = static_cast<uint32_t>(qfpIndex);

            if (supportsSurface && supportsGraphics)
                break;

        }

        if ( (graphicsQueueIndex == UINT32_MAX) || (presentQueueIndex == UINT32_MAX) )
        {
            throw std::runtime_error( "Could not find a queue for graphics or present -> terminating" );
        }

        //This is exclusive for this tutorial but we NEED the queues to be the same
        //This is because swap chain VK_SHARING_MODE
        //TODO: Add support for VK_SHARING_MODE_CONCURRENT
        if ( graphicsQueueIndex != presentQueueIndex)
        {
            throw std::runtime_error( "Could not find a queue for both graphics and present." );
        }
        
        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
            {},                               // vk::PhysicalDeviceFeatures2
            {.dynamicRendering = true },      // vk::PhysicalDeviceVulkan13Features
            {.extendedDynamicState = true }   // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
        };

        // create a Device
        float queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{ .queueFamilyIndex = graphicsQueueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority };
        vk::DeviceCreateInfo deviceCreateInfo{ .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                                .queueCreateInfoCount = 1,
                                                .pQueueCreateInfos = &deviceQueueCreateInfo,
                                                .enabledExtensionCount = static_cast<uint32_t>(_requiredDeviceExtensions.size()),
                                                .ppEnabledExtensionNames = _requiredDeviceExtensions.data() };

        _device = vk::raii::Device(_physicalDevice, deviceCreateInfo);
        _graphicsQueue = vk::raii::Queue( _device, graphicsQueueIndex, 0);
        _presentQueue = vk::raii::Queue( _device, presentQueueIndex, 0);
    }

    void createSwapChain() {
        auto surfaceCapabilities = _physicalDevice.getSurfaceCapabilitiesKHR( _surface );
        _swapChainImageFormat = chooseSwapSurfaceFormat(_physicalDevice.getSurfaceFormatsKHR( _surface ));
        _swapChainExtent = chooseSwapExtent(surfaceCapabilities);

        /*
        simply sticking to this minimum means that we may sometimes have to wait on the driver to complete 
        internal operations before we can acquire another image to render to. 
        Therefore, it is recommended to request at least one more image than the minimum
        The minimum for mailbox or fifo is 2 so 2+1
        */
        auto minImageCount = std::max( 3u, surfaceCapabilities.minImageCount );

        /*
        We should also make sure to not exceed the maximum number of images while doing this, 
        where 0 is a special value that means that there is no maximum
        */
        minImageCount = ( surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount ) ? surfaceCapabilities.maxImageCount : minImageCount;
        
        
        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .surface = _surface, 
            .minImageCount = minImageCount,
            .imageFormat = _swapChainImageFormat, 
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = _swapChainExtent, 
            .imageArrayLayers =1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
             //TODO: Add support for VK_SHARING_MODE_CONCURRENT 
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surfaceCapabilities.currentTransform, 
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = chooseSwapPresentMode(_physicalDevice.getSurfacePresentModesKHR(_surface)),
            .clipped = true };

        _swapChain = vk::raii::SwapchainKHR( _device, swapChainCreateInfo );
        _swapChainImages = _swapChain.getImages();
    }

    static vk::Format chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        const auto formatIt = std::ranges::find_if(availableFormats,
        [](const auto& format) {
            return format.format == vk::Format::eB8G8R8A8Srgb &&
                   format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
        });
        return formatIt != availableFormats.end() ? formatIt->format : availableFormats[0].format;
    }

    // presentMode defines how rendered images are presented (vsync strategy):
    // FIFO        = Swapchain is a queue (like double-buffered vsync). Always waits
    //               for vertical blank; no tearing; may add input lag.
    // MAILBOX     = Like FIFO but can replace the queued image with a newer one
    //               before vsync (triple-buffered). Low latency, no tearing.
    // IMMEDIATE   = Presents immediately without waiting for vsync (fastest, can tear).
    // RELAXED_FIFO= Like FIFO but presents immediately if frame is late (can tear).
    static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        return std::ranges::any_of(availablePresentModes,
            [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; } ) ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo;
    }

    // swapExtent defines the resolution of images in the swapchain.
    // It does NOT have to match the window size — Vulkan will scale
    // the image to fit the surface. Smaller extents = fewer pixels 
    // to render (faster, lower quality). Larger extents = more detail 
    // but heavier GPU load. Aspect ratio mismatches may cause stretching.
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        int width, height;
        SDL_GetWindowSizeInPixels(_window, &width, &height);

        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

     void createImageViews() {
        _swapChainImageViews.clear();

        vk::ImageViewCreateInfo imageViewCreateInfo
        { 
            .viewType = vk::ImageViewType::e2D, 
            .format = _swapChainImageFormat,
            .subresourceRange = { .aspectMask = vk::ImageAspectFlagBits::eColor, 
                                .baseMipLevel = 0, 
                                .levelCount = 1, 
                                .baseArrayLayer = 0, 
                                .layerCount = 1 
                            } 
        };
        for ( auto image : _swapChainImages )
        {
            imageViewCreateInfo.image = image;
            _swapChainImageViews.emplace_back( _device, imageViewCreateInfo );
        }
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags( vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError );
        vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags( vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation );
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
