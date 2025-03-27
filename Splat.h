#pragma once

#include <glm/glm.hpp>

struct SplatRaw {
	glm::vec3 position;
	glm::vec3 scale;
	glm::u8vec4 color;
	glm::u8vec4 rotation;
};

struct Splat {
	glm::mat4 transform;
	glm::vec4 color;
};