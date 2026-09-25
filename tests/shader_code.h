#pragma once
#include <NoGraphicsAPI/NoGraphicsAPI.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline gpu::Span<byte> load_test_shader(const char* path) noexcept
{
    FILE* file = fopen(path, "rb");
    if (!file) { fprintf(stderr, "Cannot open test shader: %s\n", path); return {}; }
    fseek(file, 0, SEEK_END);
    const long size = ftell(file);
    rewind(file);
    if (size < 20 || size > 16 * 1024 * 1024)
    {
        fclose(file);
        return {};
    }
    byte* data = static_cast<byte*>(malloc(size_t(size)));
    bool valid = fread(data, 1, size_t(size), file) == size_t(size);
    fclose(file);
#ifdef __APPLE__
    valid = valid && memcmp(data, "MTLB", 4) == 0;
#else
    valid = valid && !(size & 3) && *reinterpret_cast<const uint32*>(data) == 0x07230203u;
#endif
    if (!valid)
    {
        fprintf(stderr, "Invalid test shader: %s\n", path);
        free(data);
        return {};
    }
    return {data, uint64(size)};
}
