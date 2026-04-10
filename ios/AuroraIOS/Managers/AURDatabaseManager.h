#import <Foundation/Foundation.h>
#import "../Models/AURGame.h"

@interface AURDatabaseManager : NSObject
+ (instancetype)sharedManager;
- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType;
- (void)addGame:(AURGame *)game;
@end
