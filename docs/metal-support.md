# Metal 4 implementation

NoGraphicsAPI's native Metal 4 backend implements the same GPU-pointer and descriptor-heap model as
[the Vulkan backend](vulkan-support.md). CMake selects it on macOS and iOS; it does not use MoltenVK.
The application owns memory allocation, descriptor indices, resource lifetime, and synchronization.
Native Metal objects remain internal where the driver requires them.

## Metal feature surface

Device creation requires both `MTLGPUFamilyApple7` and `MTLGPUFamilyMetal4`, with macOS/iOS/iPadOS 26 or later.
See [supported devices](../README.md#metal-4) for the Mac, iPhone, and iPad baseline.

| Requirement or facility | Contribution to the model |
| --- | --- |
| Metal 4 command queues, allocators and encoders | Explicit recording, retained command storage, and independent queues. |
| GPU addresses and placement heaps | Real 64-bit GPU pointers and application-managed data/texture storage. |
| `MTLTextureViewPool` | Application-owned texture slots with contiguous resource IDs. |
| Argument tables and sampler resource IDs | Shared root bytes and indexed texture/sampler access without per-material bindings. |
| Direct object/mesh shaders | Task/mesh draws throughout the Apple7+ baseline. |
| Native indirect mesh arguments (Apple9+) | `DeviceCaps::indirect_mesh_draw`; callers check this before indirect mesh draws. |
| Producer barriers and shared events | Stage-scoped hazards, cross-queue waits, and CPU completion checks. |
| Counter heaps | GPU markers retrieved into ordinary CPU memory after completion. |
| `CAMetalLayer` | Native macOS/iOS presentation. |

M1/M2 support direct task/mesh drawing. Applications can keep work GPU-driven by launching direct task groups
whose shaders read GPU-produced counts; the backend does not emulate unsupported indirect commands.
Query texture-format support for the intended usage. ASTC is available throughout the baseline;
BC compression is queried separately and is not implied by Metal 4 support.

## GPU heaps and application-side allocation

`GpuHeap` exposes application-sized storage and real 64-bit GPU addresses. CPU-visible and readback
heaps use shared placement buffers; GPU-only heaps use private placement buffers. `TextureHeap`
provides application-managed private placement storage. Shaders follow GPU pointers directly;
native buffer copies resolve address ranges to their internal backing buffers.

Buffer/texture copies support mip levels, physical slices, subregions and row/slice pitches.
`supports_texture_format` is authoritative for each usage. Combined depth/stencil transfers are not
advertised because the copy API has no aspect selector.

Metal supports at most 64 live `GpuHeap` allocations. Its address index uses atomic bump allocation
and compare-exchange publication of sorted snapshots in a fixed 32 KiB ring. Readers perform atomic
loads without mutexes or atomic read-modify-write. Ring reuse assumes no reader or writer remains
stalled through a complete reuse of its snapshot storage.

## Application-owned descriptor heaps

Descriptor heaps are opaque, capacity-based namespaces shared by both backends:

```cpp
gpu::TextureDescriptorHeap* textures = gpu::create_texture_descriptor_heap(device, 4096);
gpu::write_texture_descriptor(textures, 17, texture, gpu::TextureDescriptorType::sampled);
gpu::set_texture_descriptor_heap(commands, textures);
```

Texture and sampler heaps provide indexed write/copy operations, including overlapping range copies.
These are synchronous CPU operations. Concurrent writes need disjoint destinations; copy sources
must remain stable. Wait for prior GPU users before replacing slots. Referenced textures remain
application-owned; sampler heaps retain their native sampler objects, including after descriptor copies.

Texture descriptor heaps use `MTLTextureViewPool`: the shader forms a texture handle from `baseResourceID + index`,
without a resource-ID lookup table. Sampler heaps contain a typed array of eight-byte resource IDs.
The common API exposes no native descriptor bytes, strides or addresses. Vulkan keeps its mapped
coherent descriptor storage private behind the same indexed operations.

## Root ABI and shared shaders

Include `<NoGraphicsAPI/shader.slang>` and use `GPU_ROOT`, `gpu_texture<T>` and `gpu_sampler` in the
same shader source for both targets. Stock Slang 2026.18.2+ supports this path; no compiler fork is
required. See [the shader contract](slang.md) for compilation commands and layout rules.

| Metal buffer slot | Contents |
| --- | --- |
| 0 | Application root bytes, at most 256 bytes per command |
| 1 | Texture-view-pool base resource ID |
| 2 | Typed array of sampler IDs |

Each draw or dispatch snapshots its root into reusable storage retained through GPU completion.
The Metal root wrapper uses `StructuredBuffer<Root>` to preserve shared C layout for vectors,
matrices and pointers. The Vulkan wrapper uses push data and native descriptor-heap lowering.
Texture-handle reinterpretation follows the hardware-tested
[texture-view-pool experiment](https://github.com/sebbbi/msl_heap); it is not a first-class MSL
texture-pool indexing operator. The helpers inline; sampler access requires one indexed ID load.

`ShaderStage` takes precompiled metallib bytes, an entry-point name, and compiled `numthreads`
dimensions for compute, task and mesh stages. Shader compilation is offline. Vertex and mesh outputs
use the same clip positions as Vulkan: Metal maps every viewport `(x, y, width, height)` to
`(x, y + height, width, -height)`, including caller-supplied negative heights.

## Pipelines, rendering, and mesh work

Graphics, mesh, and compute PSOs consume precompiled Metal libraries; the runtime does not compile MSL source.
Graphics and mesh PSOs retain rasterization, blend state, and attachment formats. An empty fragment stage creates a depth-only pipeline.
`MeshPSODesc::task` maps to the Metal object stage; direct draw counts launch task groups when present and mesh groups otherwise.
All graphics stages share one root structure and the bound descriptor heaps.

`begin_render_pass()` starts a native render encoder and sets the full attachment viewport/scissor and disabled depth/stencil state.
Set viewport, scissor, and depth/stencil state after beginning the pass to override those defaults.
Depth/stencil objects are cached per command pool. Suspend/resume uses matching attachments and ordered submission,
with reusable native command allocators for each segment. Submit the complete chain together without intervening action or synchronization commands.

## Submission and resource lifetime

The backend uses `MTL4CommandQueue`, command allocators, encoders and argument tables. A command pool
retains its storage until destruction; reset it only after all submitted buffers complete. Each pool
and queue requires external CPU synchronization. Different pools can record concurrently. Submission
preserves buffer order, accepts GPU timeline waits, and signals the required completion point through
`MTLSharedEvent`. Texture initialization must precede its first use.

Native residency mutations use a CPU mutex. Address indexing, timestamp-slot allocation/release and
disjoint descriptor updates do not. Ordinary queue submission has no internal lock; shader-validation
mode serializes MetalTools residency enumeration. Resource destruction is immediate: wait for every
recorded/submitted use before releasing resources. Residency does not extend application lifetimes.

## Resource-free barriers

Global barriers remain outside render passes. Source execution scopes include logically earlier
graphics stages; destination scopes include logically later stages. Fragment, depth and color
destinations wait at fragment/tile stages, permitting independent vertex, object and mesh work to
overlap. Compute and transfer map to dispatch and blit. Explicit indirect dependencies cover command
front ends. Write dependencies request device visibility; execution-only dependencies do not flush
caches. A barrier synchronizes its queue; use timeline waits for cross-queue dependencies.

## Timestamps

`DeviceDesc::timestamp_query_count` reserves slots per command context; zero disables allocation and
leaves timestamp destinations untouched. Counter pages use atomic bitmaps, remain allocated until
device destruction, and share Metal's limit of 32 native counter heaps per process. Contexts retain
slots through pool resets and release them on destruction.

Markers retain ordinary CPU destinations. After submission completion, `read_timestamps(pool)` retrieves
results with `resolveCounterRange` and fills those destinations before pool reset. It neither waits nor
encodes GPU resolve commands or barriers. Contiguous native slots are read together; Metal returns temporary
native result data. Convert tick differences with `DeviceCaps::timestamp_period_ns`.
Outside rendering, markers close the current compute/copy encoder and sample after preceding work;
Metal may start following work before sampling, so these are approximate all-commands timings even
when a narrower stage is requested. Render markers use the requested native render stage. The
[known render-counter limitation](repro-metal-render-timestamps.md) also affects suspended passes.

## Presentation

`DeviceDesc::window` is a `CAMetalLayer*`, owned by the application and retained through device use.
Drawable acquisition/presentation uses queue waits and signals. `get_drawable_extent` does not acquire
a drawable; zero extent yields no frame. Submit acquired drawables for presentation on queue zero.

The supplied examples provide AppKit windows on macOS. iOS applications supply their UIKit lifecycle and compile
metallibs for the `iphoneos` SDK; see [building and integration](building.md). Layer changes must be synchronized
with render-thread access. The layer outlives its device.

## Validation

Tests and examples cover the shared API, heap indexing, root layouts, uploads, rendering, presentation, concurrency,
and stage overlap. Hardware execution covers M3 Max and the integrating bad_sdf game on iPhone 15 Pro.
See [Metal validation](metal-validation.md) for exact coverage and the native render-timestamp and MetalTools limitations.
The supported device baseline is broader than the hardware tested so far.

## References

- [Metal 4 core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api)
- [Metal feature tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)
- [Texture view pools](https://developer.apple.com/documentation/metal/mtltextureviewpool)
- [Residency sets](https://developer.apple.com/documentation/metal/mtlresidencyset)
- [Vulkan contract](vulkan-support.md)
