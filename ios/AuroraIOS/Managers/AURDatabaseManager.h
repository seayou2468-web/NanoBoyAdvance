#import <Foundation/Foundation.h>
#import "../Models/AURGame.h"

@interface AURDatabaseManager : NSObject
+ (instancetype)sharedManager;
- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType;
- (void)addGame:(AURGame *)game;
- (void)setBIOSPath:(NSString *)path forCoreType:(EmulatorCoreType)coreType;
- (void)setBIOSURL:(NSURL *)url forCoreType:(EmulatorCoreType)coreType;
- (NSString *)BIOSPathForCoreType:(EmulatorCoreType)coreType;
- (void)setBIOSPath:(NSString *)path forIdentifier:(NSString *)identifier;
- (void)setBIOSURL:(NSURL *)url forIdentifier:(NSString *)identifier;
- (NSString *)BIOSPathForIdentifier:(NSString *)identifier;
@end
