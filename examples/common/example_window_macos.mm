#include "example_support.hpp"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>
#include <assert.h>
#include <time.h>

namespace
{
NSWindow* example_window = nil;
}

double example_time_seconds() noexcept
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return double(now.tv_sec) + double(now.tv_nsec) * 1.0e-9;
}

void* open_example_window(const char* title, uint32 width, uint32 height) noexcept
{
    assert(!example_window);
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp finishLaunching];
        example_window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, width, height)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
            backing:NSBackingStoreBuffered defer:NO];
        example_window.releasedWhenClosed = NO;
        example_window.title = [NSString stringWithUTF8String:title];
        example_window.contentView.wantsLayer = YES;
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.contentsScale = example_window.backingScaleFactor;
        layer.drawableSize = CGSizeMake(width * layer.contentsScale, height * layer.contentsScale);
        example_window.contentView.layer = layer;
        [example_window center];
        [example_window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        return layer;
    }
}

bool pump_example_window(void* window) noexcept
{
    @autoreleasepool
    {
        for (;;)
        {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:[NSDate distantPast]
                inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!event) break;
            if (event.type == NSEventTypeKeyDown && event.keyCode == 53) return false;
            [NSApp sendEvent:event];
        }
        if (!example_window.visible) return false;
        CAMetalLayer* layer = static_cast<CAMetalLayer*>(window);
        layer.contentsScale = example_window.backingScaleFactor;
        const NSSize size = example_window.contentView.bounds.size;
        layer.drawableSize = example_window.miniaturized ? CGSizeZero : CGSizeMake(size.width * layer.contentsScale, size.height * layer.contentsScale);
        return true;
    }
}

void close_example_window(void*& window) noexcept
{
    @autoreleasepool
    {
        [example_window close];
        [example_window release];
        example_window = nil;
        window = nullptr;
    }
}
