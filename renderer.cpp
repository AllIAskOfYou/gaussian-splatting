#include "renderer.hpp"

void Renderer::init(GLFWwindow* window) {
    this->window = window;
    glfwGetWindowSize(window, &width, &height);

    // Create a WebGPU instance
    Instance instance = wgpuCreateInstance(nullptr);

    // Create a WebGPU surface
    surface = glfwGetWGPUSurface(instance, window);
    if (!surface) {
        std::cerr << "Failed to create WebGPU surface." << std::endl;
        return;
    }
    std::cout << "Got WebGPU surface." << std::endl;

    // Request an adapter
    RequestAdapterOptions adapter_options = {};
    adapter_options.compatibleSurface = surface;
    adapter_options.powerPreference = PowerPreference::HighPerformance;
    Adapter adapter = instance.requestAdapter(adapter_options);
    if (!adapter) {
        std::cerr << "Failed to get WebGPU adapter." << std::endl;
        return;
    }
    std::cout << "Got WebGPU adapter." << std::endl;

    // Release the instance
    instance.release();

    // Request a device
    DeviceDescriptor device_desc = {};
    device_desc.label = "Default Device";
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.label = "Default Queue";
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.deviceLostCallback = [] (
            WGPUDeviceLostReason reason, const char* message, void*) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };

    // Get the required limits
    RequiredLimits required_limits = get_required_limits(adapter);
    device_desc.requiredLimits = &required_limits;

    // Request the device
    device = adapter.requestDevice(device_desc);
    if (!device) {
        std::cerr << "Failed to get WebGPU device." << std::endl;
        return;
    }
    std::cout << "Got WebGPU device." << std::endl;

    // Set the uncaptured error callback
    uncaptured_error_callback_handle = device.setUncapturedErrorCallback(
        [](ErrorType type, const char* message) {
            std::cout << "Uncaptured device error: type " << type;
            if (message) std::cout << " (" << message << ")";
            std::cout << std::endl;
        });

    // Get the queue
    queue = device.getQueue();
    if (!queue) {
        std::cerr << "Failed to get WebGPU queue." << std::endl;
        return;
    }
    std::cout << "Got WebGPU queue." << std::endl;

    // Get the surface capabilities
    SurfaceCapabilities surface_capabilities;
    surface.getCapabilities(adapter, &surface_capabilities);

    // Configure the surface
    surface_format = surface_capabilities.formats[0];

    SurfaceConfiguration surface_config = {};
    surface_config.width = width;
    surface_config.height = height;
    surface_config.usage = TextureUsage::RenderAttachment;
    surface_config.format = surface_format;

    // view format
    surface_config.viewFormatCount = 0;
    surface_config.viewFormats = nullptr;
    surface_config.device = device;
    surface_config.presentMode = PresentMode::Immediate;
    surface_config.alphaMode = CompositeAlphaMode::Auto;

    surface.configure(surface_config);

    std::cout << "Configured WebGPU surface." << std::endl;

    // Release the adapter
    adapter.release();

}

RequiredLimits Renderer::get_required_limits(Adapter &adapter) {
    // Get the adapter's supported limits
    SupportedLimits supported_limits;
    adapter.getLimits(&supported_limits);
    // Set the required limits
    RequiredLimits required_limits = Default;
    required_limits.limits.maxVertexAttributes = 10;
    required_limits.limits.maxVertexBuffers = 4;
    required_limits.limits.maxInterStageShaderComponents = 10;
    required_limits.limits.minUniformBufferOffsetAlignment =
        supported_limits.limits.minUniformBufferOffsetAlignment;
    required_limits.limits.minStorageBufferOffsetAlignment =
        supported_limits.limits.minStorageBufferOffsetAlignment;

    required_limits.limits.maxBufferSize = 
        supported_limits.limits.maxBufferSize;
    required_limits.limits.maxStorageBufferBindingSize =
        supported_limits.limits.maxStorageBufferBindingSize;
    return required_limits;
}

TextureView Renderer::get_next_surface_texture_view() {
    // Get the surface texture
    SurfaceTexture surface_texture;
    surface.getCurrentTexture(&surface_texture);
    if (surface_texture.status != SurfaceGetCurrentTextureStatus::Success) {
        return nullptr;
    }
    Texture texture = surface_texture.texture;

    // Create a view for this surface texture
    TextureViewDescriptor view_desc = {};
    view_desc.label = "Surface texture view";
    view_desc.format = texture.getFormat();
    view_desc.dimension = TextureViewDimension::Undefined;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = TextureAspect::All;
    TextureView view = texture.createView(view_desc);

#ifndef WEBGPU_BACKEND_WGPU
    // We no longer need the texture, only its view
    // (NB: with wgpu-native, surface textures must not be manually released)
    texture.release();
#endif // WEBGPU_BACKEND_WGPU

    return view;
}

RenderPassEncoder Renderer::create_render_pass_encoder(
        TextureView &targetView, CommandEncoder &encoder) {
	// Create the render pass that clears the screen with our color
	RenderPassDescriptor renderPassDesc = {};

	// The attachment part of the render pass descriptor describes the target texture of the pass
	RenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 0.05, 0.05, 0.05, 1.0 };
	#ifndef WEBGPU_BACKEND_WGPU
		renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
	#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	return renderPass;
}

Renderer::~Renderer() {
    surface.unconfigure();
    queue.release();
    surface.release();
    device.release();
}