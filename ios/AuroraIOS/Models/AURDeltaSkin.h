#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "AURControllerSkin.h"
#include "../../../../src/core/emulator_core_c_api.h"

@interface AURDeltaSkin : AURControllerSkin
@property (nonatomic, strong, readonly) NSSet<NSNumber *> *supportedCoreTypes;
+ (instancetype)skinWithJSONData:(NSData *)data folderPath:(NSString *)path;
- (BOOL)supportsCoreType:(EmulatorCoreType)coreType;
- (void)applyLayoutForTraits:(AURControllerSkinTraits *)traits;
@end
