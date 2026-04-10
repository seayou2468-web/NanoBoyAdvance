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
    NSMutableDictionary *rects = [NSMutableDictionary dictionary];

    // Normalized coordinates based on a 375pt wide reference
    if (coreType == EMULATOR_CORE_TYPE_GBA) {
        skin.name = @"GBA Default";
        // D-PAD
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(40, 50, 45, 45)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(40, 130, 45, 45)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 90, 45, 45)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(80, 90, 45, 45)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(290, 70, 70, 70)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(220, 110, 70, 70)];
        // Shoulders
        rects[@(EMULATOR_KEY_L).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 0, 100, 40)];
        rects[@(EMULATOR_KEY_R).stringValue] = [NSValue valueWithCGRect:CGRectMake(275, 0, 100, 40)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(200, 250, 60, 30)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(115, 250, 60, 30)];
    } else {
        skin.name = @"NES Default";
        // D-PAD
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 80, 40, 40)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 160, 40, 40)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(20, 120, 40, 40)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(100, 120, 40, 40)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(300, 120, 55, 55)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(235, 120, 55, 55)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(210, 220, 50, 25)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(120, 220, 50, 25)];
    }

    skin.buttonRects = rects;
    return skin;
}

@end
