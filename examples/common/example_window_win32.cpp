#include "example_support.hpp"
#include <assert.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

double example_time_seconds() noexcept
{
    static double seconds_per_tick = 0.0;
    if (seconds_per_tick == 0.0)
    {
        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);
        seconds_per_tick = 1.0 / double(frequency.QuadPart);
    }
    LARGE_INTEGER counter{};
    QueryPerformanceCounter(&counter);
    return double(counter.QuadPart) * seconds_per_tick;
}

namespace
{

constexpr const char* window_class_name = "NoGraphicsAPI_example_window";
constexpr DWORD window_style = WS_OVERLAPPEDWINDOW;

LRESULT CALLBACK example_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
    switch (message)
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            ShowWindow(hwnd, SW_HIDE);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hwnd, message, wparam, lparam);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wparam, lparam);
    }
}

} // namespace

void* open_example_window(const char* title, uint32 width, uint32 height) noexcept
{
    assert(title && width && height);
    const HINSTANCE instance = GetModuleHandleA(nullptr);
    WNDCLASSEXA window_class{
        .cbSize = sizeof(WNDCLASSEXA),
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = example_window_proc,
        .hInstance = instance,
        .hCursor = LoadCursorA(nullptr, IDC_ARROW),
        .lpszClassName = window_class_name,
    };
    if (!RegisterClassExA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return {};

    RECT rectangle{
        .right = static_cast<LONG>(width),
        .bottom = static_cast<LONG>(height),
    };
    if (!AdjustWindowRectEx(&rectangle, window_style, FALSE, 0))
        return {};

    const HWND hwnd = CreateWindowExA(
        0,
        window_class_name,
        title,
        window_style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!hwnd)
        return {};
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);
    return hwnd;
}

bool pump_example_window(void* window) noexcept
{
    for (;;)
    {
        MSG message{};
        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
                return false;
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        if (!IsIconic(static_cast<HWND>(window)))
            return true;
        WaitMessage();
    }
}

void close_example_window(void*& window) noexcept
{
    if (window)
        DestroyWindow(static_cast<HWND>(window));
    window = nullptr;
}
