# Graphics and GPU research

## Implemented experiment
`src/graphics/` contains a **CPU-only software RGBA8 surface**, not a GPU implementation. `Surface::create` requires nonzero dimensions and a caller-provided pixel limit; allocation/length failures return no surface. A move-only surface owns its pixels; a moved-from surface is empty. A command batch can clear the surface, overwrite a rectangle, or copy a rectangle within the same surface. RGBA values are stored verbatim; there is no alpha blending or color-space conversion. Rectangles must fit entirely; empty rectangles are permitted at boundaries. Coordinates and extents are checked without overflow. Copy uses snapshot-equivalent semantics for overlap. The whole batch is validated before drawing, so an invalid command leaves the surface unchanged. The API is single-threaded; callers must synchronize access and treat returned pixel spans as invalid after moving or destroying the surface.

`nightfall_graphics_demo` produces an independently created PPM image. It does not draw a game or use GPU hardware. Tests use a separately implemented snapshot reference and 100 deterministic scenarios of 200 batches each, plus edge and overlap cases. See TESTING.md. This is a starting point for experiments in explicit command/resource behavior, not an emulation of any real GPU.

## Research still needed
RDNA 2 branding is not a public specification of the console's command protocol. Compare Vulkan's explicit queues and synchronization with Direct3D 12 on Windows using original workloads, but neither translates console commands automatically. Guest command formats, shaders, resource binding, coherency, queues, texture formats, synchronization, CPU/GPU integration and PS5 compatibility are unimplemented. No graphics hardware was available in this environment.

## Triangle and depth software experiments
`Surface` supports constant-color clipped triangles with integer vertices,
pixel-center sampling and the top-left fill rule. Bounds [-32768,32767]
keep integer edge calculations safe; invalid batches leave the image unchanged.
`DepthSurface` separately implements flat-color triangles and a 16-bit affine
depth buffer with strict LESS testing, rounded depth interpolation, explicit
clear and the same coverage rules. It has no shaders, interpolation of vertex
colors, texture sampling, perspective correction, GPU backend, console command
translation or PS5 compatibility. Its explicit pixel limit bounds allocation;
it is single-threaded and is not a security boundary.

`nightfall_depth_demo output.ppm` produces an original, overlapping-triangle
image on the CPU. Tests cover edge ownership, clipping, order, depth
interpolation, transactional rejection, 10,000 triangle reference cases and
2,000 randomized multi-triangle scenes against a full-surface software model.
These are software comparisons, not an independent GPU conformance test.
