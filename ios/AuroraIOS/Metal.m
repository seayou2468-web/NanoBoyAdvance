//
// Metal.m
// gbemu-ハヤブサ
//
// Created by Anonym on 08.04.26.
//
// ── 改良ポイント ────────────────────────────────────────────────────────────
//  [1] drawable を UIView 実ピクセル解像度に設定 (nativeScale)
//      → GBA 240x160 を iPhone の全物理ピクセルで描画。「画素が細かい」を実現。
//  [2] Catmull-Rom Bicubic アップスケール (16 tap)
//      → バイリニアより遥かに高品質。なめらか＆シャープ。
//  [3] Vibrance (選択的彩度ブースト)
//      → 既に鮮やかな色は保護しつつ、くすんだ色だけ自然に鮮やかにする。
//  [4] 上下反転修正 (UV.y = 1 - uv.y)
//  [5] LUT pixelFormat を RGBA32Float に統一 (float データと型一致)
//  [6] LUT = 輝度ベース S字コントラスト (色相・彩度を保護)
//  [7] clearFrame で MTLLoadActionClear による確実な黒クリア
// ──────────────────────────────────────────────────────────────────────────

#import "./Metal.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <string.h>
#include <stdlib.h>
#import <simd/simd.h>

// ── Uniform バッファ構造体 ─────────────────────────────────────────────────
// Metal シェーダー側の struct Params と メンバー順・型・パディングを厳密に合わせること。
typedef struct {
    vector_float2 sourceSize;  //  8 bytes: GBA フレームサイズ (例: 240x160)
    vector_float2 outputSize;  //  8 bytes: drawable のピクセルサイズ (実画面解像度)
    float         saturation;  //  4 bytes: 全体彩度倍率 (1.0 = 原色)
    float         vibrance;    //  4 bytes: Vibrance 強度 (彩度低い色だけ選択的ブースト)
    float         contrast;    //  4 bytes: コントラスト倍率 (1.0 = 無変化)
    float         sharpen;     //  4 bytes: Laplacian シャープネス係数
    float         lutMix;      //  4 bytes: 3D LUT ブレンド比率 (0=LUT無効, 1=LUT全適用)
    float         _pad;        //  4 bytes: アライメント
} AURPostProcessParams;        // 合計 40 bytes

@interface AURMetalView ()
@property (nonatomic, strong) id<MTLDevice>              device;
@property (nonatomic, strong) id<MTLCommandQueue>        commandQueue;
@property (nonatomic, strong) id<MTLBuffer>              frameBuffer;
@property (nonatomic, strong) id<MTLTexture>             sourceTexture;
@property (nonatomic, strong) id<MTLTexture>             colorLUT3D;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLRenderPipelineState> fastPipelineState;
@property (nonatomic, strong) id<MTLSamplerState>        samplerState;
@property (nonatomic, assign) NSUInteger                 frameWidth;
@property (nonatomic, assign) NSUInteger                 frameHeight;
@property (nonatomic, assign) NSUInteger                 frameBytesPerRow;
@property (nonatomic, assign) dispatch_semaphore_t       inFlightSemaphore;
@end

@implementation AURMetalView

+ (Class)layerClass { return CAMetalLayer.class; }

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (!self) return nil;

    _device       = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];
    _inFlightSemaphore = dispatch_semaphore_create(3);
    self.backgroundColor = UIColor.blackColor;
    self.opaque          = YES;

    CAMetalLayer *layer       = (CAMetalLayer *)self.layer;
    layer.device              = _device;
    layer.pixelFormat         = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly     = NO;    // blit 書き込みのため必須
    layer.opaque              = YES;
    // drawable が実画面解像度なので layer 側の拡大はほぼ 1:1 → Linear で十分
    layer.magnificationFilter = kCAFilterLinear;
    layer.minificationFilter  = kCAFilterLinear;

    // ─── Metal シェーダー ─────────────────────────────────────────────────────
    NSString *src = @
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "\n"
    "struct VSOut { float4 pos [[position]]; float2 uv; };\n"
    "\n"
    "struct Params {\n"
    "  float2 source_size;\n"
    "  float2 output_size;\n"
    "  float  saturation;\n"
    "  float  vibrance;\n"
    "  float  contrast;\n"
    "  float  sharpen;\n"
    "  float  lut_mix;\n"
    "  float  _pad;\n"
    "};\n"
    "\n"
    // ── Catmull-Rom 1D 重み ─────────────────────────────────────────────────
    // t in [0,1): 4格子点 (-1,0,1,2) の重み。合計 = 1 保証。a = -0.5 (標準CR)
    "float4 cubicW(float t) {\n"
    "  float t2 = t*t, t3 = t*t2;\n"
    "  return float4(\n"
    "    -0.5*t3 + 1.0*t2 - 0.5*t,\n"
    "     1.5*t3 - 2.5*t2 + 1.0,\n"
    "    -1.5*t3 + 2.0*t2 + 0.5*t,\n"
    "     0.5*t3 - 0.5*t2\n"
    "  );\n"
    "}\n"
    "\n"
    // ── Catmull-Rom Bicubic 2D (16 tap) ─────────────────────────────────────
    "float3 sampleBicubic(texture2d<float> tex, sampler s, float2 uv, float2 sz) {\n"
    "  float2 inv = 1.0 / sz;\n"
    "  float2 px  = uv * sz - 0.5;\n"
    "  float2 f   = fract(px);\n"
    "  float2 b   = (floor(px) + 0.5) * inv;\n"
    "  float4 wx  = cubicW(f.x);\n"
    "  float4 wy  = cubicW(f.y);\n"
    "  float3 acc = float3(0.0);\n"
    "  for (int j = 0; j < 4; j++) {\n"
    "    float  v   = b.y + float(j - 1) * inv.y;\n"
    "    float3 row =\n"
    "      wx[0]*tex.sample(s, float2(b.x - inv.x,       v)).rgb +\n"
    "      wx[1]*tex.sample(s, float2(b.x,               v)).rgb +\n"
    "      wx[2]*tex.sample(s, float2(b.x + inv.x,       v)).rgb +\n"
    "      wx[3]*tex.sample(s, float2(b.x + inv.x * 2.0, v)).rgb;\n"
    "    acc += wy[j] * row;\n"
    "  }\n"
    "  return clamp(acc, 0.0, 1.0);\n"
    "}\n"
    "\n"
    // ── 頂点シェーダー ─────────────────────────────────────────────────────
    "vertex VSOut v_main(uint vid [[vertex_id]]) {\n"
    "  const float2 p[3] = { float2(-1,-1), float2(3,-1), float2(-1,3) };\n"
    "  float2 uv = p[vid] * 0.5 + 0.5;\n"
    "  VSOut o;\n"
    "  o.pos = float4(p[vid], 0.0, 1.0);\n"
    "  o.uv  = float2(uv.x, 1.0 - uv.y);\n"  // UV.y 反転: 上下を修正
    "  return o;\n"
    "}\n"
    "\n"
    // ── フラグメントシェーダー ─────────────────────────────────────────────
    "fragment float4 f_main(\n"
    "    VSOut            in    [[stage_in]],\n"
    "    texture2d<float> tex   [[texture(0)]],\n"
    "    texture3d<float> lut3d [[texture(1)]],\n"
    "    sampler          s     [[sampler(0)]],\n"
    "    constant Params& p     [[buffer(0)]]) {\n"
    "\n"
    // ① Catmull-Rom Bicubic アップスケール (最高品質補間)
    "  float3 c = sampleBicubic(tex, s, in.uv, p.source_size);\n"
    "\n"
    // ② Laplacian シャープニング (元解像度レベルでエッジを立てる)
    "  float2 inv = 1.0 / p.source_size;\n"
    "  float3 nb  =\n"
    "    tex.sample(s, in.uv + float2( 0.0, -inv.y)).rgb +\n"
    "    tex.sample(s, in.uv + float2(-inv.x,  0.0)).rgb +\n"
    "    tex.sample(s, in.uv + float2( inv.x,  0.0)).rgb +\n"
    "    tex.sample(s, in.uv + float2( 0.0,  inv.y)).rgb;\n"
    "  c = clamp(c + (c * 4.0 - nb) * p.sharpen, 0.0, 1.0);\n"
    "\n"
    // ③ 輝度 (BT.709)
    "  float luma = dot(c, float3(0.2126, 0.7152, 0.0722));\n"
    "\n"
    // ④ Vibrance: chroma (max-min) が低い色ほど強くブースト
    //    → くすんだ色を自然に鮮やかにし、既に鮮やかな色は保護する
    "  float chroma   = max(max(c.r,c.g),c.b) - min(min(c.r,c.g),c.b);\n"
    "  float vibBoost = p.vibrance * (1.0 - chroma);\n"
    "  c = clamp(mix(float3(luma), c, p.saturation + vibBoost), 0.0, 1.0);\n"
    "\n"
    // ⑤ コントラスト (中点 0.5 基準)
    "  c = clamp((c - 0.5) * p.contrast + 0.5, 0.0, 1.0);\n"
    "\n"
    // ⑥ 3D LUT ブレンド
    "  float3 lutColor = lut3d.sample(s, c).rgb;\n"
    "  c = mix(c, lutColor, clamp(p.lut_mix, 0.0, 1.0));\n"
    "\n"
    "  return float4(c, 1.0);\n"
    "}\n"
    "\n"
    // ── 負荷軽減用の高速パス (低電力/高熱時に自動使用) ───────────────────
    "fragment float4 f_main_fast(\n"
    "    VSOut            in    [[stage_in]],\n"
    "    texture2d<float> tex   [[texture(0)]],\n"
    "    texture3d<float> lut3d [[texture(1)]],\n"
    "    sampler          s     [[sampler(0)]],\n"
    "    constant Params& p     [[buffer(0)]]) {\n"
    "  float3 c = tex.sample(s, in.uv).rgb;\n"
    "  float luma = dot(c, float3(0.2126, 0.7152, 0.0722));\n"
    "  float boost = p.vibrance * 0.45;\n"
    "  c = clamp(mix(float3(luma), c, p.saturation + boost), 0.0, 1.0);\n"
    "  c = clamp((c - 0.5) * p.contrast + 0.5, 0.0, 1.0);\n"
    "  float3 lutColor = lut3d.sample(s, c).rgb;\n"
    "  c = mix(c, lutColor, p.lut_mix * 0.3);\n"
    "  return float4(c, 1.0);\n"
    "}\n";

    NSError *err = nil;
    id<MTLLibrary> lib = [_device newLibraryWithSource:src options:nil error:&err];
    NSAssert(lib, @"[AURMetalView] シェーダーコンパイル失敗: %@", err);
    if (lib) {
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction              = [lib newFunctionWithName:@"v_main"];
        pd.fragmentFunction            = [lib newFunctionWithName:@"f_main"];
        pd.colorAttachments[0].pixelFormat = layer.pixelFormat;
        _pipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
        NSAssert(_pipelineState, @"[AURMetalView] パイプライン生成失敗: %@", err);
        pd.fragmentFunction = [lib newFunctionWithName:@"f_main_fast"];
        _fastPipelineState = [_device newRenderPipelineStateWithDescriptor:pd error:&err];
        NSAssert(_fastPipelineState, @"[AURMetalView] 高速パイプライン生成失敗: %@", err);
    }

    // バイリニアサンプラー (bicubic tap は clampToEdge が境界保護に必須)
    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
    sd.minFilter    = MTLSamplerMinMagFilterLinear;
    sd.magFilter    = MTLSamplerMinMagFilterLinear;
    sd.mipFilter    = MTLSamplerMipFilterNotMipmapped;
    sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
    sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    _samplerState   = [_device newSamplerStateWithDescriptor:sd];

    // ─── 3D LUT (16^3, RGBA32Float) ─────────────────────────────────────────
    // 輝度ベース S字コントラスト。RGB 比率不変 → 色相・彩度を完全保護。
    // 最大変化量 ≈ ±0.009 (非常に控えめな自然なメリハリ)。
    const NSUInteger lutSize = 16;
    MTLTextureDescriptor *lutDesc = [[MTLTextureDescriptor alloc] init];
    lutDesc.textureType      = MTLTextureType3D;
    lutDesc.pixelFormat      = MTLPixelFormatRGBA32Float; // float データと型一致
    lutDesc.width            = lutSize;
    lutDesc.height           = lutSize;
    lutDesc.depth            = lutSize;
    lutDesc.mipmapLevelCount = 1;
    lutDesc.usage            = MTLTextureUsageShaderRead;
    lutDesc.storageMode      = MTLStorageModeShared;
    _colorLUT3D = [_device newTextureWithDescriptor:lutDesc];

    if (_colorLUT3D) {
        const NSUInteger count = lutSize * lutSize * lutSize * 4;
        float *lutData = (float *)malloc(count * sizeof(float));
        if (lutData) {
            for (NSUInteger b = 0; b < lutSize; b++) {
                for (NSUInteger g = 0; g < lutSize; g++) {
                    for (NSUInteger r = 0; r < lutSize; r++) {
                        float rf = (float)r / (float)(lutSize - 1);
                        float gf = (float)g / (float)(lutSize - 1);
                        float bf = (float)b / (float)(lutSize - 1);
                        float lm    = 0.2126f*rf + 0.7152f*gf + 0.0722f*bf;
                        float sc    = lm * lm * (3.0f - 2.0f * lm); // smoothstep
                        float boost = (sc - lm) * 0.12f;
                        const NSUInteger idx = ((b*lutSize*lutSize) + (g*lutSize) + r) * 4;
                        lutData[idx+0] = rf + boost;
                        lutData[idx+1] = gf + boost;
                        lutData[idx+2] = bf + boost;
                        lutData[idx+3] = 1.0f;
                    }
                }
            }
            MTLRegion region = MTLRegionMake3D(0, 0, 0, lutSize, lutSize, lutSize);
            [_colorLUT3D replaceRegion:region
                           mipmapLevel:0
                                 slice:0
                             withBytes:lutData
                           bytesPerRow:lutSize * 4 * sizeof(float)
                         bytesPerImage:lutSize * lutSize * 4 * sizeof(float)];
            free(lutData);
        }
    }
    return self;
}

// ─── drawable を UIView 実ピクセル解像度に更新 ─────────────────────────────
// nativeScale = iPhone 3x, iPad 2x 等の物理ピクセル倍率。
// これにより drawable ピクセル数 = 画面物理ピクセル数 → 「画素が細かく美しい」。
- (void)_updateDrawableSize {
    CAMetalLayer *layer  = (CAMetalLayer *)self.layer;
    UIScreen     *screen = self.window.screen ?: UIScreen.mainScreen;
    CGFloat       scale  = screen.nativeScale;
    CGSize        size   = self.bounds.size;
    if (size.width > 0.0 && size.height > 0.0) {
        layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];
    [self _updateDrawableSize];
}

// ─── GBA 解像度のバッファ・テクスチャを確保 ──────────────────────────────
// drawableSize の変更はここでは行わない (_updateDrawableSize が担当)。
- (void)ensureResourcesWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (_frameBuffer && _sourceTexture && _frameWidth == width && _frameHeight == height) return;

    _frameWidth  = width;
    _frameHeight = height;

    // GBA: 240px × 4bytes = 960bytes → 256バイト境界 → 1024bytes
    const NSUInteger bpp      = sizeof(uint32_t);
    const NSUInteger tightBpr = width * bpp;
    const NSUInteger align    = 256;
    const NSUInteger bpr      = (tightBpr + align - 1) & ~(align - 1);

    _frameBuffer      = [_device newBufferWithLength:bpr * height
                                             options:MTLResourceStorageModeShared];
    _frameBytesPerRow = bpr;

    MTLTextureDescriptor *td =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    td.storageMode = MTLStorageModePrivate;
    td.usage       = MTLTextureUsageShaderRead;
    _sourceTexture = [_device newTextureWithDescriptor:td];
}

// ─── メインの描画エントリポイント ─────────────────────────────────────────
- (void)displayFrameRGBA:(const uint32_t *)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height {
    if (!pixels || width == 0 || height == 0 || !_device || !_commandQueue) return;
    [self ensureResourcesWithWidth:width height:height];
    if (!_frameBuffer || !_sourceTexture || !_pipelineState || !_fastPipelineState || !_samplerState || !_colorLUT3D) return;

    CAMetalLayer        *layer    = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable>  drawable = [layer nextDrawable];
    if (!drawable) return;

    // GPU が詰まっている場合は新規フレームをスキップして入力遅延と発熱を抑える。
    if (dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_NOW) != 0) {
        return;
    }

    // CPU → Shared Buffer (パディング行対応ライン毎コピー)
    const NSUInteger tightBpr = width * sizeof(uint32_t);
    const NSUInteger bpr      = _frameBytesPerRow;
    uint8_t       *dst = (uint8_t *)_frameBuffer.contents;
    const uint8_t *src = (const uint8_t *)pixels;
    if (bpr == tightBpr) {
        memcpy(dst, src, tightBpr * height);
    } else {
        for (NSUInteger y = 0; y < height; ++y) {
            memcpy(dst + y * bpr, src + y * tightBpr, tightBpr);
        }
    }

    id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
    [cb addCompletedHandler:^(__unused id<MTLCommandBuffer> _Nonnull buffer) {
        dispatch_semaphore_signal(self->_inFlightSemaphore);
    }];

    // Blit: Shared Buffer → Private Texture
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit copyFromBuffer:_frameBuffer
            sourceOffset:0
       sourceBytesPerRow:bpr
     sourceBytesPerImage:bpr * height
              sourceSize:MTLSizeMake(width, height, 1)
               toTexture:_sourceTexture
        destinationSlice:0
        destinationLevel:0
       destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];

    // Render Pass: Bicubic アップスケール + 色補正 → Drawable (実画面解像度)
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture     = drawable.texture;
    rpd.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

    CGSize drawableSize = layer.drawableSize;
    const float drawablePixels = (float)(drawableSize.width * drawableSize.height);
    NSProcessInfo *pi = NSProcessInfo.processInfo;
    BOOL shouldUseFastPath = pi.isLowPowerModeEnabled;
    if (@available(iOS 11.0, *)) {
        NSProcessInfoThermalState thermal = pi.thermalState;
        shouldUseFastPath = shouldUseFastPath || (thermal >= NSProcessInfoThermalStateSerious);
    }
    // iPhone 12 mini 以降の高dpi端末でも過負荷を避けるため、
    // 描画ピクセルが大きい場合は高速パスへ自動切り替え。
    shouldUseFastPath = shouldUseFastPath || (drawablePixels > 2.7e6f);

    AURPostProcessParams params = {
        .sourceSize = { (float)width,              (float)height              },
        .outputSize = { (float)drawableSize.width,  (float)drawableSize.height },
        .saturation = shouldUseFastPath ? 1.03f : 1.08f,
        .vibrance   = shouldUseFastPath ? 0.18f : 0.30f,
        .contrast   = shouldUseFastPath ? 1.03f : 1.06f,
        .sharpen    = shouldUseFastPath ? 0.0f : 0.18f,
        .lutMix     = shouldUseFastPath ? 0.06f : 0.15f,
        ._pad       = 0.0f,
    };

    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    [re setRenderPipelineState:(shouldUseFastPath ? _fastPipelineState : _pipelineState)];
    [re setFragmentTexture:_sourceTexture atIndex:0];
    [re setFragmentTexture:_colorLUT3D    atIndex:1];
    [re setFragmentSamplerState:_samplerState atIndex:0];
    [re setFragmentBytes:&params length:sizeof(params) atIndex:0];
    [re drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [re endEncoding];

    [cb presentDrawable:drawable];
    [cb commit];
}

// ─── 画面クリア ───────────────────────────────────────────────────────────
- (void)clearFrame {
    CAMetalLayer        *layer    = (CAMetalLayer *)self.layer;
    id<CAMetalDrawable>  drawable = [layer nextDrawable];
    if (!drawable || !_commandQueue) return;
    id<MTLCommandBuffer>     cb  = [_commandQueue commandBuffer];
    MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor renderPassDescriptor];
    rpd.colorAttachments[0].texture     = drawable.texture;
    rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
    rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
    rpd.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    id<MTLRenderCommandEncoder> re = [cb renderCommandEncoderWithDescriptor:rpd];
    [re endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
}

@end
