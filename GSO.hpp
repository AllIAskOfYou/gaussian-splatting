#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "Splat.h"

class GSO {
    class Node {
    public:
        uint32_t depth;
        glm::vec3 min;
        glm::vec3 max;
        std::vector<uint32_t> indices;
        std::vector<Node> children;

        Node() = default;
        ~Node() = default;
    };
    typedef std::tuple<glm::vec3, glm::vec3> BB;

public:
    Node root;
    std::vector<Splat> splats;

public:
    GSO(std::vector<SplatRaw> r_splats);
    ~GSO() = default;

    std::vector<uint32_t> get_indices(const glm::vec3 &camera_pos);

private:
    BB get_bb(std::vector<SplatRaw> &r_splats);
    void init_root(std::vector<SplatRaw> &r_splats);
    void split_node(Node node, std::vector<SplatRaw> &r_splats);
};