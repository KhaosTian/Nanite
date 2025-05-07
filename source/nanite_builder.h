#pragma once

#include <utils/utils.h>
#include <meshoptimizer.h>

namespace Nanite {
struct Vertex {
    Point3f  position;
    Vector2f uv0;
    Vector3f normal;
    Vector4f tangent;
};

struct Meshlet {
    meshopt_Meshlet info;

    float    coneCutoff; // 法线锥顶角的余弦值
    Vector3f coneAxis; // 法线锥中心轴向量
    Point3f  coneApex; // 法线锥顶点位置

    Point3f minPoint; // AABB最小点
    Point3f maxPoint; // AABB最大点
};

struct MeshletBuilderContext {
    std::vector<uint8>   indices; // 三角形索引
    std::vector<uint32>  vertices; // 顶点索引
    std::vector<Meshlet> meshlets;

    void Merge(MeshletBuilderContext&& rhs);
};

class NaniteBuilder {
public:
    explicit NaniteBuilder(std::vector<uint32>&& indices, std::vector<Vertex>&& vertices, float coneWeight);

    MeshletBuilderContext Build() const;

    const std::vector<uint32>& GetIndices() const { return m_indices; }
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }

private:
    std::vector<uint32> m_indices;
    std::vector<Vertex> m_vertices;
    float               m_coneWeight;
};
} // namespace Nanite