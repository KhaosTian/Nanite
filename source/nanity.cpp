#include "nanity.h"
#include "glm/trigonometric.hpp"
#include "utils/utils.h"
#include <limits>
#include <metis.h>
#include "utils/cityhash.h"

// nanity.cpp
namespace Nanity {

void MeshletBuilder::FuseVertices(std::vector<uint32>& indices_in, std::vector<Vertex>& vertices_in) {
    std::vector<Vertex> remapped_vertices;
    remapped_vertices.reserve(vertices_in.size());

    std::vector<uint32> remapped_indices;
    remapped_indices.reserve(indices_in.size());

    std::map<uint64, size_t> vertices_map;

    uint32 fuse_count = 0;

    for (uint32 index: indices_in) {
        const Vertex& vertex = vertices_in[index];

        const uint64 hashId = cityhash::cityhash64((const char*)&vertex, sizeof(Vertex));
        if (!vertices_map.contains(hashId)) {
            vertices_map[hashId] = remapped_vertices.size();
            remapped_vertices.push_back(vertex);
        } else {
            fuse_count++;
        }

        remapped_indices.push_back(vertices_map[hashId]);
    }

    indices_in  = std::move(remapped_indices);
    vertices_in = std::move(remapped_vertices);
}

void MeshletBuilder::RemapVertices(std::vector<uint32>& indices_in, std::vector<Vertex>& vertices_in) {
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

    indices_in  = std::move(remapped_indices);
    vertices_in = std::move(remapped_vertices);
}

MeshletsContext MeshletBuilder::BuildMeshlets(
    std::vector<uint32>& indices_in,
    std::vector<Vertex>& vertices_in,
    const BuildSettings& settings
) {
    if (settings.enable_fuse) {
        FuseVertices(indices_in, vertices_in);
    }

    if (settings.enable_remap) {
        RemapVertices(indices_in, vertices_in);
    }

    // 第2步：构建Meshlet
    MeshletsContext context {};

    std::vector<meshopt_Meshlet> meshlets;
    size_t                       max_meshlets = meshopt_buildMeshletsBound(
        indices_in.size(),
        settings.meshlet_vertex_max_num,
        settings.meshlet_triangle_max_num
    );
    meshlets.resize(max_meshlets);

    std::vector<uint32> meshlet_vertices(max_meshlets * settings.meshlet_vertex_max_num);
    std::vector<uint8>  meshlet_triangles(max_meshlets * settings.meshlet_triangle_max_num * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        indices_in.data(),
        indices_in.size(),
        &vertices_in[0].position.x,
        vertices_in.size(),
        sizeof(vertices_in[0]),
        settings.meshlet_vertex_max_num,
        settings.meshlet_triangle_max_num,
        settings.cone_weight
    );

    // 裁剪多余的数组元素
    meshlets.resize(meshlet_count);
    auto& last_meshlet = meshlets.back();
    meshlet_vertices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    meshlet_triangles.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));

    // 第3步：计算包围体
    std::vector<meshopt_Bounds> meshlet_bounds(meshlets.size());
    for (int i = 0; i < meshlets.size(); i++) {
        auto& meshlet = meshlets[i];
        auto& bounds  = meshlet_bounds[i];

        if (settings.enable_opt) {
            meshopt_optimizeMeshlet(
                &meshlet_vertices[meshlet.vertex_offset],
                &meshlet_triangles[meshlet.triangle_offset],
                meshlet.triangle_count,
                meshlet.vertex_count
            );
        }

        Vector3f pos_min = Vector3f(std::numeric_limits<float>::max());
        Vector3f pos_max = Vector3f(std::numeric_limits<float>::lowest());

        for (uint32 triangleId = 0; triangleId < meshlet.triangle_count; triangleId++) {
            for (uint32 vertexId = 0; vertexId < 3; vertexId++) {
                uint8  id  = meshlet_triangles[meshlet.triangle_offset + triangleId * 3 + vertexId];
                uint32 vid = meshlet_vertices[meshlet.vertex_offset + id];
                pos_max    = Math::max(pos_max, vertices_in[vid].position);
                pos_min    = Math::min(pos_min, vertices_in[vid].position);
            }
        }
        Vector3f center = 0.5f * (pos_max + pos_min);
        float    radius = Math::length(pos_max - center);

        // 获取meshlet的包围体和法线锥
        meshopt_Bounds meshopt_bounds = meshopt_computeMeshletBounds(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            &vertices_in[0].position.x,
            vertices_in.size(),
            sizeof(vertices_in[0])
        );

        meshopt_bounds.center[0] = center[0];
        meshopt_bounds.center[1] = center[1];
        meshopt_bounds.center[2] = center[2];
        meshopt_bounds.radius    = radius;
        meshlet_bounds[i]        = meshopt_bounds;
    }

    // 第4步：处理三角形数据
    std::vector<BoundsData> bounds_data(meshlets.size());
    std::vector<uint32_t>   meshlet_triangles_u32;
    for (int i = 0; i < meshlets.size(); i++) {
        auto& meshlet = meshlets[i];
        auto& bounds  = meshlet_bounds[i];

        bool isDegenerate = false;

        Vector3f cone_normal = { bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2] };
        Vector3f apex        = { bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2] };
        Vector3f center      = { bounds.center[0], bounds.center[1], bounds.center[2] };

        // 三角形
        uint32 triangle_offset = static_cast<uint32>(meshlet_triangles_u32.size());
        for (uint32 j = 0; j < meshlet.triangle_count; ++j) {
            uint32 i0 = 3 * j + 0 + meshlet.triangle_offset;
            uint32 i1 = 3 * j + 1 + meshlet.triangle_offset;
            uint32 i2 = 3 * j + 2 + meshlet.triangle_offset;

            uint8 vIdx0 = meshlet_triangles[i0];
            uint8 vIdx1 = meshlet_triangles[i1];
            uint8 vIdx2 = meshlet_triangles[i2];

            uint32_t packed = ((static_cast<uint32_t>(vIdx0) & 0xFF) << 0) |
                              ((static_cast<uint32_t>(vIdx1) & 0xFF) << 8) |
                              ((static_cast<uint32_t>(vIdx2) & 0xFF) << 16);
            meshlet_triangles_u32.push_back(packed);

            auto globalVertexIndex0 = meshlet_vertices[meshlet.vertex_offset + vIdx0];
            auto globalVertexIndex1 = meshlet_vertices[meshlet.vertex_offset + vIdx1];
            auto globalVertexIndex2 = meshlet_vertices[meshlet.vertex_offset + vIdx2];

            auto vertex0 = vertices_in[globalVertexIndex0];
            auto vertex1 = vertices_in[globalVertexIndex1];
            auto vertex2 = vertices_in[globalVertexIndex2];

            auto faceNormal =
                Math::normalize(Math::cross(vertex1.position - vertex0.position, vertex2.position - vertex0.position));

            if (Math::dot(faceNormal, cone_normal) < 0.1f) {
                isDegenerate = true;
            }
        }
        meshlet.triangle_offset = triangle_offset;

        // 包围体信息
        float angle          = Math::acos(bounds.cone_cutoff);
        float modifiedCutoff = -Math::cos(angle + Math::radians(90.0f));
        float apex_offset    = Math::dot(center - apex, cone_normal);

        bounds_data[i].sphere      = Vector4f(center, bounds.radius);
        bounds_data[i].normal_cone = PackCone(cone_normal, isDegenerate ? 1.0f : modifiedCutoff);
        bounds_data[i].apex_offset = apex_offset;
    }

    // 填充context结构
    context.meshlets     = std::move(meshlets);
    context.triangles    = std::move(meshlet_triangles_u32);
    context.vertices     = std::move(meshlet_vertices);
    context.bounds       = std::move(bounds_data);
    context.opt_vertices = std::move(vertices_in);

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
