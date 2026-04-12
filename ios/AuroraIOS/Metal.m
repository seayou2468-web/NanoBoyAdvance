#import "./Metal.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <string.h>
#include <stdlib.h>
#import <simd/simd.h>

typedef struct {
    vector_float2 sourceSize;
    vector_float2 outputSize;
    vector_float4 sourceRect;
    float         saturation;
    float         vibrance;
    float         contrast;
    float         sharpen;
    float         lutMix;
    float         _pad;
} AURPostProcessParams;

@interface AURMetalView ()
@property (nonatomic, strong) id<MTLDevice>              device;
@property (nonatomic, strong) id<MTLCommandQueue>        commandQueue;
@property (nonatomic, strong) id<MTLBuffer>              frameBuffer;
@property (nonatomic, strong) id<MTLTexture>             sourceTexture;
@property (nonatomic, strong) id<MTLTexture>             colorLUT3D;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLRenderPipelineState> fastPipelineState;
@property (nonatomic, strong) id<MTLSamplerState>        samplerState;
@property (nonatomic, assign) AURUpscaleMode             upscaleMode;
@property (nonatomic, assign) float                      userSaturation;
@property (nonatomic, assign) float                      userVibrance;
@property (nonatomic, assign) float                      userContrast;
@property (nonatomic, assign) float                      userSharpen;
@property (nonatomic, assign) float                      userLutMix;
@property (nonatomic, assign) NSUInteger                 frameWidth;
@property (nonatomic, assign) NSUInteger                 frameHeight;
@property (nonatomic, assign) NSUInteger                 frameBytesPerRow;
@property (nonatomic, assign) CGRect                     sourceRect;
@property (nonatomic, assign) dispatch_semaphore_t       inFlightSemaphore;
@end

@implementation AURMetalView

+ (Class)layerClass { return CAMetalLayer.class; }

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    _device = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];
    _inFlightSemaphore = dispatch_semaphore_create(3);
    _upscaleMode = AURUpscaleModeAuto;
    _userSaturation = 1.08f; _userVibrance = 0.30f; _userContrast = 1.06f; _userSharpen = 0.18f; _userLutMix = 0.15f;
    _sourceRect = CGRectMake(0, 0, 1, 1);
    self.backgroundColor = [UIColor blackColor];
    self.opaque = YES;
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    layer.device = _device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.opaque = YES;
    layer.magnificationFilter = kCAFilterLinear;
    layer.minificationFilter = kCAFilterLinear;

    NSString *src = @"#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct VSOut { float4 pos [[position]]; float2 uv; };\n"
    "struct Params { float2 src_sz; float2 out_sz; float4 src_rc; float sat; float vib; float ctr; float shp; float lut; float _pad; };\n"
    "half4 cubicW(half t) { half t2 = t*t, t3 = t*t2; return half4(-0.5h*t3+t2-0.5h*t, 1.5h*t3-2.5h*t2+1.0h, -1.5h*t3+2.0h*t2+0.5h, 0.5h*t3-0.5h*t2); }\n"
    "vertex VSOut v_main(uint vid [[vertex_id]], constant Params& p [[buffer(0)]]) {\n"
    "  const float2 pos[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)};\n"
    "  float2 uv = pos[vid]*0.5+0.5;\n"
    "  VSOut o; o.pos=float4(pos[vid],0,1); o.uv=float2(uv.x*p.src_rc.z+p.src_rc.x, (1.0-uv.y)*p.src_rc.w+p.src_rc.y); return o;\n"
    "}\n"
    "fragment float4 f_main(VSOut in [[stage_in]], texture2d<half> tex [[texture(0)]], texture3d<half> lut3d [[texture(1)]], sampler s [[sampler(0)]], constant Params& p [[buffer(0)]]) {\n"
    "  float2 px = in.uv*p.src_sz-0.5; float2 f = fract(px); float2 inv = 1.0/p.src_sz; float2 b = (floor(px)+0.5)*inv;\n"
    "  half4 wx = cubicW((half)f.x), wy = cubicW((half)f.y);\n"
    "  half3 acc = 0.0h; for(int j=0; j<4; j++) { float v = b.y+float(j-1)*inv.y;\n"
    "    acc += wy[j]*(wx[0]*tex.sample(s,float2(b.x-inv.x,v)).rgb + wx[1]*tex.sample(s,float2(b.x,v)).rgb + wx[2]*tex.sample(s,float2(b.x+inv.x,v)).rgb + wx[3]*tex.sample(s,float2(b.x+inv.x*2,v)).rgb); }\n"
    "  half3 c = clamp(acc,0.0h,1.0h); half luma = dot(c,half3(0.2126h,0.7152h,0.0722h));\n"
    "  half chr = max(max(c.r,c.g),c.b)-min(min(c.r,c.g),c.b); half vib = (half)p.vib*(1.0h-chr);\n"
    "  c = mix(half3(luma),c,(half)p.sat+vib); c = clamp((c-0.5h)*(half)p.ctr+0.5h,0.0h,1.0h);\n"
    "  return float4((float3)mix(c, lut3d.sample(s,(float3)c).rgb,(half)p.lut),1.0);\n"
    "}\n"
    "fragment float4 f_main_fast(VSOut in [[stage_in]], texture2d<half> tex [[texture(0)]], texture3d<half> lut3d [[texture(1)]], sampler s [[sampler(0)]], constant Params& p [[buffer(0)]]) {\n"
    "  half3 c = tex.sample(s,in.uv).rgb; half luma = dot(c,half3(0.2126h,0.7152h,0.0722h));\n"
    "  c = mix(half3(luma),c,(half)p.sat+(half)p.vib*0.45h); c = clamp((c-0.5h)*(half)p.ctr+0.5h,0.0h,1.0h);\n"
    "  return float4((float3)mix(c, lut3d.sample(s,(float3)c).rgb,(half)p.lut*0.3h),1.0);\n"
    "}\n";

    NSError *err = nil;
    id<MTLLibrary> lib = [_device newLibraryWithSource:src options:nil error:&err];
    if (lib) {
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = [lib newFunctionWithName:@"v_main"];
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
        pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main_fast"];
        _fastPipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
    }
    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    _samplerState = [_device newSamplerStateWithDescriptor:sd];
    _colorLUT3D = [_device newTextureWithDescriptor:[MTLTextureDescriptor texture3DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:16 height:16 depth:16 mipmapped:NO]];
    return self;
}

- (void)_updateDrawableSize {
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    CGFloat scale = self.window.screen.nativeScale ?: [UIScreen mainScreen].nativeScale;
    CGSize size = self.bounds.size;
    if (size.width > 0.0 && size.height > 0.0) layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
}

- (void)layoutSubviews { [super layoutSubviews]; [self _updateDrawableSize]; }

- (void)ensureResourcesWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (_frameBuffer && _sourceTexture && _frameWidth == width && _frameHeight == height) return;
    _frameWidth = width; _frameHeight = height;
    _frameBytesPerRow = (width * 4 + 255) & ~255;
    _frameBuffer = [_device newBufferWithLength:_frameBytesPerRow * height options:MTLResourceStorageModeShared];
    _sourceTexture = [_device newTextureWithDescriptor:[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO]];
}

- (void)displayFrameRGBA:(const uint32_t *)pixels width:(NSUInteger)width height:(NSUInteger)height {
    [self displayFrameRGBA:pixels width:width height:height sourceRect:CGRectMake(0, 0, width, height)];
}

- (void)displayFrameRGBA:(const uint32_t *)pixels width:(NSUInteger)width height:(NSUInteger)height sourceRect:(CGRect)sourceRect {
    _sourceRect = sourceRect;
    if (!pixels || width == 0 || height == 0) return;
    [self ensureResourcesWithWidth:width height:height];
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) return;
    if (dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_NOW) != 0) return;
    uint8_t *dst = (uint8_t *)_frameBuffer.contents;
    const uint8_t *src = (const uint8_t *)pixels;
    const NSUInteger tightBpr = width * 4;
    if (_frameBytesPerRow == tightBpr) memcpy(dst, src, tightBpr * height);
    else for (NSUInteger y = 0; y < height; ++y) memcpy(dst + y * _frameBytesPerRow, src + y * tightBpr, tightBpr);
    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    [cb addCompletedHandler:^(__unused id<MTLCommandBuffer> b) { dispatch_semaphore_signal(self->_inFlightSemaphore); }];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit copyFromBuffer:_frameBuffer sourceOffset:0 sourceBytesPerRow:_frameBytesPerRow sourceBytesPerImage:_frameBytesPerRow * height sourceSize:MTLSizeMake(width, height, 1) toTexture:_sourceTexture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = drawable.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    AURPostProcessParams params = {
        .sourceSize = {(float)width, (float)height},
        .outputSize = {(float)layer.drawableSize.width, (float)layer.drawableSize.height},
        .sourceRect = {(float)(sourceRect.origin.x/width), (float)(sourceRect.origin.y/height), (float)(sourceRect.size.width/width), (float)(sourceRect.size.height/height)},
        .saturation = _userSaturation, .vibrance = _userVibrance, .contrast = _userContrast, .sharpen = _userSharpen, .lutMix = _userLutMix
    };
    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    [re setRenderPipelineState:_pipelineState];
    [re setFragmentTexture:_sourceTexture atIndex:0];
    [re setFragmentTexture:_colorLUT3D atIndex:1];
    [re setFragmentSamplerState:_samplerState atIndex:0];
    [re setVertexBytes:&params length:sizeof(params) atIndex:0];
    [re setFragmentBytes:&params length:sizeof(params) atIndex:0];
    [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [re endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
}

- (void)clearFrame {
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable> d = [layer nextDrawable];
    if (!d) return;
    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = d.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
    [[cb renderCommandEncoderWithDescriptor:rpd] endEncoding];
    [cb presentDrawable:d];
    [cb commit];
}
- (void)setPostProcessSaturation:(float)s vibrance:(float)v contrast:(float)c sharpen:(float)sh lutMix:(float)l {
    _userSaturation = s; _userVibrance = v; _userContrast = c; _userSharpen = sh; _userLutMix = l;
}
- (void)setUpscaleMode:(AURUpscaleMode)m { _upscaleMode = m; }
@end
