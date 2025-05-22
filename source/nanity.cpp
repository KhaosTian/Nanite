#include "nanity.h"
#include "glm/trigonometric.hpp"
#include "utils/utils.h"
#include <limits>
#include <metis.h>
#include <vector>
#include "utils/cityhash.h"

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

    MeshletsContext context {};

    std::vector<meshopt_Meshlet> meshlets;
    size_t max_meshlets = meshopt_buildMeshletsBound(indices_in.size(), settings.max_vertices, settings.max_triangles);
    meshlets.resize(max_meshlets);

    std::vector<uint32> meshlet_vertices(max_meshlets * settings.max_vertices);
    std::vector<uint8>  meshlet_triangles(max_meshlets * settings.max_triangles * 3);

    size_t meshlet_count = meshopt_buildMeshlets(
        meshlets.data(),
        meshlet_vertices.data(),
        meshlet_triangles.data(),
        indices_in.data(),
        indices_in.size(),
        &vertices_in[0].position.x,
        vertices_in.size(),
        sizeof(vertices_in[0]),
        settings.max_vertices,
        settings.max_triangles,
        settings.cone_weight
    );

    meshlets.resize(meshlet_count);
    auto& last_meshlet = meshlets.back();
    meshlet_vertices.resize(last_meshlet.vertex_offset + last_meshlet.vertex_count);
    meshlet_triangles.resize(last_meshlet.triangle_offset + ((last_meshlet.triangle_count * 3 + 3) & ~3));

    std::vector<BoundsData> meshlet_bounds(meshlets.size());
    std::vector<uint32_t>   meshlet_triangles_u32;
    meshlet_triangles_u32.reserve(indices_in.size() / 3);
    for (int i = 0; i < meshlets.size(); i++) {
        auto& meshlet     = meshlets[i];
        auto& bounds_data = meshlet_bounds[i];

        if (settings.enable_opt) {
            meshopt_optimizeMeshlet(
                &meshlet_vertices[meshlet.vertex_offset],
                &meshlet_triangles[meshlet.triangle_offset],
                meshlet.triangle_count,
                meshlet.vertex_count
            );
        }

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &meshlet_vertices[meshlet.vertex_offset],
            &meshlet_triangles[meshlet.triangle_offset],
            meshlet.triangle_count,
            &vertices_in[0].position.x,
            vertices_in.size(),
            sizeof(vertices_in[0])
        );

        Vector3f pos_min = Vector3f(std::numeric_limits<float>::max());
        Vector3f pos_max = Vector3f(std::numeric_limits<float>::lowest());

        uint32 triangle_offset = static_cast<uint32>(meshlet_triangles_u32.size());

        bool isDegenerate = false;

        Vector3f cone_axis = { bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2] };
        Vector3f cone_apex = { bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2] };

        Vector3f triangleVertices[3];
        for (uint32 triangleId = 0; triangleId < meshlet.triangle_count; triangleId++) {
            uint32_t packed = 0;
            for (uint32 j = 0; j < 3; j++) {
                uint8  vIdx              = meshlet_triangles[3 * triangleId + j + meshlet.triangle_offset];
                uint32 globalVertexIndex = meshlet_vertices[meshlet.vertex_offset + vIdx];
                triangleVertices[j]      = vertices_in[globalVertexIndex].position;
                pos_max                  = Math::max(pos_max, triangleVertices[j]);
                pos_min                  = Math::min(pos_min, triangleVertices[j]);
                packed |= (static_cast<uint32_t>(vIdx) & 0xFF) << (8 * j);
            }
            meshlet_triangles_u32.push_back(packed);

            auto faceNormal = Math::normalize(
                Math::cross(triangleVertices[1] - triangleVertices[0], triangleVertices[2] - triangleVertices[1])
            );
            if (Math::dot(faceNormal, cone_axis) < 0.1f) {
                isDegenerate = true;
            }
        }

        meshlet.triangle_offset = triangle_offset;
        Vector3f center         = 0.5f * (pos_max + pos_min);
        float    radius         = Math::length(pos_max - center);

        float angle          = Math::acos(bounds.cone_cutoff);
        float modifiedCutoff = isDegenerate ? 1.0f : -Math::cos(angle + 1.5707963268f);
        float apex_offset    = Math::dot(center - cone_apex, cone_axis);

        bounds_data.sphere      = Vector4f(center, bounds.radius);
        bounds_data.normal_cone = PackCone(cone_axis, modifiedCutoff);
        bounds_data.apex_offset = apex_offset;
    }

    // 填充context结构
    context.meshlets     = std::move(meshlets);
    context.triangles    = std::move(meshlet_triangles_u32);
    context.vertices     = std::move(meshlet_vertices);
    context.bounds       = std::move(meshlet_bounds);
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
