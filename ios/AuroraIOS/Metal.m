//
// Metal.m
// gbemu-ハヤブサ
//
// Created by Anonym on 08.04.26.
//

#import "Metal.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <string.h>
#include <simd/simd.h>
#include <stdlib.h>

static inline float AURClamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static inline float AURFilmicCurve(float x) {
    return AURClamp01((x * (x + 0.18f)) / (x * (1.0f + 0.18f) + 0.18f));
}

typedef struct {
    vector_float2 sourceSize;
    float saturation;
    float contrast;
    float sharpen;
    float lutMix;
} AURPostProcessParams;

@interface AURMetalView ()
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLBuffer> frameBuffer;
@property (nonatomic, strong) id<MTLTexture> sourceTexture;
@property (nonatomic, strong) id<MTLTexture> colorLUT3D;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLSamplerState> samplerState;
@property (nonatomic, assign) NSUInteger frameWidth;
@property (nonatomic, assign) NSUInteger frameHeight;
@property (nonatomic, assign) NSUInteger frameBytesPerRow;
@end

@implementation AURMetalView

+ (Class)layerClass {
    return CAMetalLayer.class;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) { return nil; }

    _device = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];
    self.backgroundColor = UIColor.blackColor;
    self.opaque = YES;

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    layer.device = _device;
    // C API 側で BGRA8 (LE [B,G,R,A]) に正規化したバッファを受け取る
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    // blitCommandEncoder で drawable.texture へコピーするため NO が必要
    layer.framebufferOnly = NO;
    layer.opaque = YES;
    layer.magnificationFilter = kCAFilterNearest;
    layer.minificationFilter = kCAFilterNearest;

    NSString* source = @
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VSOut { float4 pos [[position]]; float2 uv; };\n"
    "struct Params { float2 source_size; float saturation; float contrast; float sharpen; float lut_mix; };\n"
    "vertex VSOut v_main(uint vid [[vertex_id]]) {\n"
    "  float2 p[3] = { float2(-1.0,-1.0), float2(3.0,-1.0), float2(-1.0,3.0) };\n"
    "  VSOut o; o.pos = float4(p[vid], 0.0, 1.0); o.uv = p[vid] * 0.5 + 0.5; return o;\n"
    "}\n"
    "fragment float4 f_main(VSOut in [[stage_in]], texture2d<float> tex [[texture(0)]], texture3d<float> lut3d [[texture(1)]], sampler s [[sampler(0)]], constant Params& p [[buffer(0)]]) {\n"
    "  const float2 texel = 1.0 / p.source_size;\n"
    "  float3 c = tex.sample(s, in.uv).rgb;\n"
    "  float3 n = tex.sample(s, in.uv + float2(0.0, -texel.y)).rgb;\n"
    "  float3 w = tex.sample(s, in.uv + float2(-texel.x, 0.0)).rgb;\n"
    "  float3 e = tex.sample(s, in.uv + float2(texel.x, 0.0)).rgb;\n"
    "  float3 so = tex.sample(s, in.uv + float2(0.0, texel.y)).rgb;\n"
    "  float3 sharp = c * (1.0 + p.sharpen * 2.0) - (n + w + e + so) * (p.sharpen * 0.5);\n"
    "  c = clamp(sharp, 0.0, 1.0);\n"
    "  float luma = dot(c, float3(0.299, 0.587, 0.114));\n"
    "  c = mix(float3(luma), c, p.saturation);\n"
    "  c = (c - 0.5) * p.contrast + 0.5;\n"
    "  c = clamp(c, 0.0, 1.0);\n"
    "  float3 lutColor = lut3d.sample(s, c).rgb;\n"
    "  c = mix(c, lutColor, clamp(p.lut_mix, 0.0, 1.0));\n"
    "  return float4(c, 1.0);\n"
    "}\n";
    NSError* error = nil;
    id<MTLLibrary> lib = [self.device newLibraryWithSource:source options:nil error:&error];
    if (lib) {
        MTLRenderPipelineDescriptor* pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = [lib newFunctionWithName:@"v_main"];
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
        pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
        self.pipelineState = [self.device newRenderPipelineStateWithDescriptor:pd error:&error];
    }

    MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
    sd.minFilter = MTLSamplerMinMagFilterLinear;
    sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    self.samplerState = [self.device newSamplerStateWithDescriptor:sd];

    // Optional 3D LUT (16^3): identity + very subtle toe/shoulder curve for natural vividness.
    const NSUInteger lutSize = 16;
    MTLTextureDescriptor* lutDesc =
        [MTLTextureDescriptor texture3DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                           width:lutSize
                                                          height:lutSize
                                                           depth:lutSize
                                                        mipmapped:NO];
    lutDesc.usage = MTLTextureUsageShaderRead;
    lutDesc.storageMode = MTLStorageModeShared;
    self.colorLUT3D = [self.device newTextureWithDescriptor:lutDesc];
    if (self.colorLUT3D) {
        const NSUInteger count = lutSize * lutSize * lutSize * 4;
        float* lutData = (float*)malloc(count * sizeof(float));
        if (!lutData) {
            return self;
        }
        for (NSUInteger i = 0; i < count; i++) {
            lutData[i] = 1.0f;
        }

        for (NSUInteger b = 0; b < lutSize; b++) {
            for (NSUInteger g = 0; g < lutSize; g++) {
                for (NSUInteger r = 0; r < lutSize; r++) {
                    float rf = (float)r / (float)(lutSize - 1);
                    float gf = (float)g / (float)(lutSize - 1);
                    float bf = (float)b / (float)(lutSize - 1);
                    rf = AURFilmicCurve(rf);
                    gf = AURFilmicCurve(gf);
                    bf = AURFilmicCurve(bf);
                    const NSUInteger idx = ((b * lutSize * lutSize) + (g * lutSize) + r) * 4;
                    lutData[idx + 0] = rf;
                    lutData[idx + 1] = gf;
                    lutData[idx + 2] = bf;
                    lutData[idx + 3] = 1.0f;
                }
            }
        }

        MTLRegion region = MTLRegionMake3D(0, 0, 0, lutSize, lutSize, lutSize);
        [self.colorLUT3D replaceRegion:region
                           mipmapLevel:0
                             slice:0
                         withBytes:lutData
                       bytesPerRow:lutSize * 4 * sizeof(float)
                     bytesPerImage:lutSize * lutSize * 4 * sizeof(float)];
        free(lutData);
    }
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    if (self.frameWidth > 0 && self.frameHeight > 0) {
        layer.drawableSize = CGSizeMake(self.frameWidth, self.frameHeight);
    }
}

- (void)ensureResourcesWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (self.frameBuffer && self.sourceTexture && self.frameWidth == width && self.frameHeight == height) {
        return;
    }

    self.frameWidth = width;
    self.frameHeight = height;

    const NSUInteger bytesPerPixel = sizeof(uint32_t);
    const NSUInteger tightBytesPerRow = width * bytesPerPixel;
    // Metal の buffer->texture blit は sourceBytesPerRow に厳しいアライメント制約がある。
    // 240 * 4 = 960 bytes は 256-byte 境界を満たさないため、パディング行を使う。
    const NSUInteger alignment = 256;
    const NSUInteger bytesPerRow = (tightBytesPerRow + alignment - 1) & ~(alignment - 1);
    const NSUInteger length = bytesPerRow * height;
    self.frameBuffer = [self.device newBufferWithLength:length options:MTLResourceStorageModeShared];
    self.frameBytesPerRow = bytesPerRow;
    MTLTextureDescriptor* td =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    td.storageMode = MTLStorageModePrivate;
    td.usage = MTLTextureUsageShaderRead;
    self.sourceTexture = [self.device newTextureWithDescriptor:td];

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    layer.drawableSize = CGSizeMake(width, height);
}

- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height {
    if (!pixels || width == 0 || height == 0 || !self.device || !self.commandQueue) { return; }
    [self ensureResourcesWithWidth:width height:height];
    if (!self.frameBuffer || !self.sourceTexture || !self.pipelineState || !self.samplerState || !self.colorLUT3D) { return; }

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) { return; }

    const NSUInteger bytesPerPixel = sizeof(uint32_t);
    const NSUInteger tightBytesPerRow = width * bytesPerPixel;
    const NSUInteger bytesPerRow = self.frameBytesPerRow;
    const NSUInteger bytesPerImage = bytesPerRow * height;
    uint8_t* dst = (uint8_t*)self.frameBuffer.contents;
    const uint8_t* src = (const uint8_t*)pixels;
    memset(dst, 0, bytesPerImage);
    for (NSUInteger y = 0; y < height; ++y) {
        memcpy(dst + y * bytesPerRow, src + y * tightBytesPerRow, tightBytesPerRow);
    }

    id<MTLCommandBuffer> cb = [self.commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit copyFromBuffer:self.frameBuffer
            sourceOffset:0
       sourceBytesPerRow:bytesPerRow
     sourceBytesPerImage:bytesPerImage
              sourceSize:MTLSizeMake(width, height, 1)
               toTexture:self.sourceTexture
        destinationSlice:0
        destinationLevel:0
       destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];

    MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = drawable.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    AURPostProcessParams params = {
        .sourceSize = { (float)width, (float)height },
        .saturation = 1.08f,
        .contrast = 1.05f,
        .sharpen = 0.22f,
        .lutMix = 0.35f,
    };

    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    [re setRenderPipelineState:self.pipelineState];
    [re setFragmentTexture:self.sourceTexture atIndex:0];
    [re setFragmentTexture:self.colorLUT3D atIndex:1];
    [re setFragmentSamplerState:self.samplerState atIndex:0];
    [re setFragmentBytes:&params length:sizeof(params) atIndex:0];
    [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [re endEncoding];

    [cb presentDrawable:drawable];
    [cb commit];
}

- (void)clearFrame {
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable || !self.commandQueue) { return; }
    id<MTLCommandBuffer> cb = [self.commandQueue commandBuffer];
    [cb presentDrawable:drawable];
    [cb commit];
}

@end
