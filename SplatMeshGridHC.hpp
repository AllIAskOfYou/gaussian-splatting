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
#include "GridHC.hpp"
#include "gui.hpp"
#include "SplatMesh.h"
using namespace std;

class SplatMeshGridHC : public SplatMesh{
public:
    GridHC gridhc;
    
    void render(RenderPassEncoder &renderPass,
            Camera::Ptr camera, GUI::Parameters &params) override;

    void loadData(const std::string &path, bool center) override;
};