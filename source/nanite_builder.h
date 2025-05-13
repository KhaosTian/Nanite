#pragma once

#include <utils/utils.h>
#include <meshoptimizer.h>

namespace Nanite {
using Meshlet = meshopt_Meshlet;

struct MeshletsContext {
    std::vector<uint32>  primitives; // a set of indices that define the triangles in the meshlet
    std::vector<uint32>  indices; // a list of vertex indices mappings
    std::vector<Meshlet> meshlets;
};

struct Vertex {
    Point3f position;
};

class NaniteBuilder {
public:
    explicit NaniteBuilder(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices);

    MeshletsContext Build() const;
    void            FuseVertices(const std::vector<uint32>& indices, const std::vector<Vertex>& vertices) const;

    const std::vector<uint32>& GetIndices() const { return m_indices; }
    const std::vector<Vertex>& GetVertices() const { return m_vertices; }

private:
    std::vector<uint32> m_indices;
    std::vector<Vertex> m_vertices;
};
} // namespace Nanite