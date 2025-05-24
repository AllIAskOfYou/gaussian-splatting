#pragma once

#include <webgpu/webgpu.hpp>
#include <vector>
#include <algorithm>
#include <execution>
#include "Splat.h"
#include "ResourceManager.h"
// include library for measuring time
#include <chrono>

// include library for parallel execution
#include <thread>
#include <future>
#include <tbb/task_arena.h>


using namespace wgpu;


// C++ program to perform
// iterative merge sort.
#include <iostream>
#include <vector>
using namespace std;

class SplatMesh {
public:
    std::vector<Splat> splatData;
    uint32_t splatCount;

    Buffer splatBuffer;
    Buffer sortIndexBuffer;

    Buffer quadBuffer;
    Buffer indexQuadBuffer;

    std::vector<VertexAttribute> splatAttribs;
    std::vector<VertexAttribute> quadAttribs;
    std::vector<VertexBufferLayout> vertexBufferLayouts;

    Device device;
    Queue queue;

    std::vector<uint32_t> sortIndices;

    void loadData(const std::string &path, bool center) {
        bool success = ResourceManager::loadSplats(path, splatData, center);       
        if (!success) {
            std::cerr << "Could not load splat!" << std::endl;
            exit(1);
        }
        splatCount = static_cast<uint32_t>(splatData.size());	
    }

    void initialize(Device &device, Queue &queue) {
        this->device = device;
        this->queue = queue;
        initializeBuffers();
        initializeVertexBufferLayouts();
        sortIndices.resize(splatCount);
        for (uint32_t i = 0; i < splatCount; i++) {
            sortIndices[i] = i;
        }
        //queue.writeBuffer(sortIndexBuffer, 0, sortIndices.data(), splatCount * sizeof(uint32_t)); 
    }

    void update(glm::vec3 cameraPos) {
        sortIndices = sortSplats(cameraPos);
        queue.writeBuffer(sortIndexBuffer, 0, sortIndices.data(), splatCount * sizeof(uint32_t)); 
    }

    void setBuffers(RenderPassEncoder &renderPass) {
        renderPass.setVertexBuffer(0, quadBuffer, 0, quadBuffer.getSize());
        renderPass.setIndexBuffer(indexQuadBuffer, IndexFormat::Uint16, 0, indexQuadBuffer.getSize());
    }

    std::vector<uint32_t> sortSplats(glm::vec3 cameraPos) {
        // measure the time needed to sort the splats

        std::vector<uint32_t> sort_indices(splatCount);
        for (uint32_t i = 0; i < splatCount; i++) {
            sort_indices[i] = sortIndices[i];
        }

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<float> distances(splatCount);
        for (uint32_t i = 0; i < splatCount; i++) {
            glm::vec3 position = glm::vec3(splatData[i].transform[3]);
            distances[i] = glm::distance(position, cameraPos);
        }


        #ifdef PARALLEL
        tbb::task_arena arena(8);
        arena.execute([&] {
            std::sort(std::execution::par, sort_indices.begin(), sort_indices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] > distances[b];
            });
        });
        #else
        std::sort(sort_indices.begin(), sort_indices.end(), [&](uint32_t a, uint32_t b) {
            return distances[a] > distances[b];
        });
        #endif
        

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time needed to sort the splats: " << elapsed.count() << "s" << std::endl;

        return sort_indices;
    }

    ~SplatMesh() {
        splatBuffer.release();
        sortIndexBuffer.release();
        quadBuffer.release();
        indexQuadBuffer.release();
        vertexBufferLayouts.clear();
    }

private:
    void initializeBuffers() {
        initializeSplatBuffer();
        initializeSortIndexBuffer();
        initializeQuadBuffer();
        initializeIndexQuadBuffer();
    }

    void initializeVertexBufferLayouts() {
        vertexBufferLayouts.resize(1);

        // Quad atributes (per vertex)
        initializeQuadBufferLayout();
    }

    void initializeSplatBuffer() {
        BufferDescriptor bufferDesc;
        bufferDesc.size = splatData.size() * sizeof(Splat);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage;
        bufferDesc.mappedAtCreation = false;
        splatBuffer = device.createBuffer(bufferDesc);

        queue.writeBuffer(splatBuffer, 0, splatData.data(), bufferDesc.size);
    }

    void initializeSortIndexBuffer() {
        BufferDescriptor bufferDesc;
        bufferDesc.size = splatData.size() * sizeof(uint32_t);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage;
        bufferDesc.mappedAtCreation = false;
        sortIndexBuffer = device.createBuffer(bufferDesc);
    }

    void initializeIndexQuadBuffer() {        
        std::vector<uint16_t> indexQuadData = {
            0, 1, 2, 0, 2, 3
        };

        BufferDescriptor bufferDesc;
        bufferDesc.size = indexQuadData.size() * sizeof(uint16_t);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Index;
        indexQuadBuffer = device.createBuffer(bufferDesc);

        queue.writeBuffer(indexQuadBuffer, 0, indexQuadData.data(), bufferDesc.size);
    }

    void initializeQuadBuffer() {
        std::vector<glm::vec2> quadData = {
            glm::vec2(-1.0f, -1.0f),
            glm::vec2(1.0f, -1.0f),
            glm::vec2(1.0f, 1.0f),
            glm::vec2(-1.0f, 1.0f)
        };
        
        BufferDescriptor bufferDesc;
        bufferDesc.size = quadData.size() * sizeof(glm::vec2);
        bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Vertex;
        quadBuffer = device.createBuffer(bufferDesc);

        // Copy the quad data to the buffer
        queue.writeBuffer(quadBuffer, 0, quadData.data(), bufferDesc.size);
    }

    void initializeQuadBufferLayout() {
        quadAttribs.resize(1);

        // Describe the position attribute
        quadAttribs[0].shaderLocation = 0; // @location(4)
        quadAttribs[0].format = VertexFormat::Float32x3;
        quadAttribs[0].offset = 0;

        vertexBufferLayouts[0].attributeCount = static_cast<uint32_t>(quadAttribs.size());
        vertexBufferLayouts[0].attributes = quadAttribs.data();
        vertexBufferLayouts[0].arrayStride = sizeof(glm::vec2);
        vertexBufferLayouts[0].stepMode = VertexStepMode::Vertex;
    }
};