#pragma once

#include <glm/glm.hpp>

struct SplatRaw {
	glm::vec3 position;
	glm::vec3 scale;
	glm::u8vec4 color;
	glm::u8vec4 rotation;
};

struct Splat {
	glm::vec4 color;
	glm::vec4 rotation;
	glm::vec3 position;
	glm::f32 _pad1;
	glm::vec3 scale;
	glm::f32 _pad2;
};