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

/*
struct Splat {
	glm::mat3 variance; 		// 3 x 12 bytes
	glm::vec3 _pad1;			// 1 x 12 bytes
	glm::vec4 color; 			// 1 x 16 bytes
	glm::vec3 position;			// 1 x 12 bytes
	glm::f32 _pad2;				// 1 x 4 bytes
	glm::f32 constant;			// 1 x 4 bytes
	glm::f32 _pad3[3];			// 3 x 4 bytes
};
*/