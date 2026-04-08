//
// Metal.m
// gbemu-ハヤブサ
//
// Created by Anonym on 08.04.26.
//

#import "Metal.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

@interface AURMetalView ()
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLTexture> frameTexture;
@property (nonatomic, assign) NSUInteger frameWidth;
@property (nonatomic, assign) NSUInteger frameHeight;
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
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    if (self.frameWidth > 0 && self.frameHeight > 0) {
        layer.drawableSize = CGSizeMake(self.frameWidth, self.frameHeight);
    }
}

- (void)ensureFrameTextureWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (self.frameTexture && self.frameWidth == width && self.frameHeight == height) {
        return;
    }

    self.frameWidth = width;
    self.frameHeight = height;

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];

    // 修正: iOS では BlitSource は未定義なので RenderTarget を使う
    desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    desc.storageMode = MTLStorageModeShared;
    self.frameTexture = [self.device newTextureWithDescriptor:desc];

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    layer.drawableSize = CGSizeMake(width, height);
}

- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height {
    if (!pixels || width == 0 || height == 0 || !self.device || !self.commandQueue) { return; }
    [self ensureFrameTextureWithWidth:width height:height];
    if (!self.frameTexture) { return; }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [self.frameTexture replaceRegion:region
                         mipmapLevel:0
                           withBytes:pixels
                         bytesPerRow:width * sizeof(uint32_t)];

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    id<CAMetalDrawable> drawable = [layer nextDrawable];
    if (!drawable) { return; }

    id<MTLCommandBuffer> cb = [self.commandQueue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [cb blitCommandEncoder];
    [blit copyFromTexture:self.frameTexture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(width, height, 1)
                toTexture:drawable.texture
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
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
