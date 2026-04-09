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

@interface AURMetalView ()
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLBuffer> frameBuffer;
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
    return self;
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    if (self.frameWidth > 0 && self.frameHeight > 0) {
        layer.drawableSize = CGSizeMake(self.frameWidth, self.frameHeight);
    }
}

- (void)ensureFrameBufferWithWidth:(NSUInteger)width height:(NSUInteger)height {
    if (self.frameBuffer && self.frameWidth == width && self.frameHeight == height) {
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

    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    layer.drawableSize = CGSizeMake(width, height);
}

- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height {
    if (!pixels || width == 0 || height == 0 || !self.device || !self.commandQueue) { return; }
    [self ensureFrameBufferWithWidth:width height:height];
    if (!self.frameBuffer) { return; }

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
