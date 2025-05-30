#include "HC.hpp"
#include <list>

void HC::build(SplatVector splats_init) {
    std::priority_queue<Node::Ptr, std::vector<Node::Ptr>, HCComparator> queue;
    splats.reserve(2 * splats_init.size());

    std::cout << "HC: Building tree with " << splats.size() << " splats." << std::endl;
    for (const auto &splat : splats_init) {
        auto node = std::make_shared<Node>();
        node->depth = 0;
        node->index = nodes.size();
        node->error = 0.0f;
        node->nodes_it = nodes.insert(nodes.end(), node);
        splats.push_back(splat);
    }

    std::cout << "HC: Building queue with " << nodes.size() << " nodes." << std::endl;

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

    std::cout << "HC: Merging " << nodes.size() << " nodes." << std::endl;

    while (!queue.empty()) {
        auto merged = queue.top();
        queue.pop();
        if (merged->children[0]->processed || merged->children[1]->processed) {
            continue;
        }

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
            queue.push(node);
        }
        
        // insert merged node into list
        merged->nodes_it = nodes.insert(nodes.end(), merged);
    }

    std::cout << "HC: Found " << nodes.size() << " root nodes." << std::endl;
    std::cout << "HC: First root depth: " << nodes.front()->depth << std::endl;
    std::cout << "HC: Built tree with " << splats.size() << " splats." << std::endl;
            
}

Indices get_indices(Camera::Ptr camera, float min_screen_area) {
    (void)camera;
    (void)min_screen_area;
    Indices indices;
    return indices;
}

Indices HC::get_indices_depth(uint32_t depth) {
    std::vector<uint32_t> indices;
    std::vector<Node::Ptr> queue;
    for (auto &node : nodes) {
        queue.push_back(node);
    }
    while (!queue.empty()) {
        auto node = queue.back();
        queue.pop_back();
        if (node->depth == depth) {
            indices.push_back(node->index);
        } else if (node->depth > depth) {
            queue.push_back(node->children[0]);
            queue.push_back(node->children[1]);
        }
    }
    std::cout << "HC: Found " << indices.size() << " splats at depth " << depth << std::endl;
    return indices;
}