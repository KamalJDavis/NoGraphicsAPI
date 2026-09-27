# Metal 4 render timestamp repro

On Apple M3 Max with macOS 26.6.2, native Metal 4 render-encoder timestamps leave some counter entries
unwritten after GPU completion. Ten writes interleaved with draws return five nonzero timestamps and
five zeros. The failure reproduces with and without Metal API validation.

The [source](../tests/repro_metal_render_timestamps.mm) does not link or call NoGraphicsAPI. It includes
only the library's integer typedefs and links to Foundation and Metal. Shaders are inline MSL compiled
by Apple's compiler; Slang is not involved. This manual target is excluded from normal builds and CTest.

## Workload

One Metal 4 command buffer uses one render pass, one color attachment, and one timestamp counter heap
with 32 entries. A loop writes ten timestamps at indices 0–9 using
`writeTimestampWithGranularity:afterStage:intoHeap:atIndex:` with
`MTL4TimestampGranularityPrecise` and `MTLRenderStageFragment`. The default mode draws a fullscreen
triangle after each write. Two command-buffer timestamps outside rendering use indices 10 and 11.
There are no suspended passes, multiple queues, resource reuse, descriptor heaps, or indirect commands.

After committing, the queue signals an `MTLSharedEvent`. The CPU waits for that signal, then calls
`resolveCounterRange:` for entries 0–11 and checks the returned size before reading tightly packed
`MTL4TimestampHeapEntry` values. All resources remain alive until completion. This follows Apple's
[documented CPU counter-resolution synchronization](https://developer.apple.com/documentation/metal/mtl4counterheap/resolvecounterrange%28_%3A%29).

Expected: all ten requested render timestamp entries contain valid nonzero values. Distinct timestamps
are not required; repeated nonzero values alone are not evidence of this failure.

## Run on macOS

Use a native Release test build configured as described in [building](building.md), with a Metal 4
device and Xcode's Metal compiler. From the NoGraphicsAPI repository root:

```sh
cmake --build build --target repro_metal_render_timestamps
export MTL_SHADER_VALIDATION=0
MTL_DEBUG_LAYER=0 build/tests/repro/repro_metal_render_timestamps
MTL_DEBUG_LAYER=0 build/tests/repro/repro_metal_render_timestamps --heavy
MTL_DEBUG_LAYER=0 build/tests/repro/repro_metal_render_timestamps --before
MTL_DEBUG_LAYER=0 build/tests/repro/repro_metal_render_timestamps --after
MTL_DEBUG_LAYER=1 build/tests/repro/repro_metal_render_timestamps --heavy
```

- Default: ten markers interleaved with ten draws to an 8 × 8 attachment.
- `--heavy`: 1920 × 1080 attachment with blending and a heavier fragment shader.
- `--before` / `--after`: all ten markers before / after a single draw.

The repro returns 0 when all ten render entries are nonzero, 1 for the observed missing entries, and
77 when no Metal 4 device is available. No GPU capture is needed.

## Observed on 2026-09-25

Apple M3 Max (Apple9), macOS 26.6.2 (25G83), Xcode 27.0 (27A266a), native Release build.
GPU shader instrumentation was disabled in all runs; API validation was enabled only where listed.

| Configuration | Observation |
| --- | --- |
| Default, API validation off | Entries 0–4 nonzero; entries 5–9 zero; exit 1 |
| `--heavy`, API validation off | Entries 0–4 nonzero; entries 5–9 zero; exit 1 |
| `--before`, API validation off | Entries 0–3 nonzero; entries 4–9 zero; exit 1 |
| `--after`, API validation off | Entries 0–3 nonzero; entries 4–9 zero; exit 1 |
| `--heavy`, API validation on | Entries 0–4 nonzero; entries 5–9 zero; exit 1; no validation error |

Captured output from the uninstrumented `--heavy` run:

```text
native counter[0]=7632444377212
native counter[1]=7632444563495
native counter[2]=7632444563495
native counter[3]=7632444563495
native counter[4]=7632444563495
native counter[5]=0
native counter[6]=0
native counter[7]=0
native counter[8]=0
native counter[9]=0
Apple M3 Max: 5/10 native timestamps, 1920 x 1080 blended
```

Outside-render command-buffer timestamps produce valid intervals in these runs. Separate native
diagnostics also found matching missing entries with CPU and GPU counter resolution; the checked-in
repro uses CPU resolution only.

## Interpretation and application impact

This points to an Apple Metal driver/runtime issue independent of NoGraphicsAPI, Slang, and suspended
rendering. The root cause is not vendor-confirmed; an undocumented restriction or other API usage issue
has not been conclusively excluded. Clean validation alone does not prove correctness. Other Apple GPU
families and iOS have not been tested for this failure.

It affects profiling inside render passes and causes NoGraphicsAPI's `test_render_continuation` to fail
its timestamp checks. Timestamp-free continuation tests pass. This reproducer does not check rendered
pixels and does not establish a rendering defect.

No reliable workaround for in-render timestamps is established. The backend preserves native marker
requests and results without inventing replacements. bad_sdf uses the working outside-render
command-buffer timestamp path; those pass-level timings do not encounter this missing-entry failure.
