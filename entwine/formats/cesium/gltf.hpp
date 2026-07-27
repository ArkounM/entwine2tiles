/******************************************************************************
* Copyright (c) 2026, Third Space Interactive
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

// The glTF counterpart to pnts.hpp: same job, current format. 3D Tiles 1.1
// deprecates pnts in favour of glTF, and Cesium for Unreal renders glTF POINTS.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
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
public:
    Gltf(const Tileset& tileset, const ChunkKey& ck, uint64_t np);
    std::vector<char> build();

private:
    void read(VectorPointTable& table);
    std::vector<char> buildFile() const;

    const Tileset& m_tileset;
    const ChunkKey m_key;
    const uint64_t m_capacity;

    // Relative to the tileset origin, which is what keeps them precise.
    std::vector<float> m_xyz;

    // 16 bit: these are linear values, and 8 bits of linear bands in the dark
    // half, where most of a scan lives.
    std::vector<uint16_t> m_rgba;

    float m_min[3] {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    float m_max[3] {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    std::size_t m_np = 0;
};

} // namespace cesium
} // namespace entwine
