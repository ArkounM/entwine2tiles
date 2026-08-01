![Entwine logo](./doc/logo/color/entwine_logo_2-color-small.png)

> **This is `entwine2tiles`, a fork of [Entwine](https://github.com/connormanning/entwine)
> by Connor Manning and Hobu, Inc.** It restores the Cesium 3D Tiles output that
> upstream removed in 2019 and updates it to write glTF, so a finished EPT build
> can be converted to 3D Tiles 1.1 without PDAL, Blender, or Node in the loop.
> Everything else is upstream Entwine, unchanged. See [FORK.md](./FORK.md) for the
> fork point, what was added, and licensing. Entwine is LGPL-2.1 and so is this.
> The readme below is upstream's.

## Build Status

[![Build](https://github.com/ArkounM/entwine2tiles/actions/workflows/build.yml/badge.svg)](https://github.com/ArkounM/entwine2tiles/actions/workflows/build.yml)
[![Conda](https://github.com/ArkounM/entwine2tiles/actions/workflows/conda.yml/badge.svg)](https://github.com/ArkounM/entwine2tiles/actions/workflows/conda.yml)
[![Docker](https://github.com/ArkounM/entwine2tiles/actions/workflows/docker.yml/badge.svg)](https://github.com/ArkounM/entwine2tiles/actions/workflows/docker.yml)

## Installing this fork

The binary links PDAL, GDAL, PROJ, curl and OpenSSL, so there is no useful bare
executable to download. Two deliveries carry all of it.

Conda, which puts `entwine2tiles` on your `PATH`:

```
conda create --yes --name entwine2tiles \
    --channel amerchant --channel conda-forge entwine2tiles
conda activate entwine2tiles
entwine2tiles help
```

`--channel conda-forge` is required, not optional: the package depends on
`libpdal-core` and `libpdal-e57`, which live there. Without it the solve fails
with "nothing provides libpdal-core".

Or the container, if you would rather install nothing:

```
docker pull ghcr.io/arkounm/entwine2tiles:latest

docker run --rm -v /data:/data ghcr.io/arkounm/entwine2tiles \
    build -i /data/scan.las -o /data/ept

docker run --rm -v /data:/data ghcr.io/arkounm/entwine2tiles \
    convert -i /data/ept -o /data/tiles -t 16 -g 16.0 --rootErrorMultiplier 16.0
```

The image is built from this source tree rather than from the conda-forge
`entwine` package, and both deliveries include `libpdal-e57`, so E57 input works
without a conversion step. Every commit on `main` publishes `sha-<short sha>` and
moves `latest`; git tags publish under the tag name as well, and also publish the
conda package. Branches and pull requests are built and smoke tested but never
published.

Releases are versioned for the fork, starting at `v1.0.0`. The tags numbered
`1.0.0` through `3.2.1` came with the preserved upstream history and are
Entwine's own.

To build from source instead, see [FORK.md](./FORK.md). The upstream instructions
below still apply, with two differences: the executable is `entwine2tiles`, and
`conda install entwine` gives you upstream, not this fork.

Entwine is a data organization library for massive point clouds, designed to conquer datasets of hundreds of billions of points as well as desktop-scale point clouds.  Entwine can index anything that is [PDAL](https://pdal.io)-readable, and can read/write to a variety of sources like S3 or Dropbox.  Builds are completely lossless, so no points will be discarded even for terabyte-scale datasets.

Check out the client demos, showcasing Entwine output with [Cesium](https://viewer.copc.io?state=209b4b8dc400dd769eed0b8c15ecb46de666b10658fb12fc9c32c81f48242ad1) (see
"Sample Data" section) and [Potree](http://potree.entwine.io) clients.

Usage
--------------------------------------------------------------------------------

Getting started with Entwine is easy with [Conda](https://conda.io/docs/).  First,
create an environment with the `entwine` package, then activate this environment:
```
conda create --yes --name entwine --channel conda-forge entwine
conda activate entwine
```

Now we can index some public data:
```
entwine build \
    -i https://data.entwine.io/red-rocks.laz \
    -o ~/entwine/red-rocks
```

Now we have our output at `~/entwine/red-rocks`.  We could have also passed a directory like `-i ~/county-data/` to index multiple files.  Now we can
statically serve `~/entwine` with a simple HTTP server:
```
docker run -it -v ~/entwine:/var/www -p 8080:8080 connormanning/http-server
```

And view the data with [Cesium](https://viewer.copc.io/?q=http://localhost:8080/red-rocks/ept.json) or [Potree](http://potree.entwine.io/data/custom.html?r=http://localhost:8080/red-rocks/ept.json).

Going further
--------------------------------------------------------------------------------

For detailed information about how to configure your builds, check out the [configuration documentation](https://entwine.io/configuration.html).  Here, you can find information about reprojecting your data, using configuration files and templates, enabling S3 capabilities, and all sorts of other settings.

To learn about the Entwine Point Tile (EPT) file format produced by Entwine, see the [file format documentation](https://entwine.io/entwine-point-tile.html).

For an alternative method of generating EPT which can also generate [COPC](https://copc.io) data, see the [Untwine](https://github.com/hobuinc/untwine) project.
