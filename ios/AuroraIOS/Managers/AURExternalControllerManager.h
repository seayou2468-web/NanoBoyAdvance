#import <Foundation/Foundation.h>
#import <GameController/GameController.h>
#include "../../../src/core/api/emulator_core_c_api.h"

@protocol AURExternalControllerDelegate <NSObject>
- (void)externalControllerDidPressKey:(EmulatorKey)key;
- (void)externalControllerDidReleaseKey:(EmulatorKey)key;
@end

@interface AURExternalControllerManager : NSObject
@property (nonatomic, weak) id<AURExternalControllerDelegate> delegate;
+ (instancetype)sharedManager;
- (void)startMonitoring;
@end
