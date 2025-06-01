#include "HC.hpp"
#include <list>

void HC::build(SplatVector splats_init, bool verbose) {
    std::priority_queue<Node::Ptr, std::vector<Node::Ptr>, HCComparator> queue;
    splats.reserve(2 * splats_init.size());

    if (verbose) {
        std::cout << "HC: Building tree with " << splats_init.size() << " splats." << std::endl;
    }
    for (const auto &splat : splats_init) {
        auto node = std::make_shared<Node>();
        node->depth = 0;
        node->index = splats.size();
        node->splat = splat;
        node->error = 0.0f;
        node->nodes_it = nodes.insert(nodes.end(), node);
        splats.push_back(splat);
    }

    if (verbose) {
        std::cout << "HC: Building queue with " << nodes.size() << " nodes." << std::endl;
    }

    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        auto a = *it;
        for (auto jt = std::next(it); jt != nodes.end(); ++jt) {
            auto b = *jt;
            //std::cout << "HC: Merging nodes " << a->index << " and " << b->index << std::endl;
            auto node = merge_nodes(a, b);
            //std::cout << "HC: Merged nodes " << a->index << " and " << b->index
            //          << " into node " << node->index << " with error " << node->error << std::endl;
            queue.push(node);
        }
    }

    if (verbose) {
        std::cout << "HC: Merging " << nodes.size() << " nodes." << std::endl;
    }

    while (!queue.empty()) {
        auto merged = queue.top();
        queue.pop();
        if (merged->children[0]->processed || merged->children[1]->processed) {
            continue;
        }

        merged->index = splats.size();
        splats.push_back(merged->splat);

        //std::cout << "HC: Merging nodes: "
        //          << merged->children[0]->index << " and "
        //          << merged->children[1]->index
        //          << " into node " << merged->index
        //          << " with error " << merged->error << std::endl;

        // remove merged nodes from list
        auto a = merged->children[0];
        auto b = merged->children[1];
        a->processed = true;
        b->processed = true;
        //std::cout << "HC: Unprocessed nodes >: " << nodes.size() << std::endl;
        if (std::distance(a->nodes_it, b->nodes_it) == 0) {
            // they're the same; erase only once
            throw std::runtime_error("Same node, this should not happen in HC.");
        } else {
            // erase in order to keep iterators valid
            if (std::distance(a->nodes_it, b->nodes_it) > 0) {
                nodes.erase(b->nodes_it);
                nodes.erase(a->nodes_it);
            } else {
                nodes.erase(a->nodes_it);
                nodes.erase(b->nodes_it);
            }
        }
        //std::cout << "HC: Unprocessed nodes :<" << nodes.size() << std::endl;

        // merge the merged node with all other nodes
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            auto c = *it;
            auto node = merge_nodes(merged, c);
            if (node->error > params.max_error) {
                continue;
            }
            queue.push(node);
        }
        
        // insert merged node into list
        merged->nodes_it = nodes.insert(nodes.end(), merged);
    }

    if (verbose) {
        std::cout << "HC: Found " << nodes.size() << " root nodes." << std::endl;
        std::cout << "HC: First root depth: " << nodes.front()->depth << std::endl;
        std::cout << "HC: Built tree with " << splats.size() << " splats." << std::endl;
    }
            
}

Indices HC::get_indices(Camera::Ptr camera, float threshold, MetricWeights w) {
    Indices indices;
    std::vector<Node::Ptr> queue;
    uint32_t pos{0};

    for (auto &node : nodes) {
        queue.push_back(node);
    }

    auto camera_pos = glm::vec3(camera->worldMatrix[3]);
    while (!queue.empty() && pos < queue.size()) {
        auto node = queue[pos];
        auto splat_pos = glm::vec3(node->splat.transform[3]);
        auto splat_dist = glm::length(splat_pos - camera_pos);
        auto weight = node->splat.weight();
        auto error = node->error;
        auto dist = 1 / (splat_dist * splat_dist);
        //float metric = glm::pow(error, w.e) *
        //    glm::pow(weight, w.w) * glm::pow(dist, w.d);
        float metric = error * weight * dist;
        if (metric < threshold || node->is_leaf()) {
            pos++;
            continue;
        }
        queue.erase(queue.begin() + pos);
        queue.push_back(node->children[0]);
        queue.push_back(node->children[1]);
    }
    for (auto &node : queue) {
        indices.push_back(node->index);
    }
    return indices;
}

Indices HC::get_indices_depth(uint32_t depth) {
    std::vector<uint32_t> indices;
    std::vector<Node::Ptr> queue;
    uint32_t counter{0};
    uint32_t pos{0};
    //std::cout << "HC: Getting indices at depth " << depth << std::endl;
    for (auto &node : nodes) {
        queue.push_back(node);
    }
    while (!queue.empty() && pos < queue.size()) {
        if (counter >= depth) {
            break;
        }
        counter++;
        auto node = queue[pos];
        if (node->is_leaf()) {
            pos++;
            continue;
        }
        queue.erase(queue.begin() + pos);
        queue.push_back(node->children[0]);
        queue.push_back(node->children[1]);
        //if (node->depth == depth) {
        //    indices.push_back(node->index);
        //} else if (node->depth > depth) {
        //    queue.push_back(node->children[0]);
        //    queue.push_back(node->children[1]);
        //}
    }
    for (auto &node : queue) {
        indices.push_back(node->index);
    }
    //std::cout << "HC: Found " << indices.size() << " splats at depth " << depth << std::endl;
    return indices;
}