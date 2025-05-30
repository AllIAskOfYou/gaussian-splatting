#pragma once

#include <webgpu/webgpu.hpp>
#include <vector>
#include <algorithm>
#include <execution>
#include "Splat.h"
#include "ResourceManager.h"
#include "Camera.h"
#include <memory>
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
#include "Octree.hpp"
#include "gui.hpp"
using namespace std;

class SplatMesh {
public:
    SplatVector splatData;
    std::vector<uint32_t> indices;

    Buffer splatBuffer;
    Buffer sortIndexBuffer;

    Buffer quadBuffer;
    Buffer indexQuadBuffer;

    std::vector<VertexAttribute> splatAttribs;
    std::vector<VertexAttribute> quadAttribs;
    std::vector<VertexBufferLayout> vertexBufferLayouts;

    Device device;
    Queue queue;

    virtual void render(RenderPassEncoder &renderPass,
            Camera::Ptr camera, GUI::Parameters &params) {
        (void)params; // to avoid unused parameter warning
        // Set the vertex buffer and index buffer for the splat mesh
        setBuffers(renderPass);
        auto cameraPos = glm::vec3(camera->worldMatrix[3]);
        sortSplats(indices, cameraPos);
        queue.writeBuffer(sortIndexBuffer, 0, indices.data(),
            indices.size() * sizeof(uint32_t));
        renderPass.drawIndexed(6, indices.size(), 0, 0, 0);
    }

    virtual void loadData(const std::string &path, bool center) {
        SplatSplitVector splats_s = ResourceManager::loadSplatsRaw(path, center);
        std::cout << splats_s.size() << " splats loaded from " << path << std::endl;
        splatData.resize(splats_s.size());
        for (size_t i = 0; i < splats_s.size(); i++) {
            splatData[i] = split_to_splat(splats_s[i]);
        }
        indices.resize(splatData.size());
        std::iota(indices.begin(), indices.end(), 0);
    }

    void initialize(Device &device, Queue &queue) {
        this->device = device;
        this->queue = queue;
        initializeBuffers();
        initializeVertexBufferLayouts();
    }

    void setBuffers(RenderPassEncoder &renderPass) {
        renderPass.setVertexBuffer(0, quadBuffer, 0, quadBuffer.getSize());
        renderPass.setIndexBuffer(indexQuadBuffer, IndexFormat::Uint16, 0, indexQuadBuffer.getSize());
    }

    void sortSplats(std::vector<uint32_t> &indices,
            glm::vec3 cameraPos) {
        // measure the time needed to sort the splats

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<float> distances(splatData.size());
        for (uint32_t i : indices) {
            glm::vec3 position = glm::vec3(splatData[i].transform[3]);
            distances[i] = glm::distance(position, cameraPos);
        }


        #ifdef PARALLEL
        tbb::task_arena arena(8);
        arena.execute([&] {
            std::sort(std::execution::par, indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] > distances[b];
            });
        });
        #else
        std::sort(indices.begin(), indices.end(), [&](uint32_t a, uint32_t b) {
            return distances[a] > distances[b];
        });
        #endif
        

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time needed to sort the splats: " << elapsed.count() << "s" << std::endl;
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
        quadAttribs[0].format = VertexFormat::Float32x2;
        quadAttribs[0].offset = 0;

        vertexBufferLayouts[0].attributeCount = static_cast<uint32_t>(quadAttribs.size());
        vertexBufferLayouts[0].attributes = quadAttribs.data();
        vertexBufferLayouts[0].arrayStride = sizeof(glm::vec2);
        vertexBufferLayouts[0].stepMode = VertexStepMode::Vertex;
    }
};