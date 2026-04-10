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
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(40, 70, 50, 50)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(40, 170, 50, 50)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 120, 50, 50)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(80, 120, 50, 50)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(285, 90, 80, 80)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(200, 130, 80, 80)];
        // Shoulders
        rects[@(EMULATOR_KEY_L).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 0, 120, 50)];
        rects[@(EMULATOR_KEY_R).stringValue] = [NSValue valueWithCGRect:CGRectMake(255, 0, 120, 50)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(200, 350, 80, 35)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(95, 350, 80, 35)];
    } else {
        skin.name = @"NES Default";
        // D-PAD
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 100, 45, 45)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 200, 45, 45)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(10, 150, 45, 45)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(110, 150, 45, 45)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(300, 150, 65, 65)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(220, 150, 65, 65)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(210, 300, 60, 25)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(105, 300, 60, 25)];
    }

    skin.buttonRects = rects;
    return skin;
}

@end
