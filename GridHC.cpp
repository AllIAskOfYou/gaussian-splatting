#include "GridHC.hpp"

#include <iostream>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <string>

#include "BB.hpp"

void GridHC::build(SplatVector splats_init) {
    float density = 4.0f;


    BB bb = BB::from_splats(splats_init, false);
    auto size = bb.size();
    //subdivisions = static_cast<glm::uvec3>(glm::ceil(size * density));
    glm::vec3 cell_size = 1.1f * size / static_cast<glm::vec3>(subdivisions);
    glm::vec3 min = bb.min() - cell_size * (0.1f / 1.1f);

    std::cout << "GridHC: Bounding box built: " << min.x << ", " << min.y << ", " << min.z << std::endl;


    
    cells.clear();
    cells.resize(subdivisions.x * subdivisions.y * subdivisions.z);
    splats.clear();
    splats.reserve(2 * splats_init.size());


    std::cout << "GridHC: Building grid with " << cells.size() << " cells." << std::endl;
    
    for (uint32_t i = 0; i < subdivisions.x; i++) {
        for (uint32_t j = 0; j < subdivisions.y; j++) {
            for (uint32_t k = 0; k < subdivisions.z; k++) {
                auto cell = std::make_shared<Cell>();
                cells[get_index(i, j, k)] = cell;
            }
        }
    }

    for (const auto &splat : splats_init) {
        glm::vec3 position = glm::vec3(splat.transform[3]);
        uint32_t i = static_cast<uint32_t>((position.x - min.x) / cell_size.x);
        uint32_t j = static_cast<uint32_t>((position.y - min.y) / cell_size.y);
        uint32_t k = static_cast<uint32_t>((position.z - min.z) / cell_size.z);
        
        if (i < subdivisions.x && j < subdivisions.y && k < subdivisions.z) {
            cells[get_index(i, j, k)]->splats.push_back(splat);
        }
        else {
            std::cerr << "Splat position out of bounds: "
                      << "i: " << i << ", j: " << j << ", k: " << k
                      << ". Splat will be ignored." << std::endl;
            continue;
        }
    }

    std::cout << "GridHC: Distributed " << splats_init.size() << " splats into "
              << cells.size() << " cells." << std::endl;

    for (uint32_t i = 0; i < subdivisions.x * subdivisions.y * subdivisions.z; i++) {
        auto cell = cells[i];
        std::cout << "GridHC: Processing cell " << i << ", with "
            << cell->splats.size() << "splats" << std::endl;
        if (cell->empty()) {
            continue;
        }
        cell->hc.build(cell->splats, false);
        splats.insert(
            splats.end(), cell->hc.splats.begin(), cell->hc.splats.end());
    }

    std::cout << "GridHC: Built grid with " << splats.size() << " splats." << std::endl;
}

Indices GridHC::get_indices_error(uint32_t depth, float error) {
    Indices indices;
    indices.reserve(splats.size());
    uint32_t offset{0};
    for (const auto &cell : cells) {
        if (cell->empty()) {
            continue;
        }
        auto cell_indices = cell->hc.get_indices_depth(depth);
        for (uint32_t i = 0; i < cell_indices.size(); i++) {
            indices.push_back(cell_indices[i] + offset); 
        }
        offset += cell->hc.splats.size();
    }             
    std::cout << "GridHC: Found " << indices.size() << " splats" << std::endl;
    return indices;
}

Indices GridHC::get_indices(
        Camera::Ptr camera, float threshold, HC::MetricWeights w) {
    Indices indices;
    indices.reserve(splats.size());
    uint32_t offset{0};
    for (const auto &cell : cells) {
        if (cell->empty()) {
            continue;
        }
        auto cell_indices = cell->hc.get_indices(camera, threshold, w);
        for (uint32_t i = 0; i < cell_indices.size(); i++) {
            indices.push_back(cell_indices[i] + offset); 
        }
        offset += cell->hc.splats.size();
    }             
    std::cout << "GridHC: Found " << indices.size() << " splats" << std::endl;
    return indices;
}