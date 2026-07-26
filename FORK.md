# entwine2tiles

This is a fork of [Entwine](https://github.com/connormanning/entwine), by Connor
Manning and Hobu, Inc. Entwine is the original work; everything here that is not
listed under "What this fork adds" was written by its upstream authors and is
used unchanged.

Fork point: upstream tag `3.2.1`, commit `771764a5b69f484cb95e307c089146e1d553c75c`.
Full upstream history is preserved in this repository, so `git log` shows the real
lineage rather than a squashed import.

## Why the fork exists

Entwine builds an [EPT](https://entwine.io/en/latest/entwine-point-tile.html)
octree. Cesium for Unreal streams
[3D Tiles](https://github.com/CesiumGS/3d-tiles). Getting from one to the other
currently takes PDAL, Blender, Node, and a pile of Python glue: six external
dependencies, and on a measured 354 MB scan those middle stages are 82.3% of
wall-clock while the Entwine build itself is 17.4%.

Entwine used to do this itself. An `entwine convert` subcommand wrote Cesium 3D
Tiles from a finished EPT build, in `entwine/formats/cesium/`. It was removed in
commit `a9d59ae` (2019-12-27), which was a refactor of `app/convert.cpp` rather
than a deliberate deprecation of the format. The last commit that contains it is
`16f9709` (2019-12-16).

This fork restores that capability against the modern codebase and writes glTF
rather than the legacy `.pnts` format, so the output is 3D Tiles 1.1.

## What this fork adds

- `entwine/formats/cesium/`, restored from `16f9709` and ported to the 3.2.1 API.
- A `convert` subcommand in `app/`, likewise restored.
- The CLI binary is named `entwine2tiles` rather than `entwine`, so it can sit
  alongside a conda-installed upstream Entwine without shadowing it. Every
  upstream subcommand still works: `entwine2tiles build`, `merge`, `info`.

Upstream Entwine is not vendored or modified beyond what those additions require.
Changes to upstream files are kept small and are visible in this repository's
history after the fork point.

## License

Entwine is LGPL-2.1, so this fork is LGPL-2.1. See `COPYING` for the license text
and `LICENSE` for the copyright notices. Additions made here carry their own
copyright line and the same LGPL-2.1 terms.

Note that `pointcloud2Tiles`, the pipeline that drives this binary, is a separate
repository under Apache-2.0. The two licenses are deliberate: LGPL follows the
Entwine code, Apache stays with the original pipeline code.

## Building

Same as upstream. On Windows the route that is known to work here is a
conda-forge environment supplying `libpdal-core` plus the Visual Studio 2022
toolchain, configured with CMake and Ninja:

```
micromamba create -n entwine-build -c conda-forge libpdal-core compilers ninja cmake curl openssl
micromamba run -n entwine-build cmake -B build -G Ninja \
    -DCMAKE_PREFIX_PATH=$CONDA_PREFIX/Library \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_SHARED_LIBS=ON
micromamba run -n entwine-build cmake --build build
```

Upstream's own `readme.md` and `scripts/ci/` remain the reference for other
platforms.

## Upstream

Bug reports about indexing, EPT, or anything not in the list above belong
upstream at https://github.com/connormanning/entwine, not here.
