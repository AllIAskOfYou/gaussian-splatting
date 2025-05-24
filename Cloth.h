#pragma once

#include <webgpu/webgpu.hpp>
#include "Node.h"
#include <vector>
#include <omp.h>
#include <algorithm>
#include <execution>
#include <tbb/task_arena.h>
#include <chrono>
#include <bits/stdc++.h>

using namespace wgpu;

class Cloth : public Node {
public:
	struct Parameters {
		size_t width = 2;
		size_t height = 2;
		size_t substeps = 30;
		float minDeltatime = 0.0166f;
        float energyPreservation = 1.0f;
	};

    struct Particle {
        alignas(16) glm::vec3 position;
        alignas(16) glm::vec3 velocity;
        alignas(16) glm::vec3 force;
        alignas(16) glm::vec3 normal;
        alignas(8) glm::vec2 uv;
        alignas(4) float mass;
        alignas(4) float _pad;
    };

    struct Constraint {
        Particle *p1;
        Particle *p2;
        size_t p1_index;
        size_t p2_index;
        float d;
        float cD;
        float bDFactor{1.0f};
    };

    size_t width;
    size_t height;
    size_t particleCount;
    size_t trisCount;
    std::vector<Particle> particles;
    Particle anchorParticle1;
    Particle anchorParticle2;

    bool pinned{false};
    Particle pinnedParticle;
    Constraint pinnedConstraint;

    std::vector<glm::vec3> currentPositions;
    std::vector<Constraint> constraints;
    
    std::vector<uint32_t> sortTris;
    std::vector<bool> isQuadVisible;

    size_t substeps;
    float minDeltatime;
    float currentDeltatime{0.0};
    float energyPreservation;
    float timeScale{0.0f};
    float breakDistance{10.0f};


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
        trisCount = (width - 1) * (height - 1) * 2;


        isQuadVisible.resize(trisCount * 2);
        for (size_t i = 0; i < trisCount * 2; ++i) {
            isQuadVisible[i] = true;
        }

        particles.resize(particleCount);
        currentPositions.resize(particleCount);
        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width; ++j) {
                size_t index = i * width + j;
                glm::f32 y = static_cast<float>(i) / static_cast<float>(height-1);
                glm::f32 x = static_cast<float>(j) / static_cast<float>(width-1); 
                glm::f32 mass = 1.0f;
                glm::f32 size = 2.0f;
                glm::vec3 pos = glm::vec3((x - 0.5f)*size, 1.0f, (y)*size);
                particles[index].position = pos;
                particles[index].velocity = glm::vec3(0);
                particles[index].force = glm::vec3(0, -9.81f * mass, 0);
                particles[index].uv = glm::vec2(x, y);
                particles[index].mass = mass;
                currentPositions[index] = pos;
            }
        }

        constraints.resize((width - 1) * height + (height - 1) * width);
        size_t index = 0;
        for (size_t i = 0; i < height; ++i) {
            for (size_t j = 0; j < width - 1; ++j) {
                auto &c = constraints[index];
                size_t p1_index = i * width + j;
                size_t p2_index = i * width + j + 1;
                c.p1_index = p1_index;
                c.p2_index = p2_index;
                Particle *p1 = &particles[p1_index];
                Particle *p2 = &particles[p2_index];
                c.p1 = p1;
                c.p2 = p2;
                c.d = glm::distance(p1->position, p2->position);
                // get random float between 0.0 and 1.0
                float rv = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                c.bDFactor = 1.0 + (rv - 0.5f) * 2.0 * 0.1f;
                index++;
            }
        }
        for (size_t i = 0; i < height - 1; ++i) {
            for (size_t j = 0; j < width; ++j) {
                auto &c = constraints[index];
                size_t p1_index = i * width + j;
                size_t p2_index = (i + 1) * width + j;
                c.p1_index = p1_index;
                c.p2_index = p2_index;
                Particle *p1 = &particles[p1_index];
                Particle *p2 = &particles[p2_index];
                c.p1 = p1;
                c.p2 = p2;
                c.d = glm::distance(p1->position, p2->position);
                
                float rv = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
                c.bDFactor = 2.0 + (rv - 0.5f) * 2.0 * 0.1f;
                index++;
            }
        }

        // Anchor particles
        anchorParticle1.position = particles[0].position;
        anchorParticle1.mass = std::numeric_limits<float>::max();
        anchorParticle2.position = particles[width - 1].position;
        anchorParticle2.mass = std::numeric_limits<float>::max();
        constraints.push_back({&particles[0], &anchorParticle1, 0, 0, 0.0f, 0.0f, 5.0f});
        constraints.push_back({&particles[width - 1], &anchorParticle2, width-1, 0, 0.0f, 0.0f, 5.0f});

        // Pinned particles
        pinnedParticle.mass = std::numeric_limits<float>::max();
        pinnedConstraint.p1 = &pinnedParticle;
        pinnedConstraint.d = 0.0f;

        // normals
        updateNormals();

        // sort triangles
        sortTris.resize(trisCount);
        for (uint32_t i = 0; i < trisCount; ++i) {
            sortTris[i] = i;
        }
    }


    void initialize(Device &device, Queue &queue) {
        this->device = device;
        this->queue = queue;
        initializeBuffers();
    }

    void update(float deltaTime, glm::vec3 cameraPos) {
        updateClothSimulation(deltaTime);
        updateNormals();
        queue.writeBuffer(particleBuffer, 0, particles.data(), particleCount * sizeof(Particle));
        auto start = std::chrono::high_resolution_clock::now();
        updateIndexes(cameraPos);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        //std::cout << "Time needed to sort the triangles: " << elapsed.count() << "s" << std::endl;
    }

    void setBuffers(RenderPassEncoder &encoder) {
        encoder.setIndexBuffer(indexBuffer, IndexFormat::Uint32, 0, indexBuffer.getSize());
    }

    void pinParticle(int action, glm::vec3 rayOrigin, glm::vec3 rayDirection) {
        if (action == 1) {
            pinned = true;
            float closestDistance = std::numeric_limits<float>::max();
            for (size_t i = 0; i < particleCount; ++i) {
                glm::vec3 pos = particles[i].position;
                glm::vec3 delta = pos - rayOrigin;
                float proj = glm::dot(delta, rayDirection);
                float dist = glm::length(delta - proj * rayDirection);
                float distScore = dist;
                if (distScore < closestDistance) {
                    closestDistance = distScore;
                    pinnedParticle.position = pos;
                    pinnedConstraint.p2 = &particles[i];
                }
            }
            constraints.push_back(pinnedConstraint);
        } else {
            pinned = false;
            constraints.pop_back();
        }
    }

    void movePinnedParticle(glm::vec3 rayOrigin, glm::vec3 rayDirection, glm::vec3 forward) {
        if (pinned) {
            // Calculate the new position of the pinned particle
            glm::vec3 delta = pinnedParticle.position - rayOrigin;
            float t = glm::dot(forward, delta) / glm::dot(forward, rayDirection);
            glm::vec3 newPos = rayOrigin + t * rayDirection;
            // Update the position of the pinned particle
            pinnedParticle.position = newPos;
            // Update the position of the constraint
        }
    }
        

private:

    void updateNormals() {
        #pragma omp parallel for
        for (size_t i = 0; i < particleCount; ++i) {
            particles[i].normal = glm::vec3(0);
        }

        #pragma omp parallel for
        for (size_t i = 0; i < height - 1; ++i) {
            for (size_t j = 0; j < width - 1; ++j) {
                size_t p1_index = i * width + j;
                size_t p2_index = i * width + j + 1;
                size_t p3_index = (i + 1) * width + j;
                glm::vec3 v1 = particles[p2_index].position - particles[p1_index].position;
                glm::vec3 v2 = particles[p3_index].position - particles[p1_index].position;
                glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
                particles[p1_index].normal += normal;
                particles[p2_index].normal += normal;
                particles[p3_index].normal += normal;
            }
            for (size_t j = 0; j < width - 1; ++j) {
                size_t p1_index = i * width + j + 1;
                size_t p2_index = (i + 1) * width + j + 1;
                size_t p3_index = (i + 1) * width + j;
                glm::vec3 v1 = particles[p2_index].position - particles[p1_index].position;
                glm::vec3 v2 = particles[p3_index].position - particles[p1_index].position;
                glm::vec3 normal = glm::normalize(glm::cross(v1, v2));
                particles[p1_index].normal += normal;
                particles[p2_index].normal += normal;
                particles[p3_index].normal += normal;
            }
        }
        #pragma omp parallel for
        for (size_t i = 0; i < particleCount; ++i) {
            particles[i].normal = glm::normalize(particles[i].normal);
        }        
    }

    void updateClothSimulation(float deltaTime) {
        
        if (deltaTime <= 0.0f || timeScale <= 0.0f) {
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

        // initialize timer to 0
        std::chrono::duration<double> elapsedTime(0.0);

        for (size_t i = 0; i < currentSubsteps; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            for (size_t j = 0; j < particleCount; ++j) {
                glm::vec3 delta = particles[j].position - currentPositions[j];
                particles[j].velocity = delta / dt * glm::pow(currentEnergyPreservation, dt);
                currentPositions[j] = particles[j].position;
                particles[j].velocity += particles[j].force / particles[j].mass * dt;
                particles[j].position += particles[j].velocity * dt;
            }

            #pragma omp parallel for
            for (size_t j = 0; j < constraints.size(); ++j) {
                Constraint &c = constraints[j];
                glm::vec3 delta = c.p2->position - c.p1->position;
                float dist = glm::length(delta);
                c.cD = dist;
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

            #pragma omp parallel for
            for (size_t j = 0; j < constraints.size(); ++j) {
                Constraint &c = constraints[j];

                if (c.cD > (c.d * c.bDFactor + 0.00001f) * breakDistance && c.d != 0.0f) {
                    //std::cout << "break j: " << j << std::endl;
                    // remove constraint
                    size_t p1_index = c.p1_index;
                    size_t p2_index = c.p2_index;
                    size_t p1_i = p1_index / width;
                    size_t p1_j = p1_index % width;
                    size_t p2_i = p2_index / width;
                    size_t p2_j = p2_index % width;
                    if (p1_i == p2_i) {
                        size_t p1_j_min = std::min(p1_j, p2_j);
                        if (p1_i > 0) {
                            isQuadVisible[(p1_i - 1) * (width - 1) + p1_j_min] = false;
                        }
                        if (p1_i < height - 1) {
                            isQuadVisible[p1_i * (width - 1) + p1_j_min] = false;
                        }
                    } else {
                        size_t p1_i_min = std::min(p1_i, p2_i);
                        if (p1_j > 0) {
                            isQuadVisible[p1_i_min * (width - 1) + p1_j - 1] = false;
                        }
                        if (p1_j < width - 1) {
                            isQuadVisible[p1_i_min * (width - 1) + p1_j] = false;
                        }
                    }
                    constraints.erase(constraints.begin() + j);
                    --j;
                    continue;
                    //constraints.erase(std::remove_if(constraints.begin(), constraints.end(),
                    //    [&](const Constraint &c1) { return c1.p1 == c.p1 && c1.p2 == c.p2; }), constraints.end()); 
                }
            }
            auto end = std::chrono::high_resolution_clock::now();
            elapsedTime += end - start;
        }
        std::cout << "Time needed for one substep: " 
                  << elapsedTime.count() / static_cast<double>(currentSubsteps) << "s" << std::endl;
    }

    void updateIndexes(glm::vec3 cameraPos) {
        std::vector<float> distances(trisCount);
        size_t w = width - 1;
        size_t h = height - 1;
        
        #pragma omp parallel for
        for (size_t i = 0; i < h; ++i) {
            for (size_t j = 0; j < w; ++j) {
                glm::vec3 pos = particles[i * width + j].position
                    + particles[i * width + j + 1].position
                    + particles[(i + 1) * width + j].position;
                pos /= 3.0f;
                distances[2*(i * w + j)] = glm::length2(pos - cameraPos);

                pos = particles[i * width + j + 1].position
                    + particles[(i + 1) * width + j + 1].position
                    + particles[(i + 1) * width + j].position;
                pos /= 3.0f;
                distances[2*(i * w + j) + 1] = glm::length2(pos - cameraPos);
            }
        }

        #ifdef PARALLEL
        tbb::task_arena arena(8);
        arena.execute([&] {
            std::sort(std::execution::par, sortTris.begin(), sortTris.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] > distances[b];
            });
        });
        #else
        std::sort(sortTris.begin(), sortTris.end(), [&](uint32_t a, uint32_t b) {
            return distances[a] > distances[b];
        });
        #endif

        std::vector<uint32_t> indexData(indexCount);
        #pragma omp parallel for
        for (size_t t = 0; t < trisCount; ++t) {
            size_t quadIndex = sortTris[t] / 2;
            size_t triIndex = sortTris[t] % 2;
            size_t i = quadIndex / w;
            size_t j = quadIndex % w;
            if (!isQuadVisible[quadIndex]) {
                continue;   
            }
            if (triIndex == 0) {
                indexData[3*t+0] = (i * width + j);
                indexData[3*t+1] = (i * width + j + 1);
                indexData[3*t+2] = ((i + 1) * width + j);
            } else {
                indexData[3*t+0] = (i * width + j + 1);
                indexData[3*t+1] = ((i + 1) * width + j + 1);
                indexData[3*t+2] = ((i + 1) * width + j);
            }
        }

        queue.writeBuffer(indexBuffer, 0, indexData.data(), indexCount * sizeof(uint32_t));
    }

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