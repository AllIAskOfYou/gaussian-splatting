// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/io.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include "ResourceManager.h"

#include "Node.h"
#include "Camera.h"
#include "OrbitCamera.h"

#include "SplatMesh.h"

using namespace wgpu;

class Application {
public:
	Application() = default;
	// Initialize everything and return true if it went all right
	bool Initialize();

	// Uninitialize everything that was initialized
	void Terminate();

	// Draw a frame and handle events
	void MainLoop();

	// Return true as long as the main loop should keep on running
	bool IsRunning();

private:
	struct Uniforms {
		glm::mat4x4 projectionMatrix;
		glm::mat4x4 viewMatrix;
		glm::mat4x4 modelMatrix;
		glm::vec3 _pad;
		float splatSize=0.03;//4.0;
	};

	struct Quad {
		glm::vec2 position;
		glm::vec2 uv;
	};

private:
	TextureView GetNextSurfaceTextureView();

	// Substep of Initialize() that creates the render pipeline
	void InitializePipeline();
	RequiredLimits GetRequiredLimits(Adapter adapter) const;
	void InitializeBuffers();
	void InitializeBindGroups();
	void InitializeScene();
	void UpdateScene();
	RenderPassEncoder createRenderPassEncoder(TextureView &targetView, CommandEncoder &encoder);

// Event handlers for interaction
private:
	void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, int mods);
    void onScroll(double xoffset, double yoffset);

private:
    bool initGui(); // called in onInit
    void terminateGui(); // called in onFinish
    void updateGui(wgpu::RenderPassEncoder renderPass); // called in onFrame

	void guiAddSliderParameter(const char* label, float* value, float min, float max);

private:
	// We put here all the variables that are shared between init and main loop
	GLFWwindow *m_window;
	Device device;
	Queue queue;
	Surface surface;
	std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle;
	TextureFormat surfaceFormat = TextureFormat::Undefined;
	RenderPipeline pipeline;


	// Uniform buffer
	BindGroup bindGroup;
	PipelineLayout layout;
	BindGroupLayout bindGroupLayout;

	// transform buffer
	Buffer transformBuffer;
	
	std::shared_ptr<Node> scene = std::make_shared<Node>();
	std::shared_ptr<OrbitCamera> orbitCamera = std::make_shared<OrbitCamera>(5.0);
	std::shared_ptr<Camera> camera = std::make_shared<Camera>();
	std::shared_ptr<Node> splatNode = std::make_shared<Node>();

	SplatMesh splatMesh;

	Uniforms uniforms;

	ImGuiIO imGuiIo;


	// time
	//double time;
	double deltaTime;
};

int main() {
	Application app;

	if (!app.Initialize()) {
		return 1;
	}

#ifdef __EMSCRIPTEN__
	// Equivalent of the main loop when using Emscripten:
	auto callback = [](void *arg) {
		Application* pApp = reinterpret_cast<Application*>(arg);
		pApp->MainLoop(); // 4. We can use the application object
	};
	emscripten_set_main_loop_arg(callback, &app, 0, true);
#else // __EMSCRIPTEN__

	
	int a = 0;
	while (app.IsRunning()) {
		a += 1;
		app.MainLoop();
	}
#endif // __EMSCRIPTEN__

	app.Terminate();

	return 0;
}

void Application::onMouseMove(double xpos, double ypos) {
	orbitCamera->onMouseMove(xpos, ypos, deltaTime);
}

void Application::onMouseButton(int button, int action, int mods) {
    if (imGuiIo.WantCaptureMouse) {
        // Don't rotate the camera if the mouse is already captured by an ImGui
        // interaction at this frame.
        return;
    }
	orbitCamera->onMouseButton(button, action, mods);
}

void Application::onScroll(double xoffset, double yoffset) {
	orbitCamera->onScroll(xoffset, yoffset, deltaTime);
}

bool Application::Initialize() {
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(640, 640, "Learn WebGPU", nullptr, nullptr);

	glfwSetWindowUserPointer(m_window, this);

	// Add window callbacks
	glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos);
	});
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods);
	});
	glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset) {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset);
	});
	
	Instance instance = wgpuCreateInstance(nullptr);
	
	// Get adapter
	std::cout << "Requesting adapter..." << std::endl;
	surface = glfwGetWGPUSurface(instance, m_window);
	RequestAdapterOptions adapterOpts = {};
	adapterOpts.compatibleSurface = surface;
	adapterOpts.powerPreference = PowerPreference::HighPerformance;

	Adapter adapter = instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << adapter << std::endl;
	
	instance.release();
	
	std::cout << "Requesting device..." << std::endl;
	DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	// Before adapter.requestDevice(deviceDesc)
	RequiredLimits requiredLimits = GetRequiredLimits(adapter);
	deviceDesc.requiredLimits = &requiredLimits;

	//std::vector<NativeFeature> requiredFeatures = {
	//	NativeFeature::VertexWritableStorage,
	//};
	//deviceDesc.requiredFeatureCount = static_cast<uint32_t>(requiredFeatures.size());
	//deviceDesc.requiredFeatures = (const WGPUFeatureName*)requiredFeatures.data();


	// Get the number of features supported by the adapter
	uint32_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
	std::vector<WGPUFeatureName> supportedFeatures(featureCount);
	wgpuAdapterEnumerateFeatures(adapter, supportedFeatures.data());


	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;

	// Device error callback
	uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});
	
	queue = device.getQueue();

	wgpu::SurfaceCapabilities surfaceCapabilities;
	surface.getCapabilities(adapter, &surfaceCapabilities);

	// Configure the surface
	SurfaceConfiguration config = {};
	
	// Configuration of the textures created for the underlying swap chain
	config.width = 640;
	config.height = 640;
	config.usage = TextureUsage::RenderAttachment;
	
	surfaceFormat = surfaceCapabilities.formats[0];
	config.format = surfaceFormat;

	// And we do not need any particular view format:
	config.viewFormatCount = 0;
	config.viewFormats = nullptr;
	config.device = device;
	config.presentMode = PresentMode::Fifo;
	config.alphaMode = CompositeAlphaMode::Auto;

	surface.configure(config);

	// Release the adapter only after it has been fully utilized
	adapter.release();

	InitializeBuffers();
	InitializePipeline();
	InitializeBindGroups();
	InitializeScene();

	// Initialize time
	//time = glfwGetTime();

	if (!initGui()) return false;

	return true;
}

void Application::Terminate() {
	pipeline.release();
	surface.unconfigure();
	queue.release();
	surface.release();
	device.release();
	glfwDestroyWindow(m_window);
	glfwTerminate();
	terminateGui();
}

void Application::MainLoop() {
	// Update time
	imGuiIo = ImGui::GetIO();
	deltaTime = imGuiIo.DeltaTime;
	

	//printf("FPS: %f\n", 1.0 / deltaTime);
	glfwPollEvents();

	// Update the scene
	UpdateScene();

	// Create a transform matrix
	uniforms.projectionMatrix = camera->getProjectionMatrix();
	uniforms.viewMatrix = camera->getViewMatrix();
	uniforms.modelMatrix = splatNode->worldMatrix;

	// Upload the transform matrix to the buffer
	queue.writeBuffer(transformBuffer, 0, &uniforms, sizeof(Uniforms));

	// Sort the splats
	glm::vec3 cameraPos = glm::vec3(camera->worldMatrix * glm::vec4(camera->position, 1.0f));
	splatMesh.sortSplats(cameraPos);

	// Get the next target texture view
	TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// Create a command encoder for the draw call
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	RenderPassEncoder renderPass = createRenderPassEncoder(targetView, encoder);

	// Select which render pipeline to use
	renderPass.setPipeline(pipeline);

	// Set vertex buffer while encoding the render pass
	splatMesh.setBuffers(renderPass);

	// Set binding group here!
	renderPass.setBindGroup(0, bindGroup, 0, nullptr);

	// Draw the splat
	renderPass.drawIndexed(6, splatMesh.splatCount, 0, 0, 0);

	updateGui(renderPass);

	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	//std::cout << "Submitting command..." << std::endl;
	queue.submit(1, &command);
	command.release();
	//std::cout << "Command submitted." << std::endl;

	// At the end of the frame
	targetView.release();
#ifndef __EMSCRIPTEN__
	surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	device.poll(false);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(m_window);
}

TextureView Application::GetNextSurfaceTextureView() {
	// Get the surface texture
	SurfaceTexture surfaceTexture;
	surface.getCurrentTexture(&surfaceTexture);
	if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
		return nullptr;
	}
	Texture texture = surfaceTexture.texture;

	// Create a view for this surface texture
	TextureViewDescriptor viewDescriptor;
	viewDescriptor.label = "Surface texture view";
	viewDescriptor.format = texture.getFormat();
	viewDescriptor.dimension = TextureViewDimension::_2D;
	viewDescriptor.baseMipLevel = 0;
	viewDescriptor.mipLevelCount = 1;
	viewDescriptor.baseArrayLayer = 0;
	viewDescriptor.arrayLayerCount = 1;
	viewDescriptor.aspect = TextureAspect::All;
	TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
	// We no longer need the texture, only its view
	// (NB: with wgpu-native, surface textures must not be manually released)
	Texture(surfaceTexture.texture).release();
#endif // WEBGPU_BACKEND_WGPU

	return targetView;
}

RenderPassEncoder Application::createRenderPassEncoder(TextureView &targetView, CommandEncoder &encoder) {
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

/*
void Application::InitializePipeline() {
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	// Check for errors
	if (shaderModule == nullptr) {
		std::cerr << "Could not load shader!" << std::endl;
		exit(1);
	}

	// Create the render pipeline
	RenderPipelineDescriptor pipelineDesc;

	// Configure the vertex pipeline
	// We use one vertex buffer
	VertexBufferLayout vertexBufferLayout;
	// We now have 2 attributes
	std::vector<VertexAttribute> vertexAttribs(4);
	
	// Describe the position attribute
	vertexAttribs[0].shaderLocation = 0; // @location(0)
	vertexAttribs[0].format = VertexFormat::Float32x3;
	vertexAttribs[0].offset = 0;

	// Describe the size attribute
	vertexAttribs[1].shaderLocation = 1; // @location(1)
	vertexAttribs[1].format = VertexFormat::Float32x3; // different type!
	vertexAttribs[1].offset = 3 * sizeof(float); // non null offset!

	
	// Describe the color attribute
	vertexAttribs[2].shaderLocation = 2; // @location(2)
	vertexAttribs[2].format = VertexFormat::Float32x4; // different type!
	vertexAttribs[2].offset = 6 * sizeof(float); // non null offset!

	// Describe the rotation attribute
	vertexAttribs[3].shaderLocation = 3; // @location(2)
	vertexAttribs[3].format = VertexFormat::Float32x4; // different type!
	vertexAttribs[3].offset = 10 * sizeof(float); // non null offset!
	
	vertexBufferLayout.attributeCount = static_cast<uint32_t>(vertexAttribs.size());
	vertexBufferLayout.attributes = vertexAttribs.data();
	
	vertexBufferLayout.arrayStride = sizeof(Splat);
	std::cout << "Splat size: " << sizeof(Splat) << std::endl;
	//                               ^^^^^^^^^^^^^^^^^ The new stride
	vertexBufferLayout.stepMode = VertexStepMode::Vertex;
	
	pipelineDesc.vertex.bufferCount = 1;
	pipelineDesc.vertex.buffers = &vertexBufferLayout;

	// NB: We define the 'shaderModule' in the second part of this chapter.
	// Here we tell that the programmable vertex shader stage is described
	// by the function called 'vs_main' in that module.
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = PrimitiveTopology::PointList;
	
	// We'll see later how to specify the order in which vertices should be
	// connected. When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;
	
	// The face orientation is defined by assuming that when looking
	// from the front of the face, its corner vertices are enumerated
	// in the counter-clockwise (CCW) order.
	
	// pipelineDesc.primitive.frontFace = FrontFace::CCW;
	
	// But the face orientation does not matter much because we do not
	// cull (i.e. "hide") the faces pointing away from us (which is often
	// used for optimization).
	
	// pipelineDesc.primitive.cullMode = CullMode::None;

	// We tell that the programmable fragment shader stage is described
	// by the function called 'fs_main' in the shader module.
	FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	BlendState blendState;
	blendState.color.srcFactor = BlendFactor::SrcAlpha;
	blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = BlendOperation::Add;
	blendState.alpha.srcFactor = BlendFactor::Zero;
	blendState.alpha.dstFactor = BlendFactor::One;
	blendState.alpha.operation = BlendOperation::Add;
	
	ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = ColorWriteMask::All; // We could write to only some of the color channels.
	
	// We have only one target because our render pass has only one output color
	// attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;

	// We do not use stencil/depth testing for now
	pipelineDesc.depthStencil = nullptr;

	// Samples per pixel
	pipelineDesc.multisample.count = 1;

	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;

	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	// Define binding layout (don't forget to = Default)
	BindGroupLayoutEntry bindingLayout = Default;
	// The binding index as used in the @binding attribute in the shader
	bindingLayout.binding = 0;
	// The stage that needs to access this resource
	bindingLayout.visibility = ShaderStage::Vertex;
	bindingLayout.buffer.type = BufferBindingType::Uniform;
	bindingLayout.buffer.minBindingSize = sizeof(Uniforms);

	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = 1;
	bindGroupLayoutDesc.entries = &bindingLayout;
	bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	layout = device.createPipelineLayout(layoutDesc);


	pipelineDesc.layout = layout;
	
	pipeline = device.createRenderPipeline(pipelineDesc);

	// We no longer need to access the shader module
	shaderModule.release();
}
*/

void Application::InitializePipeline() {
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader_quads_ordered.wgsl", device);
	std::cout << "Shader module: " << shaderModule << std::endl;

	// Check for errors
	if (shaderModule == nullptr) {
		std::cerr << "Could not load shader!" << std::endl;
		exit(1);
	}

	std::cout << "Splat size: " << sizeof(Splat) << std::endl;
	// Create the render pipeline
	RenderPipelineDescriptor pipelineDesc;

	pipelineDesc.vertex.bufferCount = splatMesh.vertexBufferLayouts.size();
	pipelineDesc.vertex.buffers = splatMesh.vertexBufferLayouts.data();

	// NB: We define the 'shaderModule' in the second part of this chapter.
	// Here we tell that the programmable vertex shader stage is described
	// by the function called 'vs_main' in that module.
	pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

	// Each sequence of 3 vertices is considered as a triangle
	pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList;
	
	// We'll see later how to specify the order in which vertices should be
	// connected. When not specified, vertices are considered sequentially.
	pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined;

	// We tell that the programmable fragment shader stage is described
	// by the function called 'fs_main' in the shader module.
	FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

	BlendState blendState;
	blendState.color.srcFactor = BlendFactor::SrcAlpha;
	blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
	blendState.color.operation = BlendOperation::Add;
	blendState.alpha.srcFactor = BlendFactor::Zero;
	blendState.alpha.dstFactor = BlendFactor::One;
	blendState.alpha.operation = BlendOperation::Add;
	
	ColorTargetState colorTarget;
	colorTarget.format = surfaceFormat;
	colorTarget.blend = &blendState;
	colorTarget.writeMask = ColorWriteMask::All; // We could write to only some of the color channels.
	
	// We have only one target because our render pass has only one output color
	// attachment.
	fragmentState.targetCount = 1;
	fragmentState.targets = &colorTarget;
	pipelineDesc.fragment = &fragmentState;

	// We do not use stencil/depth testing for now
	pipelineDesc.depthStencil = nullptr;

	// Samples per pixel
	pipelineDesc.multisample.count = 1;

	// Default value for the mask, meaning "all bits on"
	pipelineDesc.multisample.mask = ~0u;

	// Default value as well (irrelevant for count = 1 anyways)
	pipelineDesc.multisample.alphaToCoverageEnabled = false;

	// Define binding layout (don't forget to = Default)
	BindGroupLayoutEntry bindingLayouts[3] = {};
	// The binding index as used in the @binding attribute in the shader
	bindingLayouts[0].binding = 0;
	// The stage that needs to access this resource
	bindingLayouts[0].visibility = ShaderStage::Vertex;
	bindingLayouts[0].buffer.type = BufferBindingType::Uniform;
	bindingLayouts[0].buffer.minBindingSize = sizeof(Uniforms);

	bindingLayouts[1].binding = 1;
	bindingLayouts[1].visibility = ShaderStage::Vertex;
	bindingLayouts[1].buffer.type = BufferBindingType::ReadOnlyStorage;
	bindingLayouts[1].buffer.minBindingSize = splatMesh.splatCount * sizeof(Splat);

	bindingLayouts[2].binding = 2;
	bindingLayouts[2].visibility = ShaderStage::Vertex;
	bindingLayouts[2].buffer.type = BufferBindingType::ReadOnlyStorage;
	bindingLayouts[2].buffer.minBindingSize = splatMesh.splatCount * sizeof(uint32_t);

	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = 3;
	bindGroupLayoutDesc.entries = bindingLayouts;
	bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	layout = device.createPipelineLayout(layoutDesc);


	pipelineDesc.layout = layout;
	
	pipeline = device.createRenderPipeline(pipelineDesc);

	// We no longer need to access the shader module
	shaderModule.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) const {
	// Get adapter supported limits, in case we need them
	SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

	// Don't forget to = Default
	RequiredLimits requiredLimits = Default;

	// We use at most 2 vertex attributes
	requiredLimits.limits.maxVertexAttributes = 10;
	// We should also tell that we use 1 vertex buffers
	requiredLimits.limits.maxVertexBuffers = 4;
	// Maximum size of a buffer is 15 vertices of 5 float each
	//requiredLimits.limits.maxBufferSize = 15 * 5 * sizeof(float);
	//                                    ^^ This was a 6
	// Maximum stride between 2 consecutive vertices in the vertex buffer
	//requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float);

	// There is a maximum of 3 float forwarded from vertex to fragment shader
	requiredLimits.limits.maxInterStageShaderComponents = 10;

	// These two limits are different because they are "minimum" limits,
	// they are the only ones we are may forward from the adapter's supported
	// limits.
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	return requiredLimits;
}

void Application::InitializeBuffers() {
	
	
	// add a single splat at the origin
	std::vector<Splat> splats(0);
	glm::vec3 position = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec3 scale = glm::vec3(2, 0.5, 1);
	glm::vec4 color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 rotation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

	
	//glm::mat3 rotationMatrix = glm::mat3(0.0f);
	//float cnst = glm::sqrt(2.0f) / 2.0f;
	//rotationMatrix[0] = glm::vec3(cnst, 0.0f, -cnst);
	//rotationMatrix[1] = glm::vec3(0.0f, 1.0f, 0.0f);
	//rotationMatrix[2] = glm::vec3(cnst, 0.0f, cnst);
	//rotationMatrix = glm::mat3(1.0f);

	//rotationMatrix = glm::mat3(glm::rotate(glm::mat4(rotationMatrix), glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

	glm::mat3 scaleMatrix (1.0f);
	scaleMatrix[0][0] = scale.x;
	scaleMatrix[1][1] = scale.y;
	scaleMatrix[2][2] = scale.z;

	//glm::mat3 variance = rotationMatrix * scaleMatrix;
	//glm::f32 constant = 2 * glm::pi<float>() * glm::sqrt(glm::determinant(variance));

	Splat s;
	//s.position = glm::vec4(position, constant);
	//s.variance = glm::mat4(variance);
	s.color = color;

	Splat s2;
	//s2.position = glm::vec4(glm::vec3(0.0f, 0.0f, 0.0f), constant);
	//s2.variance = glm::mat4(variance);
	s2.color = glm::vec4(color);

	//splats[0] = s;
	//splats[1] = s2;

	for (int i = 0; i < 6; i++) {
		Splat s;
		glm::mat3 rotationMatrix = glm::mat3_cast(glm::angleAxis(glm::radians(30.0f)*i, glm::vec3(1.0f, 0.0f, 0.0f)));
		rotationMatrix = glm::mat3_cast(glm::angleAxis(glm::radians(30.0f)*i, glm::vec3(0.0f, 1.0f, 0.0f))) * rotationMatrix;
		glm::mat3 variance = rotationMatrix * scaleMatrix;
		s.transform = glm::mat4(variance); //glm::rotate(glm::mat4(scaleMatrix), glm::radians(30.0f)*i, glm::vec3(0.0f, 1.0f, 0.0f));
		s.transform[3] = glm::vec4(glm::vec3(0.5f*i, 0.0f, 0.0f), 1);
		//s.position = glm::vec4(glm::vec3(0.1f*i, 0.0f, 0.0f), constant);
		//s.variance = glm::mat4(variance);
		s.color = glm::vec4(color);
		splats.push_back(s);
	}
	
	splatMesh.splatData = splats;
	splatMesh.splatCount = splats.size();
	
	splatMesh.loadData(RESOURCE_DIR "/splats/nike.splat", true);

	std::cout << "Loaded " << splatMesh.splatCount << " splats" << std::endl;

	splatMesh.initialize(device, queue);

	// Create a transform uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(Uniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	transformBuffer = device.createBuffer(bufferDesc);
}

void Application::InitializeBindGroups() {
	// Create a binding
	BindGroupEntry bindings[3] = {};
	
	bindings[0].binding = 0;
	bindings[0].buffer = transformBuffer;
	bindings[0].offset = 0;
	bindings[0].size = sizeof(Uniforms);

	bindings[1].binding = 1;
	bindings[1].buffer = splatMesh.splatBuffer;
	bindings[1].offset = 0;
	bindings[1].size = splatMesh.splatCount * sizeof(Splat);

	bindings[2].binding = 2;
	bindings[2].buffer = splatMesh.sortIndexBuffer;
	bindings[2].offset = 0;
	bindings[2].size = splatMesh.splatCount * sizeof(uint32_t);

	// A bind group contains one or multiple bindings
	BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = bindGroupLayout;
	// There must be as many bindings as declared in the layout!
	bindGroupDesc.entryCount = 3;
	bindGroupDesc.entries = bindings;
	bindGroup = device.createBindGroup(bindGroupDesc);
}

void Application::InitializeScene() {
	std::cout << "Initializing scene..." << std::endl;

	camera = orbitCamera->camera;
	camera->aspect = 640 / 640;
	scene->addChild(orbitCamera);

	// Create a node for the splat
	splatNode->position = glm::vec3(0.0f, 0.0f, 0.0f);
	splatNode->scale = glm::vec3(1.0f, 1.0f, 1.0f);
	//splatNode->rotate(glm::vec3(0.0f, 1.0f, 0.0f), glm::radians(90.0f));
	scene->addChild(splatNode);
	std::cout << "Scene initialized" << std::endl;
};

void Application::UpdateScene() {
	scene->updateWorldMatrix(glm::mat4(1.0f));
};
	
bool Application::initGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(m_window, true);
    ImGui_ImplWGPU_Init(device, 3, surfaceFormat);
    return true;
}

void Application::terminateGui() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
}

void Application::guiAddSliderParameter(const char* label, float* value, float min, float max) {
	// Left cell (Text)
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(label);

	// Right cell (Slider)
	ImGui::TableSetColumnIndex(1);
	const char* id = ("##" + std::string(label)).c_str();
	ImGui::PushItemWidth(ImGui::GetColumnWidth() * 1.0f);
	ImGui::SliderFloat(id, value, min, max, "%.3f");
	ImGui::TableNextRow();
}

void Application::updateGui(RenderPassEncoder renderPass) {
    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

	
	float columnWidth = 80.0f;

    // [...] Build our UI
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
	//ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
	ImGui::Begin("Control Panel", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	
	ImGui::Text("SPLATS");
	

	if (ImGui::BeginTable("table", 2, ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_BordersOuter)) {
		ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, columnWidth); // Auto stretch
		ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_WidthStretch, 2*columnWidth); // Fixed width

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		guiAddSliderParameter("size", &uniforms.splatSize, 0.001f, 6.0f);

		ImGui::EndTable();
	}

	ImGui::Text("CAMERA");

	if (ImGui::BeginTable("table2", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter)) {
		ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, columnWidth); // Auto stretch
		ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_None, 2*columnWidth); // Fixed width

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		guiAddSliderParameter("Orbit rate", &(orbitCamera->orbitRate), 0.01f, 2.0f);
		guiAddSliderParameter("Scroll rate", &(orbitCamera->scrollRate), 0.1f, 20.0f);

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("STATS");
	// Show fps
	ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / imGuiIo.Framerate, imGuiIo.Framerate);

	ImGui::End();

    // Draw the UI
    ImGui::EndFrame();
    // Convert the UI defined above into low-level drawing commands
    ImGui::Render();
    // Execute the low-level drawing commands on the WebGPU backend
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}
