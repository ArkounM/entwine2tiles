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

#include <entwine/formats/cesium/pnts.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
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
}

Pnts::Pnts(const Tileset& tileset, const ChunkKey& ck, const uint64_t np)
    : m_tileset(tileset)
    , m_key(ck)
    , m_capacity(np)
    , m_mid(m_key.bounds().mid())
{
    m_xyz.reserve(m_capacity * 3);
    if (m_tileset.hasColor()) m_rgb.reserve(m_capacity * 3);
    if (m_tileset.hasNormals()) m_normals.reserve(m_capacity * 3);
}

std::vector<char> Pnts::build()
{
    const Metadata& m(m_tileset.metadata());

    auto layout(toLayout(m.absoluteSchema, m.dataType == io::Type::Laszip));
    VectorPointTable table(layout, m_capacity);

    table.setProcess([this, &table]() { read(table); });

    m_tileset.io().read(m_key.toString() + getPostfix(m), table);

    return buildFile();
}

void Pnts::read(VectorPointTable& table)
{
    m_np += table.numPoints();

    const ColorType colorType(m_tileset.colorType());
    const bool hasColor(m_tileset.hasColor());
    const bool hasNormals(m_tileset.hasNormals());

    uint8_t r(0), g(0), b(0);

    if (colorType == ColorType::Tile)
    {
        uint64_t h(std::hash<std::string>()(m_key.toString()));
        r = h & 0xff;
        g = (h >> 8) & 0xff;
        b = (h >> 16) & 0xff;
    }

    for (const auto& pr : table)
    {
        m_xyz.push_back(pr.getFieldAs<double>(DimId::X) - m_mid.x);
        m_xyz.push_back(pr.getFieldAs<double>(DimId::Y) - m_mid.y);
        m_xyz.push_back(pr.getFieldAs<double>(DimId::Z) - m_mid.z);

        if (hasColor)
        {
            if (colorType == ColorType::Rgb)
            {
                r = m_tileset.toEightBit(pr.getFieldAs<uint16_t>(DimId::Red));
                g = m_tileset.toEightBit(pr.getFieldAs<uint16_t>(DimId::Green));
                b = m_tileset.toEightBit(pr.getFieldAs<uint16_t>(DimId::Blue));
            }
            else if (colorType == ColorType::Intensity)
            {
                r = g = b = m_tileset.toEightBit(
                        pr.getFieldAs<uint16_t>(DimId::Intensity));
            }

            m_rgb.push_back(r);
            m_rgb.push_back(g);
            m_rgb.push_back(b);
        }

        if (hasNormals)
        {
            m_normals.push_back(pr.getFieldAs<float>(DimId::NormalX));
            m_normals.push_back(pr.getFieldAs<float>(DimId::NormalY));
            m_normals.push_back(pr.getFieldAs<float>(DimId::NormalZ));
        }
    }
}

std::vector<char> Pnts::buildFile() const
{
    json featureTable;
    featureTable["POINTS_LENGTH"] = m_np;
    featureTable["RTC_CENTER"] = m_mid;

    uint64_t byteOffset(0);
    featureTable["POSITION"]["byteOffset"] = byteOffset;
    byteOffset += m_xyz.size() * sizeof(float);

    if (m_tileset.hasColor())
    {
        featureTable["RGB"]["byteOffset"] = byteOffset;
        byteOffset += m_rgb.size();
    }

    if (m_tileset.hasNormals())
    {
        featureTable["NORMAL"]["byteOffset"] = byteOffset;
        byteOffset += m_normals.size() * sizeof(float);
    }

    std::string featureString = featureTable.dump();
    while (featureString.size() % 8) featureString += ' ';

    const uint64_t headerSize(28);
    const uint64_t binaryBytes(byteOffset);
    const uint64_t totalBytes(headerSize + featureString.size() + binaryBytes);

    std::vector<char> pnts;
    pnts.reserve(totalBytes);

    const std::string magic("pnts");
    pnts.insert(pnts.end(), magic.begin(), magic.end());
    append(pnts, 1);                                    // Version.
    append(pnts, static_cast<uint32_t>(totalBytes));
    append(pnts, static_cast<uint32_t>(featureString.size()));
    append(pnts, static_cast<uint32_t>(binaryBytes));
    append(pnts, 0);                                    // BatchTableJson.
    append(pnts, 0);                                    // BatchTableBinary.
    assert(pnts.size() == headerSize);

    pnts.insert(pnts.end(), featureString.begin(), featureString.end());
    append(pnts, m_xyz);
    append(pnts, m_rgb);
    append(pnts, m_normals);

    return pnts;
}

} // namespace cesium
} // namespace entwine
