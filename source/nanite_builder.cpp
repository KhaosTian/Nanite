#include "utils/utils.h"
#include <algorithm>
#include <metis.h>
#include <nanite_builder.h>
#include <utils/log.h>
#include <vector>

namespace Nanite {

using MehsletIndex = uint32;
using VertexIndex  = uint32;

NaniteBuilder::NaniteBuilder(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices):
    m_indices(indices),
    m_vertices(vertices) {
}

void FuseVertices(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices) {
}

MeshletsContext NaniteBuilder::Build() const {
    constexpr uint32 kMeshletVertexMaxNum   = 64;
    constexpr uint32 kMeshletTriangleMaxNum = 124;
    constexpr float  kConeWeight            = 0.0f;

    MeshletsContext context {};

    // 构建meshopt meshlet
    size_t max_meshlets = meshopt_buildMeshletsBound(m_indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    context.meshlets.resize(max_meshlets);

    std::vector<uint32> indices(max_meshlets * kMeshletVertexMaxNum);
    std::vector<uint8>  primitives(max_meshlets * kMeshletTriangleMaxNum * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        context.meshlets.data(),
        indices.data(),
        primitives.data(),
        m_indices.data(),
        m_indices.size(),
        &m_vertices[0].position.x,
        m_vertices.size(),
        sizeof(m_vertices[0]),
        kMeshletVertexMaxNum,
        kMeshletTriangleMaxNum,
        kConeWeight
    );

    // 裁剪多余的数组元素
    context.meshlets.resize(meshlet_count);
    auto& last_meshlet = context.meshlets.back();
    indices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    primitives.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3)); // 对齐到4的倍数

    // 拷贝
    context.primitives.clear();
    context.primitives.reserve(primitives.size());
    for (auto val: primitives) {
        context.primitives.push_back(static_cast<uint32_t>(val));
    }

    context.indices = std::move(indices);

    return context;
}

} // namespace Nanite