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

    // 将primitives移动到context
    context.primitives = std::move(primitives);

    // 直接分配最终大小，避免多次扩容
    context.indices.reserve(max_meshlets * kMeshletVertexMaxNum);

    // 处理所有meshlet的索引
    for (int i = 0; i < context.meshlets.size(); i++) {
        auto&        meshlet     = context.meshlets[i];
        const size_t dest_offset = i * kMeshletVertexMaxNum;

        // 使用std::copy_n复制实际顶点索引
        std::copy_n(
            m_indices.begin() + meshlet.vertex_offset,
            meshlet.vertex_count,
            context.indices.begin() + dest_offset
        );

        // 用最后一个顶点填充剩余部分
        if (meshlet.vertex_count < kMeshletVertexMaxNum) {
            uint32_t last_vertex = *(m_indices.begin() + meshlet.vertex_offset + meshlet.vertex_count - 1);
            std::fill_n(
                context.indices.begin() + dest_offset + meshlet.vertex_count,
                kMeshletVertexMaxNum - meshlet.vertex_count,
                last_vertex
            );
        }

        // 更新meshlet数据
        meshlet.vertex_offset = dest_offset;
        meshlet.vertex_count  = kMeshletVertexMaxNum;
    }

    return context;
}

} // namespace Nanite