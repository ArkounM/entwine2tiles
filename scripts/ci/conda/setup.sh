#!/bin/bash

pwd
ls
git clone https://github.com/conda-forge/entwine-feedstock.git

cd entwine-feedstock

# The feedstock recipe describes upstream entwine. Repoint it at this working
# tree and rename it, because the binary this fork installs is entwine2tiles and
# a package called "entwine" that does not provide an "entwine" executable is
# broken for anyone who installs it. The rename is also what lets the two sit
# side by side in one environment, which is the point of the fork.
yq -y -i '.context.name = "entwine2tiles"' recipe/recipe.yaml
yq -y -i '.build.number = 2112' recipe/recipe.yaml

# url and sha256 are deleted, not blanked. rattler-build's recipe schema makes
# source a tagged union: UrlSource requires "url" and forbids "path",
# LocalSource requires "path" and forbids "url", and both are
# additionalProperties: false. A source carrying url, sha256 and path at once
# matches neither, so the recipe is rejected during validation, in about two
# seconds, before a single file is compiled.
yq -y -i 'del(.source.url) | del(.source.sha256) | .source.path = "../../"' recipe/recipe.yaml

# The recipe's own package test runs the upstream binary name, which no longer
# exists here. Without this, rattler-build fails the package before CI ever
# reaches the smoke test in compile.sh.
#
# "help" rather than "--version": the app has no --version flag, so that
# argument falls through to "Invalid app type" and still exits 0, which is a
# test that cannot fail. "help" prints the version and exercises the real path.
yq -y -i '.tests[0].script = ["entwine2tiles help"]' recipe/recipe.yaml

# about.license_file is deliberately left alone. It resolves against the recipe
# directory, where the feedstock keeps its own copy of the LGPL text, and that
# copy is still the correct licence for this fork.
yq -y -i '.about.homepage = "https://github.com/ArkounM/entwine2tiles"' recipe/recipe.yaml
yq -y -i '.about.repository = "https://github.com/ArkounM/entwine2tiles"' recipe/recipe.yaml
yq -y -i '.about.documentation = "https://github.com/ArkounM/entwine2tiles"' recipe/recipe.yaml
yq -y -i '.about.summary = "Entwine with the Cesium 3D Tiles writer restored, emitting glTF"' recipe/recipe.yaml

# E57 is most of what this fork gets pointed at, and the reader is a separate
# PDAL plugin package loaded at runtime. Without it the package installs and
# runs but cannot open the input anyone came here for.
yq -y -i '.requirements.run += ["libpdal-e57"]' recipe/recipe.yaml

ls recipe

# Echoed so a failing build shows what was actually handed to rattler-build.
cat recipe/recipe.yaml
