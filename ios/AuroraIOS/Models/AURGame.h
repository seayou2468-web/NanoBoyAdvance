#import <Foundation/Foundation.h>
#include "../../../src/core/api/emulator_core_c_api.h"

@interface AURGame : NSObject <NSSecureCoding>
@property (nonatomic, copy) NSString *title;
@property (nonatomic, copy) NSString *romPath;
@property (nonatomic, assign) EmulatorCoreType coreType;
@property (nonatomic, copy) NSString *boxArtPath;
@end
