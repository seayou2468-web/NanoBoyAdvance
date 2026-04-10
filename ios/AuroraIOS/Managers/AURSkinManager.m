#import "AURSkinManager.h"

@implementation AURSkinManager

+ (instancetype)sharedManager {
    static AURSkinManager *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    AURControllerSkin *skin = [[AURControllerSkin alloc] init];

    // Logic to define button rects based on the images provided (mocking layout for now)
    // In a real implementation, these would match the pixel coordinates of the skin PNGs.

    NSMutableDictionary *rects = [NSMutableDictionary dictionary];

    if (coreType == EMULATOR_CORE_TYPE_GBA) {
        skin.name = @"GBA Default";
        // Mock GBA layout
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(50, 450, 50, 50)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(50, 550, 50, 50)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 500, 50, 50)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(100, 500, 50, 50)];
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(300, 500, 60, 60)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(230, 530, 60, 60)];
        rects[@(EMULATOR_KEY_L).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 400, 80, 40)];
        rects[@(EMULATOR_KEY_R).stringValue] = [NSValue valueWithCGRect:CGRectMake(280, 400, 80, 40)];
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(200, 650, 60, 30)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(100, 650, 60, 30)];
    } else if (coreType == EMULATOR_CORE_TYPE_NES) {
        skin.name = @"NES Default";
        // Mock NES layout
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 460, 40, 40)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 540, 40, 40)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(20, 500, 40, 40)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(100, 500, 40, 40)];
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(300, 500, 50, 50)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(240, 500, 50, 50)];
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(210, 600, 50, 20)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(130, 600, 50, 20)];
    }

    skin.buttonRects = rects;
    return skin;
}

@end
