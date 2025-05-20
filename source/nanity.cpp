#include "nanity.h"
#include "glm/ext/quaternion_geometric.hpp"
#include "glm/geometric.hpp"
#include "meshoptimizer.h"
#include "utils/utils.h"
#include <metis.h>
#include <nanity.h>
#include <utils/log.h>
#include <vector>

namespace Nanity {

using MehsletIndex = uint32;
using VertexIndex  = uint32;

NanityBuilder::NanityBuilder(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices):
    m_indices(indices),
    m_vertices(vertices) {
    // size_t              index_count = indices.size();
    // std::vector<uint32> remap(indices.size());
    // size_t              vertex_count =
    //     meshopt_generateVertexRemap(&remap[0], indices.data(), index_count, &vertices[0], index_count, sizeof(Vertex));

    // std::vector<Vertex> remapVertices(vertex_count);
    // std::vector<uint32> remapIndices(index_count);

    // meshopt_remapVertexBuffer(remapVertices.data(), vertices.data(), index_count, sizeof(Vertex), remap.data());
    // meshopt_remapIndexBuffer(remapIndices.data(), indices.data(), index_count, remap.data());

    // meshopt_optimizeVertexCache(remapIndices.data(), remapIndices.data(), index_count, vertex_count);
    // meshopt_optimizeVertexFetch(
    //     remapVertices.data(),
    //     remapIndices.data(),
    //     index_count,
    //     remapVertices.data(),
    //     vertex_count,
    //     sizeof(Vertex)
    // );

    // m_indices  = std::move(remapIndices);
    // m_vertices = std::move(remapVertices);
}

uint32 PackCone(Vector3f normal, float cutoff) {
    // 从[-1,1]转换到[0,1]范围
    normal = (normal + 1.0f) * 0.5f;

    // 将所有分量缩放到[0,255]并转为整数
    uint32 x = static_cast<uint32>(Math::clamp(normal[0] * 255.0, 0.0, 255.0));
    uint32 y = static_cast<uint32>(Math::clamp(normal[1] * 255.0, 0.0, 255.0));
    uint32 z = static_cast<uint32>(Math::clamp(normal[2] * 255.0, 0.0, 255.0));
    uint32 w = static_cast<uint32>(Math::clamp(cutoff * 255.0, 0.0, 255.0));

    // 打包成32位整数，每个分量占8位
    return (x << 0) | (y << 8) | (z << 16) | (w << 24);
}

MeshletsContext NanityBuilder::Build() const {
    constexpr uint32 kMeshletVertexMaxNum   = 64;
    constexpr uint32 kMeshletTriangleMaxNum = 124;
    constexpr float  kConeWeight            = 0.25f;

    MeshletsContext context {};

    // 构建meshopt meshlet
    std::vector<meshopt_Meshlet> meshlets;
    size_t max_meshlets = meshopt_buildMeshletsBound(m_indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    meshlets.resize(max_meshlets);

    std::vector<uint32> meshlet_vertices(max_meshlets * kMeshletVertexMaxNum);
    std::vector<uint8>  meshlet_triangles(max_meshlets * kMeshletTriangleMaxNum * 3);
    const uint32        vertices_count = m_vertices.size();
    size_t              meshlet_count  = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        m_indices.data(),
        m_indices.size(),
        &m_vertices[0].position.x,
        vertices_count,
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

    std::vector<BoundsData> meshlet_bounds(meshlets.size());
    // 填充meshlet信息到context
    for (int i = 0; i < meshlets.size(); i++) {
        auto& meshlet = meshlets[i];
        auto& bounds  = meshlet_bounds[i];

        // 优化meshlet的局部性
        meshopt_optimizeMeshlet(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            meshlet.vertex_count
        );

        // 获取meshlet的包围体和法线锥
        meshopt_Bounds meshopt_bounds = meshopt_computeMeshletBounds(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            &m_vertices[0].position.x,
            vertices_count,
            sizeof(m_vertices[0])
        );

        Vector3f center { meshopt_bounds.center[0], meshopt_bounds.center[1], meshopt_bounds.center[2] };
        Vector3f apex { meshopt_bounds.cone_apex[0], meshopt_bounds.cone_apex[1], meshopt_bounds.cone_apex[2] };
        Vector3f axis(meshopt_bounds.cone_axis[0], meshopt_bounds.cone_axis[1], meshopt_bounds.cone_axis[2]);

        float angle          = acos(meshopt_bounds.cone_cutoff); // cone_cutoff 是 cos(角度)，需要转换为 -cos(角度+90°)
        float modifiedCutoff = -cos(angle + 1.57079632679f); // 1.57... 是 90° 的弧度值
        float apex_offset    = Math::dot(center - apex, axis);

        bounds.sphere      = Vector4f(center, meshopt_bounds.radius);
        bounds.normal_cone = PackCone(axis, modifiedCutoff);
        bounds.apex_offset = apex_offset;
    }

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
    context.bounds    = std::move(meshlet_bounds);

    return context;
}

} // namespace Nanity