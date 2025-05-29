// In ResourceManager.cpp
#include "ResourceManager.h"

using namespace wgpu;

ShaderModule ResourceManager::loadShaderModule(const std::filesystem::path& path, Device device) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Failed to open WGSL file: " + path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the whole file into the buffer
    std::string shaderSource = buffer.str();

	// print the file contents
	//std::cout << "Shader source: " << std::endl;
	//std::cout << shaderSource << std::endl;
	//std::cout << "Shader size: " << shaderSource.size() << std::endl;

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


SplatSplitVector ResourceManager::loadSplatsRaw(const std::filesystem::path& path, bool center) {
	std::ifstream file{ path, std::ios::binary };
	if (!file.is_open()) {
		return {};
	}

	SplatSplitVector splats;
	splats.clear();

	SplatRaw splat;
	while (file.read(reinterpret_cast<char*>(&splat), sizeof(SplatRaw))) {
		splats.push_back(raw_to_split(splat));
	}

	file.close();

	if (center) {
		// center the splats
		glm::vec3 center(0.0f);
		for (const auto& s : splats) {
			center += s.position;
		}
		center /= static_cast<float>(splats.size());
		for (auto& s : splats) {
			s.position -= center;
		}
	}

	return splats;
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

	if (center) {
		glm::vec3 center(0.0f);
		for (const auto& s : splatsRaw) {
			center += s.position;
		}
		center /= static_cast<float>(splatsRaw.size());
		for (auto& s : splatsRaw) {
			s.position -= center;
		}
	}

	
	// change of basis
	glm::mat4 basis = glm::mat4(1.0f);
	basis[0] = glm::vec4(1.0f,  0.0f,  0.0f, 0.0f);
	basis[1] = glm::vec4(0.0f, -1.0f,  0.0f, 0.0f);
	basis[2] = glm::vec4(0.0f,  0.0f, -1.0f, 0.0f);
	basis[3] = glm::vec4(0.0f,  0.0f,  0.0f, 1.0f);
	
	//int count = 0;
	for (const auto& splatRaw : splatsRaw) {
		glm::vec3 position = splatRaw.position;
		glm::vec3 scale = splatRaw.scale;
		glm::vec4 color = static_cast<glm::vec4>(splatRaw.color);
		glm::vec4 rotation = static_cast<glm::vec4>(splatRaw.rotation);

		//position.y *= -1.0f;		// <----------- THIS GUY...............

		color = glm::pow(color / 255.0f, glm::vec4(2.2f, 2.2f, 2.2f, 1.0f));	// gamma correction
		rotation = (rotation - 128.0f) / 128.0f;
		rotation = glm::normalize(rotation);

		glm::mat4 transform = quat_to_rot(rotation);
		transform[0] *= scale.x;
		transform[1] *= scale.y;
		transform[2] *= scale.z;
		transform[3] = glm::vec4(position, 1.0f);

		transform = basis * transform * glm::transpose(basis); 	// * glm::transpose(basis) 
															   	// 	not needed since we use it to copmute the variance.
															   	// it will cancel out

		Splat s;
		s.transform = transform;
		s.color = color;

		splats.push_back(s);
	}

	return true;
}