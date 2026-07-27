/******************************************************************************
* Copyright (c) 2026, Third Space Interactive
*
* Entwine -- Point cloud indexing
*
* Entwine is available under the terms of the LGPL2 license. See COPYING
* for specific license text and more information.
*
******************************************************************************/

#include <entwine/formats/cesium/gltf.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>

#include <entwine/formats/cesium/bytes.hpp>
#include <entwine/io/io.hpp>
#include <entwine/types/dimension.hpp>

namespace entwine
{
namespace cesium
{

namespace
{

using DimId = pdal::Dimension::Id;

constexpr uint32_t magic = 0x46546C67;      // "glTF"
constexpr uint32_t chunkJson = 0x4E4F534A;
constexpr uint32_t chunkBin = 0x004E4942;

} // unnamed namespace

Gltf::Gltf(const Tileset& tileset, const ChunkKey& ck, const uint64_t np)
    : m_tileset(tileset)
    , m_key(ck)
    , m_capacity(np)
{
    m_xyz.reserve(m_capacity * 3);
    if (m_tileset.linearLut()) m_rgba.reserve(m_capacity * 4);
}

std::vector<char> Gltf::build()
{
    const Metadata& m(m_tileset.metadata());

    // The absolute schema gives real coordinates rather than the scaled
    // integers stored on disk.
    auto layout(toLayout(m.absoluteSchema, m.dataType == io::Type::Laszip));
    VectorPointTable table(layout, m_capacity);

    table.setProcess([this, &table]() { read(table); });

    m_tileset.io().read(m_key.toString() + getPostfix(m), table);

    if (!m_np) for (int i(0); i < 3; ++i) m_min[i] = m_max[i] = 0;

    return buildFile();
}

// glTF is y-up and 3D Tiles is z-up, and the renderer applies that conversion
// to the content on the way in, so writing (x, z, -y) here lands the points
// back on (x, y, z) in tile space.
void Gltf::read(VectorPointTable& table)
{
    m_np += table.numPoints();

    const Point origin(m_tileset.origin());
    const ColorType colorType(m_tileset.colorType());
    const uint16_t* const lut(m_tileset.linearLut());

    float mn[3] { m_min[0], m_min[1], m_min[2] };
    float mx[3] { m_max[0], m_max[1], m_max[2] };

    uint16_t r(0), g(0), b(0);

    if (colorType == ColorType::Tile)
    {
        // Seeded from the key so the colours are reproducible run to run.
        uint64_t h(std::hash<std::string>()(m_key.toString()));
        r = m_tileset.toLinearFromByte(h & 0xff);
        g = m_tileset.toLinearFromByte((h >> 8) & 0xff);
        b = m_tileset.toLinearFromByte((h >> 16) & 0xff);
    }

    for (const auto& pr : table)
    {
        const float v[3] {
            static_cast<float>(pr.getFieldAs<double>(DimId::X) - origin.x),
            static_cast<float>(pr.getFieldAs<double>(DimId::Z) - origin.z),
            static_cast<float>(origin.y - pr.getFieldAs<double>(DimId::Y))
        };

        for (int i(0); i < 3; ++i)
        {
            m_xyz.push_back(v[i]);
            mn[i] = std::min(mn[i], v[i]);
            mx[i] = std::max(mx[i], v[i]);
        }

        if (!lut) continue;

        if (colorType == ColorType::Rgb)
        {
            r = lut[pr.getFieldAs<uint16_t>(DimId::Red)];
            g = lut[pr.getFieldAs<uint16_t>(DimId::Green)];
            b = lut[pr.getFieldAs<uint16_t>(DimId::Blue)];
        }
        else if (colorType == ColorType::Intensity)
        {
            r = g = b = lut[pr.getFieldAs<uint16_t>(DimId::Intensity)];
        }

        m_rgba.push_back(r);
        m_rgba.push_back(g);
        m_rgba.push_back(b);
        m_rgba.push_back(65535);
    }

    for (int i(0); i < 3; ++i) { m_min[i] = mn[i]; m_max[i] = mx[i]; }
}

std::vector<char> Gltf::buildFile() const
{
    const bool hasColor(!m_rgba.empty());

    const uint64_t xyzBytes(m_xyz.size() * sizeof(float));
    const uint64_t rgbaBytes(m_rgba.size() * sizeof(uint16_t));

    json bufferViews(json::array());
    bufferViews.push_back(json::object({
        { "buffer", 0 },
        { "byteOffset", 0 },
        { "byteLength", xyzBytes },
        { "target", 34962 }
    }));

    json attributes(json::object({ { "POSITION", 0 } }));

    json accessors(json::array());
    accessors.push_back(json::object({
        { "bufferView", 0 },
        { "componentType", 5126 },
        { "count", m_np },
        { "type", "VEC3" },
        { "min", json::array({ m_min[0], m_min[1], m_min[2] }) },
        { "max", json::array({ m_max[0], m_max[1], m_max[2] }) }
    }));

    if (hasColor)
    {
        bufferViews.push_back(json::object({
            { "buffer", 0 },
            { "byteOffset", xyzBytes },
            { "byteLength", rgbaBytes },
            { "target", 34962 }
        }));

        // VEC4 rather than VEC3 because glTF requires each vertex attribute
        // element to start on a 4-byte boundary.
        accessors.push_back(json::object({
            { "bufferView", 1 },
            { "componentType", 5123 },
            { "normalized", true },
            { "count", m_np },
            { "type", "VEC4" }
        }));

        attributes["COLOR_0"] = 1;
    }

    const uint64_t binBytes(xyzBytes + rgbaBytes);
    const uint64_t binPadding((4 - (binBytes % 4)) % 4);

    const std::string name(m_key.toString());

    json j(json::object());
    j["asset"] = { { "version", "2.0" }, { "generator", "entwine2tiles" } };
    j["scene"] = 0;
    j["scenes"] = json::array({ json { { "nodes", json::array({ 0 }) } } });
    j["nodes"] = json::array({ json { { "mesh", 0 }, { "name", name } } });
    j["meshes"] = json::array({
        json {
            { "name", name },
            {
                "primitives",
                json::array({
                    json { { "attributes", std::move(attributes) }, { "mode", 0 } }
                })
            }
        }
    });
    j["accessors"] = std::move(accessors);
    j["bufferViews"] = std::move(bufferViews);
    j["buffers"] = json::array({ json { { "byteLength", binBytes + binPadding } } });

    std::string jsonString(j.dump());
    while (jsonString.size() % 4) jsonString += ' ';

    const uint64_t headerBytes(12);
    const uint64_t chunkHeaderBytes(8);
    const uint64_t totalBytes =
        headerBytes +
        chunkHeaderBytes + jsonString.size() +
        chunkHeaderBytes + binBytes + binPadding;

    if (totalBytes > std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error("Tile " + name + " exceeds the GLB size limit");
    }

    std::vector<char> glb;
    glb.reserve(totalBytes);

    append(glb, magic);
    append(glb, 2);                                             // Version.
    append(glb, static_cast<uint32_t>(totalBytes));

    append(glb, static_cast<uint32_t>(jsonString.size()));
    append(glb, chunkJson);
    glb.insert(glb.end(), jsonString.begin(), jsonString.end());

    append(glb, static_cast<uint32_t>(binBytes + binPadding));
    append(glb, chunkBin);
    append(glb, m_xyz);
    append(glb, m_rgba);
    glb.insert(glb.end(), binPadding, 0);

    return glb;
}

} // namespace cesium
} // namespace entwine
