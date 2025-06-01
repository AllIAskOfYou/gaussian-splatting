#include "SplatMeshGridHC.hpp"

void SplatMeshGridHC::render(RenderPassEncoder &renderPass,
        Camera::Ptr camera, GUI::Parameters &params) {
    // Set the vertex buffer and index buffer for the splat mesh
    setBuffers(renderPass);
    // time indices
    auto start = std::chrono::high_resolution_clock::now();
    indices = gridhc.get_indices_error(params.depth, params.min_screen_area);
    //HC::MetricWeights w{
    //    params.weight_e, params.weight_w, params.weight_d
    //};
    //indices = gridhc.get_indices(camera, params.min_screen_area * 0.1, w);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time needed to get indices: " << elapsed.count() << "s" << std::endl;
    //std::cout << "Rendering " << indices.size() << " splats at depth "
    //    << params.depth << std::endl;
    auto cameraPos = glm::vec3(camera->worldMatrix[3]);
    sortSplats(indices, cameraPos);
    queue.writeBuffer(sortIndexBuffer, 0, indices.data(),
        indices.size() * sizeof(uint32_t));
    renderPass.drawIndexed(6, indices.size(), 0, 0, 0);
}

void SplatMeshGridHC::loadData(const std::string &path, bool center) {
    SplatSplitVector splats_s = ResourceManager::loadSplatsRaw(path, center);
    SplatVector splats(splats_s.size());
    for (size_t i = 0; i < splats_s.size(); i++) {
        splats[i] = split_to_splat(splats_s[i]);
    }
    // keep only the first 100 splats
    const uint32_t maxSplats = 1000000;
    if (splats.size() > maxSplats) {
        splats.resize(maxSplats);
    }
    SplatVector splats_new;
    for (auto splat : splats) {
        if (splat.transform[3][0] > 0.0f) {
            splats_new.push_back(splat);
        }
    }
    splats = splats_new;
    std::cout << splats.size() << " splats loaded from " << path << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    gridhc.build(splats);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Time needed to build HC: " << elapsed.count() << "s" << std::endl;
    std::cout << "HC built with " << gridhc.splats.size() << " splats." << std::endl;
    splatData = gridhc.splats;
}