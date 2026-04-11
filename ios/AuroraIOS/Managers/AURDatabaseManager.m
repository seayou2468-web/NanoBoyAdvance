#import "AURDatabaseManager.h"

@interface AURDatabaseManager ()
@property (nonatomic, strong) NSMutableArray<AURGame *> *allGames;
@property (nonatomic, strong) NSMutableDictionary<NSNumber *, NSString *> *biosPaths;
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
        _biosPaths = [NSMutableDictionary dictionary];
        [self loadDatabase];
    }
    return self;
}

- (void)loadDatabase {
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *dbPath = [docs stringByAppendingPathComponent:@"aurora_database.plist"];
    NSDictionary *payload = [NSDictionary dictionaryWithContentsOfFile:dbPath];
    if (![payload isKindOfClass:[NSDictionary class]]) {
        return;
    }

    NSArray *encodedGames = payload[@"games"];
    if ([encodedGames isKindOfClass:[NSArray class]]) {
        for (NSData *data in encodedGames) {
            if (![data isKindOfClass:[NSData class]]) {
                continue;
            }
            AURGame *game = [NSKeyedUnarchiver unarchivedObjectOfClass:[AURGame class] fromData:data error:nil];
            if (game) {
                [self.allGames addObject:game];
            }
        }
    }

    NSDictionary *bios = payload[@"biosPaths"];
    if ([bios isKindOfClass:[NSDictionary class]]) {
        for (id key in bios) {
            NSString *value = bios[key];
            if ([key isKindOfClass:[NSNumber class]] && [value isKindOfClass:[NSString class]]) {
                self.biosPaths[key] = value;
            }
        }
    }
}

- (void)saveDatabase {
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *dbPath = [docs stringByAppendingPathComponent:@"aurora_database.plist"];

    NSMutableArray<NSData *> *encodedGames = [NSMutableArray arrayWithCapacity:self.allGames.count];
    for (AURGame *game in self.allGames) {
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:game requiringSecureCoding:YES error:nil];
        if (data) {
            [encodedGames addObject:data];
        }
    }

    NSDictionary *payload = @{
        @"games": encodedGames,
        @"biosPaths": self.biosPaths ?: @{}
    };
    [payload writeToFile:dbPath atomically:YES];
}

- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType {
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"coreType == %ld", (long)coreType];
    return [self.allGames filteredArrayUsingPredicate:predicate];
}

- (void)addGame:(AURGame *)game {
    [self.allGames addObject:game];
    [self saveDatabase];
}

- (void)setBIOSPath:(NSString *)path forCoreType:(EmulatorCoreType)coreType {
    self.biosPaths[@(coreType)] = path;
    [self saveDatabase];
}

- (NSString *)BIOSPathForCoreType:(EmulatorCoreType)coreType {
    return self.biosPaths[@(coreType)];
}

- (NSNumber *)biosStorageKeyForIdentifier:(NSString *)identifier {
    if ([identifier isEqualToString:@"nds_arm9"]) return @1001;
    if ([identifier isEqualToString:@"nds_arm7"]) return @1002;
    if ([identifier isEqualToString:@"nds_firmware"]) return @1003;
    if ([identifier isEqualToString:@"gba"]) return @(EMULATOR_CORE_TYPE_GBA);
    if ([identifier isEqualToString:@"gb"]) return @(EMULATOR_CORE_TYPE_GB);
    return nil;
}

- (void)setBIOSPath:(NSString *)path forIdentifier:(NSString *)identifier {
    NSNumber *key = [self biosStorageKeyForIdentifier:identifier];
    if (!key) return;
    self.biosPaths[key] = path;
    [self saveDatabase];
}

- (NSString *)BIOSPathForIdentifier:(NSString *)identifier {
    NSNumber *key = [self biosStorageKeyForIdentifier:identifier];
    if (!key) return nil;
    return self.biosPaths[key];
}

@end
