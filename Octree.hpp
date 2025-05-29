#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <iostream>

#include "Splat.h"
#include "BB.hpp"
#include "Camera.h"

// define colors up to index 32
const std::vector<glm::vec4> COLORS = {
    glm::vec4(1.0f, 0.0f, 0.0f, 1.0f), // red
    glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), // green
    glm::vec4(0.0f, 0.0f, 1.0f, 1.0f), // blue
    glm::vec4(1.0f, 1.0f, 0.0f, 1.0f), // yellow
    glm::vec4(1.0f, 0.0f, 1.0f, 1.0f), // magenta
    glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), // cyan
    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), // gray
    glm::vec4(1.0f, 0.5f, 0.0f, 1.0f), // orange
    /*
    glm::vec4(0.5f, 0.0f, 0.5f, 1.0f), // purple
    glm::vec4(0.5f, 0.5f, 1.0f, 1.0f), // light blue
    glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), // light red
    glm::vec4(0.5f, 1.0f, 0.5f, 1.0f), // light green
    glm::vec4(0.5f, 0.5f, 0.0f, 1.0f), // olive
    glm::vec4(0.0f, 0.5f, 0.5f, 1.0f), // teal
    glm::vec4(0.5f, 0.0f, 0.0f, 1.0f), // dark red
    glm::vec4(0.0f, 0.0f, 0.5f, 1.0f), // dark blue
    glm::vec4(0.0f, 0.5f, 0.0f, 1.0f), // dark green
    glm::vec4(0.5f, 0.5f, 0.5f, 1.0f), // dark gray
    glm::vec4(0.8f, 0.8f, 0.8f, 1.0f), // light gray
    glm::vec4(0.9f, 0.7f, 0.5f, 1.0f), // light brown
    glm::vec4(0.7f, 0.9f, 0.5f, 1.0f), // light lime
    glm::vec4(0.5f, 0.7f, 0.9f, 1.0f), // light sky blue
    glm::vec4(0.9f, 0.5f, 0.7f, 1.0f), // light pink
    glm::vec4(0.7f, 0.5f, 0.9f, 1.0f), // light purple
    glm::vec4(0.5f, 0.9f, 0.7f, 1.0f), // light mint
    glm::vec4(0.9f, 0.9f, 0.5f, 1.0f), // light yellow
    glm::vec4(0.5f, 0.5f, 0.9f, 1.0f), // light blue-gray
    glm::vec4(0.9f, 0.5f, 0.5f, 1.0f), // light coral
    glm::vec4(0.5f, 0.9f, 0.5f, 1.0f), // light sea green
    glm::vec4(0.9f, 0.7f, 0.9f, 1.0f), // light lavender
    glm::vec4(0.7f, 0.9f, 0.9f, 1.0f), // light cyan
    glm::vec4(0.9f, 0.9f, 0.7f, 1.0f), // light khaki
    glm::vec4(0.7f, 0.7f, 0.9f, 1.0f), // light periwinkle
    glm::vec4(0.9f, 0.7f, 0.7f, 1.0f), // light salmon
    glm::vec4(0.7f, 0.9f, 0.7f, 1.0f), // light mint green
    glm::vec4(0.7f, 0.7f, 0.7f, 1.0f), // light silver
    glm::vec4(0.9f, 0.5f, 0.9f, 1.0f), // light magenta
    */

};

class Octree {
public:
    using Indices = std::vector<uint32_t>;
    class Node {
    public:
        using Ptr = std::shared_ptr<Node>;
        uint32_t depth;
        BB bb;
        Indices indices;
        Indices indices_raw;
        std::vector<Ptr> children;
        bool is_leaf() const {
            return children.empty();
        }
    };
    using NodeVector = std::vector<std::shared_ptr<Node>>;

public:
    Node::Ptr root;
    SplatVector splats;
    uint32_t max_depth{10};
    uint32_t max_splats_per_node{1};

private:
    SplatSplitVector splats_raw;
    NodeVector queue;

public:
    void build(SplatSplitVector splats_raw);
    void generate_debug() {
        // This function generates splats from raw splats and setting a color
        // for all partitions on some depth.

        // find the node at the given depth
        /*
        uint32_t counter{0};
        while (queue[counter]->depth < depth) {
            counter++;
        }
        uint32_t node_index = 0;
        while (counter < queue.size() && queue[counter]->depth == depth) {
            Node::Ptr node = queue[counter];
            counter++;
            // generate splats for the node
            for (auto &index : node->indices) {
                Splat splat = raw_to_splat(splats_raw[index]);
                splat.color = COLORS[node_index % COLORS.size()];
                splats.push_back(splat);
            }

            node_index++;
        }
        */
        for (size_t node_index = 0; node_index < queue.size(); node_index++) {
            Node::Ptr node = queue[node_index];
            //std::cout << node_index << " " << node->depth << " "
            //          << node->indices_raw.size() << std::endl;
            for (auto &index : node->indices_raw) {
                //std::cout << index << " ";
                //std::cout << splat_raw.position.x << " "
                //          << splat_raw.position.y << " "
                //          << splat_raw.position.z << std::endl;
                Splat splat = split_to_splat(splats_raw[index]);
                //std::cout << splat.transform[3].x << " "
                //          << splat.transform[3].y << " "
                //          << splat.transform[3].z << std::endl;
                //splat.color = COLORS[node_index % COLORS.size()];
                //splat.color = COLORS[node->depth % COLORS.size()];
                //splat.color.w = 1.0f;
                splats.push_back(splat);
                node->indices.push_back(splats.size() - 1);
            }
        }
    }

    Indices get_indices_depth(uint32_t depth) {
        Indices indices;
        for (const auto &node : queue) {
            if (node->depth == depth) {
                indices.insert(indices.end(), node->indices.begin(), node->indices.end());
            }
        }
        return indices;            
    }

    SplatVector *data() {
        return &splats;
    }

    void generate();
    Indices get_indices(Camera::Ptr camera, float min_screen_area);

private:
    BB get_bb(SplatSplitVector &splats_raw);
    Splat merge_splats(const Indices &indices);
};