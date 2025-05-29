#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.hpp>

#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <imgui.h>

#include "renderer.hpp"

using namespace wgpu;

class GUI {
public:
    struct Parameters {
        float splatSize = 1.0f;
        float cutOff = 3.5f;
        float fov = 45.0f;
        uint32_t bayerSize = 1;
        float bayerScale = 1.0f;
        float orbitRate = 1.0f;
        float scrollRate = 1.0f;

        uint32_t depth = 0;
        float min_screen_area = 0.2f;
    } params;

    ImGuiIO imGuiIo;
private:
    GLFWwindow *window;
    Renderer *renderer;


public:
    GUI() = default;
    ~GUI();

    void init(GLFWwindow *window, Renderer *renderer);
    void update(RenderPassEncoder renderPass);

private:
    void add_float_slider(
        const char* label, float* value, float min, float max);
    void add_int_slider(
        const char* label, int* value, int min, int max);
};