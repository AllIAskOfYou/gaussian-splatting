#pragma once

#include <webgpu/webgpu.hpp>
#include <vector>
#include <algorithm>
#include <execution>
#include "Splat.h"
#include "ResourceManager.h"
#include "Camera.h"
#include <memory>
// include glm header for printing matrices and vectors
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
#include "HC.hpp"
#include "gui.hpp"
#include "SplatMesh.h"
using namespace std;

class SplatMeshHC : public SplatMesh{
public:
    HC hc;
    
    void render(RenderPassEncoder &renderPass,
            Camera::Ptr camera, GUI::Parameters &params) override {
        // Set the vertex buffer and index buffer for the splat mesh
        setBuffers(renderPass);
        // time indices
        auto start = std::chrono::high_resolution_clock::now();
        indices = hc.get_indices_depth(params.depth);
        //std::vector<uint32_t> indices =
        //    octree.get_indices(camera, params.min_screen_area*0.01);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time needed to get indices: " << elapsed.count() << "s" << std::endl;
        //std::cout << "Rendering " << indices.size() << " splats at depth "
        //    << params.depth << std::endl;
        auto cameraPos = glm::vec3(camera->worldMatrix[3]);
        sortSplats(indices, cameraPos);
        queue.writeBuffer(sortIndexBuffer, 0, indices.data(),
            indices.size() * sizeof(uint32_t));
        renderPass.drawIndexed(6, indices.size(), 0, 0, 0);
    }

    void loadData(const std::string &path, bool center) override {
        //bool success = ResourceManager::loadSplats(path, splatData, center);       
        //if (!success) {
        //    std::cerr << "Could not load splat!" << std::endl;
        //    exit(1);
        //}
        //splatCount = static_cast<uint32_t>(splatData.size());	

        SplatSplitVector splats_s = ResourceManager::loadSplatsRaw(path, center);
        SplatVector splats(splats_s.size());
        for (size_t i = 0; i < splats_s.size(); i++) {
            splats[i] = split_to_splat(splats_s[i]);
        }
        // keep only the first 100 splats
        const uint32_t maxSplats = 500;
        if (splats.size() > maxSplats) {
            splats.resize(maxSplats);
        }
        //SplatVector splats_new;
        //for (auto splat : splats) {
        //    if (splat.transform[3][0] > 1.25f) {
        //        splats_new.push_back(splat);
        //    }
        //}
        //splats = splats_new;
        std::cout << splats.size() << " splats loaded from " << path << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        hc.build(splats);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time needed to build HC: " << elapsed.count() << "s" << std::endl;
        std::cout << "HC built with " << hc.splats.size() << " splats." << std::endl;
        splatData = hc.splats;
    }
};