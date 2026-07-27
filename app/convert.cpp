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

    m_ap.add(
            "--input",
            "-i",
            "Path to a completed entwine build",
            [this](json j) { m_json["input"] = j; });

    addOutput("Path for Cesium 3D Tiles output");
    addTmp();
    addSimpleThreads();
    addArbiter();

    m_ap.add(
            "--geometricErrorDivisor",
            "-g",
            "The root geometric error is determined as the width of the "
            "dataset cube divided by \"geometricErrorDivisor\", which defaults "
            "to 32.  Smaller values will result in the data being loaded "
            "at higher density\n"
            "Example: --geometricErrorDivisor 16.0",
            [this](json j)
            {
                m_json["geometricErrorDivisor"] =
                    json::parse(j.get<std::string>()).get<double>();
            });

    m_ap.add(
            "--rootErrorMultiplier",
            "Multiply the geometric error of the root by this value and give "
            "it a content-free parent node, so the top of the tileset loads "
            "from farther away.  Defaults to 1, which adds no such node.\n"
            "Example: --rootErrorMultiplier 16.0",
            [this](json j)
            {
                m_json["rootErrorMultiplier"] =
                    json::parse(j.get<std::string>()).get<double>();
            });

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
            "'tile': random color for each tile",
            [this](json j) { m_json["colorType"] = j; });

    m_ap.add(
            "--truncate",
            "3D Tiles supports 8-bit color values.  If RGB (or Intensity, if "
            "using intensity colorType) values are 16-bit, set this option to "
            "scale them to 8-bit.",
            [this](json j)
            {
                checkEmpty(j);
                m_json["truncate"] = true;
            });
}

void Convert::run()
{
    cesium::Tileset tileset(m_json);

    std::cout << "Converting:" << std::endl;
    std::cout << "\tInput:  " << tileset.in().output.prefixedRoot() << "\n";
    std::cout << "\tOutput: " << tileset.out().prefixedRoot() << "\n";
    std::cout << "\tFormat: " << tileset.formatString() << "\n";
    std::cout << "\tColor:  " << tileset.colorString() << "\n";
    std::cout << "\tTruncate: " << yesNo(tileset.truncate()) << "\n";
    std::cout << "\tThreads: " << tileset.threadPool().numThreads() << "\n";
    std::cout << "\tOrigin: " << tileset.origin() << "\n";
    std::cout << "\tRoot geometric error: " <<
        tileset.rootGeometricError() * tileset.rootErrorMultiplier() <<
        std::endl;

    std::cout << "Running..." << std::endl;
    tileset.build();
    std::cout << "\tTiles: " << commify(tileset.tileCount()) << "\n";
    std::cout << "\tPoints: " << commify(tileset.pointCount()) << "\n";
    std::cout << "\tDone." << std::endl;
}

} // namespace app
} // namespace entwine
