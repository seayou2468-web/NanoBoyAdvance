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
    if (!game || game.romPath.length == 0) {
        return;
    }

    // 同一ROMパスの重複登録を防ぐ
    NSUInteger existingIndex = [self.allGames indexOfObjectPassingTest:^BOOL(AURGame * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        return [obj.romPath isEqualToString:game.romPath];
    }];
    if (existingIndex != NSNotFound) {
        self.allGames[existingIndex] = game;
    } else {
        [self.allGames addObject:game];
    }
    [self saveDatabase];
}

- (void)removeGame:(AURGame *)game removeROMFile:(BOOL)removeROMFile {
    if (!game) {
        return;
    }

    NSString *romPath = game.romPath;
    [self.allGames removeObject:game];
    [self saveDatabase];

    if (!removeROMFile || romPath.length == 0) {
        return;
    }

    NSError *error = nil;
    if ([[NSFileManager defaultManager] fileExistsAtPath:romPath]) {
        [[NSFileManager defaultManager] removeItemAtPath:romPath error:&error];
        if (error) {
            NSLog(@"[AUR][DB] Failed to delete ROM file at %@: %@", romPath, error.localizedDescription);
        }
    }
}

- (void)setBIOSPath:(NSString *)path forIdentifier:(NSString *)identifier {
    NSNumber *key = [self biosStorageKeyForIdentifier:identifier];
    if (!key) return;
    if (path.length == 0) {
        [self.biosPaths removeObjectForKey:key.stringValue];
    } else {
        self.biosPaths[key.stringValue] = path;
    }
    [self saveDatabase];
}

- (NSNumber *)biosStorageKeyForIdentifier:(NSString *)identifier {
    if ([identifier isEqualToString:@"3ds_boot9"]) return @2001;
    if ([identifier isEqualToString:@"3ds_boot11"]) return @2002;
    if ([identifier isEqualToString:@"3ds_firmware"]) return @2003;
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
    if (coreType != EMULATOR_CORE_TYPE_3DS) {
        return;
    }
    [self setBIOSURL:url forIdentifier:@"3ds_boot9"];
}

- (void)setBIOSPath:(NSString *)path forCoreType:(EmulatorCoreType)coreType {
    if (coreType != EMULATOR_CORE_TYPE_3DS) {
        return;
    }
    [self setBIOSPath:path forIdentifier:@"3ds_boot9"];
}

- (NSString *)BIOSPathForCoreType:(EmulatorCoreType)coreType {
    if (coreType != EMULATOR_CORE_TYPE_3DS) {
        return nil;
    }
    return [self BIOSPathForIdentifier:@"3ds_boot9"];
}

@end
