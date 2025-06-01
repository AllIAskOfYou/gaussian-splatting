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
#include "Renderer.hpp"
#include "GUI.hpp"

#include "Node.h"
#include "Camera.h"
#include "OrbitCamera.h"

#include "SplatMesh.h"
#include "SplatMeshOctree.hpp"
#include "SplatMeshHC.hpp"
#include "SplatMeshGridHC.hpp"

using namespace wgpu;

class Application {
public:
	Application() = default;
	bool Initialize();
	void Terminate();
	void MainLoop();
	bool IsRunning();

private:
	struct Uniforms {
		glm::mat4x4 projectionMatrix;
		glm::mat4x4 viewMatrix;
		glm::mat4x4 modelMatrix;
		float splatSize = 1;
		float cutOff = 3.5;
		float fov = 45.0;
		uint32_t bayerSize = 1;
		float bayerScale = 1.0f;
		float padding[3]; // Padding to 16 bytes
	} uniforms;

private:
	// Substep of Initialize() that creates the render pipeline
	void InitializePipeline();
	void InitializeBuffers();
	void InitializeBindGroups();
	void InitializeScene();
	void UpdateScene();

// Event handlers for interaction
private:
	void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, [[maybe_unused]] int mods);
    void onScroll([[maybe_unused]] double xoffset, double yoffset);

private:
	// We put here all the variables that are shared between init and main loop
	GLFWwindow *m_window;
	Renderer m_renderer;
	GUI gui;
	RenderPipeline pipeline;


	// Uniform buffer
	BindGroup bindGroup;
	PipelineLayout layout;
	BindGroupLayout bindGroupLayout;

	// transform buffer
	Buffer transformBuffer;
	
	std::shared_ptr<Node> scene = std::make_shared<Node>();
	std::shared_ptr<OrbitCamera> orbitCamera =
		std::make_shared<OrbitCamera>(5.0);
	std::shared_ptr<Camera> camera = std::make_shared<Camera>();
	std::shared_ptr<Node> splatNode = std::make_shared<Node>();

	//SplatMesh splatMesh;
	//SplatMeshOctree splatMesh;
	SplatMeshGridHC splatMesh;

	// OTHER ----------------------------------------------------------
	float width = 1000;
	float height = 800;


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

void Application::onMouseButton(
		int button, int action, [[maybe_unused]] int mods) {
    if (gui.imGuiIo.WantCaptureMouse) {
        // Don't rotate the camera if the mouse is already captured by an ImGui
        // interaction at this frame.
        return;
    }
	orbitCamera->onMouseButton(button, action);
}

void Application::onScroll([[maybe_unused]] double xoffset, double yoffset) {
	orbitCamera->onScroll(yoffset, deltaTime);
}

bool Application::Initialize() {
	// Open window
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	m_window = glfwCreateWindow(
		width, height, "Gaussian Splatting", nullptr, nullptr);

	glfwSetWindowUserPointer(m_window, this);

	// Add window callbacks
	glfwSetCursorPosCallback(m_window, [](
			GLFWwindow* window, double xpos, double ypos) {
		auto that = reinterpret_cast<Application*>(
			glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos);
	});
	glfwSetMouseButtonCallback(m_window, [](
			GLFWwindow* window, int button, int action, int mods) {
		auto that = reinterpret_cast<Application*>(
			glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods);
	});
	glfwSetScrollCallback(m_window, [](
			GLFWwindow* window, double xoffset, double yoffset) {
		auto that = reinterpret_cast<Application*>(
			glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset);
	});
	
	// Create m_renderer
	m_renderer.init(m_window);

	InitializeBuffers();
	InitializePipeline();
	InitializeBindGroups();
	InitializeScene();

	// Initialize time
	//time = glfwGetTime();

	gui.init(m_window, &m_renderer);

	return true;
}

void Application::Terminate() {
	pipeline.release();
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Application::MainLoop() {
	// Update time
	gui.imGuiIo = ImGui::GetIO();
	deltaTime = gui.imGuiIo.DeltaTime;
	

	//printf("FPS: %f\n", 1.0 / deltaTime);
	glfwPollEvents();

	// Update the scene
	UpdateScene();

	// Create a transform matrix
	uniforms.projectionMatrix = camera->getProjectionMatrix();
	uniforms.viewMatrix = camera->getViewMatrix();
	uniforms.modelMatrix = splatNode->worldMatrix;

	// Read params from GUI and update uniforms
	uniforms.splatSize = gui.params.splatSize;
	uniforms.cutOff = gui.params.cutOff;
	uniforms.fov = gui.params.fov;
	uniforms.bayerSize = gui.params.bayerSize;
	uniforms.bayerScale = gui.params.bayerScale;
	// Update the camera
	camera->fov = uniforms.fov;
	// Update the orbit camera
	orbitCamera->orbitRate = gui.params.orbitRate;
	orbitCamera->scrollRate = gui.params.scrollRate;

	// Upload the transform matrix to the buffer
	m_renderer.queue.writeBuffer(
			transformBuffer, 0, &uniforms, sizeof(Uniforms));

	// Get the next target texture view
	TextureView targetView = m_renderer.get_next_surface_texture_view();
	if (!targetView) return;

	// Create a command encoder for the draw call
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder =
		wgpuDeviceCreateCommandEncoder(m_renderer.device, &encoderDesc);

	RenderPassEncoder renderPass = m_renderer.create_render_pass_encoder(
		targetView, encoder);

	// Select which render pipeline to use
	renderPass.setPipeline(pipeline);	

	// Set binding group here!
	renderPass.setBindGroup(0, bindGroup, 0, nullptr);

	splatMesh.render(renderPass, camera, gui.params);

	gui.update(renderPass);

	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	//std::cout << "Submitting command..." << std::endl;
	m_renderer.queue.submit(1, &command);
	command.release();
	//std::cout << "Command submitted." << std::endl;

	// At the end of the frame
	targetView.release();
#ifndef __EMSCRIPTEN__
	m_renderer.surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	m_renderer.device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	m_renderer.device.poll(false);
#endif
}

bool Application::IsRunning() {
	return !glfwWindowShouldClose(m_window);
}

void Application::InitializePipeline() {
	std::cout << "Creating shader module..." << std::endl;
	ShaderModule shaderModule = ResourceManager::loadShaderModule(
		RESOURCE_DIR "/shader_quads_ordered.wgsl", m_renderer.device);
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
	colorTarget.format = m_renderer.surface_format;
	colorTarget.blend = &blendState;
	// We could write to only some of the color channels.
	colorTarget.writeMask = ColorWriteMask::All;
	
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
	bindingLayouts[0].visibility =
		ShaderStage::Vertex | ShaderStage::Fragment;
	bindingLayouts[0].buffer.type = BufferBindingType::Uniform;
	bindingLayouts[0].buffer.minBindingSize = sizeof(Uniforms);

	bindingLayouts[1].binding = 1;
	bindingLayouts[1].visibility = ShaderStage::Vertex;
	bindingLayouts[1].buffer.type = BufferBindingType::ReadOnlyStorage;
	bindingLayouts[1].buffer.minBindingSize =
		splatMesh.splatData.size() * sizeof(Splat);

	bindingLayouts[2].binding = 2;
	bindingLayouts[2].visibility = ShaderStage::Vertex;
	bindingLayouts[2].buffer.type = BufferBindingType::ReadOnlyStorage;
	bindingLayouts[2].buffer.minBindingSize =
		splatMesh.splatData.size() * sizeof(uint32_t);

	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = 3;
	bindGroupLayoutDesc.entries = bindingLayouts;
	bindGroupLayout =
		m_renderer.device.createBindGroupLayout(bindGroupLayoutDesc);

	// Create the pipeline layout
	PipelineLayoutDescriptor layoutDesc{};
	layoutDesc.bindGroupLayoutCount = 1;
	layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
	layout = m_renderer.device.createPipelineLayout(layoutDesc);


	pipelineDesc.layout = layout;
	
	pipeline = m_renderer.device.createRenderPipeline(pipelineDesc);

	// We no longer need to access the shader module
	shaderModule.release();
}


void Application::InitializeBuffers() {
	std::vector<Splat> splats;
	Splat s;
	s.transform = glm::mat4(1.0f);
	s.transform[2][2] = 5.0f;
	s.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

	splats.push_back(s);
	splatMesh.splatData = splats;
	


	// Load the splat data
	splatMesh.loadData(RESOURCE_DIR "/splats/nike.splat", true);
	//std::cout << "Loaded " << splatMesh.splatData.size() << " splats" << std::endl;

	splatMesh.initialize(m_renderer.device, m_renderer.queue);

	// Create a transform uniform buffer
	BufferDescriptor bufferDesc;
	bufferDesc.size = sizeof(Uniforms);
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	bufferDesc.mappedAtCreation = false;
	transformBuffer = m_renderer.device.createBuffer(bufferDesc);
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
	bindings[1].size = splatMesh.splatData.size() * sizeof(Splat);

	bindings[2].binding = 2;
	bindings[2].buffer = splatMesh.sortIndexBuffer;
	bindings[2].offset = 0;
	bindings[2].size = splatMesh.splatData.size() * sizeof(uint32_t);

	// A bind group contains one or multiple bindings
	BindGroupDescriptor bindGroupDesc{};
	bindGroupDesc.layout = bindGroupLayout;
	// There must be as many bindings as declared in the layout!
	bindGroupDesc.entryCount = 3;
	bindGroupDesc.entries = bindings;
	bindGroup = m_renderer.device.createBindGroup(bindGroupDesc);
}

void Application::InitializeScene() {
	std::cout << "Initializing scene..." << std::endl;

	camera = orbitCamera->camera;
	camera->aspect = width / height;
	scene->addChild(orbitCamera);

	// Create a node for the splat
	splatNode->position = glm::vec3(0.0f, 0.0f, 0.0f);
	splatNode->scale = glm::vec3(1.0f, 1.0f, 1.0f);
	scene->addChild(splatNode);
	std::cout << "Scene initialized" << std::endl;
};

void Application::UpdateScene() {
	scene->updateWorldMatrix(glm::mat4(1.0f));
	camera->fov = uniforms.fov;
};