#pragma once

#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <glm/glm.hpp>

using namespace wgpu;

class Renderer {
public:
    GLFWwindow *window;
    int width;
    int height;

    Device device;
	Queue queue;
	Surface surface;
    TextureFormat surface_format = TextureFormat::Undefined;
    RenderPipeline pipeline;

    std::unique_ptr<ErrorCallback> uncaptured_error_callback_handle;

public:
    Renderer() = default;
    ~Renderer();

    void init(GLFWwindow* window);
    TextureView get_next_surface_texture_view();
    RenderPassEncoder create_render_pass_encoder(
        TextureView &targetView, CommandEncoder &encoder);

private:
    RequiredLimits get_required_limits(Adapter &adapter);
};