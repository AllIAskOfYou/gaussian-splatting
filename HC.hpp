#pragma once

#include <glm/glm.hpp>

#include "BB.hpp"
#include "Splat.h"
#include <vector>
#include <array>
#include <list>
#include <queue>
#include <memory>

#include <iostream>
#include <chrono>

class HC {
public:
    struct Params {
        float max_error{std::numeric_limits<float>::infinity()};
    };

    struct MetricWeights {
        float e{0.0f};
        float w{0.0f};
        float d{0.0f};
    };

    class Node {
    public:
        using Ptr = std::shared_ptr<Node>;
        uint32_t depth;
        Splat splat;
        uint32_t index;
        std::array<Ptr, 2> children;
        float error{0.0f};
        bool is_leaf() const {
            return children[0] == nullptr && children[1] == nullptr;
        }
        std::list<Ptr>::iterator nodes_it;
        bool processed{false};
    };

public:
    Params params;
    std::list<Node::Ptr> nodes;
    SplatVector splats;
private:
    struct HCComparator {
        bool operator()(const Node::Ptr &a, const Node::Ptr &b) const {
            return a->error > b->error; 
        }
    };

public:
    HC() = default;
    HC(const Params &params) : params(params) {}

    void build(SplatVector splats_init, bool verbose = true);
    Indices get_indices(
        Camera::Ptr camera, float threshold, MetricWeights w);
    Indices get_indices_depth(uint32_t depth);

private:
    Node::Ptr merge_nodes(const Node::Ptr &a, const Node::Ptr &b) {
        //std::cout << "Merging nodes: "
        //          << "a: " << a->index << " b: " << b->index
        //          << std::endl;
        //std::cout << "Splats size: " << splats.size() << std::endl;
        auto splat_a = splats[a->index];
        //std::cout<< "Got splat a" << std::endl;
        auto splat_b = splats[b->index];
        auto w_a = splat_a.weight();
        auto w_b = splat_b.weight();
        auto total_weight = w_a + w_b;
        w_a /= total_weight;
        w_b /= total_weight;

        //std::cout << "MERGER: weights: "
        //          << w_a << " " << w_b << std::endl;
        auto splat_c = merge_splats(splat_a, splat_b, w_a, w_b);
        //std::cout << "MERGER: merged splats" << std::endl;
        auto div_a = splat_divergence(splat_a, splat_c);
        auto div_b = splat_divergence(splat_b, splat_c);
        auto error = (w_a * div_a + w_b * div_b);

        auto node = std::make_shared<Node>();
        node->depth = std::max(a->depth, b->depth) + 1;
        node->splat = splat_c;
        node->error = error;
        node->children[0] = a;
        node->children[1] = b;

        return node;
    }
};
