#pragma once

#include <utils/utils.h>
#include <meshoptimizer.h>

namespace Nanite {
using Meshlet = meshopt_Meshlet;

struct MeshletsContext {
    std::vector<uint8>   triangles; // a set of indices that define the triangles in the meshlet
    std::vector<uint32>  vertices; // a list of vertex indices mappings
    std::vector<Meshlet> meshlets;
};

class NaniteBuilder {
public:
    explicit NaniteBuilder(const std::vector<uint32>& indices, const std::vector<Point3f>& positions);

    MeshletsContext Build() const;

    const std::vector<uint32>&  GetIndices() const { return m_indices; }
    const std::vector<Point3f>& GetVertices() const { return m_positions; }

private:
    std::vector<uint32>  m_indices;
    std::vector<Point3f> m_positions;
};
} // namespace Nanite