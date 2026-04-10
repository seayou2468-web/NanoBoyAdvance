#import "AURSkinImageOperation.h"

@interface AURSkinImageOperation ()
@property (nonatomic, strong) AURControllerSkin *skin;
@property (nonatomic, strong) AURControllerSkinTraits *traits;
@property (nonatomic, strong, readwrite) UIImage *image;
@end

@implementation AURSkinImageOperation

- (instancetype)initWithSkin:(AURControllerSkin *)skin traits:(AURControllerSkinTraits *)traits {
    self = [super init];
    if (self) {
        _skin = skin;
        _traits = traits;
    }
    return self;
}

- (void)main {
    if (self.isCancelled) return;

    // In Delta, this handles complex mapping. Here we just take the background.
    UIImage *rawImage = self.skin.backgroundImage;
    if (!rawImage) return;

    // Force decompression
    UIGraphicsBeginImageContextWithOptions(CGSizeMake(1, 1), YES, 1.0);
    [rawImage drawAtPoint:CGPointZero];
    UIGraphicsEndImageContext();

    self.image = rawImage;

    if (self.resultHandler && !self.isCancelled) {
        dispatch_async(dispatch_get_main_queue(), ^{
            self.resultHandler(self.image);
        });
    }
}

@end
