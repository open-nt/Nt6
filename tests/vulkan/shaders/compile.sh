#!/bin/sh
# MIT License - see LICENSE
#
# Compiles the ray tracing shaders of nt_vulkan_tests (the rtriangle example's) to SPIR-V with glslc (from shaderc, in the Vulkan SDK or
# the glslc package). The .spv files are checked in, so building Nt needs no shader compiler.
set -e
cd "$(dirname "$0")"
for stage in rgen rmiss rchit; do
    glslc --target-env=vulkan1.2 -fshader-stage=$stage -O rtriangle.$stage -o rtriangle.$stage.spv
done

# The shaders of nt_rhi_vulkan_tests (QRhi): a vertex colored triangle with a tint and a texture, and a compute fill.
glslc --target-env=vulkan1.0 rhi_tri.vert -o rhi_tri.vert.spv
glslc --target-env=vulkan1.0 rhi_tri.frag -o rhi_tri.frag.spv
glslc --target-env=vulkan1.0 rhi_fill.comp -o rhi_fill.comp.spv
