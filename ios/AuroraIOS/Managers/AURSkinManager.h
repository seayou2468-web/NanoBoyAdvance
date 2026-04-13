#import <Foundation/Foundation.h>
#import "../Models/AURDeltaSkin.h"
#include "../../../src/core/api/emulator_core_c_api.h"

@interface AURSkinManager : NSObject
+ (instancetype)sharedManager;
- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape;
- (void)importSkinAtURL:(NSURL *)url completion:(void(^)(BOOL success))completion;
- (NSArray<AURControllerSkin *> *)allSkins;
@end
