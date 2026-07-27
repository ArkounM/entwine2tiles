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

ColorType getColorType(const json& config, const Metadata& metadata)
{
    if (config.count("colorType"))
    {
        return toColorType(config.at("colorType").get<std::string>());
    }

    if (contains(metadata.schema, "Red") &&
        contains(metadata.schema, "Green") &&
        contains(metadata.schema, "Blue"))
    {
        return ColorType::Rgb;
    }

    if (contains(metadata.schema, "Intensity")) return ColorType::Intensity;

    return ColorType::None;
}

bool getTruncate(
    const json& config,
    const Metadata& metadata,
    const ColorType colorType)
{
    if (config.count("truncate")) return config.at("truncate").get<bool>();

    if (colorType == ColorType::None || colorType == ColorType::Tile)
    {
        return false;
    }

    const std::string name(
        colorType == ColorType::Intensity ? "Intensity" : "Red");

    if (const Dimension* d = maybeFind(metadata.schema, name))
    {
        if (d->stats) return d->stats->maximum > 255.0;
    }

    return false;
}

// glTF defines COLOR_0 as linear and scanner colour is sRGB encoded, so
// writing source values unchanged makes everything wash out. This is the
// conversion the Blender step in the old pipeline was applying.
std::vector<uint16_t> makeLinearLut(
    const json& config,
    const Format format,
    const ColorType colorType,
    const bool truncate)
{
    if (format != Format::Glb || colorType == ColorType::None)
    {
        return std::vector<uint16_t>();
    }

    const bool raw(config.value("rawColor", false));
    const double denominator(truncate ? 65535.0 : 255.0);

    std::vector<uint16_t> lut(65536);

    for (std::size_t i(0); i < lut.size(); ++i)
    {
        double v(std::min(1.0, static_cast<double>(i) / denominator));

        if (!raw)
        {
            v = v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        }

        lut[i] = static_cast<uint16_t>(std::lround(v * 65535.0));
    }

    return lut;
}

} // unnamed namespace

const FormatTraits& traits(const Format format)
{
    static const FormatTraits glb { "glb", ".glb", "1.1" };
    static const FormatTraits pnts { "pnts", ".pnts", "1.0" };

    return format == Format::Pnts ? pnts : glb;
}

Format toFormat(const std::string s)
{
    if (s == "glb" || s == "gltf") return Format::Glb;
    if (s == "pnts") return Format::Pnts;
    throw std::runtime_error("Invalid cesium format: " + s);
}

ColorType toColorType(const std::string s)
{
    if (s == "none") return ColorType::None;
    if (s == "rgb") return ColorType::Rgb;
    if (s == "intensity") return ColorType::Intensity;
    if (s == "tile") return ColorType::Tile;
    throw std::runtime_error("Invalid cesium colorType: " + s);
}

std::string toString(const ColorType c)
{
    switch (c)
    {
        case ColorType::None:       return "none";
        case ColorType::Rgb:        return "rgb";
        case ColorType::Intensity:  return "intensity";
        case ColorType::Tile:       return "tile";
    }

    return "unknown";
}

Tileset::Tileset(const json& config)
    : m_in(
            std::shared_ptr<arbiter::Arbiter>(config::getArbiter(config)),
            config.at("input").get<std::string>(),
            config::getTmp(config))
    , m_out(m_in.arbiter->getEndpoint(config.at("output").get<std::string>()))
    , m_metadata(loadMetadata(m_in))
    , m_hierarchy(hierarchy::load(m_in.hierarchy, config::getThreads(config)))
    , m_io(Io::create(m_metadata, m_in))
    , m_format(toFormat(config.value("format", "glb")))
    , m_origin(m_metadata.bounds.mid())
    , m_colorType(getColorType(config, m_metadata))
    , m_truncate(getTruncate(config, m_metadata, m_colorType))
    , m_linearLut(makeLinearLut(config, m_format, m_colorType, m_truncate))
    , m_hasNormals(
            contains(m_metadata.schema, "NormalX") &&
            contains(m_metadata.schema, "NormalY") &&
            contains(m_metadata.schema, "NormalZ"))
    , m_rootGeometricError(
            m_metadata.bounds.width() /
                config.value("geometricErrorDivisor", 32.0))
    , m_rootErrorMultiplier(config.value("rootErrorMultiplier", 1.0))
    , m_threadPool(
            std::max<uint64_t>(4, config::getThreads(config)),
            std::max<uint64_t>(4, config::getThreads(config)) * 4)
{
    if (m_metadata.subset)
    {
        throw std::runtime_error(
                "This is a subset build. Merge it before converting.");
    }

    arbiter::mkdirp(m_out.root());
}

void Tileset::build() const
{
    const ChunkKey rootKey(m_metadata.bounds, getStartDepth(m_metadata));

    json root(build(rootKey));
    if (root.is_null()) throw std::runtime_error("This build has no points");

    if (m_rootErrorMultiplier != 1.0)
    {
        // A structural node above the octree root, holding no content of its
        // own, so the top of the tileset starts loading from farther away.
        json wrapper(json::object());
        wrapper["boundingVolume"] = root.at("boundingVolume");
        wrapper["geometricError"] = rootGeometricError();
        wrapper["refine"] = "ADD";
        wrapper["children"] = json::array();
        wrapper["children"].push_back(std::move(root));

        root = std::move(wrapper);
    }

    json asset(json::object());
    asset["version"] = traits(m_format).assetVersion;

    // Anything that needs to place this tileset on the globe needs the origin
    // its content is relative to.
    asset["extras"]["entwine2tiles"]["origin"] = m_origin;

    json j(json::object());
    j["asset"] = std::move(asset);
    j["geometricError"] = rootGeometricError();
    j["root"] = std::move(root);

    m_threadPool.join();

    // Pool swallows task exceptions into a list. Without this check a failed
    // write still produces a tileset referencing missing content, and exits 0.
    const auto& errors(m_threadPool.errors());
    if (errors.size())
    {
        throw std::runtime_error(
                std::to_string(errors.size()) + " tiles failed to write, "
                "the first with: " + errors.front());
    }

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

    json j(toTile(*this, ck));

    for (std::size_t i(0); i < dirEnd(); ++i)
    {
        json child(build(ck.getStep(toDir(i))));
        if (!child.is_null()) j["children"].push_back(std::move(child));
    }

    return j;
}

} // namespace cesium
} // namespace entwine
