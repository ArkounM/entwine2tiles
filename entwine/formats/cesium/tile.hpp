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

// Bounding volumes are relative to the tileset origin, matching the content.
// https://github.com/CesiumGS/3d-tiles/tree/main/specification#tile-metadata
inline json toBox(const Bounds& bounds, const Point& origin)
{
    const Point& mid(bounds.mid());

    return json::array({
        mid.x - origin.x,     mid.y - origin.y,     mid.z - origin.z,
        bounds.width() / 2.0, 0,                    0,
        0,                    bounds.depth() / 2.0, 0,
        0,                    0,                    bounds.height() / 2.0
    });
}

inline json toTile(const Tileset& tileset, const ChunkKey& ck)
{
    json j(json::object());

    j["boundingVolume"] = json::object({
        { "box", toBox(ck.bounds(), tileset.origin()) }
    });
    j["geometricError"] = tileset.geometricErrorAt(ck.depth());
    j["content"] = json::object({
        { "uri", ck.toString() + tileset.contentExtension() }
    });

    if (!ck.depth()) j["refine"] = "ADD";

    return j;
}

} // namespace cesium
} // namespace entwine
