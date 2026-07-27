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

#include <entwine/formats/cesium/gltf.hpp>
#include <entwine/formats/cesium/pnts.hpp>
#include <entwine/formats/cesium/tile.hpp>
#include <entwine/formats/cesium/tileset.hpp>

#include <algorithm>
#include <cmath>

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

Format getFormat(const json& config)
{
    const std::string s(config.value("format", "glb"));
    if (s == "glb" || s == "gltf") return Format::Glb;
    if (s == "pnts") return Format::Pnts;
    throw std::runtime_error("Invalid cesium format: " + s);
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
    , m_format(getFormat(config))
    , m_origin(m_metadata.bounds.mid())
    , m_colorType(getColorType(config))
    , m_truncate(getTruncate(config))
    , m_rawColor(config.value("rawColor", false))
    , m_colorLut(getColorLut())
    , m_hasNormals(
            contains(m_metadata.schema, "NormalX") &&
            contains(m_metadata.schema, "NormalY") &&
            contains(m_metadata.schema, "NormalZ"))
    , m_rootGeometricError(
            m_metadata.bounds.width() /
                config.value("geometricErrorDivisor", 32.0))
    , m_rootErrorMultiplier(config.value("rootErrorMultiplier", 1.0))
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

std::string Tileset::formatString() const
{
    switch (m_format)
    {
        case Format::Glb:   return "glb";
        case Format::Pnts:  return "pnts";
        default:            return "unknown";
    }
}

std::string Tileset::contentExtension() const
{
    return m_format == Format::Pnts ? ".pnts" : ".glb";
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

bool Tileset::getTruncate(const json& config) const
{
    if (config.count("truncate")) return config.at("truncate").get<bool>();

    // 3D Tiles colors are 8 bit and LAS stores them in 16, but whether a file
    // uses the high byte varies by scanner, so ask the data rather than making
    // the caller know. Entwine records per-dimension stats during the build.
    if (m_colorType == ColorType::None || m_colorType == ColorType::Tile)
    {
        return false;
    }

    const std::string name(
            m_colorType == ColorType::Intensity ? "Intensity" : "Red");

    if (const Dimension* d = maybeFind(m_metadata.schema, name))
    {
        if (d->stats) return d->stats->maximum > 255.0;
    }

    return false;
}

std::vector<uint16_t> Tileset::getColorLut() const
{
    // glTF defines COLOR_0 as linear, and scanner colour is sRGB encoded, so
    // writing the source values unchanged makes everything wash out. This is
    // the same conversion Blender was applying on the way out of the pipeline
    // this replaces.
    auto srgbToLinear([](double v)
    {
        if (v <= 0.04045) return v / 12.92;
        return std::pow((v + 0.055) / 1.055, 2.4);
    });

    const double denominator = m_truncate ? 65535.0 : 255.0;

    std::vector<uint16_t> lut(65536);

    for (std::size_t i(0); i < lut.size(); ++i)
    {
        double v(std::min(1.0, static_cast<double>(i) / denominator));
        if (!m_rawColor) v = srgbToLinear(v);
        lut[i] = static_cast<uint16_t>(std::lround(v * 65535.0));
    }

    return lut;
}

void Tileset::build() const
{
    const ChunkKey rootKey(m_metadata.bounds, getStartDepth(m_metadata));

    json root(build(rootKey));
    if (root.is_null()) throw std::runtime_error("This build has no points");

    double geometricError(m_rootGeometricError);

    if (m_rootErrorMultiplier != 1.0)
    {
        // A structural node above the octree root, holding no content of its
        // own, so the top of the tileset starts loading from farther away.
        geometricError *= m_rootErrorMultiplier;

        json wrapper(json::object());
        wrapper["boundingVolume"] = root.at("boundingVolume");
        wrapper["geometricError"] = geometricError;
        wrapper["refine"] = "ADD";
        wrapper["children"] = json::array({ root });

        root = wrapper;
    }

    json asset(json::object());
    asset["version"] = m_format == Format::Pnts ? "1.0" : "1.1";

    // Content and bounding volumes are relative to this point. Anything that
    // needs to place the tileset on the globe needs it.
    asset["extras"]["entwine2tiles"]["origin"] = m_origin;

    json j(json::object());
    j["asset"] = asset;
    j["geometricError"] = geometricError;
    j["root"] = root;

    m_threadPool.await();

    ensurePut(m_out, "tileset.json", j.dump(2));
}

void Tileset::write(const ChunkKey& ck, const uint64_t np) const
{
    const std::string path(ck.toString() + contentExtension());

    if (m_format == Format::Pnts)
    {
        Pnts pnts(*this, ck, np);
        ensurePut(m_out, path, pnts.build());
    }
    else
    {
        Gltf gltf(*this, ck, np);
        ensurePut(m_out, path, gltf.build());
    }
}

json Tileset::build(const ChunkKey& ck) const
{
    const auto it(m_hierarchy.map.find(ck.dxyz()));
    if (it == m_hierarchy.map.end()) return json();

    const int64_t np(it->second);
    if (np <= 0) return json();

    ++m_tileCount;
    m_pointCount += np;

    m_threadPool.add([this, ck, np]() { write(ck, np); });

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
