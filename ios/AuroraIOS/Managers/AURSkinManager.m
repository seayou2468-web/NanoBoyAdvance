#import "AURSkinManager.h"
#import "../External/miniz/miniz.h"

@interface AURSkinManager ()
@property (nonatomic, strong) NSMutableArray *importedSkins;
@end

@implementation AURSkinManager

+ (instancetype)sharedManager {
    static AURSkinManager *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        _importedSkins = [NSMutableArray array];
        [self loadSkinsFromDisk];
    }
    return self;
}

- (NSString *)skinsDirectoryPath {
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *path = [docs stringByAppendingPathComponent:@"Skins"];
    [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:nil];
    return path;
}

- (void)loadSkinsFromDisk {
    NSString *dir = [self skinsDirectoryPath];
    NSArray *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil];
    for (NSString *item in contents) {
        NSString *itemPath = [dir stringByAppendingPathComponent:item];
        BOOL isDir = NO;
        if ([[NSFileManager defaultManager] fileExistsAtPath:itemPath isDirectory:&isDir] && isDir) {
            NSString *jsonPath = [itemPath stringByAppendingPathComponent:@"info.json"];
            NSData *data = [NSData dataWithContentsOfFile:jsonPath];
            if (data) {
                AURDeltaSkin *skin = [AURDeltaSkin skinWithJSONData:data folderPath:itemPath];
                if (skin) [self.importedSkins addObject:skin];
            }
        }
    }
}

- (void)importSkinAtURL:(NSURL *)url completion:(void(^)(BOOL success))completion {
    BOOL accessing = [url startAccessingSecurityScopedResource];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString *tempDir = [NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
        [[NSFileManager defaultManager] createDirectoryAtPath:tempDir withIntermediateDirectories:YES attributes:nil error:nil];

        BOOL unzipSuccess = [self unzipFileAtPath:url.path toDirectory:tempDir];

        if (accessing) {
            [url stopAccessingSecurityScopedResource];
        }

        if (unzipSuccess) {
            NSString *jsonPath = [tempDir stringByAppendingPathComponent:@"info.json"];
            NSData *jsonData = [NSData dataWithContentsOfFile:jsonPath];
            if (jsonData) {
                NSDictionary *json = [NSJSONSerialization JSONObjectWithData:jsonData options:0 error:nil];
                NSString *identifier = json[@"identifier"] ?: [[NSUUID UUID] UUIDString];
                NSString *destPath = [[self skinsDirectoryPath] stringByAppendingPathComponent:identifier];

                [[NSFileManager defaultManager] removeItemAtPath:destPath error:nil];
                [[NSFileManager defaultManager] moveItemAtPath:tempDir toPath:destPath error:nil];

                AURDeltaSkin *skin = [AURDeltaSkin skinWithJSONData:jsonData folderPath:destPath];
                if (skin) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self.importedSkins addObject:skin];
                        completion(YES);
                    });
                    return;
                }
            }
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            completion(NO);
        });
    });
}

- (BOOL)unzipFileAtPath:(NSString *)path toDirectory:(NSString *)dest {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, [path fileSystemRepresentation], 0)) {
        return NO;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    for (mz_uint i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            continue;
        }

        NSString *fileName = [NSString stringWithUTF8String:file_stat.m_filename];
        NSString *filePath = [dest stringByAppendingPathComponent:fileName];

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) {
            [[NSFileManager defaultManager] createDirectoryAtPath:filePath withIntermediateDirectories:YES attributes:nil error:nil];
        } else {
            [[NSFileManager defaultManager] createDirectoryAtPath:[filePath stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
            if (!mz_zip_reader_extract_to_file(&zip_archive, i, [filePath fileSystemRepresentation], 0)) {
                mz_zip_reader_end(&zip_archive);
                return NO;
            }
        }
    }

    mz_zip_reader_end(&zip_archive);
    return YES;
}

- (NSArray<AURControllerSkin *> *)allSkins {
    return self.importedSkins;
}

- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    NSString *targetType = @"com.rileytestut.delta.game.gba";
    if (coreType == EMULATOR_CORE_TYPE_NES) targetType = @"com.rileytestut.delta.game.nes";
    else if (coreType == EMULATOR_CORE_TYPE_GB) targetType = @"com.rileytestut.delta.game.gbc";

    for (AURDeltaSkin *skin in self.importedSkins) {
        if ([skin.gameTypeIdentifier isEqualToString:targetType]) {
            return skin;
        }
    }
    return [self defaultSkinForCore:coreType isLandscape:isLandscape];
}

- (AURControllerSkin *)defaultSkinForCore:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    AURControllerSkin *skin = [[AURControllerSkin alloc] init];
    skin.isStandard = YES;
    NSMutableDictionary *rects = [NSMutableDictionary dictionary];

    if (coreType == EMULATOR_CORE_TYPE_GBA) {
        skin.name = @"GBA Default";
        rects[@"up"] = [NSValue valueWithCGRect:CGRectMake(45, 120, 45, 45)];
        rects[@"down"] = [NSValue valueWithCGRect:CGRectMake(45, 200, 45, 45)];
        rects[@"left"] = [NSValue valueWithCGRect:CGRectMake(5, 160, 45, 45)];
        rects[@"right"] = [NSValue valueWithCGRect:CGRectMake(85, 160, 45, 45)];
        rects[@"a"] = [NSValue valueWithCGRect:CGRectMake(280, 140, 75, 75)];
        rects[@"b"] = [NSValue valueWithCGRect:CGRectMake(200, 170, 75, 75)];
        rects[@"l"] = [NSValue valueWithCGRect:CGRectMake(0, 0, 110, 45)];
        rects[@"r"] = [NSValue valueWithCGRect:CGRectMake(265, 0, 110, 45)];
        rects[@"start"] = [NSValue valueWithCGRect:CGRectMake(195, 380, 70, 25)];
        rects[@"select"] = [NSValue valueWithCGRect:CGRectMake(110, 380, 70, 25)];
    } else {
        skin.name = @"NES Default";
        rects[@"up"] = [NSValue valueWithCGRect:CGRectMake(60, 130, 40, 40)];
        rects[@"down"] = [NSValue valueWithCGRect:CGRectMake(60, 210, 40, 40)];
        rects[@"left"] = [NSValue valueWithCGRect:CGRectMake(20, 170, 40, 40)];
        rects[@"right"] = [NSValue valueWithCGRect:CGRectMake(100, 170, 40, 40)];
        rects[@"a"] = [NSValue valueWithCGRect:CGRectMake(300, 170, 60, 60)];
        rects[@"b"] = [NSValue valueWithCGRect:CGRectMake(225, 170, 60, 60)];
        rects[@"start"] = [NSValue valueWithCGRect:CGRectMake(210, 350, 55, 20)];
        rects[@"select"] = [NSValue valueWithCGRect:CGRectMake(110, 350, 55, 20)];
    }

    skin.buttonRects = rects;
    return skin;
}

@end
