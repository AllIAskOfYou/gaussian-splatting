#include "gui.hpp"

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>
#include <GLFW/glfw3.h>

void GUI::init(GLFWwindow *window, Renderer *renderer) {
    this->window = window;
    this->renderer = renderer;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_Init(renderer->device, 3, renderer->surface_format);
}

GUI::~GUI() {
    ImGui_ImplGlfw_Shutdown();
    ImGui_ImplWGPU_Shutdown();
    ImGui::DestroyContext();
}

void GUI::update(RenderPassEncoder renderPass) {
    
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
	

	if (ImGui::BeginTable("table", 2,
        ImGuiTableFlags_SizingFixedSame | ImGuiTableFlags_BordersOuter))
        {
		ImGui::TableSetupColumn(
            "Text", ImGuiTableColumnFlags_WidthFixed, columnWidth);
		ImGui::TableSetupColumn(
            "Slider", ImGuiTableColumnFlags_WidthStretch, 2*columnWidth);

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		add_float_slider("Size", &params.splatSize, 0.001f, 5.0f);
		add_float_slider("Cut Off", &params.cutOff, 0.001f, 5.0f);
		add_int_slider("BayerSize", reinterpret_cast<int*>(&params.bayerSize), 0, 6);
		add_float_slider("BayerScale", &params.bayerScale, 0.5f, 2.0f);

		ImGui::EndTable();
	}

	ImGui::Text("CAMERA");

	if (ImGui::BeginTable("table2", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter)) {
		ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, columnWidth); // Auto stretch
		ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_None, 2*columnWidth); // Fixed width

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		add_float_slider("FoV", &params.fov, 0.01f, 100.0f);

		ImGui::EndTable();
	}

	if (ImGui::BeginTable("table2", 2,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter))
        {
		ImGui::TableSetupColumn(
            "Text", ImGuiTableColumnFlags_WidthFixed, columnWidth);
		ImGui::TableSetupColumn(
            "Slider", ImGuiTableColumnFlags_None, 2*columnWidth);

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		add_float_slider("Orbit rate", &params.orbitRate, 0.01f, 2.0f);
		add_float_slider("Scroll rate", &params.scrollRate, 0.1f, 20.0f);

		ImGui::EndTable();
	}

	ImGui::Text("Debug");

	if (ImGui::BeginTable("table4", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter)) {
		ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthFixed, columnWidth); // Auto stretch
		ImGui::TableSetupColumn("Slider", ImGuiTableColumnFlags_None, 2*columnWidth); // Fixed width

		ImGui::TableNextRow();
		ImGui::TableNextRow();

		add_int_slider("Depth", reinterpret_cast<int*>(&params.depth), 0, 4);
		add_float_slider("Min Screen Area", &params.min_screen_area, 0.0f, 1.0f);

		ImGui::EndTable();
	}

	ImGui::Separator();
	ImGui::Text("STATS");
	// Show fps
	ImGui::Text("%.3f ms/frame (%.1f FPS)",
        1000.0f / imGuiIo.Framerate, imGuiIo.Framerate);

	ImGui::End();

    // Draw the UI
    ImGui::EndFrame();
    // Convert the UI defined above into low-level drawing commands
    ImGui::Render();
    // Execute the low-level drawing commands on the WebGPU backend
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}

void GUI::add_float_slider(
        const char* label, float* value, float min, float max) {
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

void GUI::add_int_slider(
        const char* label, int* value, int min, int max) {
    // Left cell (Text)
	ImGui::TableSetColumnIndex(0);
	ImGui::Text(label);

	// Right cell (Slider)
	ImGui::TableSetColumnIndex(1);
	const char* id = ("##" + std::string(label)).c_str();
	ImGui::PushItemWidth(ImGui::GetColumnWidth() * 1.0f);
	ImGui::SliderInt(id, value, min, max, "%d");
	ImGui::TableNextRow();
}