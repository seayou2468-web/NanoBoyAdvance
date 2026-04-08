#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#include <stdint.h>

NS_ASSUME_NONNULL_BEGIN

@interface AURMetalView : UIView
- (void)displayFrameRGBA:(const uint32_t*)pixels
                   width:(NSUInteger)width
                  height:(NSUInteger)height;
- (void)clearFrame;
@end

NS_ASSUME_NONNULL_END
