#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, AURControllerSkinOrientation) {
    AURControllerSkinOrientationPortrait,
    AURControllerSkinOrientationLandscape
};

typedef NS_ENUM(NSInteger, AURControllerSkinDevice) {
    AURControllerSkinDeviceIPhone,
    AURControllerSkinDeviceIPad,
    AURControllerSkinDeviceTV
};

typedef NS_ENUM(NSInteger, AURControllerSkinDisplayType) {
    AURControllerSkinDisplayTypeStandard,
    AURControllerSkinDisplayTypeEdgeToEdge,
    AURControllerSkinDisplayTypeSplitView
};

@interface AURControllerSkinTraits : NSObject <NSCopying>
@property (nonatomic, assign) AURControllerSkinOrientation orientation;
@property (nonatomic, assign) AURControllerSkinDevice device;
@property (nonatomic, assign) AURControllerSkinDisplayType displayType;

+ (instancetype)defaultTraitsForWindow:(UIWindow *)window;
@end
