#import "AURDatabaseManager.h"

@interface AURDatabaseManager ()
@property (nonatomic, strong) NSMutableArray<AURGame *> *allGames;
@end

@implementation AURDatabaseManager

+ (instancetype)sharedManager {
    static AURDatabaseManager *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _allGames = [NSMutableArray array];
        [self loadDatabase];
    }
    return self;
}

- (void)loadDatabase {
    // Basic persistence placeholder
}

- (void)saveDatabase {
    // Basic persistence placeholder
}

- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType {
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"coreType == %ld", (long)coreType];
    return [self.allGames filteredArrayUsingPredicate:predicate];
}

- (void)addGame:(AURGame *)game {
    [self.allGames addObject:game];
    [self saveDatabase];
}

@end
