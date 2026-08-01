#!/bin/bash

git clone --depth 1 https://github.com/conda-forge/entwine-feedstock.git

cd entwine-feedstock

# On a tag, the package version comes from the tag with any leading "v" removed:
# v1.0.0 publishes as conda 1.0.0. The tags are v-prefixed because the upstream
# tags came with the preserved history, so 1.0.0 through 3.2.1 are Entwine's and
# cannot be reused. Conda versions also cannot start with a letter.
#
# Off a tag the feedstock's own version is left alone. Those builds are never
# published, so the number does not matter, and leaving it keeps branch and PR
# runs working unchanged.
VERSION=""
case "${GITHUB_REF:-}" in
    refs/tags/*)
        VERSION="${GITHUB_REF#refs/tags/}"
        VERSION="${VERSION#v}"
        ;;
esac

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
yq -y -i --arg version "$VERSION" '
    .context.name = "entwine2tiles"
  | (if $version != "" then .context.version = $version else . end)
  | .build.number = 2112
  | del(.source.url) | del(.source.sha256) | .source.path = "../../"
  | .tests[0].script = ["entwine2tiles help"]
  | .requirements.run += ["libpdal-e57"]
  | .about.homepage = "https://github.com/ArkounM/entwine2tiles"
  | .about.repository = "https://github.com/ArkounM/entwine2tiles"
  | .about.documentation = "https://github.com/ArkounM/entwine2tiles"
  | .about.summary = "Entwine with the Cesium 3D Tiles writer restored, emitting glTF"
' recipe/recipe.yaml

# A tagged build that quietly kept the feedstock's version would publish this
# fork under an upstream Entwine version number, which is the kind of wrong
# thing that looks green. Refuse to continue instead.
if [ -n "$VERSION" ]; then
    RECIPE_VERSION=$(yq -r '.context.version' recipe/recipe.yaml)
    if [ "$RECIPE_VERSION" != "$VERSION" ]; then
        echo "ERROR: tag says version $VERSION, recipe says $RECIPE_VERSION." >&2
        echo "  The recipe's version is not at .context.version any more." >&2
        exit 1
    fi
    echo "Building version $VERSION from tag ${GITHUB_REF#refs/tags/}"
fi

# Echoed so a failing build shows what was actually handed to rattler-build.
cat recipe/recipe.yaml
