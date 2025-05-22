#pragma once

#include <utils/utils.h>
#include <meshoptimizer.h>
#include <vector>

// nanity.h
namespace Nanity {

struct Vertex {
    Vector3f position;
};

using Meshlet = meshopt_Meshlet;

struct BoundsData {
    Vector4f sphere; // xyz = center, w = radius
    uint32   normal_cone; // 紧凑的法线锥表示
    float    apex_offset; // 锥顶点相对于球心的偏移距离
};

struct MeshletsContext {
    std::vector<uint32>     triangles; // meshlet局部三角形索引
    std::vector<uint32>     vertices; // meshlet顶点映射到原始顶点的索引
    std::vector<Meshlet>    meshlets; // meshlet描述数据
    std::vector<BoundsData> bounds; // meshlet包围盒数据
    std::vector<Vertex>     opt_vertices; // 优化后的顶点数组
};

struct BuildSettings {
    bool   enable_fuse   = true;
    bool   enable_opt    = true;
    bool   enable_remap  = true;
    uint32 max_vertices  = 64;
    uint32 max_triangles = 124;
    float  cone_weight   = 1.0f;
};

// 新的静态类设计
class MeshletBuilder {
public:
    static MeshletsContext
    BuildMeshlets(std::vector<uint32>& indices, std::vector<Vertex>& vertices, const BuildSettings& settings);

private:
    // 工具函数
    static void   RemapVertices(std::vector<uint32>& indices_in, std::vector<Vertex>& vertices_in);
    static void   FuseVertices(std::vector<uint32>& indices, std::vector<Vertex>& vertices);
    static int32  HashPosition(const Vector3f& position);
    static uint32 PackCone(Vector3f normal, float cutoff);
};

} // namespace Nanity
