#import <Foundation/Foundation.h>
#import "../Models/AURControllerSkin.h"
#include "../../../src/core/emulator_core_c_api.h"

@interface AURSkinManager : NSObject
+ (instancetype)sharedManager;
- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape;
@end
