#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AURUpscaleMode) {
    AURUpscaleModeAuto = 0,
    AURUpscaleModeQuality = 1,
    AURUpscaleModePerformance = 2,
};

@interface AURMetalView : UIView
- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height;
// Optimized NDS support: upload the full combined buffer once, and tell each view which part to show.
- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height
              sourceRect:(CGRect)sourceRect;
- (void)clearFrame;
- (void)setPostProcessSaturation:(float)saturation
                        vibrance:(float)vibrance
                        contrast:(float)contrast
                         sharpen:(float)sharpen
                          lutMix:(float)lutMix;
- (void)setUpscaleMode:(AURUpscaleMode)mode;
@end

NS_ASSUME_NONNULL_END
