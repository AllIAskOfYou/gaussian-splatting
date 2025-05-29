#pragma once

#include <glm/glm.hpp>

struct SplatRaw {
	glm::vec3 position;
	glm::vec3 scale;
	glm::u8vec4 color;
	glm::u8vec4 rotation;
};

struct SplatSplit {
	glm::vec3 position;
	glm::vec3 scale;
	glm::vec4 color;
	glm::vec4 rotation;
};

struct Splat {
	glm::mat4 transform;
	glm::vec4 color;
};


using SplatRawVector = std::vector<SplatRaw>;
using SplatSplitVector = std::vector<SplatSplit>;
using SplatVector = std::vector<Splat>;

inline glm::mat4 quat_to_rot(glm::vec4 q) {
	glm::mat4 rot = glm::mat3(1.0f);
	float qr = q[0];
	float qi = q[1];
	float qj = q[2];
	float qk = q[3];

	rot[0][0] = 1 - 2*(qj*qj + qk*qk);
	rot[0][1] = 2*(qi*qj + qk*qr);
	rot[0][2] = 2*(qi*qk - qj*qr);
	
	rot[1][0] = 2*(qi*qj - qk*qr);
	rot[1][1] = 1 - 2*(qi*qi + qk*qk);
	rot[1][2] = 2*(qj*qk + qi*qr);

	rot[2][0] = 2*(qi*qk + qj*qr);
	rot[2][1] = 2*(qj*qk - qi*qr);
	rot[2][2] = 1 - 2*(qi*qi + qj*qj);
	
	return rot;
}

inline SplatSplit raw_to_split(const SplatRaw &splatRaw) {
	glm::vec3 position = splatRaw.position;
	glm::vec3 scale = splatRaw.scale;
	glm::vec4 color = static_cast<glm::vec4>(splatRaw.color);
	glm::vec4 rotation = static_cast<glm::vec4>(splatRaw.rotation);

	SplatSplit splatSplit;
	rotation = (rotation - 128.0f) / 128.0f;
	rotation = glm::normalize(rotation);
	splatSplit.position = position;
	splatSplit.scale = scale;
	splatSplit.color = glm::pow(color / 255.0f,
		glm::vec4(2.2f, 2.2f, 2.2f, 1.0f));
	splatSplit.rotation = rotation;
	return splatSplit;
}

inline Splat split_to_splat(const SplatSplit &splatSplit) {
	Splat splat;
	auto xyz = quat_to_rot(splatSplit.rotation);
	xyz[0] *= splatSplit.scale.x;
	xyz[1] *= splatSplit.scale.y;
	xyz[2] *= splatSplit.scale.z;

	auto cov = xyz * glm::transpose(xyz);
	splat.transform = glm::mat4(cov);

	splat.transform[3] = glm::vec4(splatSplit.position, 1.0f);

	splat.color = splatSplit.color;
	return splat;
}

inline Splat raw_to_splat(const SplatRaw &splatRaw) {
	SplatSplit splatSplit = raw_to_split(splatRaw);
	return split_to_splat(splatSplit);
}

inline float splat_weight(const SplatSplit &splat) {
	float alpha = splat.color.w;
	float volume = splat.scale.x * splat.scale.y * splat.scale.z;
	return alpha * volume; 
}

inline Splat merge(SplatSplitVector &splats_raw) {
	Splat splat;
	auto covariance = glm::mat3(0.0f);
	auto color = glm::vec4(0.0f);

	std::vector<float> weights(splats_raw.size());
	float total_weight = 0.0f;
	for (size_t i = 0; i < splats_raw.size(); i++) {
		weights[i] = splat_weight(splats_raw[i]);
		total_weight += weights[i];
	}

	for (size_t i = 0; i < splats_raw.size(); i++) {
		weights[i] /= total_weight;
	}

	SplatVector splats(splats_raw.size());
	for (size_t i = 0; i < splats_raw.size(); i++) {
		splats[i] = split_to_splat(splats_raw[i]);
	}

	for (size_t i = 0; i < splats.size(); i++) {
		color += splats[i].color * weights[i];
	}

	glm::vec3 center = glm::vec3(0.0f);
	for (size_t i = 0; i < splats.size(); i++) {
		center += glm::vec3(splats[i].transform[3]) * weights[i];
	}

	for (size_t i = 0; i < splats.size(); i++) {
		auto cov = glm::mat3(splats[i].transform);
		auto diff = glm::vec3(splats[i].transform[3]) - center;
		covariance += (cov + glm::outerProduct(diff, diff)) * weights[i];
	}

	splat.transform = glm::mat4(covariance);
	splat.transform[3] = glm::vec4(center, 1.0f);
	splat.color = color;
	return splat;
}