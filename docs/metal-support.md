# Metal 4 implementation

Metal 4 implements the same GPU-pointer and descriptor-heap model as [Vulkan](vulkan-support.md).
The backend uses native Metal commands and precompiled metallibs. Applications own allocation,
descriptor indices, resource lifetime and synchronization.
See [supported devices](../README.md#metal-4) for the hardware baseline.

## GPU pointers and address-based commands

`GpuHeap` exposes real 64-bit addresses from Metal buffers. Applications partition this storage and
put typed GPU pointers in shared CPU/shader structures. Shaders dereference them directly, including
vertex fetch, without buffer descriptors or per-resource binding calls. CPU-visible heaps use shared
storage; GPU-only heaps use private storage. Textures occupy application-managed placement heaps.

Metal 4 also accepts GPU addresses in its command API:

| NoGraphicsAPI operation | Metal 4 implementation |
| --- | --- |
| Indexed drawing | `drawIndexedPrimitives` receives the index GPU address and byte length. |
| Indirect drawing | `drawPrimitives` / `drawIndexedPrimitives` receive the argument GPU address. |
| Indirect dispatch | `dispatchThreadgroupsWithIndirectBuffer` receives the argument GPU address. |
| Indirect mesh drawing | `drawMeshThreadgroupsWithIndirectBuffer` receives the argument GPU address. |

Despite the `Buffer` names, these Metal 4 arguments are addresses, not `MTLBuffer` objects. The backend
passes them through directly. Native copy commands still require buffer objects, so copies translate
addresses through an internal index of at most 64 live GPU heaps. Lookup uses atomic snapshots without
mutexes. Metal residency-set updates are serialized; ordinary command recording and submission are not.

## Application-owned descriptor heaps

Texture descriptor heaps use `MTLTextureViewPool`, which provides application-selected slots with contiguous
resource IDs. A shader selects a texture by forming a handle from **`baseResourceID + index`**.
There is no per-texture ID-table load. Sampler heaps use an indexed array of eight-byte resource IDs.

Metal does not expose writable texture-descriptor bytes. The common API therefore exposes opaque
texture and sampler heaps with indexed write/copy operations; Vulkan keeps its mapped descriptor
storage behind the same interface. Applications still own the slots and must retain their contents
and referenced textures until GPU use finishes.

Stock Slang has no texture-pool indexing operator. The shared shader header supplies the handle
reinterpretation, following the tested [MSL heap experiment](https://github.com/sebbbi/msl_heap).
`gpu_texture<T>(index)` and `gpu_sampler(index)` inline on both backends; no Slang fork is required.

## Root ABI and shared shaders

Each draw or dispatch copies up to 256 bytes of root arguments into a fixed command-pool arena and
points a Metal argument-table entry at them. Pool creation reserves a configurable 4 MiB by default;
copies use 16-byte-aligned ranges, reclaimed on pool reset. All graphics stages share that root.
`set_root_pointer` binds application-owned or GPU-written roots directly. Empty root bytes retain
the binding. See [root lifetime and synchronization](slang.md#root-and-pointer-layout).

| Metal buffer slot | Contents |
| --- | --- |
| 0 | Root arguments |
| 1 | Texture-view-pool base resource ID |
| 2 | Sampler resource IDs |

`GPU_ROOT` preserves the shared C layout instead of Metal's ordinary constant-buffer vector alignment.
The same Slang sources compile to metallib and SPIR-V. The backend mirrors viewport Y, so vertex and
mesh clip positions need no platform wrapper. See the [shader guide](slang.md) for layout and compilation.

## Mesh work and format support

Task shaders map to Metal's object stage. Direct task/mesh drawing works throughout the supported
baseline. Native indirect mesh drawing requires Apple9 (A17 Pro / M3) or newer and is reported through
`DeviceCaps::indirect_mesh_draw`. M1/M2 applications can launch direct task groups that read GPU-produced
counts; NoGraphicsAPI does not emulate an indirect command.

Metal 4 does not imply BC texture compression support. Query the required texture formats before
choosing assets; ASTC is available throughout the baseline.

## Synchronization

Resource-free barriers map to Metal producer barriers. Fragment, depth and color destinations wait
at fragment/tile stages, allowing independent vertex, object and mesh work to overlap previous passes.
Compute and copy work use their own scopes. Write dependencies request device visibility;
execution-only dependencies do not flush caches.

Command pools map to reusable Metal command allocators. Pools and queues are externally synchronized;
independent pools can record concurrently. `MTLSharedEvent` supplies cross-queue waits and CPU completion.
Resource destruction and pool reset require completion of their submitted uses.

Timestamps use native counter heaps. After the existing completion wait, `read_timestamps(pool)`
retrieves results into CPU memory before pool reset. It adds no GPU resolve, barrier or wait, preserving
the opportunity for overlap between frames. Outside-render markers are approximate all-commands timings;
inside-render markers have a [known driver limitation](repro-metal-render-timestamps.md).

## References

- [Metal 4 core API](https://developer.apple.com/documentation/metal/understanding-the-metal-4-core-api)
- [Texture view pools](https://developer.apple.com/documentation/metal/mtltextureviewpool)
- [Metal feature tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)
- [Validation and limitations](metal-validation.md)
