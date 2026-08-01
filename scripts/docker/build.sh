#!/bin/bash

# Build the image locally the way .github/workflows/docker.yml does, and run the
# same smoke test. Never pushes: publishing happens from CI, on main and tags.

set -e

IMAGE=${IMAGE:-ghcr.io/arkounm/entwine2tiles}
VERSION=${1:-local}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)

docker build -t "$IMAGE:$VERSION" -f "$ROOT/scripts/docker/Dockerfile" "$ROOT"

for args in "help" "build --help" "convert --help"; do
    docker run --rm "$IMAGE:$VERSION" $args > /dev/null
done

echo "built and smoke tested $IMAGE:$VERSION"
