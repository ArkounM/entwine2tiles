/******************************************************************************
* Copyright (c) 2026, Third Space Interactive
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

// Written for entwine2tiles. This is the glTF counterpart to pnts.hpp: same
// job, same inputs, current format. 3D Tiles 1.1 deprecates pnts in favour of
// glTF content, and Cesium for Unreal renders glTF POINTS, so a new build has
// no reason to emit pnts.

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

// A single binary glTF file holding one POINTS primitive.
class Gltf
{
    using Xyz = std::vector<float>;
    using Rgba = std::vector<uint8_t>;

public:
    Gltf(const Tileset& tileset, const ChunkKey& ck, uint64_t np);
    std::vector<char> build();

private:
    void buildXyz(VectorPointTable& table);
    void buildRgba(VectorPointTable& table);

    std::vector<char> buildFile() const;

    const Tileset& m_tileset;
    const ChunkKey m_key;
    const uint64_t m_capacity;

    // Positions are stored relative to the tileset origin so that they fit in
    // 32-bit floats without losing precision. Absolute coordinates in a scan
    // this size quantize to a quarter of a metre once they are floats.
    const Point m_origin;

    Xyz m_xyz;
    Rgba m_rgba;

    float m_min[3] { 0, 0, 0 };
    float m_max[3] { 0, 0, 0 };

    std::size_t m_np = 0;
};

} // namespace cesium
} // namespace entwine
