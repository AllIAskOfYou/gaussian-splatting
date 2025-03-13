// In ResourceManager.cpp
#include "ResourceManager.h"

#include <fstream>
#include <sstream>
#include <string>

using namespace wgpu;

bool ResourceManager::loadGeometry(
	const std::filesystem::path& path,
	std::vector<float>& pointData,
	std::vector<uint16_t>& indexData
) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return false;
	}

	pointData.clear();
	indexData.clear();

	enum class Section {
		None,
		Points,
		Indices,
	};
	Section currentSection = Section::None;

	float value;
	uint16_t index;
	std::string line;
	while (!file.eof()) {
		getline(file, line);

		// overcome the `CRLF` problem
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		if (line == "[points]") {
			currentSection = Section::Points;
		}
		else if (line == "[indices]") {
			currentSection = Section::Indices;
		}
		else if (line[0] == '#' || line.empty()) {
			// Do nothing, this is a comment
		}
		else if (currentSection == Section::Points) {
			std::istringstream iss(line);
			// Get x, y, r, g, b
			for (int i = 0; i < 5; ++i) {
				iss >> value;
				pointData.push_back(value);
			}
		}
		else if (currentSection == Section::Indices) {
			std::istringstream iss(line);
			// Get corners #0 #1 and #2
			for (int i = 0; i < 3; ++i) {
				iss >> index;
				indexData.push_back(index);
			}
		}
	}
	return true;
}

ShaderModule ResourceManager::loadShaderModule(const std::filesystem::path& path, Device device) {
	std::ifstream file(path);
	if (!file.is_open()) {
		return nullptr;
	}
	file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(shaderSource.data(), size);

	ShaderModuleWGSLDescriptor shaderCodeDesc{};
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
	shaderCodeDesc.code = shaderSource.c_str();

	ShaderModuleDescriptor shaderDesc{};
#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	return device.createShaderModule(shaderDesc);
}



bool ResourceManager::loadSplats(
	const std::filesystem::path& path,
	std::vector<Splat>& splats,
	bool center
) {
	std::ifstream file
	{
		path,
		std::ios::binary
	};
	if (!file.is_open()) {
		return false;
	}

	std::vector<SplatRaw> splatsRaw;
	splatsRaw.clear();

	SplatRaw splat;
	while (file.read(reinterpret_cast<char*>(&splat), sizeof(SplatRaw))) {
		splatsRaw.push_back(splat);
	}

	splats.clear();
	splats.reserve(splatsRaw.size());
	
	for (const auto& splatRaw : splatsRaw) {
		Splat s;
		s.position = splatRaw.position;
		s.scale = splatRaw.scale;
		s.color = static_cast<glm::vec4>(splatRaw.color) / 255.0f;
		s.rotation = static_cast<glm::vec4>(splatRaw.rotation);
		s.rotation = (s.rotation - 128.0f) / 128.0f;
		splats.push_back(s);
	}

	if (center) {
		glm::vec3 center(0.0f);
		for (const auto& s : splats) {
			center += s.position;
		}
		center /= static_cast<float>(splats.size());
		for (auto& s : splats) {
			s.position -= center;
			s.position.y *= -1.0f;
		}
	}

	return true;
}