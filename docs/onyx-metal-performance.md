# Metal performance extensions

The `codex/onyx-metal-performance` branch contains two changes:

- Repeated viewport, scissor, depth/stencil and descriptor-heap setters avoid
  redundant Metal calls. Encoder state resets at every render-pass boundary;
  descriptor addresses reset when the command buffer begins recording.
- `import_host_memory` wraps caller-owned, 16 KiB-aligned storage with a shared
  Metal buffer. It is a Metal-only API. The caller synchronizes writes and keeps
  storage alive through GPU completion and wrapper destruction.

The import test changes CPU bytes after import, reads and writes offset ranges
through the GPU, and repeats 70 times to check address-registration recycling.
It does not establish application-level geometry synchronization or performance.

Release validation on 2026-09-30: 32 of 33 tests passed with Metal validation.
`test_render_continuation` failed its iteration-zero pixel/timestamp check on
both this branch and an untouched f9e09e25 control using the same compiler and
shader tools. The cause remains open. The import test also passed separately
with shader validation enabled.

Onyx has not switched its pinned dependency to this checkout. Its remaining
adaptations, including shader source/library caching, formats, visibility and
sampling policy, still live in its dependency-preparation script. Port those
incrementally before switching; this branch alone is not yet a drop-in Onyx
replacement. No geometry import is enabled in the game yet.
