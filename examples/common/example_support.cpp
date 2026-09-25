#include "example_support.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

using namespace gpu;

Span<byte> read_shader(const char* path) noexcept
{
    assert(path);
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open shader file: %s\n", path);
        return {};
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fprintf(stderr, "Failed to read shader file: %s\n", path);
        fclose(file);
        return {};
    }
    const long byte_count = ftell(file);
    if (byte_count < 4)
    {
        fprintf(stderr, "Invalid shader file size: %s\n", path);
        fclose(file);
        return {};
    }
    rewind(file);

    Span<byte> code(static_cast<byte*>(malloc(size_t(byte_count))), size_t(byte_count));
    bool valid = fread(code.data, 1, code.size, file) == code.size;
    fclose(file);
#if defined(__APPLE__)
    valid = valid && memcmp(code.data, "MTLB", 4) == 0;
#else
    valid = valid && code.size >= 5 * sizeof(uint32) && code.size % sizeof(uint32) == 0 &&
        *reinterpret_cast<const uint32*>(code.data) == 0x07230203u;
#endif
    if (!valid)
    {
        fprintf(stderr, "Invalid shader file: %s\n", path);
        free(code.data);
        return {};
    }
    return code;
}

bool read_binary_file(const char* path, Span<byte> data) noexcept
{
    assert(path && data.data && data.size);
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open resource file: %s\n", path);
        return false;
    }
    const bool size_succeeded = fseek(file, 0, SEEK_END) == 0 && ftell(file) == static_cast<long>(data.size);
    rewind(file);
    const bool read_succeeded = size_succeeded && fread(data.data, 1, data.size, file) == data.size;
    fclose(file);
    if (!read_succeeded)
        fprintf(stderr, "Invalid resource file: %s\n", path);
    return read_succeeded;
}

bool example_frame_limit(int argc, char** argv, uint64& frame_limit) noexcept
{
    frame_limit = 0;
    if (argc == 1) return true;
    if (argc == 3 && strcmp(argv[1], "--frames") == 0 && argv[2][0] >= '0' && argv[2][0] <= '9')
    {
        char* end = nullptr;
        errno = 0;
        frame_limit = strtoull(argv[2], &end, 10);
        if (frame_limit && end && !*end && errno != ERANGE) return true;
    }
    fprintf(stderr, "Usage: %s [--frames positive-count]\n", argv[0]);
    return false;
}
