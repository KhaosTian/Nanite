#include "utils/utils.h"
#include <metis.h>
#include <nanite_builder.h>
#include <utils/log.h>

namespace Nanite {

using MehsletIndex = uint32;
using VertexIndex  = uint32;

NaniteBuilder::NaniteBuilder(const std::vector<uint32>& indices, const std::vector<Point3f>& positions):
    m_indices(indices),
    m_positions(positions) {
}

MeshletsContext NaniteBuilder::Build() const {
    constexpr uint32 kMeshletVertexMaxNum   = 64;
    constexpr uint32 kMeshletTriangleMaxNum = 124;
    constexpr uint32 kConeWeight            = 0.0f;

    MeshletsContext context {};

    // 构建meshopt meshlet
    size_t max_meshlets = meshopt_buildMeshletsBound(m_indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    context.meshlets.resize(max_meshlets);
    context.vertices.resize(max_meshlets * kMeshletVertexMaxNum);
    context.triangles.resize(max_meshlets * kMeshletTriangleMaxNum * 3);

    size_t meshletCount = meshopt_buildMeshlets(
        context.meshlets.data(),
        context.vertices.data(),
        context.triangles.data(),
        m_indices.data(),
        m_indices.size(),
        reinterpret_cast<const float*>(m_positions.data()),
        m_positions.size(),
        sizeof(Vector3f),
        kMeshletVertexMaxNum,
        kMeshletTriangleMaxNum,
        kConeWeight
    );

    auto& last = context.meshlets[meshletCount - 1];
    context.vertices.resize(last.vertex_offset + last.vertex_count);
    context.triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    context.meshlets.resize(meshletCount);
    return context;
}

} // namespace Nanite