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
#include <limits>

#include <entwine/io/io.hpp>
#include <entwine/types/dimension.hpp>

namespace entwine
{
namespace cesium
{

namespace
{

using DimId = pdal::Dimension::Id;

constexpr uint32_t chunkJson = 0x4E4F534A;
constexpr uint32_t chunkBin = 0x004E4942;

void push(std::vector<char>& v, const uint32_t n)
{
    const char* p(reinterpret_cast<const char*>(&n));
    v.insert(v.end(), p, p + sizeof(n));
}

} // unnamed namespace

Gltf::Gltf(const Tileset& tileset, const ChunkKey& ck, const uint64_t np)
    : m_tileset(tileset)
    , m_key(ck)
    , m_capacity(np)
    , m_origin(tileset.origin())
{
    for (int i(0); i < 3; ++i)
    {
        m_min[i] = std::numeric_limits<float>::max();
        m_max[i] = std::numeric_limits<float>::lowest();
    }
}

std::vector<char> Gltf::build()
{
    const Metadata& m(m_tileset.metadata());

    auto layout(toLayout(m.absoluteSchema, m.dataType == io::Type::Laszip));
    VectorPointTable table(layout, m_capacity);

    table.setProcess([this, &table]()
    {
        m_np += table.numPoints();
        buildXyz(table);
        buildRgba(table);
    });

    m_tileset.io().read(m_key.toString() + getPostfix(m), table);

    if (!m_np)
    {
        for (int i(0); i < 3; ++i) m_min[i] = m_max[i] = 0;
    }

    return buildFile();
}

// glTF is y-up and 3D Tiles is z-up, and the renderer applies that conversion
// to the content on the way in. Writing (x, z, -y) here means the points land
// back on (x, y, z) in tile space.
void Gltf::buildXyz(VectorPointTable& table)
{
    m_xyz.reserve(m_xyz.size() + table.numPoints() * 3);

    for (const auto& pr : table)
    {
        const float v[3] {
            static_cast<float>(pr.getFieldAs<double>(DimId::X) - m_origin.x),
            static_cast<float>(pr.getFieldAs<double>(DimId::Z) - m_origin.z),
            static_cast<float>(m_origin.y - pr.getFieldAs<double>(DimId::Y))
        };

        for (int i(0); i < 3; ++i)
        {
            m_xyz.push_back(v[i]);
            m_min[i] = std::min(m_min[i], v[i]);
            m_max[i] = std::max(m_max[i], v[i]);
        }
    }
}

void Gltf::buildRgba(VectorPointTable& table)
{
    if (!m_tileset.hasColor()) return;
    m_rgba.reserve(m_rgba.size() + table.numPoints() * 4);

    // Read as 16 bit and narrow here rather than asking PDAL for a uint8,
    // which throws on anything above 255 rather than clamping.
    auto getByte([this](const pdal::PointRef& pr, DimId id) -> uint8_t
    {
        const uint16_t v(pr.getFieldAs<uint16_t>(id));
        if (m_tileset.truncate()) return v >> 8;
        return static_cast<uint8_t>(std::min<uint16_t>(v, 255));
    });

    uint8_t r(0), g(0), b(0);

    if (m_tileset.colorType() == ColorType::Tile)
    {
        r = std::rand() % 256;
        g = std::rand() % 256;
        b = std::rand() % 256;
    }

    for (const auto& pr : table)
    {
        if (m_tileset.colorType() == ColorType::Rgb)
        {
            r = getByte(pr, DimId::Red);
            g = getByte(pr, DimId::Green);
            b = getByte(pr, DimId::Blue);
        }
        else if (m_tileset.colorType() == ColorType::Intensity)
        {
            r = g = b = getByte(pr, DimId::Intensity);
        }

        m_rgba.push_back(r);
        m_rgba.push_back(g);
        m_rgba.push_back(b);
        m_rgba.push_back(255);
    }
}

std::vector<char> Gltf::buildFile() const
{
    const bool hasColor(m_rgba.size());

    const uint64_t xyzBytes(m_xyz.size() * sizeof(float));
    const uint64_t rgbaBytes(m_rgba.size());

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

        // glTF requires each vertex attribute element to start on a 4-byte
        // boundary, which is why colors are VEC4 bytes rather than VEC3.
        accessors.push_back(json::object({
            { "bufferView", 1 },
            { "componentType", 5121 },
            { "normalized", true },
            { "count", m_np },
            { "type", "VEC4" }
        }));

        attributes["COLOR_0"] = 1;
    }

    const uint64_t binBytes(xyzBytes + rgbaBytes);
    const uint64_t binPadding((4 - (binBytes % 4)) % 4);

    const std::string name(m_key.toString());

    json primitive(json::object({
        { "attributes", attributes },
        { "mode", 0 }
    }));

    json mesh(json::object({ { "name", name } }));
    mesh["primitives"] = json::array({ primitive });

    json node(json::object({ { "mesh", 0 }, { "name", name } }));

    json scene(json::object());
    scene["nodes"] = json::array({ 0 });

    json j(json::object());
    j["asset"] = json::object({
        { "version", "2.0" },
        { "generator", "entwine2tiles" }
    });
    j["scene"] = 0;
    j["scenes"] = json::array({ scene });
    j["nodes"] = json::array({ node });
    j["meshes"] = json::array({ mesh });
    j["accessors"] = accessors;
    j["bufferViews"] = bufferViews;
    j["buffers"] = json::array({
        json::object({ { "byteLength", binBytes + binPadding } })
    });

    std::string jsonString(j.dump());
    while (jsonString.size() % 4) jsonString += ' ';

    const uint64_t headerBytes(12);
    const uint64_t chunkHeaderBytes(8);
    const uint64_t totalBytes =
        headerBytes +
        chunkHeaderBytes + jsonString.size() +
        chunkHeaderBytes + binBytes + binPadding;

    std::vector<char> glb;
    glb.reserve(totalBytes);

    push(glb, 0x46546C67);  // "glTF"
    push(glb, 2);           // Version.
    push(glb, static_cast<uint32_t>(totalBytes));

    push(glb, static_cast<uint32_t>(jsonString.size()));
    push(glb, chunkJson);
    glb.insert(glb.end(), jsonString.begin(), jsonString.end());

    push(glb, static_cast<uint32_t>(binBytes + binPadding));
    push(glb, chunkBin);
    glb.insert(
            glb.end(),
            reinterpret_cast<const char*>(m_xyz.data()),
            reinterpret_cast<const char*>(m_xyz.data() + m_xyz.size()));
    glb.insert(
            glb.end(),
            reinterpret_cast<const char*>(m_rgba.data()),
            reinterpret_cast<const char*>(m_rgba.data() + m_rgba.size()));
    glb.insert(glb.end(), binPadding, 0);

    return glb;
}

} // namespace cesium
} // namespace entwine
