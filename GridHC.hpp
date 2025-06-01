#pragma once

#include <vector>
#include <memory>

#include "Splat.h"
#include "HC.hpp"

class GridHC {
public:
    struct Cell {
        using Ptr = std::shared_ptr<Cell>;
        std::vector<Splat> splats;
        HC hc;
        bool empty() const {
            return splats.empty();
        }
    };

public:
    glm::uvec3 subdivisions{20};
    std::vector<Cell::Ptr> cells;
    std::vector<Splat> splats;

public:
    void build(SplatVector splats_init);
    Indices get_indices_error(uint32_t depth, float error);
    Indices get_indices(
        Camera::Ptr camera, float threshold, HC::MetricWeights w);

private:
    uint32_t get_index(uint32_t i, uint32_t j, uint32_t k) const {
        auto index = i * subdivisions.y * subdivisions.z + j * subdivisions.z + k;
        if (index >= cells.size()) {
            throw std::out_of_range("Cell index out of range");
        }
        return index;
    }
};