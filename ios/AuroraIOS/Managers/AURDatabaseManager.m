#import "AURDatabaseManager.h"

@interface AURDatabaseManager ()
@property (nonatomic, strong) NSMutableArray<AURGame *> *allGames;
@property (nonatomic, strong) NSMutableDictionary<id, NSString *> *biosPaths; // Stores relative paths
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

- (NSString *)documentsDirectory {
    return NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
}

- (void)loadDatabase {
    NSString *dbPath = [[self documentsDirectory] stringByAppendingPathComponent:@"aurora_database.plist"];
    NSDictionary *payload = [NSDictionary dictionaryWithContentsOfFile:dbPath];
    if (![payload isKindOfClass:[NSDictionary class]]) {
        return;
    }

    NSArray *encodedGames = payload[@"games"];
    if ([encodedGames isKindOfClass:[NSArray class]]) {
        for (NSData *data in encodedGames) {
            AURGame *game = [NSKeyedUnarchiver unarchivedObjectOfClass:[AURGame class] fromData:data error:nil];
            if (game) [self.allGames addObject:game];
        }
    }

    NSDictionary *bios = payload[@"biosPaths"];
    if ([bios isKindOfClass:[NSDictionary class]]) {
        [self.biosPaths addEntriesFromDictionary:bios];
    }
}

- (void)saveDatabase {
    NSString *dbPath = [[self documentsDirectory] stringByAppendingPathComponent:@"aurora_database.plist"];

    NSMutableArray<NSData *> *encodedGames = [NSMutableArray arrayWithCapacity:self.allGames.count];
    for (AURGame *game in self.allGames) {
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:game requiringSecureCoding:YES error:nil];
        if (data) [encodedGames addObject:data];
    }

    NSDictionary *payload = @{
        @"games": encodedGames,
        @"biosPaths": self.biosPaths ?: @{}
    };
    [payload writeToFile:dbPath atomically:YES];
}

- (NSString *)persistBIOSFileAtURL:(NSURL *)url storageKey:(NSString *)storageKey {
    if (!url || !url.isFileURL) return nil;

    BOOL didAccess = [url startAccessingSecurityScopedResource];

    NSString *docs = [self documentsDirectory];
    NSString *biosDir = [docs stringByAppendingPathComponent:@"BIOS"];
    [[NSFileManager defaultManager] createDirectoryAtPath:biosDir withIntermediateDirectories:YES attributes:nil error:nil];

    NSString *fileName = [NSString stringWithFormat:@"%@.%@", storageKey, url.pathExtension];
    NSString *destPath = [biosDir stringByAppendingPathComponent:fileName];

    NSError *error = nil;
    if ([[NSFileManager defaultManager] fileExistsAtPath:destPath]) {
        [[NSFileManager defaultManager] removeItemAtPath:destPath error:nil];
    }

    [[NSFileManager defaultManager] copyItemAtURL:url toURL:[NSURL fileURLWithPath:destPath] error:&error];

    if (didAccess) [url stopAccessingSecurityScopedResource];

    if (error) {
        NSLog(@"[AUR][DB] BIOS Copy failed: %@", error);
        return nil;
    }

    return [@"BIOS" stringByAppendingPathComponent:fileName]; // Return relative path
}

- (NSArray<AURGame *> *)gamesForCoreType:(EmulatorCoreType)coreType {
    NSPredicate *predicate = [NSPredicate predicateWithFormat:@"coreType == %ld", (long)coreType];
    return [self.allGames filteredArrayUsingPredicate:predicate];
}

- (void)addGame:(AURGame *)game {
    [self.allGames addObject:game];
    [self saveDatabase];
}

- (NSNumber *)biosStorageKeyForIdentifier:(NSString *)identifier {
    if ([identifier isEqualToString:@"nds_arm9"]) return @1001;
    if ([identifier isEqualToString:@"nds_arm7"]) return @1002;
    if ([identifier isEqualToString:@"nds_firmware"]) return @1003;
    if ([identifier isEqualToString:@"gba"]) return @(EMULATOR_CORE_TYPE_GBA);
    if ([identifier isEqualToString:@"gb"]) return @1004; // Use 1004 for GB
    if ([identifier isEqualToString:@"gbc"]) return @1005; // Use 1005 for GBC
    return nil;
}

- (void)setBIOSURL:(NSURL *)url forIdentifier:(NSString *)identifier {
    NSNumber *key = [self biosStorageKeyForIdentifier:identifier];
    if (!key) return;
    NSString *relPath = [self persistBIOSFileAtURL:url storageKey:identifier];
    if (relPath) {
        self.biosPaths[key.stringValue] = relPath;
        [self saveDatabase];
    }
}

- (NSString *)BIOSPathForIdentifier:(NSString *)identifier {
    NSNumber *key = [self biosStorageKeyForIdentifier:identifier];
    if (!key) return nil;
    NSString *relPath = self.biosPaths[key.stringValue];
    if (!relPath) return nil;

    NSString *fullPath = [[self documentsDirectory] stringByAppendingPathComponent:relPath];
    return [[NSFileManager defaultManager] fileExistsAtPath:fullPath] ? fullPath : nil;
}

- (void)setBIOSURL:(NSURL *)url forCoreType:(EmulatorCoreType)coreType {
    // Legacy support
    NSString *identifier = (coreType == EMULATOR_CORE_TYPE_GBA) ? @"gba" : @"gb";
    [self setBIOSURL:url forIdentifier:identifier];
}

- (NSString *)BIOSPathForCoreType:(EmulatorCoreType)coreType {
    NSString *identifier = (coreType == EMULATOR_CORE_TYPE_GBA) ? @"gba" : @"gb";
    return [self BIOSPathForIdentifier:identifier];
}

@end
