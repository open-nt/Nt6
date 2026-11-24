# QRhi in Nt GUI

Step 2 of the ray tracing roadmap (see the memory note `ray-tracing-roadmap`): a Qt-compatible subset of QRhi, so that
Nt Quick and Nt Quick 3D (steps 4 and 5) and the Nt-only ray tracing layer (step 3) have a GPU abstraction to sit on.

## Scope of the subset

Header paths follow Qt (`<rhi/qrhi.h>`, `<rhi/qshader.h>` with `QtGui` on the include path), names and signatures follow
Qt 6 where Nt has the pieces they need.

| Piece | Status |
| --- | --- |
| `QShader`, `QShaderKey`, `QShaderVersion`, `QShaderCode` (SPIR-V and GLSL variants) | milestone 1, done |
| `ntsb` (MIT replacement of `qsb`: `glslc` to SPIR-V, packs a `.nsb`) | milestone 1, done |
| `QRhi`, resources (`QRhiBuffer`, `QRhiTexture`, `QRhiSampler`, `QRhiRenderBuffer`), render targets, pass descriptors | milestone 1, done |
| `QRhiShaderResourceBindings`, `QRhiGraphicsPipeline`, `QRhiComputePipeline` | milestone 1, done |
| `QRhiResourceUpdateBatch`, `QRhiCommandBuffer`, offscreen frames, readback | milestone 1, done |
| Null backend (bookkeeping only, for tests without a GPU) | milestone 1, done |
| Vulkan backend (`QRhiVulkanInitParams`, native handles) on `QVulkanInstance` | milestone 1, done |
| `QRhiSwapChain` on `QWindow` (Vulkan) | milestone 2 |
| OpenGL ES 2 / desktop GL backend on `QOpenGLContext` | milestone 3 |
| Ray tracing (Nt only): acceleration structures, ray tracing pipeline, `traceRays` | step 3 |

D3D11, D3D12 and Metal are not built: Nt targets Vulkan and OpenGL. The `QRhi::Implementation` enumerators exist so code
that names them compiles, and `QRhi::create()` returns `nullptr` for them.

## Differences from Qt (to go in each class's `\differences`)

* Shaders are Nt `.nsb` containers made by `ntsb`, not Qt `.qsb` files; there is no reflection (`QShaderDescription`), the
  application gives the bindings itself. SPIR-V is the only variant the Vulkan backend reads.
* No multiview, no tessellation or geometry shader pipelines, no timestamps, no pipeline cache data in milestone 1.

## Tests

Run on lavapipe, as the Vulkan tests do: draw a triangle into an offscreen texture, read it back, compare pixels. The Null
backend is tested for resource bookkeeping and for the argument checks that every backend shares.

## Status (2026-11-24)

Milestone 1 is done: the generated public header is `include/QtGui/rhi/qrhi.h` (`tools/rhi-gen/rhi_gen.py` writes it, so
that every member has a doc comment), the backends are `src/gui/rhi/qrhinull.cpp` and `qrhivulkan.cpp`, the tests are
`tests/test_rhi.cpp` (Null, shader container, ntsb) and `tests/test_rhi_vulkan.cpp` (a draw with a vertex buffer, a dynamic
uniform buffer and a texture, read back; a compute fill; it ran on an RTX GPU and on lavapipe).

Known limits of the Vulkan backend, to lift in milestone 2: frames are synchronous (one in flight, the queue is waited on at
the end of each frame), memory is allocated for each resource without a sub-allocator, textures and buffers are tracked with
whole-resource barriers (`ALL_COMMANDS`), a texture is in one layout for all its levels and layers, and storage images are put
in the general layout when the bindings are made.
