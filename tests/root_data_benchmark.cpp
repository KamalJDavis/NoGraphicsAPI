#include "root_data_shared.h"
#include <NoGraphicsAPIUtility/bump_allocator.hpp>
#include "shader_code.h"
#include <time.h>

static uint64 nanoseconds()
{
    timespec value{};
    timespec_get(&value, TIME_UTC);
    return uint64(value.tv_sec) * 1000000000 + uint64(value.tv_nsec);
}

struct Sample
{
    double cpu_ms;
    double gpu_ms;
};

static int compare_double(const void* a, const void* b)
{
    const double x = *static_cast<const double*>(a), y = *static_cast<const double*>(b);
    return (x > y) - (x < y);
}

int main(int argc, char** argv)
{
    if (argc != 2) { fprintf(stderr, "Usage: benchmark_root_data <shader-directory>\n"); return 1; }
    gpu::DeviceInit init = gpu::create_device({.timestamp_query_count = 2});
    if (init.error != gpu::Error::none) return 1;
    gpu::Device* device = init.device;
    const gpu::DeviceCaps& caps = gpu::get_device_caps(device);
    if (!caps.timestamp_period_ns) return 1;
    printf("GPU: %s\n", caps.device_name);
    constexpr uint32 modes = 9;
    const char* names[modes] = {"uniform_user_bump_direct", "structured_user_bump_direct", "physical_user_bump_direct",
        "uniform_gpu_pointer", "structured_gpu_pointer", "physical_gpu_pointer",
        "uniform_user_atomic_direct", "structured_user_atomic_direct", "physical_user_atomic_direct"};
    const char* shaders[modes] = {"uniform", "structured", "physical", "uniform", "structured", "physical", "uniform", "structured", "physical"};
    constexpr uint32 samples = 61, warmup = 16;
    const gpu::GpuHeap roots = gpu::create_gpu_heap(device, 1024 * sizeof(RootData));
    const gpu::GpuHeap device_roots = gpu::create_gpu_heap(device, roots.range.size, gpu::MemoryType::gpu_only);
    const gpu::GpuHeap output = gpu::create_gpu_heap(device, 16 * 8192 * 64 * sizeof(uint32), gpu::MemoryType::gpu_only);
    const gpu::GpuHeap readback = gpu::create_gpu_heap(device, output.range.size, gpu::MemoryType::readback);
    gpu::BumpAllocator arena(roots.range);
    gpu::CommandPool* pool = gpu::create_command_pool(device);
    gpu::TimelineSemaphore* timeline = gpu::create_timeline_semaphore(device);
    uint64 submission = 0;
    bool valid = true;
    printf("workload,root_bytes,mode,cpu_median_ms,cpu_p10_ms,cpu_p90_ms,gpu_median_ms,gpu_p10_ms,gpu_p90_ms\n");
    for (uint32 workload = 0; workload < 2; ++workload)
    {
        const uint32 draws = workload ? 16 : 1024;
        const uint32 groups = workload ? 8192 : 1;
        const uint32 lanes = groups * 64;
        for (uint32 i = 0; i < draws; ++i)
        {
            RootData* root = reinterpret_cast<RootData*>(roots.range.cpu) + i;
            root->output = reinterpret_cast<uint32*>(output.range.gpu);
            root->output_index = i * lanes;
            root->seed = i;
            for (uint32 j = 0; j < 60; ++j) root->values[j] = i * 7 + j;
        }
        // Only the pre-existing GPU-only-root reference uses this untimed upload.
        gpu::CommandBuffer* upload = gpu::begin_commands(pool);
        gpu::copy_memory(upload, gpu::gpu_range(roots), gpu::gpu_range(device_roots));
        gpu::barrier(upload, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::compute, gpu::Access::shader_read);
        gpu::end_commands(upload);
        gpu::submit(device, {.commands = {upload}, .completion = {.semaphore = timeline, .value = ++submission}});
        gpu::wait_timeline({.semaphore = timeline, .value = submission});
        gpu::reset_command_pool(pool);
        for (uint32 size_index = 0; size_index < 2; ++size_index)
        {
            const uint32 root_size = size_index ? 256 : 64;
            gpu::PSO* psos[modes]{};
            for (uint32 mode = 0; mode < modes; ++mode)
            {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s-%u.spv", argv[1], shaders[mode], root_size);
                gpu::Span<byte> code = load_test_shader(path);
                if (!code.data) return 1;
                psos[mode] = gpu::create_compute_pso(device, {.code = {code.data, code.size},
                    .entry_point = size_index ? "computeMain" : "smallMain", .threadgroup_size = {.x = 64, .y = 1, .z = 1}});
                free(code.data);
                if (!psos[mode]) return 1;
            }
            Sample measurements[modes][samples]{};
            for (uint32 round = 0; round < warmup + samples; ++round)
                for (uint32 ordinal = 0; ordinal < modes; ++ordinal)
                {
                    const uint32 mode = (round + ordinal) % modes;
                    gpu::CommandBuffer* commands = gpu::begin_commands(pool);
                    gpu::bind_pso(commands, psos[mode]);
                    uint64 begin = 0, end = 0;
                    gpu::write_timestamp(commands, &begin);
                    arena.reset();
                    const uint64 cpu_begin = nanoseconds();
                    for (uint32 i = 0; i < draws; ++i)
                    {
                        if (mode >= 3 && mode < 6)
                            gpu::dispatch(commands, device_roots.range.gpu + i * sizeof(RootData), {.x = groups, .y = 1, .z = 1});
                        else
                        {
                            const gpu::GpuCpuRange<byte> root = mode < 3 ? arena.allocate(root_size) : arena.allocate_atomic(root_size);
                            RootData* data = reinterpret_cast<RootData*>(root.cpu);
                            data->output = reinterpret_cast<uint32*>(output.range.gpu);
                            data->output_index = i * lanes;
                            data->seed = round * 1024 + i;
                            for (uint32 j = 0; j < (size_index ? 60u : 12u); ++j) data->values[j] = (round * 1024 + i) * 7 + j;
                            gpu::dispatch(commands, root.gpu, {.x = groups, .y = 1, .z = 1});
                        }
                    }
                    const uint64 cpu_end = nanoseconds();
                    gpu::write_timestamp(commands, &end);
                    const bool verify = round == 0 || round == warmup + samples - 1;
                    if (verify)
                    {
                        gpu::barrier(commands, gpu::Stage::compute, gpu::Access::shader_write, gpu::Stage::transfer, gpu::Access::transfer_read);
                        gpu::copy_memory(commands, {.gpu = output.range.gpu, .size = uint64(draws) * lanes * sizeof(uint32)},
                            {.gpu = readback.range.gpu, .size = uint64(draws) * lanes * sizeof(uint32)});
                        gpu::barrier(commands, gpu::Stage::transfer, gpu::Access::transfer_write, gpu::Stage::host, gpu::Access::host_read);
                    }
                    gpu::barrier(commands, gpu::Stage::compute, gpu::Access::shader_write, gpu::Stage::compute, gpu::Access::shader_write);
                    gpu::end_commands(commands);
                    gpu::submit(device, {.commands = {commands}, .completion = {.semaphore = timeline, .value = ++submission}});
                    gpu::wait_timeline({.semaphore = timeline, .value = submission});
                    gpu::read_timestamps(pool);
                    if (round >= warmup) measurements[mode][round - warmup] = {
                        .cpu_ms = double(cpu_end - cpu_begin) * 1e-6, .gpu_ms = double(end - begin) * caps.timestamp_period_ns * 1e-6};
                    if (verify)
                    {
                        const uint32* actual = reinterpret_cast<const uint32*>(readback.range.cpu);
                        for (uint32 i = 0; i < draws; ++i)
                            for (uint32 sample = 0; sample < 64; ++sample)
                            {
                                const uint32 lane = sample * (lanes - 1) / 63;
                                const uint32 seed = mode >= 3 && mode < 6 ? i : round * 1024 + i;
                                uint32 expected = seed + lane;
                                for (uint32 j = 0; j < (size_index ? 60u : 12u); ++j) expected = (expected * 33u) ^ (seed * 7 + j);
                                if (actual[i * lanes + lane] != expected) valid = false;
                            }
                    }
                    gpu::reset_command_pool(pool);
                }
            for (uint32 mode = 0; mode < modes; ++mode)
            {
                double cpu_samples[samples], gpu_samples[samples];
                for (uint32 i = 0; i < samples; ++i)
                {
                    cpu_samples[i] = measurements[mode][i].cpu_ms;
                    gpu_samples[i] = measurements[mode][i].gpu_ms;
                }
                qsort(cpu_samples, samples, sizeof(double), compare_double);
                qsort(gpu_samples, samples, sizeof(double), compare_double);
                printf("%s,%u,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n", workload ? "16x8192groups" : "1024x1group", root_size, names[mode],
                    cpu_samples[30], cpu_samples[6], cpu_samples[54], gpu_samples[30], gpu_samples[6], gpu_samples[54]);
                gpu::destroy_pso(psos[mode]);
            }
        }
    }
    gpu::wait_idle(device);
    gpu::destroy_command_pool(pool);
    gpu::destroy_timeline_semaphore(timeline);
    gpu::destroy_gpu_heap(readback);
    gpu::destroy_gpu_heap(output);
    gpu::destroy_gpu_heap(device_roots);
    gpu::destroy_gpu_heap(roots);
    gpu::destroy_device(device);
    printf("Readback: %s\n", valid ? "PASS" : "FAIL");
    return valid ? 0 : 1;
}
