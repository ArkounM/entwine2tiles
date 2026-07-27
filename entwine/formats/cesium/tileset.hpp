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

// Restored in entwine2tiles from upstream commit 16f9709 (2019-12-16), the last
// commit before the Cesium writer was removed, and ported to the 3.2.1 API.
// The 2019 version walked ept-hierarchy JSON files itself and emitted one
// external tileset per hierarchy subtree. Entwine now loads the whole hierarchy
// into a flat map, so this writes a single tileset.json.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <entwine/builder/hierarchy.hpp>
#include <entwine/io/io.hpp>
#include <entwine/third/arbiter/arbiter.hpp>
#include <entwine/types/endpoints.hpp>
#include <entwine/types/key.hpp>
#include <entwine/types/metadata.hpp>
#include <entwine/util/json.hpp>
#include <entwine/util/pool.hpp>

namespace entwine
{
namespace cesium
{

enum class ColorType
{
    None,
    Rgb,
    Intensity,
    Tile
};

enum class Format
{
    Glb,
    Pnts
};

// This class is the entrypoint of a 3D Tiles tileset definition:
// https://github.com/CesiumGS/3d-tiles/tree/main/specification#tilesetjson
class Tileset
{
public:
    Tileset(const json& config);

    void build() const;

    const Endpoints& in() const { return m_in; }
    const arbiter::Endpoint& out() const { return m_out; }

    const Metadata& metadata() const { return m_metadata; }
    const Hierarchy& hierarchy() const { return m_hierarchy; }
    const Io& io() const { return *m_io; }

    bool hasColor() const { return m_colorType != ColorType::None; }
    bool hasNormals() const { return m_hasNormals; }
    bool truncate() const { return m_truncate; }
    ColorType colorType() const { return m_colorType; }
    std::string colorString() const;

    Format format() const { return m_format; }
    std::string formatString() const;
    std::string contentExtension() const;

    bool rawColor() const { return m_rawColor; }

    // Maps a raw colour value from the source, which is 16 bit whether or not
    // the file uses the high byte, to the 16 bit value written to the tile.
    // Carries the 8 bit scaling and the sRGB to linear conversion, since
    // neither is worth recomputing per point.
    uint16_t toStoredColor(uint16_t v) const { return m_colorLut[v]; }

    // Content is written relative to this point, and so are the bounding
    // volumes, which keeps everything within float range of its own origin.
    const Point& origin() const { return m_origin; }

    double rootGeometricError() const { return m_rootGeometricError; }
    double rootErrorMultiplier() const { return m_rootErrorMultiplier; }
    double geometricErrorAt(uint64_t depth) const
    {
        return m_rootGeometricError / std::pow(2.0, depth);
    }

    uint64_t tileCount() const { return m_tileCount; }
    uint64_t pointCount() const { return m_pointCount; }
    Pool& threadPool() const { return m_threadPool; }

private:
    json build(const ChunkKey& ck) const;
    void write(const ChunkKey& ck, uint64_t np) const;

    ColorType getColorType(const json& config) const;
    bool getTruncate(const json& config) const;
    std::vector<uint16_t> getColorLut() const;

    std::shared_ptr<arbiter::Arbiter> m_arbiter;
    const Endpoints m_in;
    const arbiter::Endpoint m_out;

    const Metadata m_metadata;
    const Hierarchy m_hierarchy;
    const std::unique_ptr<Io> m_io;

    const Format m_format;
    const Point m_origin;
    const ColorType m_colorType;
    const bool m_truncate;
    const bool m_rawColor;
    const std::vector<uint16_t> m_colorLut;
    const bool m_hasNormals;
    const double m_rootGeometricError;
    const double m_rootErrorMultiplier;

    mutable uint64_t m_tileCount = 0;
    mutable uint64_t m_pointCount = 0;
    mutable Pool m_threadPool;
};

} // namespace cesium
} // namespace entwine
