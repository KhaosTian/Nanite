#include "nanity.h"
#include <cstdint>
// Define export macros for DLL
#if defined(_WIN32) || defined(_WIN64)
    #define EXPORT_API extern "C" __declspec(dllexport)
#else
    #define EXPORT_API extern "C"
#endif
// Create and return a NanityBuilder instance
EXPORT_API void*
CreateNanityBuilder(const uint32_t* indices, uint32_t indicesCount, const float* positions, uint32_t positionsCount) {
    try {
        std::vector<uint32_t> indicesVec(indices, indices + indicesCount);

        // Convert positions from float array to Point3f vector
        std::vector<Nanity::Vertex> positionsVec;
        positionsVec.reserve(positionsCount / 3);
        for (uint32_t i = 0; i < positionsCount; i += 3) {
            positionsVec.emplace_back(Nanity::Vertex { { positions[i], positions[i + 1], positions[i + 2] } });
        }

        // Create and return a new NanityBuilder
        return new Nanity::NanityBuilder(indicesVec, positionsVec);
    } catch (const std::exception&) {
        return nullptr;
    }
}
// Free the NanityBuilder instance
EXPORT_API void DestroyNanityBuilder(void* builder) {
    if (builder) {
        delete static_cast<Nanity::NanityBuilder*>(builder);
    }
}
// Build and return a MeshletsContext
EXPORT_API void* BuildMeshlets(void* builder) {
    try {
        if (!builder) {
            printf("BuildMeshlets: Null builder pointer\n");
            return nullptr;
        }
        auto NanityBuilder = static_cast<Nanity::NanityBuilder*>(builder);

        // Create a new context
        Nanity::MeshletsContext* context = new Nanity::MeshletsContext();

        // Copy the result from Build()
        *context = NanityBuilder->Build();

        return context;
    } catch (const std::exception& e) {
        // Log the exception for debugging
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
