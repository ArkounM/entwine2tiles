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

#include <entwine/formats/cesium/tileset.hpp>

namespace entwine
{
namespace cesium
{

// This class represents the metadata for a single tile:
// https://github.com/CesiumGS/3d-tiles/tree/main/specification#tile-metadata
class Tile
{
public:
    Tile(const Tileset& tileset, const ChunkKey& ck)
        : m_tileset(tileset)
        , m_json(json::object())
    {
        m_json["boundingVolume"] = json::object({
            { "box", toBox(ck.bounds()) }
        });
        m_json["geometricError"] = m_tileset.geometricErrorAt(ck.depth());
        m_json["content"] = json::object({
            { "uri", ck.toString() + m_tileset.contentExtension() }
        });

        if (!ck.depth()) m_json["refine"] = "ADD";
    }

    json get() const { return m_json; }

private:
    // Relative to the tileset origin, matching the content.
    json toBox(Bounds in) const
    {
        const Point& o(m_tileset.origin());

        return json::array({
            in.mid().x - o.x,     in.mid().y - o.y,     in.mid().z - o.z,
            in.width() / 2.0,     0,                    0,
            0,                    in.depth() / 2.0,     0,
            0,                    0,                    in.height() / 2.0
        });
    }

    const Tileset& m_tileset;
    json m_json;
};

inline void to_json(json& j, const Tile& t) { j = t.get(); }

} // namespace cesium
} // namespace entwine
