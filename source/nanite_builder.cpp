#include "utils/utils.h"
#include <algorithm>
#include <metis.h>
#include <Nanity_builder.h>
#include <utils/log.h>
#include <vector>

namespace Nanity {

using MehsletIndex = uint32;
using VertexIndex  = uint32;

NanityBuilder::NanityBuilder(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices):
    m_indices(indices),
    m_vertices(vertices) {
}

MeshletsContext NanityBuilder::Build() const {
    constexpr uint32 kMeshletVertexMaxNum   = 64;
    constexpr uint32 kMeshletTriangleMaxNum = 124;
    constexpr float  kConeWeight            = 0.0f;

    MeshletsContext context {};

    // 构建meshopt meshlet
    std::vector<Meshlet> meshlets;
    size_t max_meshlets = meshopt_buildMeshletsBound(m_indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    meshlets.resize(max_meshlets);

    std::vector<uint32> meshlet_vertices(max_meshlets * kMeshletVertexMaxNum);
    std::vector<uint8>  meshlet_triangles(max_meshlets * kMeshletTriangleMaxNum * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
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
    meshlets.resize(meshlet_count);
    auto& last_meshlet = meshlets.back();
    meshlet_vertices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    meshlet_triangles.resize(
        last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3)
    ); // 对齐到4的倍数

    std::vector<uint32_t> meshlet_triangles_u32;
    for (auto& m: meshlets) {
        uint32 triangle_offset = static_cast<uint32>(meshlet_triangles_u32.size());

        for (uint32 i = 0; i < m.triangle_count; ++i) {
            uint32 i0 = 3 * i + 0 + m.triangle_offset;
            uint32 i1 = 3 * i + 1 + m.triangle_offset;
            uint32 i2 = 3 * i + 2 + m.triangle_offset;

            uint8 vIdx0 = meshlet_triangles[i0];
            uint8 vIdx1 = meshlet_triangles[i1];
            uint8 vIdx2 = meshlet_triangles[i2];

            uint32_t packed = ((static_cast<uint32_t>(vIdx0) & 0xFF) << 0) |
                              ((static_cast<uint32_t>(vIdx1) & 0xFF) << 8) |
                              ((static_cast<uint32_t>(vIdx2) & 0xFF) << 16);
            meshlet_triangles_u32.push_back(packed);
        }
        m.triangle_offset = triangle_offset;
    }

    // 拷贝
    context.meshlets  = std::move(meshlets);
    context.triangles = std::move(meshlet_triangles_u32);
    context.vertices  = std::move(meshlet_vertices);

    return context;
}

} // namespace Nanity