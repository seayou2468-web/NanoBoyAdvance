#import "AURControllerSkinTraits.h"

@implementation AURControllerSkinTraits

+ (instancetype)defaultTraitsForWindow:(UIWindow *)window {
    AURControllerSkinTraits *traits = [[AURControllerSkinTraits alloc] init];
    traits.device = (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) ? AURControllerSkinDeviceIPad : AURControllerSkinDeviceIPhone;
    traits.displayType = AURControllerSkinDisplayTypeStandard; // Simplified
    traits.orientation = (window.bounds.size.width > window.bounds.size.height) ? AURControllerSkinOrientationLandscape : AURControllerSkinOrientationPortrait;
    return traits;
}

- (id)copyWithZone:(NSZone *)zone {
    AURControllerSkinTraits *copy = [[[self class] allocWithZone:zone] init];
    copy.orientation = self.orientation;
    copy.device = self.device;
    copy.displayType = self.displayType;
    return copy;
}

- (BOOL)isEqual:(id)object {
    if (![object isKindOfClass:[AURControllerSkinTraits class]]) return NO;
    AURControllerSkinTraits *other = object;
    return self.orientation == other.orientation && self.device == other.device && self.displayType == other.displayType;
}

- (NSUInteger)hash {
    return self.orientation ^ self.device ^ self.displayType;
}

@end
