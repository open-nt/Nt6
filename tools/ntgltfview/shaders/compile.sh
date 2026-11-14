#!/bin/sh
# MIT License - see LICENSE
#
# Compiles the ray tracing shaders of ntgltfview to SPIR-V with glslc (from shaderc, in the Vulkan SDK or the glslc
# package). The .spv files are checked in, so building Nt needs no shader compiler.
set -e
cd "$(dirname "$0")"
for shader in ntgltfview.rgen:rgen ntgltfview.rmiss:rmiss ntgltfview_shadow.rmiss:rmiss ntgltfview.rchit:rchit \
    ntgltfview.rahit:rahit; do
    file=${shader%%:*}
    stage=${shader##*:}
    glslc --target-env=vulkan1.2 -fshader-stage=$stage -O "$file" -o "$file.spv"
done
