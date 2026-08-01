#!/bin/bash

git clone --depth 1 https://github.com/conda-forge/entwine-feedstock.git

cd entwine-feedstock

# The feedstock recipe builds upstream entwine. Point it at this tree and rename
# it: the binary here is entwine2tiles, and a package named "entwine" that ships
# no "entwine" executable is broken for whoever installs it. The rename is also
# what lets the two sit side by side in one environment.
#
# url and sha256 are deleted rather than blanked, because rattler-build's source
# is a tagged union: one carrying url and path at once matches neither variant
# and the recipe is rejected before anything compiles.
#
# The test runs "help" rather than "--version" because the app has no --version
# flag, so that argument exits 0 whatever happens. libpdal-e57 is the E57 reader
# plugin, loaded at runtime. about.license_file is left alone: it resolves
# against the recipe directory, which holds the LGPL text this fork ships under.
yq -y -i '
    .context.name = "entwine2tiles"
  | .build.number = 2112
  | del(.source.url) | del(.source.sha256) | .source.path = "../../"
  | .tests[0].script = ["entwine2tiles help"]
  | .requirements.run += ["libpdal-e57"]
  | .about.homepage = "https://github.com/ArkounM/entwine2tiles"
  | .about.repository = "https://github.com/ArkounM/entwine2tiles"
  | .about.documentation = "https://github.com/ArkounM/entwine2tiles"
  | .about.summary = "Entwine with the Cesium 3D Tiles writer restored, emitting glTF"
' recipe/recipe.yaml

# Echoed so a failing build shows what was actually handed to rattler-build.
cat recipe/recipe.yaml
