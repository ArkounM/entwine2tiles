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

#include "convert.hpp"

#include <iostream>

#include <entwine/formats/cesium/tileset.hpp>
#include <entwine/util/json.hpp>

namespace entwine
{
namespace app
{

void Convert::addArgs()
{
    m_ap.setUsage("entwine2tiles convert <options>");

    auto addDouble([this](std::string name, std::string abbrev, std::string key,
                std::string description)
    {
        auto handler([this, key](json j)
        {
            m_json[key] = json::parse(j.get<std::string>()).get<double>();
        });

        // An empty abbreviation would register as the default handler, which
        // is how ArgParser spells "the positional argument".
        if (abbrev.empty()) m_ap.add(name, description, handler);
        else m_ap.add(name, abbrev, description, handler);
    });

    m_ap.add(
            "--input",
            "-i",
            "Path to a completed entwine build",
            [this](json j) { m_json["input"] = j; });

    addOutput("Path for Cesium 3D Tiles output");
    addConfig();
    addTmp();
    addSimpleThreads();
    addArbiter();

    addDouble(
            "--geometricErrorDivisor",
            "-g",
            "geometricErrorDivisor",
            "The root geometric error is determined as the width of the "
            "dataset cube divided by \"geometricErrorDivisor\", which defaults "
            "to 32.  Smaller values will result in the data being loaded "
            "at higher density\n"
            "Example: --geometricErrorDivisor 16.0");

    addDouble(
            "--rootErrorMultiplier",
            "",
            "rootErrorMultiplier",
            "Multiply the geometric error of the root by this value and give "
            "it a content-free parent node, so the top of the tileset loads "
            "from farther away.  Defaults to 1, which adds no such node.\n"
            "Example: --rootErrorMultiplier 16.0");

    m_ap.add(
            "--format",
            "-f",
            "Content format for the tiles.\n"
            "Valid values:\n"
            "'glb': binary glTF, the 3D Tiles 1.1 content format (default)\n"
            "'pnts': the legacy point cloud format, deprecated in 3D Tiles 1.1",
            [this](json j) { m_json["format"] = j; });

    m_ap.add(
            "--colorType",
            "The coloring for the output tileset.  May be omitted to "
            "default to RGB or Intensity, in that order, if they exist.\n"
            "Valid values:\n"
            "'none': no color\n"
            "'rgb': color by RGB values\n"
            "'intensity': grayscale by intensity values\n"
            "'tile': a color per tile, for inspecting the octree",
            [this](json j) { m_json["colorType"] = j; });

    m_ap.add(
            "--rawColor",
            "Write color values as they are found, rather than converting "
            "them from sRGB to the linear values glTF expects.  Only useful "
            "for a source whose color is already linear, which is unusual.",
            [this](json j) { checkEmpty(j); m_json["rawColor"] = true; });

    m_ap.add(
            "--truncate",
            "3D Tiles supports 8-bit color values.  If RGB (or Intensity, if "
            "using intensity colorType) values are 16-bit, set this option to "
            "scale them to 8-bit.  Detected from the data if omitted.",
            [this](json j) { checkEmpty(j); m_json["truncate"] = true; });
}

void Convert::run()
{
    cesium::Tileset tileset(m_json);

    std::cout << "Converting:" << std::endl;
    std::cout << "\tInput:  " << tileset.in().output.prefixedRoot() << "\n";
    std::cout << "\tOutput: " << tileset.out().prefixedRoot() << "\n";
    std::cout << "\tFormat: " << toString(tileset.format()) << "\n";
    std::cout << "\tColor:  " << toString(tileset.colorType()) << "\n";
    std::cout << "\tThreads: " << tileset.threadPool().numThreads() << "\n";
    std::cout << "\tOrigin: " << tileset.origin() << "\n";
    std::cout << "\tRoot geometric error: " <<
        tileset.rootGeometricError() << std::endl;

    if (tileset.hasNormals() && tileset.format() != cesium::Format::Pnts)
    {
        std::cout << "\tNote: this build has normals, which only the pnts "
            "format writes." << std::endl;
    }

    std::cout << "Running..." << std::endl;
    tileset.build();
    std::cout << "\tTiles: " << commify(tileset.tileCount()) << "\n";
    std::cout << "\tPoints: " << commify(tileset.pointCount()) << "\n";
    std::cout << "\tDone." << std::endl;
}

} // namespace app
} // namespace entwine
