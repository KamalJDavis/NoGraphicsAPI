#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    const gpu::DeviceInit initialized = gpu::create_device();
    if (initialized.error != gpu::Error::none) return 1;
    gpu::Device* device = initialized.device;
    gpu::CommandPool* pool = gpu::create_command_pool(device);
    gpu::TimelineSemaphore* timeline = gpu::create_timeline_semaphore(device);
    const gpu::GpuHeap readback = gpu::create_gpu_heap(device, 16, gpu::MemoryType::readback);
    uint8* host = static_cast<uint8*>(aligned_alloc(16384, 16384));
    bool passed = true;
    for (uint32 iteration = 0; iteration < 70; ++iteration)
    {
        memset(host, 0, 16384);
        const gpu::GpuHeap imported = gpu::import_host_memory(device, host, 16384);
        if (!imported.owner) { passed = false; break; }
        passed = passed && imported.range.cpu == reinterpret_cast<byte*>(host) && imported.range.size == 16384;
        for (uint32 i = 0; i < 16; ++i) host[256 + i] = static_cast<uint8>(iteration + i);
        gpu::CommandBuffer* commands = gpu::begin_commands(pool);
        gpu::copy_memory(commands, {.gpu = imported.range.gpu + 256, .size = 16}, gpu::gpu_range(readback));
        gpu::end_commands(commands);
        gpu::submit(device, {.commands = {commands}, .completion = {.semaphore = timeline, .value = iteration * 2 + 1}});
        gpu::wait_timeline({.semaphore = timeline, .value = iteration * 2 + 1});
        passed = passed && memcmp(readback.range.cpu, host + 256, 16) == 0;
        gpu::reset_command_pool(pool);
        commands = gpu::begin_commands(pool);
        gpu::copy_memory(commands, gpu::gpu_range(readback), {.gpu = imported.range.gpu + 16368, .size = 16});
        gpu::end_commands(commands);
        gpu::submit(device, {.commands = {commands}, .completion = {.semaphore = timeline, .value = iteration * 2 + 2}});
        gpu::wait_timeline({.semaphore = timeline, .value = iteration * 2 + 2});
        passed = passed && memcmp(host + 16368, host + 256, 16) == 0;
        gpu::reset_command_pool(pool);
        gpu::destroy_gpu_heap(imported);
        host[0] = 0x5a;
    }
    gpu::wait_idle(device);
    free(host);
    gpu::destroy_gpu_heap(readback);
    gpu::destroy_command_pool(pool);
    gpu::destroy_timeline_semaphore(timeline);
    gpu::destroy_device(device);
    printf("%s: borrowed host memory aliases CPU storage and survives 70 import/copy/release cycles\n", passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
