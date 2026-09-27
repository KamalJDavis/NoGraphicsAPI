#import <Metal/Metal.h>
#include <NoGraphicsAPI/types.h>
#include <stdio.h>
#include <string.h>

// Native Metal reproducer: no NoGraphicsAPI implementation or external shaders.
// Default: ten timestamps with a draw after each. --before/--after groups all markers; --heavy uses blended 1080p work.
int main(int argc, char** argv)
{
    uint32 mode = 0;
    bool heavy = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--before") == 0) mode = 1;
        else if (strcmp(argv[i], "--after") == 0) mode = 2;
        else if (strcmp(argv[i], "--heavy") == 0) heavy = true;
        else { fprintf(stderr, "Usage: %s [--before|--after] [--heavy]\n", argv[0]); return 1; }
    }
    @autoreleasepool
    {
        NSError* error = nil;
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (!device || ![device supportsFamily:MTLGPUFamilyMetal4]) { [device release]; return 77; }
        id<MTL4CommandQueue> queue = [device newMTL4CommandQueue];
        id<MTL4CommandAllocator> allocator = [device newCommandAllocator];
        id<MTL4CommandBuffer> commands = [device newCommandBuffer];
        id<MTLSharedEvent> completed = [device newSharedEvent];
        MTL4CompilerDescriptor* compiler_desc = [MTL4CompilerDescriptor new];
        id<MTL4Compiler> compiler = [device newCompilerWithDescriptor:compiler_desc error:&error];
        if (!queue || !allocator || !commands || !completed || !compiler) { fprintf(stderr, "Metal initialization failed.\n"); return 1; }
        NSString* vertex_source = @"#include <metal_stdlib>\nusing namespace metal;\n"
            "vertex float4 vertexMain(uint i[[vertex_id]]){return float4(i==1?3.0:-1.0,i==2?3.0:-1.0,0,1);}\n";
        NSString* fragment_source = heavy ?
            @"fragment float4 fragmentMain(float4 p[[position]]){float3 v=fract(p.xyx*0.173+float3(0.3,0.5,0.7));"
            "for(uint j=0;j<256;++j)v=fract(v.yzx*v.zxy*float3(7.73,3.71,5.57)+0.39);return float4(v,0.5);}" :
            @"fragment float4 fragmentMain(){return float4(1,0,0,1);}";
        MTLCompileOptions* options = [MTLCompileOptions new];
        options.languageVersion = MTLLanguageVersion4_0;
        id<MTLLibrary> library = [device newLibraryWithSource:[vertex_source stringByAppendingString:fragment_source] options:options error:&error];
        if (!library) { fprintf(stderr, "%s\n", error.description.UTF8String); return 1; }
        MTL4LibraryFunctionDescriptor* vertex = [MTL4LibraryFunctionDescriptor new];
        vertex.library = library;
        vertex.name = @"vertexMain";
        MTL4LibraryFunctionDescriptor* fragment = [MTL4LibraryFunctionDescriptor new];
        fragment.library = library;
        fragment.name = @"fragmentMain";
        MTL4RenderPipelineDescriptor* pipeline_desc = [MTL4RenderPipelineDescriptor new];
        pipeline_desc.vertexFunctionDescriptor = vertex;
        pipeline_desc.fragmentFunctionDescriptor = fragment;
        pipeline_desc.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        pipeline_desc.colorAttachments[0].blendingState = heavy ? MTL4BlendStateEnabled : MTL4BlendStateDisabled;
        pipeline_desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        pipeline_desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        id<MTLRenderPipelineState> pipeline = [compiler newRenderPipelineStateWithDescriptor:pipeline_desc compilerTaskOptions:nil error:&error];
        if (!pipeline) { fprintf(stderr, "%s\n", error.description.UTF8String); return 1; }
        const uint32 width = heavy ? 1920 : 8;
        const uint32 height = heavy ? 1080 : 8;
        MTLTextureDescriptor* texture_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                            width:width height:height mipmapped:NO];
        texture_desc.storageMode = MTLStorageModePrivate;
        texture_desc.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> target = [device newTextureWithDescriptor:texture_desc];
        MTLResidencySetDescriptor* residency_desc = [MTLResidencySetDescriptor new];
        residency_desc.initialCapacity = 8;
        id<MTLResidencySet> residency = [device newResidencySetWithDescriptor:residency_desc error:&error];
        if (!target || !residency) { fprintf(stderr, "Metal render target initialization failed.\n"); return 1; }
        [residency addAllocation:target];
        [residency addAllocation:pipeline];
        [residency commit];
        [queue addResidencySet:residency];
        MTL4CounterHeapDescriptor* counter_desc = [MTL4CounterHeapDescriptor new];
        counter_desc.type = MTL4CounterHeapTypeTimestamp;
        counter_desc.count = 32;
        id<MTL4CounterHeap> counters = [device newCounterHeapWithDescriptor:counter_desc error:&error];
        if (!counters) { fprintf(stderr, "%s\n", error.description.UTF8String); return 1; }
        MTL4RenderPassDescriptor* pass = [MTL4RenderPassDescriptor new];
        pass.colorAttachments[0].texture = target;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.renderTargetWidth = width;
        pass.renderTargetHeight = height;
        [commands beginCommandBufferWithAllocator:allocator];
        [commands writeTimestampIntoHeap:counters atIndex:10];
        id<MTL4RenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
        [encoder setRenderPipelineState:pipeline];
        [encoder setViewport:MTLViewport{0, 0, double(width), double(height), 0, 1}];
        if (mode == 2) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        for (uint32 i = 0; i < 10; ++i)
        {
            [encoder writeTimestampWithGranularity:MTL4TimestampGranularityPrecise afterStage:MTLRenderStageFragment intoHeap:counters atIndex:i];
            if (mode == 0) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        if (mode == 1) [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        [commands writeTimestampIntoHeap:counters atIndex:11];
        [commands endCommandBuffer];
        id<MTL4CommandBuffer> batch[1] = {commands};
        [queue commit:batch count:1];
        [queue signalEvent:completed value:1];
        if (![completed waitUntilSignaledValue:1 timeoutMS:10000]) { fprintf(stderr, "GPU timeout\n"); return 1; }
        NSData* data = [counters resolveCounterRange:NSMakeRange(0, 12)];
        if (data.length != 12 * sizeof(MTL4TimestampHeapEntry)) { fprintf(stderr, "Counter resolve failed.\n"); return 1; }
        const MTL4TimestampHeapEntry* values = static_cast<const MTL4TimestampHeapEntry*>(data.bytes);
        uint32 valid = 0;
        for (uint32 i = 0; i < 10; ++i)
        {
            printf("native counter[%u]=%llu\n", i, values[i].timestamp);
            valid += values[i].timestamp != 0;
        }
        printf("%s: %u/10 native timestamps, %u x %u%s\n", device.name.UTF8String, valid, width, height, heavy ? " blended" : "");
        if (mode == 0 && values[0].timestamp && values[1].timestamp)
            printf("First interval: %.3f ms\n", double(values[1].timestamp - values[0].timestamp) * 1000.0 / double([device queryTimestampFrequency]));
        printf("Command-buffer interval: %.3f ms\n", double(values[11].timestamp - values[10].timestamp) * 1000.0 / double([device queryTimestampFrequency]));
        [queue removeResidencySet:residency];
        [counters release]; [pass release]; [counter_desc release]; [residency release]; [residency_desc release]; [target release];
        [pipeline release]; [pipeline_desc release]; [vertex release]; [fragment release]; [library release]; [options release];
        [compiler release]; [compiler_desc release]; [completed release]; [commands release]; [allocator release]; [queue release]; [device release];
        return valid == 10 ? 0 : 1;
    }
}
