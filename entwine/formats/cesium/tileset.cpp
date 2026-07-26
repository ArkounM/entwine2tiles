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
#include <entwine/formats/cesium/tile.hpp>
#include <entwine/formats/cesium/tileset.hpp>

#include <entwine/types/dir.hpp>
#include <entwine/util/config.hpp>
#include <entwine/util/io.hpp>

namespace entwine
{
namespace cesium
{

namespace
{

// An EPT build writes its metadata across two files, and the builder merges
// them the same way when it reloads a build.
Metadata loadMetadata(const Endpoints& in)
{
    const json j = entwine::merge(
        json::parse(ensureGet(in.output, "ept-build.json")),
        json::parse(ensureGet(in.output, "ept.json")));
    return config::getMetadata(j);
}

} // unnamed namespace

Tileset::Tileset(const json& config)
    : m_arbiter(
            std::shared_ptr<arbiter::Arbiter>(config::getArbiter(config)))
    , m_in(
            m_arbiter,
            config.at("input").get<std::string>(),
            config::getTmp(config))
    , m_out(m_arbiter->getEndpoint(config.at("output").get<std::string>()))
    , m_metadata(loadMetadata(m_in))
    , m_hierarchy(
            hierarchy::load(m_in.hierarchy, config::getThreads(config)))
    , m_io(Io::create(m_metadata, m_in))
    , m_colorType(getColorType(config))
    , m_truncate(config.value("truncate", false))
    , m_hasNormals(
            contains(m_metadata.schema, "NormalX") &&
            contains(m_metadata.schema, "NormalY") &&
            contains(m_metadata.schema, "NormalZ"))
    , m_rootGeometricError(
            m_metadata.bounds.width() /
                config.value("geometricErrorDivisor", 32.0))
    , m_threadPool(std::max<uint64_t>(4, config::getThreads(config)))
{
    if (m_metadata.subset)
    {
        throw std::runtime_error(
                "This is a subset build. Merge it before converting.");
    }

    arbiter::mkdirp(m_out.root());
}

std::string Tileset::colorString() const
{
    switch (m_colorType)
    {
        case ColorType::None:       return "none";
        case ColorType::Rgb:        return "rgb";
        case ColorType::Intensity:  return "intensity";
        case ColorType::Tile:       return "tile";
        default:                    return "unknown";
    }
}

ColorType Tileset::getColorType(const json& config) const
{
    if (config.count("colorType"))
    {
        const auto s(config.at("colorType").get<std::string>());
        if (s == "none")        return ColorType::None;
        if (s == "rgb")         return ColorType::Rgb;
        if (s == "intensity")   return ColorType::Intensity;
        if (s == "tile")        return ColorType::Tile;
        throw std::runtime_error("Invalid cesium colorType: " + s);
    }
    else if (
            contains(m_metadata.schema, "Red") &&
            contains(m_metadata.schema, "Green") &&
            contains(m_metadata.schema, "Blue"))
    {
        return ColorType::Rgb;
    }
    else if (contains(m_metadata.schema, "Intensity"))
    {
        return ColorType::Intensity;
    }

    return ColorType::None;
}

void Tileset::build() const
{
    const ChunkKey root(m_metadata.bounds, getStartDepth(m_metadata));

    const json j {
        { "asset", { { "version", "1.0" } } },
        { "geometricError", m_rootGeometricError },
        { "root", build(root) }
    };

    m_threadPool.await();

    ensurePut(m_out, "tileset.json", j.dump(2));
}

json Tileset::build(const ChunkKey& ck) const
{
    const auto it(m_hierarchy.map.find(ck.dxyz()));
    if (it == m_hierarchy.map.end()) return json();

    const int64_t np(it->second);
    if (np <= 0) return json();

    ++m_tileCount;
    m_pointCount += np;

    m_threadPool.add([this, ck, np]()
    {
        Pnts pnts(*this, ck, np);
        ensurePut(m_out, ck.toString() + ".pnts", pnts.build());
    });

    json j(Tile(*this, ck));

    for (std::size_t i(0); i < dirEnd(); ++i)
    {
        const json child(build(ck.getStep(toDir(i))));
        if (!child.is_null()) j["children"].push_back(child);
    }

    return j;
}

} // namespace cesium
} // namespace entwine
