#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "../Models/AURControllerSkin.h"

@interface AURSkinImageOperation : NSOperation
@property (nonatomic, strong, readonly) UIImage *image;
@property (nonatomic, copy) void(^resultHandler)(UIImage *image);
- (instancetype)initWithSkin:(AURControllerSkin *)skin traits:(AURControllerSkinTraits *)traits;
@end
