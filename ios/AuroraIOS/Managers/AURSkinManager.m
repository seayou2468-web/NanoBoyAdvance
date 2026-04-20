#import "AURSkinManager.h"
#import <zlib.h>

@interface AURSkinManager ()
@property (nonatomic, strong) NSMutableArray *importedSkins;
@end

@implementation AURSkinManager

static uint16_t AURReadU16LE(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t AURReadU32LE(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static NSData *AURInflateRawDeflate(NSData *compressed, NSUInteger expectedSize) {
    if (compressed.length == 0) return [NSData data];

    z_stream stream = {0};
    if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
        return nil;
    }

    NSMutableData *output = [NSMutableData dataWithLength:MAX(expectedSize, compressed.length * 3)];
    stream.next_in = (Bytef *)compressed.bytes;
    stream.avail_in = (uInt)compressed.length;

    int status = Z_OK;
    while (status == Z_OK) {
        if (stream.total_out >= output.length) {
            [output increaseLengthBy:MAX(compressed.length, 1024)];
        }
        stream.next_out = (Bytef *)output.mutableBytes + stream.total_out;
        stream.avail_out = (uInt)(output.length - stream.total_out);
        status = inflate(&stream, Z_SYNC_FLUSH);
    }

    NSData *result = nil;
    if (status == Z_STREAM_END) {
        [output setLength:stream.total_out];
        result = output;
    }
    inflateEnd(&stream);
    return result;
}

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
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        NSString *tempDir = [NSTemporaryDirectory() stringByAppendingPathComponent:[[NSUUID UUID] UUIDString]];
        [[NSFileManager defaultManager] createDirectoryAtPath:tempDir withIntermediateDirectories:YES attributes:nil error:nil];

        // Use built-in zlib + Foundation path handling to unzip .deltaskin (ZIP container)
        BOOL unzipSuccess = [self unzipFileAtPath:url.path toDirectory:tempDir];

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
    NSData *zipData = [NSData dataWithContentsOfFile:path];
    if (!zipData || zipData.length < 30) {
        return NO;
    }

    const uint8_t *bytes = (const uint8_t *)zipData.bytes;
    NSUInteger offset = 0;
    NSFileManager *fileManager = [NSFileManager defaultManager];

    while (offset + 30 <= zipData.length) {
        if (AURReadU32LE(bytes + offset) != 0x04034B50U) {
            break;
        }

        const uint16_t flags = AURReadU16LE(bytes + offset + 6);
        const uint16_t method = AURReadU16LE(bytes + offset + 8);
        const uint32_t compressedSize = AURReadU32LE(bytes + offset + 18);
        const uint32_t uncompressedSize = AURReadU32LE(bytes + offset + 22);
        const uint16_t nameLength = AURReadU16LE(bytes + offset + 26);
        const uint16_t extraLength = AURReadU16LE(bytes + offset + 28);

        if ((flags & 0x0008U) != 0U) {
            // Data descriptor format is intentionally not supported in this lightweight extractor.
            return NO;
        }

        NSUInteger headerSize = 30U + nameLength + extraLength;
        if (offset + headerSize > zipData.length) {
            return NO;
        }

        NSData *nameData = [NSData dataWithBytes:(bytes + offset + 30U) length:nameLength];
        NSString *entryName = [[NSString alloc] initWithData:nameData encoding:NSUTF8StringEncoding];
        if (entryName.length == 0 || [entryName hasPrefix:@"/"] || [entryName containsString:@".."]) {
            return NO;
        }

        NSUInteger dataOffset = offset + headerSize;
        NSUInteger dataEnd = dataOffset + compressedSize;
        if (dataEnd > zipData.length) {
            return NO;
        }

        NSString *destinationPath = [dest stringByAppendingPathComponent:entryName];
        BOOL isDirectory = [entryName hasSuffix:@"/"];
        if (isDirectory) {
            [fileManager createDirectoryAtPath:destinationPath withIntermediateDirectories:YES attributes:nil error:nil];
        } else {
            [fileManager createDirectoryAtPath:[destinationPath stringByDeletingLastPathComponent] withIntermediateDirectories:YES attributes:nil error:nil];
            NSData *compressed = [NSData dataWithBytes:(bytes + dataOffset) length:compressedSize];
            NSData *output = nil;

            if (method == 0) {
                output = compressed;
            } else if (method == 8) {
                output = AURInflateRawDeflate(compressed, uncompressedSize);
            } else {
                return NO;
            }

            if (!output || output.length != uncompressedSize) {
                return NO;
            }

            if (![output writeToFile:destinationPath atomically:YES]) {
                return NO;
            }
        }

        offset = dataEnd;
    }

    return offset > 0;
}

- (NSArray<AURControllerSkin *> *)allSkins {
    return self.importedSkins;
}

- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    AURControllerSkinTraits *traits = [[AURControllerSkinTraits alloc] init];
    traits.device = (UIDevice.currentDevice.userInterfaceIdiom == UIUserInterfaceIdiomPad) ? AURControllerSkinDeviceIPad : AURControllerSkinDeviceIPhone;
    traits.displayType = AURControllerSkinDisplayTypeStandard;
    traits.orientation = isLandscape ? AURControllerSkinOrientationLandscape : AURControllerSkinOrientationPortrait;

    for (AURControllerSkin *candidate in self.importedSkins) {
        if (![candidate supportsTraits:traits]) {
            continue;
        }
        if ([candidate isKindOfClass:[AURDeltaSkin class]]) {
            AURDeltaSkin *deltaSkin = (AURDeltaSkin *)candidate;
            if (![deltaSkin supportsCoreType:coreType]) {
                continue;
            }
            [deltaSkin applyLayoutForTraits:traits];
        }
        return candidate;
    }
    return [self defaultSkinForCore:coreType isLandscape:isLandscape];
}

- (AURControllerSkin *)defaultSkinForCore:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    (void)coreType;
    (void)isLandscape;

    AURControllerSkin *skin = [[AURControllerSkin alloc] init];
    skin.name = @"3DS Default";

    NSMutableDictionary *rects = [NSMutableDictionary dictionary];
    rects[@"up"] = [NSValue valueWithCGRect:CGRectMake(50, 120, 45, 45)];
    rects[@"down"] = [NSValue valueWithCGRect:CGRectMake(50, 200, 45, 45)];
    rects[@"left"] = [NSValue valueWithCGRect:CGRectMake(10, 160, 45, 45)];
    rects[@"right"] = [NSValue valueWithCGRect:CGRectMake(90, 160, 45, 45)];
    rects[@"a"] = [NSValue valueWithCGRect:CGRectMake(290, 160, 65, 65)];
    rects[@"b"] = [NSValue valueWithCGRect:CGRectMake(215, 160, 65, 65)];
    rects[@"start"] = [NSValue valueWithCGRect:CGRectMake(200, 340, 60, 20)];
    rects[@"select"] = [NSValue valueWithCGRect:CGRectMake(110, 340, 60, 20)];

    skin.buttonRects = rects;
    return skin;
}

@end
