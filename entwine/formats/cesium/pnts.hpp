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
#include <cstdint>
#include <vector>

#include <entwine/formats/cesium/tileset.hpp>
#include <entwine/types/key.hpp>
#include <entwine/types/vector-point-table.hpp>

namespace entwine
{
namespace cesium
{

// A single PNTS file, the legacy point cloud format deprecated by 3D Tiles 1.1:
// https://github.com/CesiumGS/3d-tiles/tree/main/specification/TileFormats/PointCloud
class Pnts
{
public:
    Pnts(const Tileset& tileset, const ChunkKey& ck, uint64_t np);
    std::vector<char> build();

private:
    void read(VectorPointTable& table);
    std::vector<char> buildFile() const;

    const Tileset& m_tileset;
    const ChunkKey m_key;
    const uint64_t m_capacity;
    const Point m_mid;

    std::vector<float> m_xyz;
    std::vector<uint8_t> m_rgb;
    std::vector<float> m_normals;

    std::size_t m_np = 0;
};

} // namespace cesium
} // namespace entwine
