#include "GSO.hpp"

GSO::GSO(std::vector<SplatRaw> r_splats) {
    init_root(r_splats);
    split_node(root, r_splats);
}

GSO::BB GSO::get_bb(std::vector<SplatRaw> &r_splats) {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);

    for (auto &splat : r_splats) {
        min.x = std::min(min.x, splat.position.x);
        min.y = std::min(min.y, splat.position.y);
        min.z = std::min(min.z, splat.position.z);

        max.x = std::max(max.x, splat.position.x);
        max.y = std::max(max.y, splat.position.y);
        max.z = std::max(max.z, splat.position.z);
    }

    // expand the bounding box to a cube
    float maxSize = std::max(max.x - min.x, max.y - min.y, max.z - min.z);
    max.x = min.x + maxSize;
    max.y = min.y + maxSize;
    max.z = min.z + maxSize;

    return std::make_tuple(min, max);
}

void GSO::init_root(std::vector<SplatRaw> &r_splats) {
    BB bb = get_bb(r_splats);
    root.min = std::get<0>(bb);
    root.max = std::get<1>(bb);
    root.indices.reserve(r_splats.size());
    for (uint32_t i = 0; i < r_splats.size(); i++) {
        root.indices.push_back(i);
    }
}

void GSO::split_node(Node node, std::vector<SplatRaw> &splats) {


    glm::vec3 center = (node.min + node.max) / 2.0f;

    // Create 8 child nodes
    for (int i = 0; i < splats.size(); i++) {
        SplatRaw &splat = splats[i];
        int index = 0;
        if (splat.position.x > center.x) index |= 1;
        if (splat.position.y > center.y) index |= 2;
        if (splat.position.z > center.z) index |= 4;
        childrenindices[index].push_back(i);
    }

    // Recursively split the children_indices
    for (int i = 0; i < 8; i++) {
        if (children_indices[i].size() > 1) {
            BB childBB;
            if (i & 1) {
                std::get<0>(childBB).x = center.x;
                std::get<1>(childBB).x = max.x;
            } else {
                std::get<0>(childBB).x = min.x;
                std::get<1>(childBB).x = center.x;
            }
            if (i & 2) {
                std::get<0>(childBB).y = center.y;
                std::get<1>(childBB).y = max.y;
            } else {
                std::get<0>(childBB).y = min.y;
                std::get<1>(childBB).y = center.y;
            }
            if (i & 4) {
                std::get<0>(childBB).z = center.z;
                std::get<1>(childBB).z = max.z;
            } else {
                std::get<0>(childBB).z = min.z;
                std::get<1>(childBB).z = center.z;
            }
            split(childBB, children_indices[i]);
        } else {
            splat_data.push_back(children_indices[i][0]);
        }
    }
}