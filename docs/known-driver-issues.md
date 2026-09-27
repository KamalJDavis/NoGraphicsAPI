# Known driver issues

These are locally reproduced observations, not vendor-confirmed root causes.

## NVIDIA 596.99: stale address-based texture readback

Observed on an RTX 4090 with NVIDIA 596.99, with both sequential and parallel command recording.
An upload through `vkCmdCopyMemoryToImageKHR` followed by `vkCmdCopyImageToMemoryKHR` returned stale
data despite a transfer-write to transfer-read barrier. The issue also reproduced without validation.

A timeline wait between separate upload and readback submissions works. A full Vulkan memory
dependency also worked in isolation; native `vkCmdCopyImageToBuffer2` readback worked in the comparison.
The parallel texture test uses an explicit timeline wait. No driver-specific barrier widening is applied
by the graphics API.

## NVIDIA 596.99: core Vulkan 1.3 concurrent copies lose the device

On Windows with an RTX 4090, buffer copies submitted to separate general, compute, and copy queues from
three CPU threads intermittently return `VK_ERROR_DEVICE_LOST`.

The failure reproduces independently of NoGraphicsAPI using standard Vulkan 1.3, with no instance or
device extensions enabled (`--buffers --no-validation`). The standalone repro does not link or call
NoGraphicsAPI; it uses ordinary `vkCmdCopyBuffer2`, synchronization2 barriers, and timeline semaphores.
There are no shaders, PushData calls, descriptor heaps, or timestamp queries.

It also fails with core and synchronization validation enabled, without a preceding validation error.
That mode adds only the debug/validation instance extensions, not device extensions.

This points to an NVIDIA driver issue independent of the library implementation, not a confirmed
NoGraphicsAPI defect. The root cause is not vendor-confirmed, and no workaround is established.
See the [standalone repro and test results](repro-queue-device-lost.md).

## Metal 4 / macOS 26.6.2: render timestamps leave counter entries unwritten

Observed on Apple M3 Max with macOS 26.6.2 and Xcode 27. Ten native render-encoder timestamp writes
interleaved with draws return five nonzero entries and five zeros after GPU completion. The failure
reproduces with and without Metal API validation, without a preceding validation error.

The standalone repro does not link or call NoGraphicsAPI and uses inline MSL, without Slang. It uses
one ordinary render pass, so suspended/resumed rendering is not required. Counter retrieval follows
Apple's documented shared-event completion sequence.

This points to an Apple Metal driver/runtime issue; the root cause is not vendor-confirmed. There is
no established workaround for timestamps inside rendering. Outside-render command-buffer timestamps
work in the tested configuration and are used by bad_sdf. The library does not substitute timings or
hide the failing `test_render_continuation`. See the [standalone repro and test results](repro-metal-render-timestamps.md).

## MetalTools validation limitations

MetalTools has separate limitations with concurrent sampler lifetime changes, placed-resource
residency, and indirect mesh instrumentation. See [Metal validation](metal-validation.md#metaltools-limitations)
for reproductions and tool-specific handling.
