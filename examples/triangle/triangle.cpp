#include "example_support.hpp"

#include <stdio.h>
#include <stdlib.h>

using namespace gpu;

int main(int argc, char** argv)
{
    uint64 frame_limit = 0;
    if (!example_frame_limit(argc, argv, frame_limit)) return 1;
    uint64 rendered_frames = 0;
    constexpr uint32 width = 512;
    constexpr uint32 height = 512;

    void* window = open_example_window("NoGraphicsAPI triangle", width, height);
    Device* device = create_device({.window = window, .swapchain_format = Format::bgra8_srgb}).device;

    if (!window || !device)
    {
        destroy_device(device);
        close_example_window(window);
        return 1;
    }

    printf("Using %s\n", get_device_caps(device).device_name);

    const Span<byte> vertex_code = read_shader(NOGRAPHICSAPI_VERTEX_SHADER_PATH);
    const Span<byte> fragment_code = read_shader(NOGRAPHICSAPI_FRAGMENT_SHADER_PATH);
    if (!vertex_code.data || !fragment_code.data)
    {
        free(fragment_code.data);
        free(vertex_code.data);
        destroy_device(device);
        close_example_window(window);
        return 1;
    }
    PSO* triangle_pso = create_graphics_pso(device, {
        .vertex = {.code = {vertex_code.data, vertex_code.size}, .entry_point = "vertexMain"},
        .fragment = {.code = {fragment_code.data, fragment_code.size}, .entry_point = "fragmentMain"},
        .color_targets = { { .format = Format::bgra8_srgb } }
    });
    free(fragment_code.data);
    free(vertex_code.data);
    if (!triangle_pso)
    {
        destroy_device(device);
        close_example_window(window);
        return 1;
    }

    TimelinePoint latest_completion{ .semaphore = create_timeline_semaphore(device) };
    CommandPool* command_pools[] = {create_command_pool(device), create_command_pool(device)};

    while ((!frame_limit || rendered_frames < frame_limit) && pump_example_window(window))
    {
        if (latest_completion.value >= 2)
            wait_timeline({.semaphore = latest_completion.semaphore, .value = latest_completion.value - 1});
        CommandPool* command_pool = command_pools[latest_completion.value % 2];
        reset_command_pool(command_pool);
        CommandBuffer* commands = begin_commands(command_pool);
        const SwapchainFrame frame = acquire(commands);
        if (!frame.render_view)
            continue;
        begin_render_pass(commands, {
            .colors = { { .render_view = frame.render_view, .load = LoadOp::clear } },
        });
        bind_pso(commands, triangle_pso);
        draw(commands, {}, 3);
        end_render_pass(commands);
        end_commands(commands);
        latest_completion.value++;
        submit_and_present(device, {.commands = {commands}, .completion = latest_completion});
        ++rendered_frames;
    }

    wait_idle(device);

    destroy_command_pool(command_pools[1]);
    destroy_command_pool(command_pools[0]);
    destroy_timeline_semaphore(latest_completion.semaphore);
    destroy_pso(triangle_pso);

    destroy_device(device);
    close_example_window(window);
    return 0;
}
