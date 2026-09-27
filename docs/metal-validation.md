# Metal 4 validation

Native Debug and Release correctness tests run on Apple M3 Max (Apple9), macOS 26.6.2, with Xcode 27
and stock Slang 2026.18.2. The integrating bad_sdf application also executes on iPhone 15 Pro with
iOS 26.6.2. M1/M2 hardware execution, sustained mobile performance and thermal behavior remain
unverified. Vulkan shaders compile and validate on this host; Vulkan GPU testing requires a supported
device and driver. The supplied test runners and window adapters target desktop systems, not iOS.

## Running the checks

With Xcode's Metal compiler, CMake and Slang installed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_SYSROOT=macosx -DNOGRAPHICSAPI_BUILD_EXAMPLES=ON \
  -DNOGRAPHICSAPI_BUILD_TESTS=ON -DNOGRAPHICSAPI_SLANGC=/path/to/slangc
cmake --build build
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 ctest --test-dir build --output-on-failure
build/examples/triangle/example_triangle
```

The 25 September 2026 Release review passes 29 of 30 CTests with Metal API validation. The full suite retains
`test_render_continuation`, which exposes the [native render-timestamp failure](repro-metal-render-timestamps.md).
To run the remaining checks separately, add `-E '^test_render_continuation$'` to CTest.
Indirect mesh cases explicitly disable shader instrumentation; matching direct-only cases enable it.
The autorelease diagnostic runs without validation to distinguish library ownership from tool threads.

Tests cover shared root layout and snapshots, GPU-pointer access, divergent texture/sampler indices,
descriptor copies, direct/indexed/indirect commands, placed textures, pitched 3D/array/cube/BC copies,
viewport offsets, scissor, negative heights, culling, drawable resize and presentation. Native fixtures
compile offline to metallib. Plain C++ callers need no outer Objective-C autorelease pool.

## Concurrency and lifetime

The command-context and queue tests cover independent recording, cross-queue GPU waits, submission
order, pool reuse, disabled timestamps and allocations spanning multiple counter pages. CPU timestamp
retrieval covers separate destinations, unsubmitted recordings, repeated reads and discarded unread results. Timeline
retirement remains valid when completed public timelines are destroyed before their command pools.
Timestamp-free suspended/resumed rendering is checked across independent buffers and multiple native
segments in one public buffer.

`test_metal_resource_concurrency` exercises three paths:

- Two registry writers create, copy through and destroy different heaps while two queues read stable
  ranges. It reaches the 64-heap limit, wraps the address-index ring repeatedly and checks payloads
  and guards in newly published heaps.
- Four timestamp workers fill every reserved slot across multiple pages, share partial bitmap words,
  and validate fresh values, ordering and guards through repeated allocation/retirement cycles.
- Four descriptor workers write and copy disjoint ranges of one heap, then verify sampled/storage
  results on the GPU before reusing their slots.

These tests pass with API and GPU shader validation. Address indexing and timestamp allocation use
atomics; disjoint descriptor operations take no internal mutex. Native residency mutations and
shader-validation queue enumeration share the residency mutex. See [the backend contract](metal-support.md).

## Stage dependencies and overlap

`test_metal_barriers` and its timestamp-enabled variant check compute-to-fragment visibility,
geometry execution ordering, GPU-generated index/indirect arguments, attachment copies and host
readback. Visible and fully clipped geometry ensure earlier-stage side effects are ordered even
when no fragments execute.

A bounded render-to-render probe within one command buffer uses `color_output` to `fragment` barriers and observes later vertex
and mesh work before the producer's fragment work completes in eight of eight runs each. All-stage
controls wait for completion, and attachment readbacks match in all 32 cases. The same checks pass
with an outside-render timestamp between passes, including the resolved timestamp. A separate compute-to-render
probe also observes geometry overlap when only fragments depend on the compute producer.
These checks establish overlap on M3 Max, not a portable throughput or iPhone speedup claim.
The mapping follows Apple's
[producer-barrier scopes](https://developer.apple.com/documentation/metal/synchronizing-passes-with-producer-barriers)
and [compute/render overlap example](https://developer.apple.com/documentation/metal/combining-blit-and-compute-operations-in-a-single-pass).

Across separate submissions, a timestamp-enabled probe observes vertex and mesh overlap in eight of
eight runs each, including the backend's internal and public completion signals. Timestamp destinations
are ordinary CPU memory and are filled by `read_timestamps(pool)` after the final completion wait.
No timestamp GPU resolve commands or barriers are recorded. All broad-barrier controls, timestamp,
state/hash and attachment checks pass with Metal API validation.

A presentation probe with three drawables observes cross-frame overlap through `acquire` and
`submit_and_present`: three of four vertex cases and two of four mesh cases with timestamps enabled.
A matching untimed control observes three of four vertex cases and four of four mesh cases.
All broad-barrier controls and output checks pass. The CPU waits only after submitting both frames.
Drawable availability can still stall the whole batch at the presentation queue wait; overlap is an
observed scheduling opportunity, not a per-frame guarantee. Timestamp sampling and CPU retrieval
have overhead, but retrieval introduces no additional GPU wait.

## Render timestamps

Native render timestamp writes can leave destinations unwritten on the tested driver, including in
ordinary passes without suspension. This causes `test_render_continuation` to fail; the backend
preserves requested markers without substituting timings. See the [standalone repro and test results](repro-metal-render-timestamps.md)
for the workload, synchronization, commands, affected configuration, and diagnosis limits.

Outside-render markers use native command-buffer timestamps. Their ticks follow `mach_absolute_time`;
`timestamp_period_ns` uses `mach_timebase_info` for conversion. They order preceding work but can
include following work that Metal starts before sampling; treat these intervals as approximate.

## MetalTools limitations

Metal API validation can crash during concurrent sampler creation/destruction in
`MTLSamplerDescriptorHashMap::add` or `remove`. A native Metal-only sampler stress reproduces this
without command buffers or NoGraphicsAPI; uninstrumented execution passes. This can intermittently
fail `test_multithreading` before timestamp submission or retrieval.

On the tested OS, GPU shader validation cannot reliably enumerate placed resources through heap
residency. The backend therefore registers individual placed resources when
`MTL_SHADER_VALIDATION=1`; normal execution uses heap membership. Shader-validation queue commits
also take the residency mutex because MetalTools enumerates concurrently mutated residency state.
Placement, commands and shader bindings are otherwise the same.

GPU instrumentation of native indirect mesh commands crashes in
`MetalTools::resolvedSharedPacketData` on `com.Metal4.CompletionQueue` or leaves readbacks unwritten.
Direct commands pass with instrumentation, and indirect commands pass with API validation.
CTest keeps both paths covered using separate invocations. To reproduce the tool failure explicitly:

```sh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build/tests/test_metal_mesh
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1 build/tests/test_task_shader
```

The corresponding `--direct-only` invocations pass. The asynchronous tool crash can appear after
readback. These diagnostic accommodations do not replace production indirect commands or alter shaders.
