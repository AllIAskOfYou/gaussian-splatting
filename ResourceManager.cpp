// In ResourceManager.cpp
#include "ResourceManager.h"

using namespace wgpu;

glm::mat4 quat_to_rot(glm::vec4 q) {
	glm::mat4 rot = glm::mat4(1.0f);
	float qr = q[0];
	float qi = q[1];
	float qj = q[2];
	float qk = q[3];

	rot[0][0] = 1 - 2*(qj*qj + qk*qk);
	rot[0][1] = 2*(qi*qj + qk+qr);
	rot[0][2] = 2*(qi*qk - qj*qr);
	
	rot[1][0] = 2*(qi*qj - qk*qr);
	rot[1][1] = 1 - 2*(qi*qi + qk*qk);
	rot[1][2] = 2*(qj*qk + qi*qr);

	rot[2][0] = 2*(qi*qk + qj*qr);
	rot[2][1] = 2*(qj*qk - qi*qr);
	rot[2][2] = 1 - 2*(qi*qi + qj*qj);
	
	return rot;
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
	
	for (const auto& splatRaw : splatsRaw) {
		glm::vec3 position = splatRaw.position;
		glm::vec3 scale = splatRaw.scale;
		glm::vec4 color = static_cast<glm::vec4>(splatRaw.color);
		glm::vec4 rotation = static_cast<glm::vec4>(splatRaw.rotation);

		position.y *= -1.0f;
		color = color / 255.0f;
		rotation = (rotation - 128.0f) / 128.0f;
		rotation = glm::normalize(rotation);
		rotation[0] *= 1.0f;
		rotation[1] *= 1.0f;
		rotation[2] *= 1.0f;
		rotation[3] *= 1.0f;

		//glm::quat q;
		//q.x = rotation.x;
		//q.y = rotation.y;
		//q.z = rotation.z;
		//q.w = rotation.w;
		
		glm::mat4 transform = glm::mat4(1.0f);
		transform = glm::scale(transform, scale);
		
		transform = quat_to_rot(rotation) * transform;
		//transform = quat_to_rot(rotation);
		//transform[0] *= scale.x;
		//transform[1] *= scale.z;
		//transform[2] *= scale.y;
		
		transform[3] = glm::vec4(position, 1.0f);

		//glm::f32 constant = 2 * glm::pi<float>() * glm::sqrt(glm::determinant(variance));

		Splat s;
		s.transform = transform;
		//s.variance = variance;
		//s.position = glm::vec4(position, constant);
		//s.constant = constant;
		s.color = color;

		splats.push_back(s);
	}

	return true;
}