/******************************************************************************
* Copyright (c) 2018, Connor Manning (connor@hobu.co)
* Copyright (c) 2026, Third Space Interactive
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#pragma once

#include <cstddef>
#include <vector>

#include <entwine/formats/cesium/tileset.hpp>
#include <entwine/types/key.hpp>
#include <entwine/types/vector-point-table.hpp>

namespace entwine
{
namespace cesium
{

// This class represents a single PNTS file:
// https://github.com/CesiumGS/3d-tiles/tree/main/specification/TileFormats/PointCloud
class Pnts
{
    using Xyz = std::vector<float>;
    using Rgb = std::vector<uint8_t>;
    using Normals = std::vector<float>;

public:
    Pnts(const Tileset& tileset, const ChunkKey& ck, uint64_t np);
    std::vector<char> build();

private:
    void buildXyz(VectorPointTable& table);
    void buildRgb(VectorPointTable& table);
    void buildNormals(VectorPointTable& table);

    std::vector<char> buildFile() const;

    const Tileset& m_tileset;
    const ChunkKey m_key;
    const uint64_t m_capacity;
    Point m_mid;

    Xyz m_xyz;
    Rgb m_rgb;
    Normals m_normals;

    std::size_t m_np = 0;
};

} // namespace cesium
} // namespace entwine
