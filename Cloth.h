#pragma once

#include <webgpu/webgpu.hpp>
#include "Node.h"
#include <vector>
#include <omp.h>

using namespace wgpu;

class Cloth : public Node {
public:
	struct Parameters {
		size_t width = 2;
		size_t height = 2;
		size_t substeps = 50;
		float minDeltatime = 0.0166f;
        float energyPreservation = 1.0f;
	};

    struct Particle {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 velocity;
        alignas(16) glm::vec3 force;
        alignas(4) float mass;
    };

    struct Constraint {
        Particle *p1;
        Particle *p2;
        float d;
    };

    size_t width;
    size_t height;
    size_t particleCount;
    std::vector<Particle> particles;
    Particle anchorParticle1;
    Particle anchorParticle2;
    std::vector<glm::vec3> currentPositions;
    std::vector<Constraint> constraints;

    size_t substeps;
    float minDeltatime;
    float currentDeltatime{0.0};
    float energyPreservation;
    float timeScale{1.0f};


    Device device;
    Queue queue;

    Buffer particleBuffer;

    size_t indexCount;
    Buffer indexBuffer;

    Cloth() = default;

    Cloth(Parameters &params) : 
        width(params.width),
        height(params.height),
        substeps(params.substeps),
        minDeltatime(params.minDeltatime),
        energyPreservation(params.energyPreservation)
        {
        indexCount = (width - 1) * (height - 1) * 6;
        particleCount = width * height;

        particles.resize(particleCount);
        currentPositions.resize(particleCount);
        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width; ++j) {
                size_t index = i * width + j;
                glm::f32 y = static_cast<float>(i) / static_cast<float>(height-1);
                glm::f32 x = static_cast<float>(j) / static_cast<float>(width-1) - 0.5f; 
                glm::f32 mass = 1.0f;
                glm::f32 size = 2.0f;
                glm::vec3 pos = glm::vec3(x*size, 0.5f, y*size);
                particles[index].position = pos;
                particles[index].velocity = glm::vec3(0);
                particles[index].force = glm::vec3(0, -9.81f * mass, 0);
                particles[index].mass = mass;
                currentPositions[index] = pos;
            }
        }

        constraints.resize((width - 1) * height + (height - 1) * width);
        size_t index = 0;
        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width - 1; ++j) {
                size_t p1_index = i * width + j;
                size_t p2_index = i * width + j + 1;
                Particle *p1 = &particles[p1_index];
                Particle *p2 = &particles[p2_index];
                constraints[index].p1 = p1;
                constraints[index].p2 = p2;
                constraints[index].d = glm::distance(p1->position, p2->position);
                index++;
            }
        }
        for (size_t i = 0; i < height - 1; ++i) {
            for (size_t j = 0; j < width; ++j) {
                size_t p1_index = i * width + j;
                size_t p2_index = (i + 1) * width + j;
                Particle *p1 = &particles[p1_index];
                Particle *p2 = &particles[p2_index];
                constraints[index].p1 = p1;
                constraints[index].p2 = p2;
                constraints[index].d = glm::distance(p1->position, p2->position);
                index++;
            }
        }

        // Anchor particles
        anchorParticle1.position = particles[0].position;
        anchorParticle1.mass = std::numeric_limits<float>::max();
        anchorParticle2.position = particles[width - 1].position;
        anchorParticle2.mass = std::numeric_limits<float>::max();
        constraints.push_back({&particles[0], &anchorParticle1, 0.0f});
        constraints.push_back({&particles[width - 1], &anchorParticle2, 0.0f});
    }


    void initialize(Device &device, Queue &queue) {
        this->device = device;
        this->queue = queue;
        initializeBuffers();
    }

    void update(float deltaTime) {
        if (deltaTime <= 0.0f) {
            return;
        }
        omp_set_num_threads(10);
        currentDeltatime += deltaTime;
        if (currentDeltatime < minDeltatime) {
            return;
        }
        size_t currentSubsteps = substeps * timeScale;
        float currentEnergyPreservation = 
            glm::pow(energyPreservation, 
            minDeltatime * timeScale / static_cast<float>(currentSubsteps));
        float dt = minDeltatime / static_cast<float>(currentSubsteps);
        dt *= timeScale;
        currentDeltatime = 0.0f;
        for (size_t i = 0; i < currentSubsteps; ++i) {
            #pragma omp parallel for
            for (size_t j = 0; j < particleCount; ++j) {
                glm::vec3 delta = particles[j].position - currentPositions[j];
                particles[j].velocity = delta / dt * currentEnergyPreservation;
                currentPositions[j] = particles[j].position;
                particles[j].velocity += particles[j].force / particles[j].mass * dt;
                particles[j].position += particles[j].velocity * dt;
            }

            #pragma omp parallel for
            for (size_t j = 0; j < constraints.size(); ++j) {
                Constraint &c = constraints[j];
                glm::vec3 delta = c.p2->position - c.p1->position;
                float dist = glm::length(delta);
                //std::cout << "dist: " << dist << std::endl;
                if (glm::abs(dist) < 0.00001f) {
                    continue;
                }
                float diff = (dist - c.d) / dist;
                glm::vec3 correction = delta * diff;
                float w1 = c.p1->mass / (c.p1->mass + c.p2->mass);
                float w2 = c.p2->mass / (c.p1->mass + c.p2->mass);
                c.p1->position += w2 * correction;
                c.p2->position -= w1 * correction;
            }
        }
        
        queue.writeBuffer(particleBuffer, 0, particles.data(), particleCount * sizeof(Particle));
    }

    void setBuffers(RenderPassEncoder &encoder) {
        encoder.setIndexBuffer(indexBuffer, IndexFormat::Uint32, 0, indexBuffer.getSize());
    }
        

private:
    void initializeBuffers() {
        initializePositionBuffer();
        initializeIndexBuffer();
    }

    void initializePositionBuffer() {
        BufferDescriptor bufferDesc;
        bufferDesc.size = particleCount * sizeof(Particle);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage;
        bufferDesc.mappedAtCreation = false;
        particleBuffer = device.createBuffer(bufferDesc);

        queue.writeBuffer(particleBuffer, 0, particles.data(), bufferDesc.size);
    }

    void initializeIndexBuffer() {
        std::vector<uint32_t> indexData;
        for (size_t i = 0; i < height - 1; ++i) {
            for (size_t j = 0; j < width - 1; ++j) {
                indexData.push_back(i * width + j);
                indexData.push_back(i * width + j + 1);
                indexData.push_back((i + 1) * width + j);

                indexData.push_back(i * width + j + 1);
                indexData.push_back((i + 1) * width + j + 1);
                indexData.push_back((i + 1) * width + j);
            }
        }

        BufferDescriptor bufferDesc;
        bufferDesc.size = indexData.size() * sizeof(uint32_t);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
        indexBuffer = device.createBuffer(bufferDesc);

        queue.writeBuffer(indexBuffer, 0, indexData.data(), bufferDesc.size);
    }
};