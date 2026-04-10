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
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(45, 120, 45, 45)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(45, 200, 45, 45)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(5, 160, 45, 45)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(85, 160, 45, 45)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(280, 140, 75, 75)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(200, 170, 75, 75)];
        // Shoulders
        rects[@(EMULATOR_KEY_L).stringValue] = [NSValue valueWithCGRect:CGRectMake(0, 0, 110, 45)];
        rects[@(EMULATOR_KEY_R).stringValue] = [NSValue valueWithCGRect:CGRectMake(265, 0, 110, 45)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(195, 380, 70, 25)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(110, 380, 70, 25)];
    } else {
        skin.name = @"NES Default";
        // D-PAD
        rects[@(EMULATOR_KEY_UP).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 130, 40, 40)];
        rects[@(EMULATOR_KEY_DOWN).stringValue] = [NSValue valueWithCGRect:CGRectMake(60, 210, 40, 40)];
        rects[@(EMULATOR_KEY_LEFT).stringValue] = [NSValue valueWithCGRect:CGRectMake(20, 170, 40, 40)];
        rects[@(EMULATOR_KEY_RIGHT).stringValue] = [NSValue valueWithCGRect:CGRectMake(100, 170, 40, 40)];
        // Buttons
        rects[@(EMULATOR_KEY_A).stringValue] = [NSValue valueWithCGRect:CGRectMake(300, 170, 60, 60)];
        rects[@(EMULATOR_KEY_B).stringValue] = [NSValue valueWithCGRect:CGRectMake(225, 170, 60, 60)];
        // System
        rects[@(EMULATOR_KEY_START).stringValue] = [NSValue valueWithCGRect:CGRectMake(210, 350, 55, 20)];
        rects[@(EMULATOR_KEY_SELECT).stringValue] = [NSValue valueWithCGRect:CGRectMake(110, 350, 55, 20)];
    }

    skin.buttonRects = rects;
    return skin;
}

@end
