#import "./Metal.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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
@property (nonatomic, strong) id<MTLTexture>             colorLUT2D;
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
@property (nonatomic, assign) AURFramePixelFormat        framePixelFormat;
@end

@implementation AURMetalView

static void AURFillIdentityLUT(float *lutData, NSUInteger width, NSUInteger height) {
    if (!lutData || width != 16 || height != 256) {
        return;
    }
    for (NSUInteger g = 0; g < 16; ++g) {
        for (NSUInteger b = 0; b < 16; ++b) {
            for (NSUInteger r = 0; r < 16; ++r) {
                NSUInteger y = g * 16 + b;
                NSUInteger idx = (y * width + r) * 4;
                lutData[idx + 0] = (float)r / 15.0f;
                lutData[idx + 1] = (float)g / 15.0f;
                lutData[idx + 2] = (float)b / 15.0f;
                lutData[idx + 3] = 1.0f;
            }
        }
    }
}

static inline float AURClamp01f(float v) {
    return fmaxf(0.0f, fminf(1.0f, v));
}

+ (Class)layerClass {
    return CAMetalLayer.class;
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;
    
    // Metal デバイス初期化
    _device = MTLCreateSystemDefaultDevice();
    if (!_device) return nil;
    
    _commandQueue = [_device newCommandQueue];
    if (!_commandQueue) return nil;
    
    _inFlightSemaphore = dispatch_semaphore_create(3);
    
    // UI デフォルト値
    _upscaleMode = AURUpscaleModeAuto;
    _userSaturation = 1.08f;
    _userVibrance = 0.30f;
    _userContrast = 1.06f;
    _userSharpen = 0.18f;
    _userLutMix = 0.15f;
    _sourceRect = CGRectMake(0, 0, 1, 1);
    _framePixelFormat = AURFramePixelFormatRGBA8888;
    
    self.backgroundColor = [UIColor blackColor];
    self.opaque = YES;
    
    // CAMetalLayer 設定
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    layer.device = _device;
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = NO;
    layer.opaque = YES;
    layer.magnificationFilter = kCAFilterLinear;
    layer.minificationFilter = kCAFilterLinear;
    
    // Metal シェーダーソース
    NSString *metalSrc = @"#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VSOut {\n"
    "    float4 pos [[position]];\n"
    "    float2 uv;\n"
    "};\n"
    "\n"
    "struct Params {\n"
    "    float2 src_sz;\n"
    "    float2 out_sz;\n"
    "    float4 src_rc;\n"
    "    float sat;\n"
    "    float vib;\n"
    "    float ctr;\n"
    "    float shp;\n"
    "    float lut;\n"
    "    float _pad;\n"
    "};\n"
    "\n"
    "half4 cubicWeights(half t) {\n"
    "    half t2 = t * t;\n"
    "    half t3 = t2 * t;\n"
    "    return half4(\n"
    "        -0.5h * t3 + t2 - 0.5h * t,\n"
    "        1.5h * t3 - 2.5h * t2 + 1.0h,\n"
    "        -1.5h * t3 + 2.0h * t2 + 0.5h * t,\n"
    "        0.5h * t3 - 0.5h * t2\n"
    "    );\n"
    "}\n"
    "\n"
    "half3 sampleLUT2D(texture2d<half> lut, sampler s, half3 c) {\n"
    "    half cr = clamp(c.r, 0.0h, 1.0h) * 15.0h;\n"
    "    half cg = clamp(c.g, 0.0h, 1.0h) * 15.0h;\n"
    "    half cb = clamp(c.b, 0.0h, 1.0h) * 15.0h;\n"
    "    \n"
    "    uint r_idx = min(uint(cr), 15u);\n"
    "    uint g_idx = min(uint(cg), 15u);\n"
    "    uint b_idx = min(uint(cb), 15u);\n"
    "    \n"
    "    float uv_x = (float(r_idx) + 0.5h) / 16.0;\n"
    "    float uv_y = (float(g_idx * 16u + b_idx) + 0.5h) / 256.0;\n"
    "    \n"
    "    return lut.sample(s, float2(uv_x, uv_y)).rgb;\n"
    "}\n"
    "\n"
    "vertex VSOut v_main(\n"
    "    uint vid [[vertex_id]],\n"
    "    constant Params& p [[buffer(0)]]\n"
    ") {\n"
    "    const float2 pos[3] = {\n"
    "        float2(-1.0, -1.0),\n"
    "        float2(3.0, -1.0),\n"
    "        float2(-1.0, 3.0)\n"
    "    };\n"
    "    \n"
    "    float2 uv = pos[vid] * 0.5 + 0.5;\n"
    "    \n"
    "    VSOut o;\n"
    "    o.pos = float4(pos[vid], 0.0, 1.0);\n"
    "    o.uv = float2(\n"
    "        uv.x * p.src_rc.z + p.src_rc.x,\n"
    "        (1.0 - uv.y) * p.src_rc.w + p.src_rc.y\n"
    "    );\n"
    "    \n"
    "    return o;\n"
    "}\n"
    "\n"
    "fragment float4 f_main(\n"
    "    VSOut in [[stage_in]],\n"
    "    texture2d<half> tex [[texture(0)]],\n"
    "    texture2d<half> lut2d [[texture(1)]],\n"
    "    sampler s [[sampler(0)]],\n"
    "    constant Params& p [[buffer(0)]]\n"
    ") {\n"
    "    float2 px = in.uv * p.src_sz - 0.5;\n"
    "    float2 f = fract(px);\n"
    "    float2 inv = 1.0 / p.src_sz;\n"
    "    float2 base = (floor(px) + 0.5) * inv;\n"
    "    \n"
    "    half4 wx = cubicWeights((half)f.x);\n"
    "    half4 wy = cubicWeights((half)f.y);\n"
    "    \n"
    "    half3 acc = half3(0.0h);\n"
    "    \n"
    "    for (int j = 0; j < 4; ++j) {\n"
    "        float v = base.y + float(j - 1) * inv.y;\n"
    "        \n"
    "        half3 row = half3(0.0h);\n"
    "        row += wx[0] * tex.sample(s, float2(base.x - inv.x, v)).rgb;\n"
    "        row += wx[1] * tex.sample(s, float2(base.x, v)).rgb;\n"
    "        row += wx[2] * tex.sample(s, float2(base.x + inv.x, v)).rgb;\n"
    "        row += wx[3] * tex.sample(s, float2(base.x + inv.x * 2.0, v)).rgb;\n"
    "        \n"
    "        acc += wy[j] * row;\n"
    "    }\n"
    "    \n"
    "    half3 c = clamp(acc, 0.0h, 1.0h);\n"
    "    \n"
    "    half luma = dot(c, half3(0.2126h, 0.7152h, 0.0722h));\n"
    "    half chroma = max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);\n"
    "    half vib = (half)p.vib * (1.0h - chroma);\n"
    "    \n"
    "    c = mix(half3(luma), c, (half)p.sat + vib);\n"
    "    c = clamp((c - 0.5h) * (half)p.ctr + 0.5h, 0.0h, 1.0h);\n"
    "    \n"
    "    half3 lutc = sampleLUT2D(lut2d, s, c);\n"
    "    half3 final = mix(c, lutc, (half)p.lut);\n"
    "    \n"
    "    return float4((float3)final, 1.0);\n"
    "}\n"
    "\n"
    "fragment float4 f_main_fast(\n"
    "    VSOut in [[stage_in]],\n"
    "    texture2d<half> tex [[texture(0)]],\n"
    "    texture2d<half> lut2d [[texture(1)]],\n"
    "    sampler s [[sampler(0)]],\n"
    "    constant Params& p [[buffer(0)]]\n"
    ") {\n"
    "    half3 c = tex.sample(s, in.uv).rgb;\n"
    "    \n"
    "    half luma = dot(c, half3(0.2126h, 0.7152h, 0.0722h));\n"
    "    c = mix(half3(luma), c, (half)p.sat + (half)p.vib * 0.45h);\n"
    "    c = clamp((c - 0.5h) * (half)p.ctr + 0.5h, 0.0h, 1.0h);\n"
    "    \n"
    "    half3 lutc = sampleLUT2D(lut2d, s, c);\n"
    "    half3 final = mix(c, lutc, (half)p.lut * 0.3h);\n"
    "    \n"
    "    return float4((float3)final, 1.0);\n"
    "}\n";
    
    NSError *err = nil;
    id<MTLLibrary> lib = [_device newLibraryWithSource:metalSrc options:nil error:&err];
    
    if (!lib) {
        NSLog(@"Metal shaderコンパイルエラー: %@", err.localizedDescription);
        return nil;
    }
    
    // フルパイプライン
    {
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = [lib newFunctionWithName:@"v_main"];
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main"];
        pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
        
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!_pipelineState) {
            NSLog(@"フルパイプライン生成エラー: %@", err.localizedDescription);
            return nil;
        }
    }
    
    // 高速パイプライン
    {
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = [lib newFunctionWithName:@"v_main"];
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main_fast"];
        pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
        
        _fastPipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!_fastPipelineState) {
            NSLog(@"高速パイプライン生成エラー: %@", err.localizedDescription);
            return nil;
        }
    }
    
    // サンプラー
    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter = MTLSamplerMinMagFilterLinear;
    sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.rAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.normalizedCoordinates = YES;
    sd.lodMinClamp = 0.0f;
    sd.lodMaxClamp = FLT_MAX;
    
    _samplerState = [_device newSamplerStateWithDescriptor:sd];
    if (!_samplerState) {
        NSLog(@"サンプラー生成エラー");
        return nil;
    }
    
    // iOS互換: 2D LUT テクスチャ 16x256 (16x16x16 フラット化)
    // iOS では MTLStorageModePrivate のみ使用
    MTLTextureDescriptor *lutDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float width:16 height:256 mipmapped:NO];
    lutDesc.usage = MTLTextureUsageShaderRead;
    lutDesc.storageMode = MTLStorageModePrivate;
    
    _colorLUT2D = [_device newTextureWithDescriptor:lutDesc];
    if (!_colorLUT2D) {
        NSLog(@"LUT テクスチャ生成エラー");
        return nil;
    }

    const NSUInteger lutWidth = 16;
    const NSUInteger lutHeight = 256;
    const NSUInteger lutFloats = lutWidth * lutHeight * 4;
    float *identityLUT = (float *)calloc(lutFloats, sizeof(float));
    if (identityLUT) {
        AURFillIdentityLUT(identityLUT, lutWidth, lutHeight);
        [self setColorLUT3DData:identityLUT size:lutFloats];
        free(identityLUT);
    }
    
    return self;
}

- (void)_updateDrawableSize {
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    if (!layer || !self.window) return;
    
    CGFloat scale = self.window.screen.nativeScale;
    if (scale == 0.0) scale = [UIScreen mainScreen].nativeScale;
    
    CGSize size = self.bounds.size;
    if (size.width > 0.0 && size.height > 0.0) {
        layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];
    [self _updateDrawableSize];
}

- (void)ensureResourcesWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (_frameBuffer && _sourceTexture && _frameWidth == width && _frameHeight == height) {
        return;
    }
    
    _frameWidth = width;
    _frameHeight = height;
    
    // 256バイト境界アライン
    _frameBytesPerRow = (width * 4 + 255) & ~255;
    
    // iOS: MTLResourceStorageModeShared を使用（CPU-GPU 共有メモリ）
    _frameBuffer = [_device newBufferWithLength:_frameBytesPerRow * height options:MTLResourceStorageModeShared];
    if (!_frameBuffer) {
        NSLog(@"フレームバッファ割り当てエラー");
        return;
    }
    
    MTLPixelFormat sourcePixelFormat = (_framePixelFormat == AURFramePixelFormatBGRA8888)
        ? MTLPixelFormatBGRA8Unorm
        : MTLPixelFormatRGBA8Unorm;
    MTLTextureDescriptor *texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:sourcePixelFormat width:width height:height mipmapped:NO];
    texDesc.usage = MTLTextureUsageShaderRead;
    texDesc.storageMode = MTLStorageModeShared;
    
    _sourceTexture = [_device newTextureWithDescriptor:texDesc];
    if (!_sourceTexture) {
        NSLog(@"ソーステクスチャ割り当てエラー");
        return;
    }
}

- (void)displayFrameRGBA:(const uint32_t *)pixels width:(NSUInteger)width height:(NSUInteger)height {
    [self displayFrameRGBA:pixels width:width height:height sourceRect:CGRectMake(0, 0, width, height)];
}

- (void)displayFrameRGBA:(const uint32_t *)pixels width:(NSUInteger)width height:(NSUInteger)height sourceRect:(CGRect)sourceRect {
    _sourceRect = sourceRect;
    
    if (!pixels || width == 0 || height == 0) {
        return;
    }
    
    [self _updateDrawableSize];
    [self ensureResourcesWithWidth:width height:height];
    if (!_frameBuffer || !_sourceTexture || !_pipelineState || !_samplerState) {
        return;
    }
    
    // フレームレート制限（最大3フレーム）
    if (dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_NOW) != 0) {
        return;
    }
    
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) {
        dispatch_semaphore_signal(_inFlightSemaphore);
        return;
    }
    
    // ピクセルデータコピー（メモリ最適化）
    uint8_t *dst = (uint8_t *)_frameBuffer.contents;
    const uint8_t *src = (const uint8_t *)pixels;
    const NSUInteger tightBpr = width * 4;
    
    if (_frameBytesPerRow == tightBpr) {
        memcpy(dst, src, tightBpr * height);
    } else {
        // 行ごとにコピー（アライメント対応）
        for (NSUInteger y = 0; y < height; ++y) {
            memcpy(dst + y * _frameBytesPerRow, src + y * tightBpr, tightBpr);
        }
    }
    
    // コマンドバッファ作成
    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    if (!cb) {
        dispatch_semaphore_signal(_inFlightSemaphore);
        return;
    }
    
    // 完了コールバック（weak-strong dance で race condition 排除）
    __weak typeof(self) weakSelf = self;
    [cb addCompletedHandler:^(__unused id<MTLCommandBuffer> b) {
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf) {
            dispatch_semaphore_signal(strongSelf->_inFlightSemaphore);
        }
    }];
    
    // ブリットコマンド（GPU転送）
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    if (blit) {
        [blit copyFromBuffer:_frameBuffer
                sourceOffset:0
            sourceBytesPerRow:_frameBytesPerRow
          sourceBytesPerImage:_frameBytesPerRow * height
                   sourceSize:MTLSizeMake(width, height, 1)
                    toTexture:_sourceTexture
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
    }
    
    // レンダーパス設定
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = drawable.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    // パラメータ構築
    AURPostProcessParams params = {
        .sourceSize = {(float)width, (float)height},
        .outputSize = {(float)layer.drawableSize.width, (float)layer.drawableSize.height},
        .sourceRect = {
            AURClamp01f((float)(sourceRect.origin.x / width)),
            AURClamp01f((float)(sourceRect.origin.y / height)),
            AURClamp01f((float)(sourceRect.size.width / width)),
            AURClamp01f((float)(sourceRect.size.height / height))
        },
        .saturation = _userSaturation,
        .vibrance = _userVibrance,
        .contrast = _userContrast,
        .sharpen = _userSharpen,
        .lutMix = _userLutMix,
        ._pad = 0.0f
    };
    
    // レンダーコマンド
    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    if (re) {
        id<MTLRenderPipelineState> pipeline = _pipelineState;
        if (_upscaleMode == AURUpscaleModePerformance && _fastPipelineState) {
            pipeline = _fastPipelineState;
        }
        [re setRenderPipelineState:pipeline];
        [re setFragmentTexture:_sourceTexture atIndex:0];
        [re setFragmentTexture:_colorLUT2D atIndex:1];
        [re setFragmentSamplerState:_samplerState atIndex:0];
        [re setVertexBytes:&params length:sizeof(params) atIndex:0];
        [re setFragmentBytes:&params length:sizeof(params) atIndex:0];
        [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [re endEncoding];
    }
    
    // 画面表示 & コマンド実行
    [cb presentDrawable:drawable];
    [cb commit];
}

- (void)clearFrame {
    CAMetalLayer *layer = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable> d = [layer nextDrawable];
    if (!d) {
        return;
    }
    
    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    if (!cb) {
        return;
    }
    
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture = d.texture;
    rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
    rpd.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    if (re) {
        [re endEncoding];
    }
    
    [cb presentDrawable:d];
    [cb commit];
}

- (void)setPostProcessSaturation:(float)s vibrance:(float)v contrast:(float)c sharpen:(float)sh lutMix:(float)l {
    _userSaturation = s;
    _userVibrance = v;
    _userContrast = c;
    _userSharpen = sh;
    _userLutMix = l;
}

- (void)setUpscaleMode:(AURUpscaleMode)m {
    _upscaleMode = m;
}

- (void)setFramePixelFormat:(AURFramePixelFormat)pixelFormat {
    if (_framePixelFormat == pixelFormat) {
        return;
    }
    _framePixelFormat = pixelFormat;
    _frameWidth = 0;
    _frameHeight = 0;
    _frameBuffer = nil;
    _sourceTexture = nil;
}

- (void)setColorLUT3DData:(const float *)data size:(NSUInteger)size {
    if (!data || !_colorLUT2D || size == 0) {
        return;
    }
    
    // LUT データは 16x16x16 (4096 ピクセル) を 16x256 フラット化
    // size = 16x256x4 = 16384 floats expected
    NSUInteger expectedSize = 16 * 256 * 4;
    if (size < expectedSize) {
        NSLog(@"LUT データサイズ不足: %zu < %zu", size, expectedSize);
        return;
    }
    
    // iOS: CPU → GPU へのテクスチャ更新は blitCommandEncoder で行う
    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    if (!cb) return;
    
    id<MTLBuffer> tempBuffer = [_device newBufferWithBytes:data length:expectedSize * sizeof(float) options:MTLResourceStorageModeShared];
    if (!tempBuffer) return;
    
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    if (blit) {
        [blit copyFromBuffer:tempBuffer
                sourceOffset:0
            sourceBytesPerRow:16 * 4 * sizeof(float)
          sourceBytesPerImage:16 * 256 * 4 * sizeof(float)
                   sourceSize:MTLSizeMake(16, 256, 1)
                    toTexture:_colorLUT2D
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [blit endEncoding];
    }
    
    [cb commit];
}

@end
