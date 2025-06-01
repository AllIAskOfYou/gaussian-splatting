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
#include "SplatMesh.h"
using namespace std;

class SplatMeshOctree : public SplatMesh{
public:
    Octree octree;
    
    void render(RenderPassEncoder &renderPass,
            Camera::Ptr camera, GUI::Parameters &params) override {
        // Set the vertex buffer and index buffer for the splat mesh
        setBuffers(renderPass);
        // time indices
        auto start = std::chrono::high_resolution_clock::now();
        //indices = octree.get_indices_depth(params.depth);
        std::vector<uint32_t> indices =
            octree.get_indices(camera, params.min_screen_area*0.01);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        std::cout << "Time needed to get indices: " << elapsed.count() << "s" << std::endl;
        std::cout << "Rendering " << indices.size() << " splats at depth "
            << params.depth << std::endl;
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
        // keep only the first 100 splats
        //const uint32_t maxSplats = 100;
        //if (splatsRaw.size() > maxSplats) {
        //    splatsRaw.resize(maxSplats);
        //}
        std::cout << splats_s.size() << " splats loaded from " << path << std::endl;
        octree.build(splats_s);
        //octree.generate_debug();
        octree.generate();
        std::cout << "Octree built with " << octree.splats.size() << " splats." << std::endl;
        splatData = octree.splats;
        std::cout << "Splat Transform: " << splatData[0].transform << std::endl;

    }
};