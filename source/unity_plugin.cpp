#include "nanity.h"
#include <cstdint>
// Define export macros for DLL
#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_API extern "C" __declspec(dllexport)
#else
    #define EXPORT_API extern "C"
#endif
// 替换原有的CreateNanityBuilder, DestroyNanityBuilder和BuildMeshlets函数
EXPORT_API void*
BuildMeshlets(const uint32_t* indices, uint32_t indicesCount, const float* positions, uint32_t positionsCount) {
    try {
        // 转换输入数据
        std::vector<uint32_t> indicesVec(indices, indices + indicesCount);

        std::vector<Nanity::Vertex> verticesVec;
        verticesVec.reserve(positionsCount / 3);
        for (uint32_t i = 0; i < positionsCount; i += 3) {
            verticesVec.emplace_back(Nanity::Vertex { { positions[i], positions[i + 1], positions[i + 2] } });
        }

        // 直接调用静态方法构建MeshletsContext
        auto context = new Nanity::MeshletsContext();
        *context     = Nanity::MeshletBuilder::BuildMeshlets(indicesVec, verticesVec);

        return context;
    } catch (const std::exception& e) {
        printf("BuildMeshlets exception: %s\n", e.what());
        return nullptr;
    } catch (...) {
        printf("BuildMeshlets: Unknown exception occurred\n");
        return nullptr;
    }
}

// Free the MeshletsContext
EXPORT_API void DestroyMeshletsContext(void* context) {
    if (context) {
        delete static_cast<Nanity::MeshletsContext*>(context);
    }
}

// Get data from MeshletsContext
EXPORT_API uint32_t GetMeshletsCount(void* context) {
    if (!context) return 0;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    return static_cast<uint32_t>(meshletsContext->meshlets.size());
}
EXPORT_API bool GetMeshlets(void* context, Nanity::Meshlet* meshlets, uint32_t bufferSize) {
    if (!context || !meshlets) return false;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    if (bufferSize < meshletsContext->meshlets.size()) return false;

    std::memcpy(meshlets, meshletsContext->meshlets.data(), meshletsContext->meshlets.size() * sizeof(Nanity::Meshlet));
    return true;
}
EXPORT_API uint32_t GetVerticesCount(void* context) {
    if (!context) return 0;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    return static_cast<uint32_t>(meshletsContext->vertices.size());
}
EXPORT_API bool GetVertices(void* context, uint32_t* vertices, uint32_t bufferSize) {
    if (!context || !vertices) return false;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    if (bufferSize < meshletsContext->vertices.size()) return false;

    std::memcpy(vertices, meshletsContext->vertices.data(), meshletsContext->vertices.size() * sizeof(uint32_t));
    return true;
}
EXPORT_API uint32_t GetTriangleCount(void* context) {
    if (!context) return 0;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    return static_cast<uint32_t>(meshletsContext->triangles.size());
}
EXPORT_API bool GetTriangles(void* context, uint32_t* triangles, uint32_t bufferSize) {
    if (!context || !triangles) return false;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    if (bufferSize < meshletsContext->triangles.size()) return false;

    std::memcpy(triangles, meshletsContext->triangles.data(), meshletsContext->triangles.size() * sizeof(uint32_t));
    return true;
}

EXPORT_API uint32_t GetBoundsCount(void* context) {
    if (!context) return 0;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    return static_cast<uint32_t>(meshletsContext->bounds.size());
}
EXPORT_API bool GetBounds(void* context, Nanity::BoundsData* bounds_data, uint32_t bufferSize) {
    if (!context || !bounds_data) return false;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    if (bufferSize < meshletsContext->bounds.size()) return false;

    std::memcpy(
        bounds_data,
        meshletsContext->bounds.data(),
        meshletsContext->bounds.size() * sizeof(Nanity::BoundsData)
    );
    return true;
}

// 获取优化后的顶点数量
EXPORT_API uint32_t GetOptimizedVertexCount(void* context) {
    if (!context) return 0;

    auto meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    return static_cast<uint32_t>(meshletsContext->opt_vertices.size());
}
// 获取优化后的顶点位置数据
EXPORT_API bool GetOptimizedVertexPositions(void* context, float* positions, uint32_t bufferSize) {
    if (!context || !positions) return false;

    auto        meshletsContext = static_cast<Nanity::MeshletsContext*>(context);
    const auto& vertices        = meshletsContext->opt_vertices;

    // 确保缓冲区足够大 (每个顶点3个float)
    if (bufferSize < vertices.size() * 3) return false;

    // 复制顶点位置
    for (size_t i = 0; i < vertices.size(); ++i) {
        positions[i * 3 + 0] = vertices[i].position.x;
        positions[i * 3 + 1] = vertices[i].position.y;
        positions[i * 3 + 2] = vertices[i].position.z;
    }

    return true;
}
