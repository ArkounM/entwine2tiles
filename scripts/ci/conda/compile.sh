#!/bin/bash

mkdir packages

export CI_PLAT=""
if grep -q "windows" <<< "$PDAL_PLATFORM"; then
    CI_PLAT="win"
    ARCH="64"
fi

if grep -q "ubuntu" <<< "$PDAL_PLATFORM"; then
    CI_PLAT="linux"
    ARCH="64"
fi

if grep -q "macos" <<< "$PDAL_PLATFORM"; then
    CI_PLAT="osx"
    ARCH="arm64"
fi

rattler-build build -r recipe --output-dir packages -m ".ci_support/${CI_PLAT}_${ARCH}_.yaml"

# Package and binary are both entwine2tiles here, renamed in setup.sh.
conda create -y -n test -c ./packages/${CI_PLAT}-${ARCH} entwine2tiles
conda deactivate

conda activate test
entwine2tiles help
conda deactivate

