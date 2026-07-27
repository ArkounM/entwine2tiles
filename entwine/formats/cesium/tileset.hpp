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

#pragma once

#include <cstdint>
#include <memory>
#include <string>
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

struct FormatTraits
{
    const char* name;
    const char* extension;
    const char* assetVersion;
};

const FormatTraits& traits(Format format);

ColorType toColorType(std::string s);
std::string toString(ColorType c);

Format toFormat(std::string s);
inline std::string toString(Format f) { return traits(f).name; }

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
    const Io& io() const { return *m_io; }

    Format format() const { return m_format; }
    std::string contentExtension() const { return traits(m_format).extension; }

    bool hasColor() const { return m_colorType != ColorType::None; }
    bool hasNormals() const { return m_hasNormals; }
    ColorType colorType() const { return m_colorType; }

    // LAS stores colour in 16 bits, but whether a file uses the high byte
    // varies by scanner, so the depth comes from the build's own statistics.
    uint8_t toEightBit(uint16_t v) const
    {
        return m_truncate ? v >> 8 : static_cast<uint8_t>(std::min<uint16_t>(v, 255));
    }

    // Source value to the linear 16 bit value glTF wants, or null if the
    // output has no colour. Hoist out of per-point loops.
    const uint16_t* linearLut() const
    {
        return m_linearLut.empty() ? nullptr : m_linearLut.data();
    }
    uint16_t toLinearFromByte(uint8_t v) const
    {
        return m_linearLut[m_truncate ? v * 257 : v];
    }

    // Content and bounding volumes are written relative to this point, which
    // keeps them within float range of their own origin.
    const Point& origin() const { return m_origin; }

    double rootGeometricError() const
    {
        return m_rootGeometricError * m_rootErrorMultiplier;
    }
    double geometricErrorAt(uint64_t depth) const
    {
        return std::ldexp(m_rootGeometricError, -static_cast<int>(depth));
    }

    uint64_t tileCount() const { return m_tileCount; }
    uint64_t pointCount() const { return m_pointCount; }
    Pool& threadPool() const { return m_threadPool; }

private:
    json build(const ChunkKey& ck) const;
    void write(const ChunkKey& ck, uint64_t np) const;

    const Endpoints m_in;
    const arbiter::Endpoint m_out;

    const Metadata m_metadata;
    const Hierarchy m_hierarchy;
    const std::unique_ptr<Io> m_io;

    const Format m_format;
    const Point m_origin;
    const ColorType m_colorType;
    const bool m_truncate;
    const std::vector<uint16_t> m_linearLut;
    const bool m_hasNormals;
    const double m_rootGeometricError;
    const double m_rootErrorMultiplier;

    mutable uint64_t m_tileCount = 0;
    mutable uint64_t m_pointCount = 0;
    mutable Pool m_threadPool;
};

} // namespace cesium
} // namespace entwine
