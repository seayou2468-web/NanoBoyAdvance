#import <Foundation/Foundation.h>
#import "../Models/AURGame.h"

@interface AURDatabaseManager : NSObject
+ (instancetype)sharedManager;
- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType;
- (void)addGame:(AURGame *)game;
- (void)setBIOSPath:(NSString *)path forCoreType:(EmulatorCoreType)coreType;
- (NSString *)BIOSPathForCoreType:(EmulatorCoreType)coreType;
@end
