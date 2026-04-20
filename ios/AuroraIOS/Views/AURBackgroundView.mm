#import "AURBackgroundView.h"

@interface AURBackgroundView () <MTKViewDelegate>
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, assign) float time;
@end

@implementation AURBackgroundView

- (instancetype)initWithFrame:(CGRect)frame {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:frame device:device];
    if (self) {
        self.delegate = self;
        self.clearColor = MTLClearColorMake(0, 0, 0, 1);
        self.commandQueue = [device newCommandQueue];
        [self _setupPipeline];
    }
    return self;
}

- (void)_setupPipeline {
    NSString *shaderSource = @
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VertexOut { float4 pos [[position]]; float2 uv; };\n"
    "\n"
    "vertex VertexOut vertex_main(uint vid [[vertex_id]]) {\n"
    "    const float2 p[3] = {float2(-1, -1), float2(3, -1), float2(-1, 3)};\n"
    "    VertexOut o;\n"
    "    o.pos = float4(p[vid], 0, 1);\n"
    "    o.uv = p[vid] * 0.5 + 0.5;\n"
    "    return o;\n"
    "}\n"
    "\n"
    "float noise(float2 p) {\n"
    "    return fract(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "\n"
    "fragment float4 fragment_main(VertexOut in [[stage_in]], constant float& time [[buffer(0)]]) {\n"
    "    float2 uv = in.uv;\n"
    "    float3 color1 = float3(0.05, 0.0, 0.1); // Deep Purple\n"
    "    float3 color2 = float3(0.0, 0.1, 0.08); // Aurora Green\n"
    "    float3 color3 = float3(0.1, 0.0, 0.2); // Vibrant Violet\n"
    "\n"
    "    float n = noise(uv + time * 0.1);\n"
    "    float wave = sin(uv.x * 3.0 + time) * cos(uv.y * 2.0 + time * 0.5);\n"
    "    float3 finalColor = mix(color1, color2, wave * 0.5 + 0.5);\n"
    "    finalColor = mix(finalColor, color3, sin(time * 0.3) * 0.5 + 0.5);\n"
    "\n"
    "    return float4(finalColor, 1.0);\n"
    "}\n";

    NSError *error = nil;
    id<MTLLibrary> library = [self.device newLibraryWithSource:shaderSource options:nil error:&error];
    if (!library) {
        NSLog(@"Failed to create library: %@", error);
        return;
    }

    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vertex_main"];
    pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"fragment_main"];
    pipelineDescriptor.colorAttachments[0].pixelFormat = self.colorPixelFormat;

    self.pipelineState = [self.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    self.time += 1.0 / 60.0;

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;

    if (renderPassDescriptor) {
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setRenderPipelineState:self.pipelineState];
        [renderEncoder setFragmentBytes:&_time length:sizeof(_time) atIndex:0];
        [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:view.currentDrawable];
    }

    [commandBuffer commit];
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {}

@end
