#import "AURSkinManager.h"

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

- (void)loadSkinsFromDisk {
    // Logic to list folders in Documents/Skins and parse info.json
}

- (void)importSkinAtURL:(NSURL *)url completion:(void(^)(BOOL success))completion {
    // 1. Create Documents/Skins/skin_name folder
    // 2. Unzip .deltaskin (which is a ZIP) to that folder
    // 3. Parse info.json and add to importedSkins
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Simplified mock: just check if it is a deltaskin
        BOOL success = [url.pathExtension.lowercaseString isEqualToString:@"deltaskin"];
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(success);
        });
    });
}

- (NSArray<AURControllerSkin *> *)allSkins {
    return self.importedSkins;
}

- (AURControllerSkin *)skinForCoreType:(EmulatorCoreType)coreType isLandscape:(BOOL)isLandscape {
    // Check imported skins first, otherwise return default programmatic skin
    // [Implementation same as before for defaults...]
    return [self defaultSkinForCore:coreType];
}

- (AURControllerSkin *)defaultSkinForCore:(EmulatorCoreType)coreType {
    AURControllerSkin *skin = [[AURControllerSkin alloc] init];
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
    }
    skin.buttonRects = rects;
    return skin;
}

@end
