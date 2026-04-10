#import <UIKit/UIKit.h>
#include "../../../src/core/emulator_core_c_api.h"

@interface AUREmulatorViewController : UIViewController
- (instancetype)initWithROMURL:(NSURL *)romURL coreType:(EmulatorCoreType)coreType;
@end
