// generate tree and a queue using breath first search
// iterate through the queue backwards to iteratively merge the splats


#include "Octree.hpp"
#include <numeric>
#include <algorithm>

BB Octree::get_bb(SplatSplitVector &splats_raw) {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);

    for (auto &splat : splats_raw) {
        min.x = std::min(min.x, splat.position.x);
        min.y = std::min(min.y, splat.position.y);
        min.z = std::min(min.z, splat.position.z);

        max.x = std::max(max.x, splat.position.x);
        max.y = std::max(max.y, splat.position.y);
        max.z = std::max(max.z, splat.position.z);
    }

    glm::vec3 center = (max + min) / 2.0f;

    // expand around the center to a cube
    float maxSize = std::max({max.x - min.x, max.y - min.y, max.z - min.z});
    min = center - glm::vec3(maxSize / 2.0f);
    max = center + glm::vec3(maxSize / 2.0f);

    return BB::from_aabb(min, max);
}

void Octree::build(SplatSplitVector splats_raw) {
    splats.clear();
    queue.clear();
    this->splats_raw = splats_raw;
    // init root node
    root = std::make_shared<Node>();
    root->depth = 0;
    root->bb = get_bb(splats_raw);
    root->indices_raw.resize(splats_raw.size());
    std::iota(root->indices_raw.begin(), root->indices_raw.end(), 0);

    // init queue for breath first search
    uint32_t counter{0};
    queue.push_back(root);

    while (counter < queue.size()) {
        Node::Ptr node = queue[counter];
        counter++;

        if (node->depth >= max_depth ||
                node->indices_raw.size() <= max_splats_per_node) {
            continue;
        }

        // split the node
        glm::vec3 center = node->bb.center();

        // Create child nodes (some may be removed later)
        std::vector<Node::Ptr> children(8);
        for (int i = 0; i < 8; i++) {
            children[i] = std::make_shared<Node>();
            children[i]->depth = node->depth + 1;
        }

        // Distribute indices to children
        for (auto idx : node->indices_raw) {
            SplatSplit &splat = splats_raw[idx];
            int index = 0;
            if (splat.position.x > center.x) index |= 1;
            if (splat.position.y > center.y) index |= 2;
            if (splat.position.z > center.z) index |= 4;
            children[index]->indices_raw.push_back(idx);
        }

        // Set bounding boxes for children
        for (int i = 0; i < 8; i++) {
            Node::Ptr child = children[i];
            if (child->indices_raw.empty()) {
                continue; // Skip empty children
            }
            glm::vec3 min = node->bb.min();
            glm::vec3 max = node->bb.max();

            if (i & 1) {
                min.x = center.x;
            } else {
                max.x = center.x;
            }
            if (i & 2) {
                min.y = center.y;
            } else {
                max.y = center.y;
            }
            if (i & 4) {
                min.z = center.z;
            } else {
                max.z = center.z;
            }
            child->bb = BB::from_aabb(min, max);

            // Add to the node's children
            node->children.push_back(child);
            // Add to the queue for further processing
            queue.push_back(child);
        }
    }

    // clean the tree by collapsing nodes with one child
    counter = 0;
    uint32_t size = 1;
    queue[0] = root;
    uint32_t collapsed_nodes{0};

    while (counter < size) {
        Node::Ptr node = queue[counter];
        counter++;

        if (node->is_leaf()) {
            continue;
        }

        if (node->children.size() == 1) {
            collapsed_nodes++;
            // Collapse the node
            Node::Ptr child = node->children[0];
            node->depth = child->depth;
            node->bb = child->bb;
            node->indices = child->indices;
            node->indices_raw = child->indices_raw;
            node->children = child->children;
            counter--;
            continue;
        }

        for (auto &child : node->children) {
            queue[size] = child;
            size++;
        }
    }
    queue.resize(counter);


    std::cout << "Octree built with " << queue.size() << " nodes." << std::endl;
    std::cout << "Collapsed " << collapsed_nodes << " nodes." << std::endl;
}

void Octree::generate() {
    queue[0] = root;
    uint32_t counter{0};
    uint32_t size{1};

    splats.clear();
    while (counter < size) {
        Node::Ptr node = queue[counter];
        counter++;

        SplatSplitVector splats_new{node->indices_raw.size()};
        for (uint32_t i = 0; i < node->indices_raw.size(); i++) {
            uint32_t index = node->indices_raw[i];
            splats_new[i] = splats_raw[index];
        }

        //if (node->is_leaf()) {
        //    // generate splats for the node
        //    for (auto &splat : splats_new) {
        //        splats.push_back(split_to_splat(splat));
        //        node->indices.push_back(splats.size() - 1);
        //    }
        //    continue;
        //}

        Splat splat_merged = merge(splats_new);
        splats.push_back(splat_merged);
        node->indices.push_back(splats.size() - 1);

        if (node->is_leaf()) {
            continue;
        }

        for (auto &child : node->children) {
            queue[size] = child;
            size++;
        }
    }
}

Indices Octree::get_indices(Camera::Ptr camera, float min_screen_area) {
    queue[0] = root;
    uint32_t counter{0};
    uint32_t size{1};
    Indices indices;

    while (counter < size) {
        Node::Ptr node = queue[counter];
        counter++;

        float screen_area = node->bb.screen_area(camera);
        //std::cout << "Node at depth " << node->depth
        //          << " with " << node->indices_raw.size() << " splats, "
        //          << "screen area: " << screen_area << std::endl;
        if (node->is_leaf() || (screen_area < min_screen_area)) {
            indices.insert(indices.end(),
                node->indices.begin(), node->indices.end());
            continue;
        }

        for (auto &child : node->children) {
            queue[size] = child;
            size++;
        }
    }
    std::cout << "Found " << indices.size() << " splats." << std::endl;
    return indices;       
}
