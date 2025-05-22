#include "nanity.h"
#include "utils/utils.h"
#include <limits>
#include <metis.h>
#include "utils/cityhash.h"

// nanity.cpp
namespace Nanity {

void MeshletBuilder::FuseVertices(std::vector<uint32>& indices, std::vector<Vertex>& vertices) {
    std::vector<Vertex> remapVertices;
    remapVertices.reserve(vertices.size());

    std::vector<uint32> remapIndices;
    remapIndices.reserve(indices.size());

    std::map<uint64, size_t> verticesMap;

    uint32 fuseCount = 0;

    for (uint32 index: indices) {
        const Vertex& vertex = vertices[index];

        const uint64 hashId = cityhash::cityhash64((const char*)&vertex, sizeof(Vertex));
        if (!verticesMap.contains(hashId)) {
            verticesMap[hashId] = remapVertices.size();
            remapVertices.push_back(vertex);
        } else {
            fuseCount++;
        }

        remapIndices.push_back(verticesMap[hashId]);
    }

    indices  = std::move(remapIndices);
    vertices = std::move(remapVertices);
}

MeshletsContext MeshletBuilder::BuildMeshlets(std::vector<uint32>& indices_in, std::vector<Vertex>& vertices_in) {
    //FuseVertices(indices_in, vertices_in);

    size_t original_index_count  = indices_in.size();
    size_t original_vertex_count = vertices_in.size();

    // 第1步：顶点去重和优化
    std::vector<uint32_t> remap_table(original_vertex_count);
    size_t                unique_vertex_count = meshopt_generateVertexRemap(
        remap_table.data(),
        indices_in.empty() ? nullptr : indices_in.data(),
        original_index_count,
        vertices_in.data(),
        original_vertex_count,
        sizeof(Vertex)
    );

    std::vector<Vertex> remapped_vertices(unique_vertex_count);
    meshopt_remapVertexBuffer(
        remapped_vertices.data(),
        vertices_in.data(),
        original_vertex_count,
        sizeof(Vertex),
        remap_table.data()
    );

    std::vector<uint32_t> remapped_indices(original_index_count);
    meshopt_remapIndexBuffer(
        remapped_indices.data(),
        indices_in.empty() ? nullptr : indices_in.data(),
        original_index_count,
        remap_table.data()
    );

    // 优化顶点缓存和顶点提取
    meshopt_optimizeVertexCache(
        remapped_indices.data(),
        remapped_indices.data(),
        original_index_count,
        unique_vertex_count
    );

    meshopt_optimizeVertexFetch(
        remapped_vertices.data(),
        remapped_indices.data(),
        original_index_count,
        remapped_vertices.data(),
        unique_vertex_count,
        sizeof(Vertex)
    );

    // 第2步：构建Meshlet
    constexpr uint32 kMeshletVertexMaxNum   = 64;
    constexpr uint32 kMeshletTriangleMaxNum = 124;
    constexpr float  kConeWeight            = 0.25f;

    MeshletsContext context {};

    std::vector<meshopt_Meshlet> meshlets;
    size_t                       max_meshlets =
        meshopt_buildMeshletsBound(remapped_indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    meshlets.resize(max_meshlets);

    std::vector<uint32> meshlet_vertices(max_meshlets * kMeshletVertexMaxNum);
    std::vector<uint8>  meshlet_triangles(max_meshlets * kMeshletTriangleMaxNum * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        remapped_indices.data(),
        remapped_indices.size(),
        &remapped_vertices[0].position.x,
        remapped_vertices.size(),
        sizeof(remapped_vertices[0]),
        kMeshletVertexMaxNum,
        kMeshletTriangleMaxNum,
        kConeWeight
    );

    // 裁剪多余的数组元素
    meshlets.resize(meshlet_count);
    auto& last_meshlet = meshlets.back();
    meshlet_vertices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    meshlet_triangles.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));

    // 第3步：计算包围体
    std::vector<BoundsData> meshlet_bounds(meshlets.size());
    for (int i = 0; i < meshlets.size(); i++) {
        auto& meshlet = meshlets[i];
        auto& bounds  = meshlet_bounds[i];

        meshopt_optimizeMeshlet(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            meshlet.vertex_count
        );

        Vector3f pos_min = Vector3f(std::numeric_limits<float>::max());
        Vector3f pos_max = Vector3f(std::numeric_limits<float>::lowest());

        for (uint32 triangleId = 0; triangleId < meshlet.triangle_count; triangleId++) {
            for (uint32 vertexId = 0; vertexId < 3; vertexId++) {
                uint8  id  = meshlet_triangles[meshlet.triangle_offset + triangleId * 3 + vertexId];
                uint32 vid = meshlet_vertices[meshlet.vertex_offset + id];
                pos_max    = Math::max(pos_max, remapped_vertices[vid].position);
                pos_min    = Math::min(pos_min, remapped_vertices[vid].position);
            }
        }

        const Vector3f center = 0.5f * (pos_max + pos_min);
        const float    radius = Math::length(pos_max - center);

        // 获取meshlet的包围体和法线锥
        meshopt_Bounds meshopt_bounds = meshopt_computeMeshletBounds(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            &remapped_vertices[0].position.x,
            remapped_vertices.size(),
            sizeof(remapped_vertices[0])
        );

        Vector3f apex { meshopt_bounds.cone_apex[0], meshopt_bounds.cone_apex[1], meshopt_bounds.cone_apex[2] };
        Vector3f axis(meshopt_bounds.cone_axis[0], meshopt_bounds.cone_axis[1], meshopt_bounds.cone_axis[2]);

        float angle          = acos(meshopt_bounds.cone_cutoff);
        float modifiedCutoff = -cos(angle + 1.57079632679f);
        float apex_offset    = Math::dot(center - apex, axis);

        bounds.sphere      = Vector4f(center, radius);
        bounds.normal_cone = PackCone(axis, modifiedCutoff);
        bounds.apex_offset = apex_offset;
    }

    // 第4步：处理三角形数据
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

    // 填充context结构
    context.meshlets     = std::move(meshlets);
    context.triangles    = std::move(meshlet_triangles_u32);
    context.vertices     = std::move(meshlet_vertices);
    context.bounds       = std::move(meshlet_bounds);
    context.opt_vertices = std::move(remapped_vertices);

    return context;
}

// PackCone实现保持不变
uint32 MeshletBuilder::PackCone(Vector3f normal, float cutoff) {
    normal   = (normal + 1.0f) * 0.5f;
    uint32 x = static_cast<uint32>(Math::clamp(normal[0] * 255.0, 0.0, 255.0));
    uint32 y = static_cast<uint32>(Math::clamp(normal[1] * 255.0, 0.0, 255.0));
    uint32 z = static_cast<uint32>(Math::clamp(normal[2] * 255.0, 0.0, 255.0));
    uint32 w = static_cast<uint32>(Math::clamp(cutoff * 255.0, 0.0, 255.0));
    return (x << 0) | (y << 8) | (z << 16) | (w << 24);
}

} // namespace Nanity
