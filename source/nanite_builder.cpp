#include <metis.h>
#include <nanite_builder.h>
#include <utils/log.h>

namespace Nanite {

constexpr uint32 kMeshletVertexMaxNum   = 255; // Meshlet最大顶点数
constexpr uint32 kMeshletTriangleMaxNum = 128; // Meshlet最大三角面数

using MehsletIndex = uint32;
using VertexIndex  = uint32;

NaniteBuilder::NaniteBuilder(std::vector<uint32>&& indices, std::vector<Vertex>&& vertices, float coneWeight):
    m_indices(indices),
    m_vertices(vertices),
    m_coneWeight(coneWeight) {
    size_t indexCount = m_indices.size();

    std::vector<uint32> remap(indexCount);

    size_t vertexCount = meshopt_generateVertexRemap(
        remap.data(),
        m_indices.data(),
        m_indices.size(),
        m_vertices.data(),
        m_vertices.size(),
        sizeof(Vertex)
    );

    std::vector<uint32> remapIndices(indexCount);
    std::vector<Vertex> remapVertices(vertexCount);

    meshopt_remapIndexBuffer(remapIndices.data(), m_indices.data(), indexCount, remap.data());
    meshopt_remapVertexBuffer(remapVertices.data(), m_vertices.data(), vertexCount, sizeof(Vertex), remap.data());

    meshopt_optimizeVertexCache(remapIndices.data(), remapIndices.data(), indexCount, vertexCount);
    meshopt_optimizeVertexFetch(
        remapVertices.data(),
        remapIndices.data(),
        indexCount,
        remapVertices.data(),
        vertexCount,
        sizeof(Vertex)
    );

    m_indices  = std::move(remapIndices);
    m_vertices = std::move(remapVertices);
}

uint32 LoadVertexIndex(
    const MeshletBuilderContext& context,
    const meshopt_Meshlet&       meshlet,
    uint32                       triangleId,
    uint32                       vertexId
) {
    uint8 id = context.indices[meshlet.triangle_offset + triangleId * 3 + vertexId];
    return context.vertices[meshlet.vertex_offset + id];
}

static MeshletBuilderContext BuildMeshlets(
    const std::vector<uint32>& indices,
    const std::vector<Vertex>& vertices,

    float coneWeight
) {
    MeshletBuilderContext context {};

    const uint32 vertexCount = vertices.size();
    uint32 meshletCount      = meshopt_buildMeshletsBound(indices.size(), kMeshletVertexMaxNum, kMeshletTriangleMaxNum);
    std::vector<meshopt_Meshlet> meshlets(meshletCount);

    {
        std::vector<uint32> meshletVertices(meshlets.size() * kMeshletVertexMaxNum);
        std::vector<uint8>  meshletTriangles(meshlets.size() * kMeshletTriangleMaxNum);

        meshletCount = meshopt_buildMeshlets(
            meshlets.data(),
            meshletVertices.data(),
            meshletTriangles.data(),
            indices.data(),
            indices.size(),
            &vertices.front().position.x,
            vertexCount,
            sizeof(vertices.front()),
            kMeshletVertexMaxNum,
            kMeshletTriangleMaxNum,
            coneWeight
        );
        meshlets.resize(meshletCount);

        context.indices  = std::move(meshletTriangles);
        context.vertices = std::move(meshletVertices);
    }

    context.meshlets = {};
    context.meshlets.reserve(meshlets.size());

    for (const auto& meshlet: meshlets) {
        meshopt_optimizeMeshlet(
            &context.vertices[meshlet.vertex_offset],
            &context.indices[meshlet.triangle_offset],
            meshlet.triangle_count,
            meshlet.vertex_count
        );

        meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &context.vertices[meshlet.vertex_offset],
            &context.indices[meshlet.triangle_offset],
            meshlet.triangle_count,
            &vertices.front().position.x,
            vertexCount,
            sizeof(vertices.front())
        );

        Point3f posMin = Point3f(std::numeric_limits<float>::max());
        Point3f posMax = Point3f(std::numeric_limits<float>::lowest());

        Check(meshlet.triangle_count < 256);
        Check(meshlet.vertex_count < 256);

        // 构建meshlet的包围盒
        for (uint32 triangleId = 0; triangleId < meshlet.triangle_count; triangleId++) {
            for (uint32 i = 0; i < 3; i++) {
                VertexIndex vIdx = LoadVertexIndex(context, meshlet, triangleId, i);
                posMax           = Math::max(posMax, vertices[vIdx].position);
                posMin           = Math::min(posMin, vertices[vIdx].position);
            }
        }
    }

    return context;
}

MeshletBuilderContext NaniteBuilder::Build() const {
    MeshletBuilderContext context = BuildMeshlets(m_indices, m_vertices, m_coneWeight);

    return context;
}

} // namespace Nanite