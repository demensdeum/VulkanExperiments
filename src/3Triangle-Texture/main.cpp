#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>
#include "EnumerateScheme.h"
#include "ErrorHandling.h"
#include "ExtensionLoader.h"
#include "Vertex.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

const int screenWidth = 640;
const int screenHeight = 480;


using std::exception;
using std::runtime_error;
using std::string;
using std::to_string;
using std::vector;

#define VULKAN_VALIDATION 1

const char *appName = "Hello Vulkan Triangle";

// layers and debug
#if VULKAN_VALIDATION
	constexpr VkDebugUtilsMessageSeverityFlagsEXT debugSeverity =
		0
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
	;
	constexpr VkDebugUtilsMessageTypeFlagsEXT debugType =
		0
		| VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
	;

	constexpr bool useAssistantLayer = false;
#endif

constexpr bool fpsCounter = true;

// window and swapchain
constexpr uint32_t initialWindowWidth = 800;
constexpr uint32_t initialWindowHeight = 800;

//constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR; // better not be used often because of coil whine
constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
//constexpr VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR;

// pipeline settings
constexpr VkClearValue clearColor = {  { {0.1f, 0.1f, 0.1f, 1.0f} }  };

// Makes present queue from different Queue Family than Graphics, for testing purposes
constexpr bool forceSeparatePresentQueue = false;

// needed stuff for main() -- forward declarations
//////////////////////////////////////////////////////////////////////////////////
VkDescriptorSet createDescriptorSet(
	VkImageView textureImageView,
	VkSampler textureSampler, 
	VkDescriptorSetLayout descriptorSetLayout, 
	VkDescriptorPool descriptorPool, 
	VkDevice device
);
std::tuple<VkImage, VkDeviceMemory> createTextureImage(const char *imagePath, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);
VkImageView createTextureImageView(VkDevice device, VkImage textureImage);
VkSampler createTextureSampler(VkDevice device);
VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device);
VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool, VkDevice device);
void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
bool isLayerSupported( const char* layer, const vector<VkLayerProperties>& supportedLayers );
bool isExtensionSupported( const char* extension, const vector<VkExtensionProperties>& supportedExtensions );
// treat layers as optional; app can always run without em -- i.e. return those supported
vector<const char*> checkInstanceLayerSupport( const vector<const char*>& requestedLayers, const vector<VkLayerProperties>& supportedLayers );
vector<VkExtensionProperties> getSupportedInstanceExtensions( const vector<const char*>& providingLayers );
bool checkExtensionSupport( const vector<const char*>& extensions, const vector<VkExtensionProperties>& supportedExtensions );

VkInstance initInstance( const vector<const char*>& layers = {}, const vector<const char*>& extensions = {} );
void killInstance( VkInstance instance );

VkPhysicalDevice getPhysicalDevice( VkInstance instance, VkSurfaceKHR surface = VK_NULL_HANDLE /*seek presentation support if !NULL*/ ); // destroyed with instance
VkPhysicalDeviceProperties getPhysicalDeviceProperties( VkPhysicalDevice physicalDevice );
VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties( VkPhysicalDevice physicalDevice );

std::pair<uint32_t, uint32_t> getQueueFamilies( VkPhysicalDevice physDevice, VkSurfaceKHR surface );
vector<VkQueueFamilyProperties> getQueueFamilyProperties( VkPhysicalDevice device );

VkDevice initDevice(
	VkPhysicalDevice physDevice,
	const VkPhysicalDeviceFeatures& features,
	uint32_t graphicsQueueFamily,
	uint32_t presentQueueFamily,
	const vector<const char*>& layers = {},
	const vector<const char*>& extensions = {}
);
void killDevice( VkDevice device );

VkQueue getQueue( VkDevice device, uint32_t queueFamily, uint32_t queueIndex );


enum class ResourceType{ Buffer, Image };

template< ResourceType resourceType, class T >
VkDeviceMemory initMemory(
	VkDevice device,
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties,
	T resource,
	const std::vector<VkMemoryPropertyFlags>& memoryTypePriority
);
void setMemoryData( VkDevice device, VkDeviceMemory memory, void* begin, size_t size );
void killMemory( VkDevice device, VkDeviceMemory memory );

VkBuffer initBuffer( VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage );
void killBuffer( VkDevice device, VkBuffer buffer );

VkImage initImage(
	VkDevice device,
	VkFormat format,
	uint32_t width, uint32_t height,
	VkSampleCountFlagBits samples,
	VkImageUsageFlags usage
);
void killImage( VkDevice device, VkImage image );

VkImageView initImageView( VkDevice device, VkImage image, VkFormat format );
void killImageView( VkDevice device, VkImageView imageView );

// initSurface() is platform dependent
void killSurface( VkInstance instance, VkSurfaceKHR surface );

VkSurfaceCapabilitiesKHR getSurfaceCapabilities( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );
VkSurfaceFormatKHR getSurfaceFormat( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface );

VkSwapchainKHR initSwapchain(
	VkPhysicalDevice physicalDevice,
	VkDevice device,
	VkSurfaceKHR surface,
	VkSurfaceFormatKHR surfaceFormat,
	VkSurfaceCapabilitiesKHR capabilities,
	uint32_t graphicsQueueFamily,
	uint32_t presentQueueFamily,
	VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE
);
void killSwapchain( VkDevice device, VkSwapchainKHR swapchain );

uint32_t getNextImageIndex( VkDevice device, VkSwapchainKHR swapchain, VkSemaphore imageReadyS );

vector<VkImageView> initSwapchainImageViews( VkDevice device, vector<VkImage> images, VkFormat format );
void killSwapchainImageViews( VkDevice device, vector<VkImageView>& imageViews );


VkRenderPass initRenderPass( VkDevice device, VkSurfaceFormatKHR surfaceFormat );
void killRenderPass( VkDevice device, VkRenderPass renderPass );

vector<VkFramebuffer> initFramebuffers(
	VkDevice device,
	VkRenderPass renderPass,
	vector<VkImageView> imageViews,
	uint32_t width, uint32_t height
);
void killFramebuffers( VkDevice device, vector<VkFramebuffer>& framebuffers );


void killShaderModule( VkDevice device, VkShaderModule shaderModule );

VkPipelineLayout initPipelineLayout(
	VkDevice device,
	VkDescriptorSetLayout descriptorSetLayout
);
void killPipelineLayout( VkDevice device, VkPipelineLayout pipelineLayout );

VkPipeline initPipeline(
	VkDevice device,
	VkPhysicalDeviceLimits limits,
	VkPipelineLayout pipelineLayout,
	VkRenderPass renderPass,
	VkShaderModule vertexShader,
	VkShaderModule fragmentShader,
	const uint32_t vertexBufferBinding,
	uint32_t width, uint32_t height
);
void killPipeline( VkDevice device, VkPipeline pipeline );


VkDescriptorPool createDescriptorPool(VkDevice device);
void setVertexData( VkDevice device, VkDeviceMemory memory, vector<Vertex3D_UV> vertices );

VkSemaphore initSemaphore( VkDevice device );
vector<VkSemaphore> initSemaphores( VkDevice device, size_t count );
void killSemaphore( VkDevice device, VkSemaphore semaphore );
void killSemaphores( VkDevice device, vector<VkSemaphore>& semaphores );

VkCommandPool initCommandPool( VkDevice device, const uint32_t queueFamily );
void killCommandPool( VkDevice device, VkCommandPool commandPool );

vector<VkFence> initFences( VkDevice device, size_t count, VkFenceCreateFlags flags = 0 );
void killFences( VkDevice device, vector<VkFence>& fences );

void acquireCommandBuffers( VkDevice device, VkCommandPool commandPool, uint32_t count, vector<VkCommandBuffer>& commandBuffers );
void beginCommandBuffer( VkCommandBuffer commandBuffer );
void endCommandBuffer( VkCommandBuffer commandBuffer );

void recordBeginRenderPass(
	VkCommandBuffer commandBuffer,
	VkRenderPass renderPass,
	VkFramebuffer framebuffer,
	VkClearValue clearValue,
	uint32_t width, uint32_t height
);
void recordEndRenderPass( VkCommandBuffer commandBuffer );

void recordBindPipeline( VkCommandBuffer commandBuffer, VkPipeline pipeline );
void recordBindVertexBuffer( VkCommandBuffer commandBuffer, const uint32_t vertexBufferBinding, VkBuffer vertexBuffer );

void recordDraw( VkCommandBuffer commandBuffer, uint32_t vertexCount );

void submitToQueue( VkQueue queue, VkCommandBuffer commandBuffer, VkSemaphore imageReadyS, VkSemaphore renderDoneS, VkFence fence = VK_NULL_HANDLE );
void present( VkQueue queue, VkSwapchainKHR swapchain, uint32_t swapchainImageIndex, VkSemaphore renderDoneS );

// cleanup dangerous semaphore with signal pending from vkAcquireNextImageKHR
void cleanupUnsafeSemaphore( VkQueue queue, VkSemaphore semaphore );


// main()!
//////////////////////////////////////////////////////////////////////////////////

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

//VkShaderModule createShaderModule(const std::vector<char>& code) {
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

int start() try{
	const uint32_t vertexBufferBinding = 0;

	const float triangleSize = 1.6f;
	const vector<Vertex3D_UV> triangle = {
		{{{ 0.5f * triangleSize,  sqrtf(3.0f) * 0.25f * triangleSize, 0.f}}, {{1.0f, 0.0f}}},
		{{{                0.0f, -sqrtf(3.0f) * 0.25f * triangleSize, 0.f}}, {{0.0f, 1.0f}}},
		{{{-0.5f * triangleSize,  sqrtf(3.0f) * 0.25f * triangleSize, 0.f}}, {{0.0f, 0.0f}}},
	};

	const auto supportedLayers = enumerate<VkInstance, VkLayerProperties>();
	vector<const char*> requestedLayers;

#if VULKAN_VALIDATION
	if(  isLayerSupported( "VK_LAYER_KHRONOS_validation", supportedLayers )  ) requestedLayers.push_back( "VK_LAYER_KHRONOS_validation" );
	else throw "VULKAN_VALIDATION is enabled but VK_LAYER_KHRONOS_validation layers are not supported!";

	if( ::useAssistantLayer ){
		if(  isLayerSupported( "VK_LAYER_LUNARG_assistant_layer", supportedLayers )  ) requestedLayers.push_back( "VK_LAYER_LUNARG_assistant_layer" );
		else throw "VULKAN_VALIDATION is enabled but VK_LAYER_LUNARG_assistant_layer layer is not supported!";
	}
#endif
    const char *windowTitle = "Triangle - Vulkan";

    SDL_Window *window = SDL_CreateWindow(
        windowTitle, 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        screenWidth, 
        screenHeight, 
        SDL_WINDOW_VULKAN
    );

    uint32_t sdlExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr)) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }
    
    std::vector<const char*> supportedInstanceExtensions(sdlExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, supportedInstanceExtensions.data())) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }

#if VULKAN_VALIDATION
	DebugObjectType debugExtensionTag;

    uint32_t requestedExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &requestedExtensionCount, nullptr)) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }
    
    vector<const char*> requestedInstanceExtensions(requestedExtensionCount);
    requestedInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &requestedExtensionCount, requestedInstanceExtensions.data())) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }

	if(true){
		debugExtensionTag = DebugObjectType::debugUtils;
		requestedInstanceExtensions.push_back( VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}
	else if(true){
		debugExtensionTag = DebugObjectType::debugReport;
		requestedInstanceExtensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
	}
	else throw "VULKAN_VALIDATION is enabled but neither VK_EXT_debug_utils nor VK_EXT_debug_report extension is supported!";
#else
    uint32_t requestedExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &requestedExtensionCount, nullptr)) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }
    
    vector<const char*> requestedInstanceExtensions(requestedExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &requestedExtensionCount, requestedInstanceExtensions.data())) {
        throw std::runtime_error("failed to get SDL Vulkan extensions");
    }
#endif

	//checkExtensionSupport(requestedInstanceExtensions, supportedInstanceExtensions);


	const VkInstance instance = initInstance( requestedLayers, requestedInstanceExtensions );

#if VULKAN_VALIDATION
	const auto debugHandle = initDebug( instance, debugExtensionTag, ::debugSeverity, ::debugType );

	const int32_t uncoded = 0;
	const char* introMsg = "Validation Layers are enabled!";
	if( debugExtensionTag == DebugObjectType::debugUtils ){
		VkDebugUtilsObjectNameInfoEXT object = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr, // pNext
			VK_OBJECT_TYPE_INSTANCE,
			handleToUint64(instance),
			"instance"
		};
		const VkDebugUtilsMessengerCallbackDataEXT dumcd = {
			VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT,
			nullptr, // pNext
			0, // flags
			"VULKAN_VALIDATION", // VUID
			0, // VUID hash
			introMsg,
			0, nullptr, 0, nullptr,
			1, &object
		};
		vkSubmitDebugUtilsMessageEXT( instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT, VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT, &dumcd );
	}
	else if( debugExtensionTag == DebugObjectType::debugReport ){
		vkDebugReportMessageEXT( instance, VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, (uint64_t)instance, __LINE__, uncoded, "Application", introMsg );
	}
#endif

    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, &surface);

	const VkPhysicalDevice physicalDevice = getPhysicalDevice( instance, surface );
	const VkPhysicalDeviceProperties physicalDeviceProperties = getPhysicalDeviceProperties( physicalDevice );
	const VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties = getPhysicalDeviceMemoryProperties( physicalDevice );

	uint32_t graphicsQueueFamily, presentQueueFamily;
	std::tie( graphicsQueueFamily, presentQueueFamily ) = getQueueFamilies( physicalDevice, surface );

	const VkPhysicalDeviceFeatures features = {}; // don't need any special feature for this demo
	const vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	const VkDevice device = initDevice( physicalDevice, features, graphicsQueueFamily, presentQueueFamily, requestedLayers, deviceExtensions );
	const VkQueue graphicsQueue = getQueue( device, graphicsQueueFamily, 0 );
	const VkQueue presentQueue = getQueue( device, presentQueueFamily, 0 );


	VkSurfaceFormatKHR surfaceFormat = getSurfaceFormat( physicalDevice, surface );
	VkRenderPass renderPass = initRenderPass( device, surfaceFormat );

    auto vertexShaderCode = readFile("vertexShader.spv");
    auto fragShaderCode = readFile("fragmentShader.spv");

	VkCommandPool commandPool = initCommandPool( device, graphicsQueueFamily );

    auto descriptorSetLayout = createDescriptorSetLayout(device);
    auto descriptorPool = createDescriptorPool(device);
	auto createTextureResult = createTextureImage(
		"brick.texture.bmp",
		device,
		physicalDevice,
		commandPool,
		graphicsQueue
	);
	VkImage textureImage = std::get<0>(createTextureResult);
	VkDeviceMemory textureImageMemory = std::get<1>(createTextureResult);
	auto textureImageView = createTextureImageView(
		device,
		textureImage
	);
	auto textureSampler = createTextureSampler(
		device
	);
    auto descriptorSet = createDescriptorSet(
		textureImageView,
		textureSampler,
		descriptorSetLayout,
		descriptorPool,
		device
	);

    VkShaderModule vertexShader = createShaderModule(device, vertexShaderCode);
    VkShaderModule fragmentShader = createShaderModule(device,fragShaderCode);    

	VkPipelineLayout pipelineLayout = initPipelineLayout(device, descriptorSetLayout);

	VkBuffer vertexBuffer = initBuffer( device, sizeof( decltype( triangle )::value_type ) * triangle.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT );
	const std::vector<VkMemoryPropertyFlags> memoryTypePriority{
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // preferably wanna device-side memory that can be updated from host without hassle
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // guaranteed to allways be supported
	};
	VkDeviceMemory vertexBufferMemory = initMemory<ResourceType::Buffer>(
		device,
		physicalDeviceMemoryProperties,
		vertexBuffer,
		memoryTypePriority
	);
	setVertexData( device, vertexBufferMemory, triangle ); // Writes throug memory map. Synchronization is implicit for any subsequent vkQueueSubmit batches.

	// might need synchronization if init is more advanced than this
	//VkResult errorCode = vkDeviceWaitIdle( device ); RESULT_HANDLER( errorCode, "vkDeviceWaitIdle" );


	// place-holder swapchain dependent objects
	VkSwapchainKHR swapchain = VK_NULL_HANDLE; // has to be NULL -- signifies that there's no swapchain
	vector<VkImageView> swapchainImageViews;
	vector<VkFramebuffer> framebuffers;

	VkPipeline pipeline = VK_NULL_HANDLE; // has to be NULL for the case the app ends before even first swapchain
	vector<VkCommandBuffer> commandBuffers;

	vector<VkSemaphore> imageReadySs;
	vector<VkSemaphore> renderDoneSs;

	// workaround for validation layer "memory leak" + might also help the driver to cleanup old resources
	// this should not be needed for a real-word app, because they are likely to use fences naturaly (e.g. responding to user input )
	// read https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1628
	const uint32_t maxInflightSubmissions = 2; // more than 2 probably does not make much sense
	uint32_t submissionNr = 0; // index of the current submission modulo maxInflightSubmission
	vector<VkFence> submissionFences;


	const std::function<bool(void)> recreateSwapchain = [&](){
		// swapchain recreation -- will be done before the first frame too;
		TODO( "This may be triggered from many sources (e.g. WM_SIZE event, and VK_ERROR_OUT_OF_DATE_KHR too). Should prevent duplicate swapchain recreation." )

		const VkSwapchainKHR oldSwapchain = swapchain;
		swapchain = VK_NULL_HANDLE;

		VkSurfaceCapabilitiesKHR capabilities = getSurfaceCapabilities( physicalDevice, surface );

		if( capabilities.currentExtent.width == UINT32_MAX && capabilities.currentExtent.height == UINT32_MAX ){
			capabilities.currentExtent.width = screenWidth;
			capabilities.currentExtent.height = screenHeight;
		}
		VkExtent2D surfaceSize = { capabilities.currentExtent.width, capabilities.currentExtent.height };

		const bool swapchainCreatable = {
			   surfaceSize.width >= capabilities.minImageExtent.width
			&& surfaceSize.width <= capabilities.maxImageExtent.width
			&& surfaceSize.width > 0
			&& surfaceSize.height >= capabilities.minImageExtent.height
			&& surfaceSize.height <= capabilities.maxImageExtent.height
			&& surfaceSize.height > 0
		};


		// cleanup old
		vector<VkSemaphore> oldImageReadySs = imageReadySs; imageReadySs.clear();
		if( oldSwapchain ){
			{VkResult errorCode = vkDeviceWaitIdle( device ); RESULT_HANDLER( errorCode, "vkDeviceWaitIdle" );}

			// fences might be in unsignaled state, so kill them too to get fresh signaled
			killFences( device, submissionFences );

			// semaphores might be in signaled state, so kill them too to get fresh unsignaled
			killSemaphores( device, renderDoneSs );
			// kill imageReadySs later when oldSwapchain is destroyed

			// only reset + later reuse already allocated and create new only if needed
			{VkResult errorCode = vkResetCommandPool( device, commandPool, 0 ); RESULT_HANDLER( errorCode, "vkResetCommandPool" );}

			killPipeline( device, pipeline );
			killFramebuffers( device, framebuffers );
			killSwapchainImageViews( device, swapchainImageViews );

			// kill oldSwapchain later, after it is potentially used by vkCreateSwapchainKHR
		}

		// creating new
		if( swapchainCreatable ){
			// reuses & destroys the oldSwapchain
			swapchain = initSwapchain( physicalDevice, device, surface, surfaceFormat, capabilities, graphicsQueueFamily, presentQueueFamily, oldSwapchain );

			vector<VkImage> swapchainImages = enumerate<VkImage>( device, swapchain );
			swapchainImageViews = initSwapchainImageViews( device, swapchainImages, surfaceFormat.format );
			framebuffers = initFramebuffers( device, renderPass, swapchainImageViews, surfaceSize.width, surfaceSize.height );

			pipeline = initPipeline(
				device,
				physicalDeviceProperties.limits,
				pipelineLayout,
				renderPass,
				vertexShader,
				fragmentShader,
				vertexBufferBinding,
				surfaceSize.width, surfaceSize.height
			);

			acquireCommandBuffers(  device, commandPool, static_cast<uint32_t>( swapchainImages.size() ), commandBuffers  );
			for( size_t i = 0; i < swapchainImages.size(); ++i ){
				beginCommandBuffer( commandBuffers[i] );
					recordBeginRenderPass( commandBuffers[i], renderPass, framebuffers[i], ::clearColor, surfaceSize.width, surfaceSize.height );

					recordBindPipeline( commandBuffers[i], pipeline );
					recordBindVertexBuffer( commandBuffers[i], vertexBufferBinding, vertexBuffer );

					vkCmdBindDescriptorSets(
						commandBuffers[i], 
						VK_PIPELINE_BIND_POINT_GRAPHICS, 
						pipelineLayout, 
						0, 
						1, 
						&descriptorSet, 
						0, 
						nullptr
					);

					recordDraw(  commandBuffers[i], static_cast<uint32_t>( triangle.size() )  );

					recordEndRenderPass( commandBuffers[i] );
				endCommandBuffer( commandBuffers[i] );
			}

			imageReadySs = initSemaphores( device, maxInflightSubmissions );
			// per https://github.com/KhronosGroup/Vulkan-Docs/issues/1150 need upto swapchain-image count
			renderDoneSs = initSemaphores( device, swapchainImages.size());

			submissionFences = initFences( device, maxInflightSubmissions, VK_FENCE_CREATE_SIGNALED_BIT ); // signaled fence means previous execution finished, so we start rendering presignaled
			submissionNr = 0;
		}

		if( oldSwapchain ){
			killSwapchain( device, oldSwapchain );

			// per current spec, we can't really be sure these are not used :/ at least kill them after the swapchain
			// https://github.com/KhronosGroup/Vulkan-Docs/issues/152
			killSemaphores( device, oldImageReadySs );
		}

		return swapchain != VK_NULL_HANDLE;
	};


	// Finally, rendering! Yay!
	const std::function<void(void)> render = [&](){
		assert( swapchain ); // should be always true; should have yielded CPU if false

		// vkAcquireNextImageKHR produces unsafe semaphore that needs extra cleanup. Track that with this variable.
		bool unsafeSemaphore = false;

		try{
			// remove oldest frame from being in flight before starting new one
			// refer to doc/, which talks about the cycle of how the synch primitives are (re)used here
			{VkResult errorCode = vkWaitForFences( device, 1, &submissionFences[submissionNr], VK_TRUE, UINT64_MAX ); RESULT_HANDLER( errorCode, "vkWaitForFences" );}
			{VkResult errorCode = vkResetFences( device, 1, &submissionFences[submissionNr] ); RESULT_HANDLER( errorCode, "vkResetFences" );}

			unsafeSemaphore = true;
			uint32_t nextSwapchainImageIndex = getNextImageIndex( device, swapchain, imageReadySs[submissionNr] );
			unsafeSemaphore = false;

			submitToQueue( graphicsQueue, commandBuffers[nextSwapchainImageIndex], imageReadySs[submissionNr], renderDoneSs[nextSwapchainImageIndex], submissionFences[submissionNr] );
			present( presentQueue, swapchain, nextSwapchainImageIndex, renderDoneSs[nextSwapchainImageIndex] );

			submissionNr = (submissionNr + 1) % maxInflightSubmissions;
		}
		catch( VulkanResultException ex ){
			if( ex.result == VK_SUBOPTIMAL_KHR || ex.result == VK_ERROR_OUT_OF_DATE_KHR ){
				if( unsafeSemaphore && ex.result == VK_SUBOPTIMAL_KHR ){
					cleanupUnsafeSemaphore( graphicsQueue, imageReadySs[submissionNr] );
					// no way to sanitize vkQueuePresentKHR semaphores, really
				}
				recreateSwapchain();

				// we need to start over...
				render();
			}
			else throw;
		}
	};

	recreateSwapchain();

    SDL_Event event;
    while (
        event.type != SDL_QUIT && 
        (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) == false
    ) {
        SDL_PollEvent(&event);
        render();
    }

    int exitStatus = 0;

	// proper Vulkan cleanup
	VkResult errorCode = vkDeviceWaitIdle( device ); 
    RESULT_HANDLER( errorCode, "vkDeviceWaitIdle" );


	// kill swapchain
	killSemaphores( device, renderDoneSs );
	// imageReadySs killed after the swapchain

	// command buffers killed with pool

	killPipeline( device, pipeline );

	killFramebuffers( device, framebuffers );

	killSwapchainImageViews( device, swapchainImageViews );
	killSwapchain( device, swapchain );

	// per current spec, we can't really be sure these are not used :/ at least kill them after the swapchain
	// https://github.com/KhronosGroup/Vulkan-Docs/issues/152
	killSemaphores( device, imageReadySs );


	// kill vulkan
	killFences( device, submissionFences );

	killCommandPool( device,  commandPool );

	killBuffer( device, vertexBuffer );
	killMemory( device, vertexBufferMemory );

	killPipelineLayout( device, pipelineLayout );
	killShaderModule( device, fragmentShader );
	killShaderModule( device, vertexShader );

	killRenderPass( device, renderPass );

	killDevice( device );

	killSurface( instance, surface );
	//killWindow( window );

#if VULKAN_VALIDATION
	killDebug( instance, debugHandle );
#endif
	killInstance( instance );

	return exitStatus;
}
catch( VulkanResultException vkE ){
	logger << "ERROR: Terminated due to an uncaught VkResult exception: "
	       << vkE.file << ":" << vkE.line << ":" << vkE.func << "() " << vkE.source << "() returned " << to_string( vkE.result )
	       << std::endl;
	return EXIT_FAILURE;
}
catch( const char* e ){
	logger << "ERROR: Terminated due to an uncaught exception: " << e << std::endl;
	return EXIT_FAILURE;
}
catch( string e ){
	logger << "ERROR: Terminated due to an uncaught exception: " << e << std::endl;
	return EXIT_FAILURE;
}
catch( std::exception e ){
	logger << "ERROR: Terminated due to an uncaught exception: " << e.what() << std::endl;
	return EXIT_FAILURE;
}
catch( ... ){
	logger << "ERROR: Terminated due to an unrecognized uncaught exception." << std::endl;
	return EXIT_FAILURE;
}


#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int ){
	return helloTriangle();
}
#else
int main(){
	return start();
}
#endif

// Implementation
//////////////////////////////////////////////////////////////////////////////////

VkDescriptorSetLayout createDescriptorSetLayout(VkDevice device) {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

	VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

	return descriptorSetLayout;
}

VkDescriptorPool createDescriptorPool(VkDevice device) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

	VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
	return descriptorPool;
}

VkDescriptorSet createDescriptorSet(
	VkImageView textureImageView,
	VkSampler textureSampler, 
	VkDescriptorSetLayout descriptorSetLayout, 
	VkDescriptorPool descriptorPool, 
	VkDevice device
) {
    VkDescriptorSetLayout layouts[] = {descriptorSetLayout};
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = layouts;

	VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureImageView;
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	return descriptorSet;
}

void endSingleTimeCommands(VkQueue graphicsQueue, VkCommandPool commandPool, VkDevice device, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBufferToImage(VkQueue graphicsQueue, VkCommandPool commandPool, VkDevice device, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool, device);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(graphicsQueue, commandPool, device, commandBuffer);
}

VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool, VkDevice device) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void transitionImageLayout(VkQueue graphicsQueue, VkCommandPool commandPool, VkDevice device, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(commandPool, device);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(graphicsQueue, commandPool, device, commandBuffer);
}

std::tuple<VkImage, VkDeviceMemory> createTextureImage(const char *imagePath, VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue) {
    SDL_Surface* surface = SDL_LoadBMP(imagePath);
    if (!surface) {
        throw std::runtime_error("failed to load texture image!");
    }

    // Конвертация пикселей из BGR в RGBA
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!rgbaSurface) {
        SDL_FreeSurface(surface);
        throw std::runtime_error("failed to convert surface to RGBA format!");
    }

    VkDeviceSize imageSize = rgbaSurface->w * rgbaSurface->h * 4;  // 4 bytes per pixel (RGBA)

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, rgbaSurface->pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

    createImage(
		device, 
		physicalDevice, 
		rgbaSurface->w, 
		rgbaSurface->h, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_TILING_OPTIMAL, 
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		textureImage, 
		textureImageMemory
	);

    transitionImageLayout(
		graphicsQueue, 
		commandPool, 
		device, 
		textureImage, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_LAYOUT_UNDEFINED, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);
    copyBufferToImage(
		graphicsQueue,
		commandPool,
		device,
		stagingBuffer, 
		textureImage, 
		static_cast<uint32_t>(rgbaSurface->w), 
		static_cast<uint32_t>(rgbaSurface->h)
	);
    transitionImageLayout(
		graphicsQueue, 
		commandPool, 
		device, 
		textureImage, 
		VK_FORMAT_R8G8B8A8_SRGB, 
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    SDL_FreeSurface(rgbaSurface);
    SDL_FreeSurface(surface);

	return std::make_tuple(textureImage, textureImageMemory);
}

VkImageView createTextureImageView(VkDevice device, VkImage textureImage) {
    auto textureImageView = createImageView(device, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	return textureImageView;
}

VkSampler createTextureSampler(VkDevice device) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    //samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

	VkSampler textureSampler;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
	return textureSampler;
}

void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

bool isLayerSupported( const char* layer, const vector<VkLayerProperties>& supportedLayers ){
	const auto isSupportedPred = [layer]( const VkLayerProperties& prop ) -> bool{
		return std::strcmp( layer, prop.layerName ) == 0;
	};

	return std::any_of( supportedLayers.begin(), supportedLayers.end(), isSupportedPred );
}

bool isExtensionSupported( const char* extension, const vector<VkExtensionProperties>& supportedExtensions ){
	const auto isSupportedPred = [extension]( const VkExtensionProperties& prop ) -> bool{
		return std::strcmp( extension, prop.extensionName ) == 0;
	};

	return std::any_of( supportedExtensions.begin(), supportedExtensions.end(), isSupportedPred );
}

vector<const char*> checkInstanceLayerSupport( const vector<const char*>& requestedLayers, const vector<VkLayerProperties>& supportedLayers ){
	vector<const char*> compiledLayerList;

	for( const auto layer : requestedLayers ){
		if(  isLayerSupported( layer, supportedLayers )  ) compiledLayerList.push_back( layer );
		else logger << "WARNING: Requested layer " << layer << " is not supported. It will not be enabled." << std::endl;
	}

	return compiledLayerList;
}

vector<const char*> checkInstanceLayerSupport( const vector<const char*>& optionalLayers ){
	return checkInstanceLayerSupport( optionalLayers, enumerate<VkInstance, VkLayerProperties>() );
}

vector<VkExtensionProperties> getSupportedInstanceExtensions( const vector<const char*>& providingLayers ){
	auto supportedExtensions = enumerate<VkInstance, VkExtensionProperties>();

	for( const auto pl : providingLayers ){
		const auto providedExtensions = enumerate<VkInstance, VkExtensionProperties>( pl );
		supportedExtensions.insert( supportedExtensions.end(), providedExtensions.begin(), providedExtensions.end() );
	}

	return supportedExtensions;
}

vector<VkExtensionProperties> getSupportedDeviceExtensions( const VkPhysicalDevice physDevice, const vector<const char*>& providingLayers ){
	auto supportedExtensions = enumerate<VkExtensionProperties>( physDevice );

	for( const auto pl : providingLayers ){
		const auto providedExtensions = enumerate<VkExtensionProperties>( physDevice, pl );
		supportedExtensions.insert( supportedExtensions.end(), providedExtensions.begin(), providedExtensions.end() );
	}

	return supportedExtensions;
}

bool checkExtensionSupport( const vector<const char*>& extensions, const vector<VkExtensionProperties>& supportedExtensions ){
	bool allSupported = true;

	for( const auto extension : extensions ){
		if(  !isExtensionSupported( extension, supportedExtensions )  ){
			allSupported = false;
			logger << "WARNING: Requested extension " << extension << " is not supported. Trying to enable it will likely fail." << std::endl;
		}
	}

	return allSupported;
}

bool checkDeviceExtensionSupport( const VkPhysicalDevice physDevice, const vector<const char*>& extensions, const vector<const char*>& providingLayers ){
	return checkExtensionSupport(  extensions, getSupportedDeviceExtensions( physDevice, providingLayers )  );
}

VkInstance initInstance( const vector<const char*>& layers, const vector<const char*>& extensions ){
	const VkApplicationInfo appInfo = {
		VK_STRUCTURE_TYPE_APPLICATION_INFO,
		nullptr, // pNext
		::appName, // Nice to meetcha, and what's your name driver?
		0, // app version
		nullptr, // engine name
		0, // engine version
		VK_API_VERSION_1_0 // this app is written against the Vulkan 1.0 spec
	};

#if VULKAN_VALIDATION
	// in effect during vkCreateInstance and vkDestroyInstance duration (because callback object cannot be created without instance)
	const VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo{
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		nullptr, // pNext
		translateFlags( ::debugSeverity, ::debugType ),
		::genericDebugReportCallback,
		nullptr // pUserData
	};

	const VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		nullptr, // pNext
		0, // flags
		debugSeverity,
		debugType,
		::genericDebugUtilsCallback,
		nullptr // pUserData
	};

	bool debugUtils = std::find_if( extensions.begin(), extensions.end(), [](const char* e){ return std::strcmp( e, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) == 0; } ) != extensions.end();
	bool debugReport = std::find_if( extensions.begin(), extensions.end(), [](const char* e){ return std::strcmp( e, VK_EXT_DEBUG_REPORT_EXTENSION_NAME ) == 0; } ) != extensions.end();
	if( !debugUtils && !debugReport ) throw "VULKAN_VALIDATION is enabled but neither VK_EXT_debug_utils nor VK_EXT_debug_report extension is being enabled!";
	const void* debugpNext = debugUtils ? (void*)&debugUtilsCreateInfo : (void*)&debugReportCreateInfo;
#endif

	const VkInstanceCreateInfo instanceInfo{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
#if VULKAN_VALIDATION
		debugpNext,
#else
		nullptr, // pNext
#endif
		0, // flags - reserved for future use
		&appInfo,
		static_cast<uint32_t>( layers.size() ),
		layers.data(),
		static_cast<uint32_t>( extensions.size() ),
		extensions.data()
	};

	VkInstance instance;
	const VkResult errorCode = vkCreateInstance( &instanceInfo, nullptr, &instance ); RESULT_HANDLER( errorCode, "vkCreateInstance" );

	loadInstanceExtensionsCommands( instance, extensions );

	return instance;
}

void killInstance( const VkInstance instance ){
	unloadInstanceExtensionsCommands( instance );

	vkDestroyInstance( instance, nullptr );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool isPresentationSupported( const VkPhysicalDevice physDevice, const uint32_t queueFamily, const VkSurfaceKHR surface ){
	VkBool32 supported;
	const VkResult errorCode = vkGetPhysicalDeviceSurfaceSupportKHR( physDevice, queueFamily, surface, &supported ); RESULT_HANDLER( errorCode, "vkGetPhysicalDeviceSurfaceSupportKHR" );

	return supported == VK_TRUE;
}

bool isPresentationSupported( const VkPhysicalDevice physDevice, const VkSurfaceKHR surface ){
	uint32_t qfCount;
	vkGetPhysicalDeviceQueueFamilyProperties( physDevice, &qfCount, nullptr );

	for( uint32_t qf = 0; qf < qfCount; ++qf ){
		if(  isPresentationSupported( physDevice, qf, surface )  ) return true;
	}

	return false;
}

VkPhysicalDevice getPhysicalDevice( const VkInstance instance, const VkSurfaceKHR surface ){
	vector<VkPhysicalDevice> devices = enumerate<VkPhysicalDevice>( instance );

	if( surface ){
		for( auto it = devices.begin(); it != devices.end(); ){
			const auto& pd = *it;

			if(  !isPresentationSupported( pd, surface )  ) it = devices.erase( it );
			else ++it;
		}
	}

	if( devices.empty() ) throw string("ERROR: No Physical Devices (GPUs) ") + (surface ? "with presentation support " : "") + "detected!";
	else if( devices.size() == 1 ){
		return devices[0];
	}
	else{
		for( const auto pd : devices ){
			const VkPhysicalDeviceProperties pdp = getPhysicalDeviceProperties( pd );

			if( pdp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ){
#if VULKAN_VALIDATION
				vkDebugReportMessageEXT(
					instance, VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, handleToUint64(instance), __LINE__, 
					1, u8"application", u8"More than one Physical Devices (GPU) found. Choosing the first dedicated one."
				);
#endif

				return pd;
			}
		}

#if VULKAN_VALIDATION
		vkDebugReportMessageEXT(
			instance, VK_DEBUG_REPORT_WARNING_BIT_EXT, VK_DEBUG_REPORT_OBJECT_TYPE_INSTANCE_EXT, handleToUint64(instance), __LINE__, 
			1, u8"application", u8"More than one Physical Devices (GPU) found. Just choosing the first one."
		);
#endif

		return devices[0];
	}
}

VkPhysicalDeviceProperties getPhysicalDeviceProperties( VkPhysicalDevice physicalDevice ){
	VkPhysicalDeviceProperties properties;
	vkGetPhysicalDeviceProperties( physicalDevice, &properties );
	return properties;
}

VkPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties( VkPhysicalDevice physicalDevice ){
	VkPhysicalDeviceMemoryProperties memoryInfo;
	vkGetPhysicalDeviceMemoryProperties( physicalDevice, &memoryInfo );
	return memoryInfo;
}

vector<VkQueueFamilyProperties> getQueueFamilyProperties( VkPhysicalDevice device ){
	uint32_t queueFamiliesCount;
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamiliesCount, nullptr );

	vector<VkQueueFamilyProperties> queueFamilies( queueFamiliesCount );
	vkGetPhysicalDeviceQueueFamilyProperties( device, &queueFamiliesCount, queueFamilies.data() );

	return queueFamilies;
}

std::pair<uint32_t, uint32_t> getQueueFamilies( const VkPhysicalDevice physDevice, const VkSurfaceKHR surface ){
	constexpr uint32_t notFound = VK_QUEUE_FAMILY_IGNORED;
	const auto qfps = getQueueFamilyProperties( physDevice );
	const auto findQueueFamilyThat = [&qfps, notFound](std::function<bool (const VkQueueFamilyProperties&, const uint32_t)> predicate) -> uint32_t{
		for( uint32_t qf = 0; qf < qfps.size(); ++qf ) if( predicate(qfps[qf], qf) ) return qf;
		return notFound;
	};

	const auto isGraphics = [](const VkQueueFamilyProperties& props, const uint32_t = 0){
		return props.queueFlags & VK_QUEUE_GRAPHICS_BIT;
	};
	const auto isPresent = [=](const VkQueueFamilyProperties&, const uint32_t queueFamily){
		return isPresentationSupported( physDevice, queueFamily, surface );
	};
	const auto isFusedGraphicsAndPresent = [=](const VkQueueFamilyProperties& props, const uint32_t queueFamily){
		return isGraphics( props ) && isPresent( props, queueFamily );
	};

	uint32_t graphicsQueueFamily = notFound;
	uint32_t presentQueueFamily = notFound;
	if( ::forceSeparatePresentQueue ){
		graphicsQueueFamily = findQueueFamilyThat( isGraphics );

		const auto isSeparatePresent = [graphicsQueueFamily, isPresent](const VkQueueFamilyProperties& props, const uint32_t queueFamily){
			return queueFamily != graphicsQueueFamily && isPresent( props, queueFamily );
		};
		presentQueueFamily = findQueueFamilyThat( isSeparatePresent );
	}
	else{
		graphicsQueueFamily = presentQueueFamily = findQueueFamilyThat( isFusedGraphicsAndPresent );
		if( graphicsQueueFamily == notFound || presentQueueFamily == notFound ){
			graphicsQueueFamily = findQueueFamilyThat( isGraphics );
			presentQueueFamily = findQueueFamilyThat( isPresent );
		}
	}

	if( graphicsQueueFamily == notFound ) throw "Cannot find a graphics queue family!";
	if( presentQueueFamily == notFound ) throw "Cannot find a presentation queue family!";

	return std::make_pair( graphicsQueueFamily, presentQueueFamily );
}

VkDevice initDevice(
	const VkPhysicalDevice physDevice,
	const VkPhysicalDeviceFeatures& features,
	const uint32_t graphicsQueueFamily,
	const uint32_t presentQueueFamily,
	const vector<const char*>& layers,
	const vector<const char*>& extensions
){
	checkDeviceExtensionSupport( physDevice, extensions, layers );

	const float priority[] = {1.0f};

	vector<VkDeviceQueueCreateInfo> queues = {
		{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr, // pNext
			0, // flags
			graphicsQueueFamily,
			1, // queue count
			priority
		}
	};

	if( presentQueueFamily != graphicsQueueFamily ){
		queues.push_back({
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr, // pNext
			0, // flags
			presentQueueFamily,
			1, // queue count
			priority
		});
	}

	const VkDeviceCreateInfo deviceInfo{
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		static_cast<uint32_t>( queues.size() ),
		queues.data(),
		static_cast<uint32_t>( layers.size() ),
		layers.data(),
		static_cast<uint32_t>( extensions.size() ),
		extensions.data(),
		&features
	};


	VkDevice device;
	const VkResult errorCode = vkCreateDevice( physDevice, &deviceInfo, nullptr, &device ); RESULT_HANDLER( errorCode, "vkCreateDevice" );

	loadDeviceExtensionsCommands( device, extensions );

	return device;
}

void killDevice( const VkDevice device ){
	unloadDeviceExtensionsCommands( device );

	vkDestroyDevice( device, nullptr );
}

VkQueue getQueue( const VkDevice device, const uint32_t queueFamily, const uint32_t queueIndex ){
	VkQueue queue;
	vkGetDeviceQueue( device, queueFamily, queueIndex, &queue );

	return queue;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template< ResourceType resourceType, class T >
VkMemoryRequirements getMemoryRequirements( VkDevice device, T resource );

template<>
VkMemoryRequirements getMemoryRequirements< ResourceType::Buffer >( VkDevice device, VkBuffer buffer ){
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements( device, buffer, &memoryRequirements );

	return memoryRequirements;
}

template<>
VkMemoryRequirements getMemoryRequirements< ResourceType::Image >( VkDevice device, VkImage image ){
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements( device, image, &memoryRequirements );

	return memoryRequirements;
}

template< ResourceType resourceType, class T >
void bindMemory( VkDevice device, T buffer, VkDeviceMemory memory, VkDeviceSize offset );

template<>
void bindMemory< ResourceType::Buffer >( VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset ){
	VkResult errorCode = vkBindBufferMemory( device, buffer, memory, offset ); RESULT_HANDLER( errorCode, "vkBindBufferMemory" );
}

template<>
void bindMemory< ResourceType::Image >( VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset ){
	VkResult errorCode = vkBindImageMemory( device, image, memory, offset ); RESULT_HANDLER( errorCode, "vkBindImageMemory" );
}

template< ResourceType resourceType, class T >
VkDeviceMemory initMemory(
	VkDevice device,
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties,
	T resource,
	const std::vector<VkMemoryPropertyFlags>& memoryTypePriority
){
	const VkMemoryRequirements memoryRequirements = getMemoryRequirements<resourceType>( device, resource );

	const auto indexToBit = []( const uint32_t index ){ return 0x1 << index; };

	const uint32_t memoryTypeNotFound = UINT32_MAX;
	uint32_t memoryType = memoryTypeNotFound;
	for( const auto desiredMemoryType : memoryTypePriority ){
		const uint32_t maxMemoryTypeCount = 32;
		for( uint32_t i = 0; memoryType == memoryTypeNotFound && i < maxMemoryTypeCount; ++i ){
			if( memoryRequirements.memoryTypeBits & indexToBit(i) ){
				if( (physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & desiredMemoryType) == desiredMemoryType ){
					memoryType = i;
				}
			}
		}
	}

	if( memoryType == memoryTypeNotFound ) throw "Can't find compatible mappable memory for the resource";

	VkMemoryAllocateInfo memoryInfo{
		VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		nullptr, // pNext
		memoryRequirements.size,
		memoryType
	};

	VkDeviceMemory memory;
	VkResult errorCode = vkAllocateMemory( device, &memoryInfo, nullptr, &memory ); RESULT_HANDLER( errorCode, "vkAllocateMemory" );

	bindMemory<resourceType>( device, resource, memory, 0 /*offset*/ );

	return memory;
}

void setMemoryData( VkDevice device, VkDeviceMemory memory, void* begin, size_t size ){
	void* data;
	VkResult errorCode = vkMapMemory( device, memory, 0 /*offset*/, VK_WHOLE_SIZE, 0 /*flags - reserved*/, &data ); 
	RESULT_HANDLER( errorCode, "vkMapMemory" );
	memcpy( data, begin, size );
	vkUnmapMemory( device, memory );
}

void killMemory( VkDevice device, VkDeviceMemory memory ){
	vkFreeMemory( device, memory, nullptr );
}


VkBuffer initBuffer( VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage ){
	VkBufferCreateInfo bufferInfo{
		VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		size,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0, // queue family count -- ignored for EXCLUSIVE
		nullptr // queue families -- ignored for EXCLUSIVE
	};

	VkBuffer buffer;
	VkResult errorCode = vkCreateBuffer( device, &bufferInfo, nullptr, &buffer ); RESULT_HANDLER( errorCode, "vkCreateBuffer" );
	return buffer;
}

void killBuffer( VkDevice device, VkBuffer buffer ){
	vkDestroyBuffer( device, buffer, nullptr );
}

VkImage initImage( VkDevice device, VkFormat format, uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImageUsageFlags usage ){
	VkExtent3D size{
		width,
		height,
		1 // depth
	};

	VkImageCreateInfo ici{
		VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		VK_IMAGE_TYPE_2D,
		format,
		size,
		1, // mipLevels
		1, // arrayLayers
		samples,
		VK_IMAGE_TILING_OPTIMAL,
		usage,
		VK_SHARING_MODE_EXCLUSIVE,
		0, // queueFamilyIndexCount -- ignored for EXCLUSIVE
		nullptr, // pQueueFamilyIndices -- ignored for EXCLUSIVE
		VK_IMAGE_LAYOUT_UNDEFINED
	};

	VkImage image;
	VkResult errorCode = vkCreateImage( device, &ici, nullptr, &image ); RESULT_HANDLER( errorCode, "vkCreateImage" );

	return image;
}

void killImage( VkDevice device, VkImage image ){
	vkDestroyImage( device, image, nullptr );
}

VkImageView initImageView( VkDevice device, VkImage image, VkFormat format ){
	VkImageViewCreateInfo iciv{
		VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		image,
		VK_IMAGE_VIEW_TYPE_2D,
		format,
		{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
		{
			VK_IMAGE_ASPECT_COLOR_BIT,
			0, // base mip-level
			VK_REMAINING_MIP_LEVELS, // level count
			0, // base array layer
			VK_REMAINING_ARRAY_LAYERS // array layer count
		}
	};

	VkImageView imageView;
	VkResult errorCode = vkCreateImageView( device, &iciv, nullptr, &imageView ); RESULT_HANDLER( errorCode, "vkCreateImageView" );

	return imageView;
}

void killImageView( VkDevice device, VkImageView imageView ){
	vkDestroyImageView( device, imageView, nullptr );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// initSurface is platform dependent

void killSurface( VkInstance instance, VkSurfaceKHR surface ){
	vkDestroySurfaceKHR( instance, surface, nullptr );
}

VkSurfaceFormatKHR getSurfaceFormat( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface ){
	const VkFormat preferredFormat1 = VK_FORMAT_B8G8R8A8_UNORM; 
	const VkFormat preferredFormat2 = VK_FORMAT_B8G8R8A8_SRGB;

	vector<VkSurfaceFormatKHR> formats = enumerate<VkSurfaceFormatKHR>( physicalDevice, surface );

	if( formats.empty() ) throw "No surface formats offered by Vulkan!";

	if( formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED ){
		formats[0].format = preferredFormat1;
	}

	VkSurfaceFormatKHR chosenFormat1 = {VK_FORMAT_UNDEFINED};
	VkSurfaceFormatKHR chosenFormat2 = {VK_FORMAT_UNDEFINED};

	for( auto f : formats ){
		if( f.format == preferredFormat1 ){
			chosenFormat1 = f;
			break;
		}

		if( f.format == preferredFormat2 ){
			chosenFormat2 = f;
		}
	}

	if( chosenFormat1.format ) return chosenFormat1;
	else if( chosenFormat2.format ) return chosenFormat2;
	else return formats[0];
}

VkSurfaceCapabilitiesKHR getSurfaceCapabilities( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface ){
	VkSurfaceCapabilitiesKHR capabilities;
	VkResult errorCode = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( physicalDevice, surface, &capabilities ); RESULT_HANDLER( errorCode, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR" );

	return capabilities;
}

int selectedMode = 0;

TODO( "Could use debug_report instead of log" )
VkPresentModeKHR getSurfacePresentMode( VkPhysicalDevice physicalDevice, VkSurfaceKHR surface ){
	vector<VkPresentModeKHR> modes = enumerate<VkPresentModeKHR>( physicalDevice, surface );

	for( auto m : modes ){
		if( m == ::presentMode ){
			if( selectedMode != 0 ){
				logger << "INFO: Your preferred present mode became supported. Switching to it.\n";
			}

			selectedMode = 0;
			return m;
		}
	}

	for( auto m : modes ){
		if( m == VK_PRESENT_MODE_FIFO_KHR ){
			if( selectedMode != 1 ){
				logger << "WARNING: Your preferred present mode is not supported. Switching to VK_PRESENT_MODE_FIFO_KHR.\n";
			}

			selectedMode = 1;
			return m;
		}
	}

	TODO( "Workaround for bad (Intel Linux Mesa) drivers" )
	if( modes.empty() ) throw "Bugged driver reports no supported present modes.";
	else{
		if( selectedMode != 2 ){
			logger << "WARNING: Bugged drivers. VK_PRESENT_MODE_FIFO_KHR not supported. Switching to whatever is.\n";
		}

		selectedMode = 2;
		return modes[0];
	}
}

VkSwapchainKHR initSwapchain(
	VkPhysicalDevice physicalDevice,
	VkDevice device,
	VkSurfaceKHR surface,
	VkSurfaceFormatKHR surfaceFormat,
	VkSurfaceCapabilitiesKHR capabilities,
	uint32_t graphicsQueueFamily,
	uint32_t presentQueueFamily,
	VkSwapchainKHR oldSwapchain
){
	// we don't care as we are always setting alpha to 1.0
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlag;
	if( capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR ) compositeAlphaFlag = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	else if( capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR ) compositeAlphaFlag = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	else if( capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR ) compositeAlphaFlag = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
	else if( capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR ) compositeAlphaFlag = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	else throw "Unknown composite alpha reported.";

	// minImageCount + 1 seems a sensible default. It means 2 images should always be readily available without blocking. May lead to memory waste though if we care about that.
	uint32_t myMinImageCount = capabilities.minImageCount + 1;
	if( capabilities.maxImageCount ) myMinImageCount = std::min<uint32_t>( myMinImageCount, capabilities.maxImageCount );

	std::vector<uint32_t> queueFamilies = { graphicsQueueFamily };
	if(graphicsQueueFamily != presentQueueFamily) queueFamilies.push_back( presentQueueFamily );

	VkSwapchainCreateInfoKHR swapchainInfo{
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr, // pNext for extensions use
		0, // flags - reserved for future use
		surface,
		myMinImageCount, // minImageCount
		surfaceFormat.format,
		surfaceFormat.colorSpace,
		capabilities.currentExtent,
		1,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VkImage usage flags
		// It should be fine to just use CONCURRENT in the off chance we encounter the elusive GPU with separate present queue
		queueFamilies.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		static_cast<uint32_t>( queueFamilies.size() ),
		queueFamilies.data(),
		capabilities.currentTransform,
		compositeAlphaFlag,
		getSurfacePresentMode( physicalDevice, surface ),
		VK_TRUE, // clipped
		oldSwapchain
	};

	VkSwapchainKHR swapchain;
	VkResult errorCode = vkCreateSwapchainKHR( device, &swapchainInfo, nullptr, &swapchain ); RESULT_HANDLER( errorCode, "vkCreateSwapchainKHR" );

	return swapchain;
}

void killSwapchain( VkDevice device, VkSwapchainKHR swapchain ){
	vkDestroySwapchainKHR( device, swapchain, nullptr );
}

uint32_t getNextImageIndex( VkDevice device, VkSwapchainKHR swapchain, VkSemaphore imageReadyS ){
	uint32_t nextImageIndex;
	VkResult errorCode = vkAcquireNextImageKHR(
		device,
		swapchain,
		UINT64_MAX /* no timeout */,
		imageReadyS,
		VK_NULL_HANDLE,
		&nextImageIndex
	); RESULT_HANDLER( errorCode, "vkAcquireNextImageKHR" );

	return nextImageIndex;
}

vector<VkImageView> initSwapchainImageViews( VkDevice device, vector<VkImage> images, VkFormat format ){
	vector<VkImageView> imageViews;

	for( auto image : images ){
		VkImageView imageView = initImageView( device, image, format );

		imageViews.push_back( imageView );
	}

	return imageViews;
}

void killSwapchainImageViews( VkDevice device, vector<VkImageView>& imageViews ){
	for( auto imageView : imageViews ) vkDestroyImageView( device, imageView, nullptr );
	imageViews.clear();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

VkRenderPass initRenderPass( VkDevice device, VkSurfaceFormatKHR surfaceFormat ){
	VkAttachmentDescription colorAtachment{
		0, // flags
		surfaceFormat.format,
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR, // color + depth
		VK_ATTACHMENT_STORE_OP_STORE, // color + depth
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, // stencil
		VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencil
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	VkAttachmentReference colorReference{
		0, // attachment
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	};

	VkSubpassDescription subpass{
		0, // flags - reserved for future use
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, // input attachment count
		nullptr, // input attachments
		1, // color attachment count
		&colorReference, // color attachments
		nullptr, // resolve attachments
		nullptr, // depth stencil attachment
		0, // preserve attachment count
		nullptr // preserve attachments
	};

	VkSubpassDependency srcDependency{
		VK_SUBPASS_EXTERNAL, // srcSubpass
		0, // dstSubpass
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
		0, // srcAccessMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // dstAccessMask
		VK_DEPENDENCY_BY_REGION_BIT, // dependencyFlags
	};

	// implicitly defined dependency would cover this, but let's replace it with this explicitly defined dependency!
	VkSubpassDependency dstDependency{
		0, // srcSubpass
		VK_SUBPASS_EXTERNAL, // dstSubpass
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // srcAccessMask
		0, // dstAccessMask
		VK_DEPENDENCY_BY_REGION_BIT, // dependencyFlags
	};

	VkSubpassDependency dependencies[] = {srcDependency, dstDependency};

	VkRenderPassCreateInfo renderPassInfo{
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		1, // attachment count
		&colorAtachment, // attachments
		1, // subpass count
		&subpass, // subpasses
		2, // dependency count
		dependencies // dependencies
	};

	VkRenderPass renderPass;
	VkResult errorCode = vkCreateRenderPass( device, &renderPassInfo, nullptr, &renderPass ); RESULT_HANDLER( errorCode, "vkCreateRenderPass" );

	return renderPass;
}

void killRenderPass( VkDevice device, VkRenderPass renderPass ){
	vkDestroyRenderPass( device, renderPass, nullptr );
}

vector<VkFramebuffer> initFramebuffers(
	VkDevice device,
	VkRenderPass renderPass,
	vector<VkImageView> imageViews,
	uint32_t width, uint32_t height
){
	vector<VkFramebuffer> framebuffers;

	for( auto imageView : imageViews ){
		VkFramebufferCreateInfo framebufferInfo{
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr, // pNext
			0, // flags - reserved for future use
			renderPass,
			1, // ImageView count
			&imageView,
			width, // width
			height, // height
			1 // layers
		};

		VkFramebuffer framebuffer;
		VkResult errorCode = vkCreateFramebuffer( device, &framebufferInfo, nullptr, &framebuffer ); RESULT_HANDLER( errorCode, "vkCreateFramebuffer" );
		framebuffers.push_back( framebuffer );
	}

	return framebuffers;
}

void killFramebuffers( VkDevice device, vector<VkFramebuffer>& framebuffers ){
	for( auto framebuffer : framebuffers ) vkDestroyFramebuffer( device, framebuffer, nullptr );
	framebuffers.clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename Type = uint8_t>
vector<Type> loadBinaryFile( string filename ){
	using std::ifstream;
	using std::istreambuf_iterator;

	vector<Type> data;

	try{
		ifstream ifs;
		ifs.exceptions( ifs.failbit | ifs.badbit | ifs.eofbit );
		ifs.open( filename, ifs.in | ifs.binary | ifs.ate );

		const auto fileSize = static_cast<size_t>( ifs.tellg() );

		if( fileSize > 0 && (fileSize % sizeof(Type) == 0) ){
			ifs.seekg( ifs.beg );
			data.resize( fileSize / sizeof(Type) );
			ifs.read( reinterpret_cast<char*>(data.data()), fileSize );
		}
	}
	catch( ... ){
		data.clear();
	}

	return data;
}

void killShaderModule( VkDevice device, VkShaderModule shaderModule ){
	vkDestroyShaderModule( device, shaderModule, nullptr );
}

VkPipelineLayout initPipelineLayout(
	VkDevice device,
	VkDescriptorSetLayout descriptorSetLayout
){
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		0, // descriptorSetLayout count
		nullptr,
		0, // push constant range count
		nullptr // push constant ranges
	};

	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	VkPipelineLayout pipelineLayout;
	VkResult errorCode = vkCreatePipelineLayout( device, &pipelineLayoutInfo, nullptr, &pipelineLayout ); 
	RESULT_HANDLER( errorCode, "vkCreatePipelineLayout" );

	return pipelineLayout;
}

void killPipelineLayout( VkDevice device, VkPipelineLayout pipelineLayout ){
	vkDestroyPipelineLayout( device, pipelineLayout, nullptr );
}

VkPipeline initPipeline(
	VkDevice device,
	VkPhysicalDeviceLimits limits,
	VkPipelineLayout pipelineLayout,
	VkRenderPass renderPass,
	VkShaderModule vertexShader,
	VkShaderModule fragmentShader,
	const uint32_t vertexBufferBinding,
	uint32_t width, uint32_t height
){
	VkPipelineShaderStageCreateInfo shaderStageStates[] = { 
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr, // pNext
			0, // flags - reserved for future use
			VK_SHADER_STAGE_VERTEX_BIT,
			vertexShader,
			u8"main",
			nullptr // SpecializationInfo - constants pushed to shader on pipeline creation time
		}, 
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr, // pNext
			0, // flags - reserved for future use
			VK_SHADER_STAGE_FRAGMENT_BIT,
			fragmentShader,
			u8"main",
			nullptr // SpecializationInfo - constants pushed to shader on pipeline creation time
		}
	};

	const uint32_t vertexBufferStride = sizeof( Vertex3D_UV );
	if( vertexBufferBinding > limits.maxVertexInputBindings ){
		throw string("Implementation does not allow enough input bindings. Needed: ")
		    + to_string( vertexBufferBinding ) + string(", max: ")
		    + to_string( limits.maxVertexInputBindings );
	}
	if( vertexBufferStride > limits.maxVertexInputBindingStride ){
		throw string("Implementation does not allow big enough vertex buffer stride: ")
		    + to_string( vertexBufferStride ) 
		    + string(", max: ")
		    + to_string( limits.maxVertexInputBindingStride );
	}

	VkVertexInputBindingDescription vertexInputBindingDescription{
		vertexBufferBinding,
		sizeof( Vertex3D_UV ), // stride in bytes
		VK_VERTEX_INPUT_RATE_VERTEX
	};

	vector<VkVertexInputBindingDescription> inputBindingDescriptions = { vertexInputBindingDescription };
	if( inputBindingDescriptions.size() > limits.maxVertexInputBindings ){
		throw "Implementation does not allow enough input bindings.";
	}

	const uint32_t positionLocation = 0;
	const uint32_t colorLocation = 1;

	if( colorLocation >= limits.maxVertexInputAttributes ){
		throw "Implementation does not allow enough input attributes.";
	}
	if(offsetof( Vertex3D_UV, uv) > limits.maxVertexInputAttributeOffset ){
		throw "Implementation does not allow sufficient attribute offset.";
	}

	VkVertexInputAttributeDescription positionInputAttributeDescription{
		positionLocation,
		vertexBufferBinding,
		VK_FORMAT_R32G32B32_SFLOAT,
		offsetof(Vertex3D_UV, position)
	};

	VkVertexInputAttributeDescription colorInputAttributeDescription{
		colorLocation,
		vertexBufferBinding,
		VK_FORMAT_R32G32B32_SFLOAT,
		offsetof(Vertex3D_UV, uv)
	};

	vector<VkVertexInputAttributeDescription> inputAttributeDescriptions = {
		positionInputAttributeDescription,
		colorInputAttributeDescription
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState{
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		static_cast<uint32_t>( inputBindingDescriptions.size() ),
		inputBindingDescriptions.data(),
		static_cast<uint32_t>( inputAttributeDescriptions.size() ),
		inputAttributeDescriptions.data()
	};

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		VK_FALSE // primitive restart
	};

	VkViewport viewport{
		0.0f, // x
		0.0f, // y
		static_cast<float>( width ? width : 1 ),
		static_cast<float>( height ? height : 1 ),
		0.0f, // min depth
		1.0f // max depth
	};

	VkRect2D scissor{
		{0, 0}, // offset
		{width, height}
	};

	VkPipelineViewportStateCreateInfo viewportState{
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		1, // Viewport count
		&viewport,
		1, // scisor count,
		&scissor
	};

	VkPipelineRasterizationStateCreateInfo rasterizationState{
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		VK_FALSE, // depth clamp
		VK_FALSE, // rasterizer discard
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_BACK_BIT,
		VK_FRONT_FACE_COUNTER_CLOCKWISE,
		VK_FALSE, // depth bias
		0.0f, // bias constant factor
		0.0f, // bias clamp
		0.0f, // bias slope factor
		1.0f // line width
	};

	VkPipelineMultisampleStateCreateInfo multisampleState{
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		VK_SAMPLE_COUNT_1_BIT,
		VK_FALSE, // no sample shading
		0.0f, // min sample shading - ignored if disabled
		nullptr, // sample mask
		VK_FALSE, // alphaToCoverage
		VK_FALSE // alphaToOne
	};

	VkPipelineColorBlendAttachmentState blendAttachmentState{
		VK_FALSE, // blending enabled?
		VK_BLEND_FACTOR_ZERO, // src blend factor -ignored?
		VK_BLEND_FACTOR_ZERO, // dst blend factor
		VK_BLEND_OP_ADD, // blend op
		VK_BLEND_FACTOR_ZERO, // src alpha blend factor
		VK_BLEND_FACTOR_ZERO, // dst alpha blend factor
		VK_BLEND_OP_ADD, // alpha blend op
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT // color write mask
	};

	VkPipelineColorBlendStateCreateInfo colorBlendState{
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - reserved for future use
		VK_FALSE, // logic ops
		VK_LOGIC_OP_COPY,
		1, // attachment count - must be same as color attachment count in renderpass subpass!
		&blendAttachmentState,
		{0.0f, 0.0f, 0.0f, 0.0f} // blend constants
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr, // pNext
		0, // flags - e.g. disable optimization
		2, // shader stages count - vertex and fragment
		shaderStageStates,
		&vertexInputState,
		&inputAssemblyState,
		nullptr, // tesselation
		&viewportState,
		&rasterizationState,
		&multisampleState,
		nullptr, // depth stencil
		&colorBlendState,
		nullptr, // dynamic state
		pipelineLayout,
		renderPass,
		0, // subpass index in renderpass
		VK_NULL_HANDLE, // base pipeline
		-1 // base pipeline index
	};

	VkPipeline pipeline;
	VkResult errorCode = vkCreateGraphicsPipelines(
		device,
		VK_NULL_HANDLE /* pipeline cache */,
		1 /* info count */,
		&pipelineInfo,
		nullptr,
		&pipeline
	); RESULT_HANDLER( errorCode, "vkCreateGraphicsPipelines" );
	return pipeline;
}

void killPipeline( VkDevice device, VkPipeline pipeline ){
	vkDestroyPipeline( device, pipeline, nullptr );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setVertexData( VkDevice device, VkDeviceMemory memory, vector<Vertex3D_UV> vertices ){
	TODO( "Should be in Device Local memory instead" )
	setMemoryData(  device, memory, vertices.data(), sizeof( decltype(vertices)::value_type ) * vertices.size()  );
}

VkSemaphore initSemaphore( VkDevice device ){
	const VkSemaphoreCreateInfo semaphoreInfo{
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		nullptr, // pNext
		0 // flags - reserved for future use
	};

	VkSemaphore semaphore;
	VkResult errorCode = vkCreateSemaphore( device, &semaphoreInfo, nullptr, &semaphore ); RESULT_HANDLER( errorCode, "vkCreateSemaphore" );
	return semaphore;
}

vector<VkSemaphore> initSemaphores( VkDevice device, size_t count ){
	vector<VkSemaphore> semaphores;
	std::generate_n(  std::back_inserter( semaphores ), count, [device]{ return initSemaphore( device ); }  );
	return semaphores;
}

void killSemaphore( VkDevice device, VkSemaphore semaphore ){
	vkDestroySemaphore( device, semaphore, nullptr );
}

void killSemaphores( VkDevice device, vector<VkSemaphore>& semaphores ){
	for( const auto s : semaphores ) killSemaphore( device, s );
	semaphores.clear();
}

VkCommandPool initCommandPool( VkDevice device, const uint32_t queueFamily ){
	const VkCommandPoolCreateInfo commandPoolInfo{
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr, // pNext
		0, // flags
		queueFamily
	};

	VkCommandPool commandPool;
	VkResult errorCode = vkCreateCommandPool( device, &commandPoolInfo, nullptr, &commandPool ); RESULT_HANDLER( errorCode, "vkCreateCommandPool" );
	return commandPool;
}

void killCommandPool( VkDevice device, VkCommandPool commandPool ){
	vkDestroyCommandPool( device, commandPool, nullptr );
}

VkFence initFence( const VkDevice device, const VkFenceCreateFlags flags = 0 ){
	const VkFenceCreateInfo fci{
		VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		nullptr, // pNext
		flags
	};

	VkFence fence;
	VkResult errorCode = vkCreateFence( device, &fci, nullptr, &fence ); RESULT_HANDLER( errorCode, "vkCreateFence" );
	return fence;
}

void killFence( const VkDevice device, const VkFence fence ){
	vkDestroyFence( device, fence, nullptr );
}

vector<VkFence> initFences( const VkDevice device, const size_t count, const VkFenceCreateFlags flags ){
	vector<VkFence> fences;
	std::generate_n(  std::back_inserter( fences ), count, [=]{return initFence( device, flags );}  );
	return fences;
}

void killFences( const VkDevice device, vector<VkFence>& fences ){
	for( const auto f : fences ) killFence( device, f );
	fences.clear();
}

void acquireCommandBuffers( VkDevice device, VkCommandPool commandPool, uint32_t count, vector<VkCommandBuffer>& commandBuffers ){
	const auto oldSize = static_cast<uint32_t>( commandBuffers.size() );

	if( count > oldSize ){
		VkCommandBufferAllocateInfo commandBufferInfo{
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			nullptr, // pNext
			commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			count - oldSize // count
		};

		commandBuffers.resize( count );
		VkResult errorCode = vkAllocateCommandBuffers( device, &commandBufferInfo, &commandBuffers[oldSize] ); RESULT_HANDLER( errorCode, "vkAllocateCommandBuffers" );
	}

	if( count < oldSize ) {
		vkFreeCommandBuffers( device, commandPool, oldSize - count, &commandBuffers[count] );
		commandBuffers.resize( count );
	}
}

void beginCommandBuffer( VkCommandBuffer commandBuffer ){
	VkCommandBufferBeginInfo commandBufferInfo{
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr, // pNext
		// same buffer can be re-executed before it finishes from last submit
		VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // flags
		nullptr // inheritance
	};

	VkResult errorCode = vkBeginCommandBuffer( commandBuffer, &commandBufferInfo ); RESULT_HANDLER( errorCode, "vkBeginCommandBuffer" );
}

void endCommandBuffer( VkCommandBuffer commandBuffer ){
	VkResult errorCode = vkEndCommandBuffer( commandBuffer ); RESULT_HANDLER( errorCode, "vkEndCommandBuffer" );
}


void recordBeginRenderPass(
	VkCommandBuffer commandBuffer,
	VkRenderPass renderPass,
	VkFramebuffer framebuffer,
	VkClearValue clearValue,
	uint32_t width, uint32_t height
){
	VkRenderPassBeginInfo renderPassInfo{
		VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		nullptr, // pNext
		renderPass,
		framebuffer,
		{{0,0}, {width,height}}, //render area - offset plus extent
		1, // clear value count
		&clearValue
	};

	vkCmdBeginRenderPass( commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );
}

void recordEndRenderPass( VkCommandBuffer commandBuffer ){
	vkCmdEndRenderPass( commandBuffer );
}

void recordBindPipeline( VkCommandBuffer commandBuffer, VkPipeline pipeline ){
	vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
}

void recordBindVertexBuffer( VkCommandBuffer commandBuffer, const uint32_t vertexBufferBinding, VkBuffer vertexBuffer ){
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers( commandBuffer, vertexBufferBinding, 1 /*binding count*/, &vertexBuffer, offsets );
}

void recordDraw( VkCommandBuffer commandBuffer, const uint32_t vertexCount ){
	vkCmdDraw( commandBuffer, vertexCount, 1 /*instance count*/, 0 /*first vertex*/, 0 /*first instance*/ );
}

void submitToQueue( VkQueue queue, VkCommandBuffer commandBuffer, VkSemaphore imageReadyS, VkSemaphore renderDoneS, VkFence fence ){
	const VkPipelineStageFlags psw = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	const VkSubmitInfo submit{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr, // pNext
		1, &imageReadyS, // wait semaphores
		&psw, // pipeline stages to wait for semaphore
		1, &commandBuffer,
		1, &renderDoneS // signal semaphores
	};

	const VkResult errorCode = vkQueueSubmit( queue, 1 /*submit count*/, &submit, fence ); RESULT_HANDLER( errorCode, "vkQueueSubmit" );
}

void present( VkQueue queue, VkSwapchainKHR swapchain, uint32_t swapchainImageIndex, VkSemaphore renderDoneS ){
	const VkPresentInfoKHR presentInfo{
		VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		nullptr, // pNext
		1, &renderDoneS, // wait semaphores
		1, &swapchain, &swapchainImageIndex,
		nullptr // pResults
	};

	const VkResult errorCode = vkQueuePresentKHR( queue, &presentInfo ); RESULT_HANDLER( errorCode, "vkQueuePresentKHR" );
}

// cleanup dangerous semaphore with signal pending from vkAcquireNextImageKHR (tie it to a specific queue)
// https://github.com/KhronosGroup/Vulkan-Docs/issues/1059
void cleanupUnsafeSemaphore( VkQueue queue, VkSemaphore semaphore ){
	VkPipelineStageFlags psw = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	const VkSubmitInfo submit{
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr, // pNext
		1, &semaphore, // wait semaphores
		&psw, // pipeline stages to wait for semaphore
		0, nullptr, // command buffers
		0, nullptr // signal semaphores
	};

	const VkResult errorCode = vkQueueSubmit( queue, 1 /*submit count*/, &submit, VK_NULL_HANDLE ); RESULT_HANDLER( errorCode, "vkQueueSubmit" );
}
