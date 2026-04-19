#import "AURDeltaSkin.h"

@interface AURDeltaSkin ()
@property (nonatomic, copy) NSString *basePath;
@property (nonatomic, strong) NSDictionary<NSString *, NSDictionary *> *layoutByTraitKey;
@property (nonatomic, strong, readwrite) NSSet<NSNumber *> *supportedCoreTypes;
@end

@implementation AURDeltaSkin

static NSString *AURTraitKey(AURControllerSkinDevice device, AURControllerSkinDisplayType displayType, AURControllerSkinOrientation orientation) {
    return [NSString stringWithFormat:@"%ld|%ld|%ld", (long)device, (long)displayType, (long)orientation];
}

static NSString *AURCanonicalInputName(NSString *value) {
    if (value.length == 0) return nil;
    NSString *normalized = [[value lowercaseString] stringByReplacingOccurrencesOfString:@"_" withString:@""];
    normalized = [normalized stringByReplacingOccurrencesOfString:@"-" withString:@""];

    static NSDictionary<NSString *, NSString *> *map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
            @"a": @"a", @"buttona": @"a", @"primary": @"a",
            @"b": @"b", @"buttonb": @"b", @"secondary": @"b",
            @"x": @"a", @"y": @"b",
            @"up": @"up", @"dpadup": @"up",
            @"down": @"down", @"dpaddown": @"down",
            @"left": @"left", @"dpadleft": @"left",
            @"right": @"right", @"dpadright": @"right",
            @"l": @"l", @"leftshoulder": @"l", @"l1": @"l",
            @"r": @"r", @"rightshoulder": @"r", @"r1": @"r",
            @"start": @"start", @"menu": @"start", @"plus": @"start",
            @"select": @"select", @"minus": @"select",
        };
    });
    return map[normalized];
}

static BOOL AURParseFrame(id rawFrame, CGRect *outFrame) {
    if (!outFrame) return NO;
    if ([rawFrame isKindOfClass:[NSDictionary class]]) {
        NSDictionary *dict = rawFrame;
        CGFloat x = [dict[@"x"] doubleValue];
        CGFloat y = [dict[@"y"] doubleValue];
        CGFloat w = [dict[@"width"] doubleValue];
        CGFloat h = [dict[@"height"] doubleValue];
        if (w > 0 && h > 0) {
            *outFrame = CGRectMake(x, y, w, h);
            return YES;
        }
    } else if ([rawFrame isKindOfClass:[NSArray class]]) {
        NSArray *arr = rawFrame;
        if (arr.count >= 4) {
            CGFloat x = [arr[0] doubleValue];
            CGFloat y = [arr[1] doubleValue];
            CGFloat w = [arr[2] doubleValue];
            CGFloat h = [arr[3] doubleValue];
            if (w > 0 && h > 0) {
                *outFrame = CGRectMake(x, y, w, h);
                return YES;
            }
        }
    }
    return NO;
}

static AURControllerSkinConfigurations AURConfigurationForTraits(AURControllerSkinDevice device,
                                                                 AURControllerSkinDisplayType display,
                                                                 AURControllerSkinOrientation orientation) {
    switch (device) {
        case AURControllerSkinDeviceIPhone:
            if (display == AURControllerSkinDisplayTypeEdgeToEdge) {
                return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPhoneEdgeToEdgeLandscape : AURControllerSkinConfigurationIPhoneEdgeToEdgePortrait;
            }
            return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPhoneStandardLandscape : AURControllerSkinConfigurationIPhoneStandardPortrait;
        case AURControllerSkinDeviceIPad:
            if (display == AURControllerSkinDisplayTypeEdgeToEdge) {
                return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPadEdgeToEdgeLandscape : AURControllerSkinConfigurationIPadEdgeToEdgePortrait;
            }
            if (display == AURControllerSkinDisplayTypeSplitView) {
                return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPadSplitViewLandscape : AURControllerSkinConfigurationIPadSplitViewPortrait;
            }
            return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationIPadStandardLandscape : AURControllerSkinConfigurationIPadStandardPortrait;
        case AURControllerSkinDeviceTV:
            return (orientation == AURControllerSkinOrientationLandscape) ? AURControllerSkinConfigurationTVStandardLandscape : AURControllerSkinConfigurationTVStandardPortrait;
    }
    return 0;
}

static NSArray<NSString *> *AURStringArrayFromInputs(id rawInputs) {
    if (![rawInputs isKindOfClass:[NSArray class]]) return @[];
    NSMutableArray<NSString *> *values = [NSMutableArray array];
    for (id entry in (NSArray *)rawInputs) {
        if ([entry isKindOfClass:[NSString class]]) {
            [values addObject:entry];
        } else if ([entry isKindOfClass:[NSDictionary class]]) {
            NSDictionary *dict = entry;
            NSString *name = dict[@"input"] ?: dict[@"name"] ?: dict[@"kind"];
            if ([name isKindOfClass:[NSString class]]) {
                [values addObject:name];
            }
        }
    }
    return values;
}

static NSDictionary *AURBuildLayoutDictionary(NSDictionary *layout, NSString *basePath) {
    if (![layout isKindOfClass:[NSDictionary class]]) return nil;

    NSString *backgroundName = nil;
    id assets = layout[@"assets"];
    if ([assets isKindOfClass:[NSDictionary class]]) {
        id rawBackground = assets[@"background"] ?: assets[@"controller"];
        if ([rawBackground isKindOfClass:[NSString class]]) {
            backgroundName = rawBackground;
        } else if ([rawBackground isKindOfClass:[NSDictionary class]]) {
            for (id value in [(NSDictionary *)rawBackground allValues]) {
                if ([value isKindOfClass:[NSString class]]) {
                    backgroundName = value;
                    break;
                }
            }
        }
    }

    NSMutableDictionary<NSString *, NSValue *> *buttonRects = [NSMutableDictionary dictionary];
    NSArray *items = [layout[@"items"] isKindOfClass:[NSArray class]] ? layout[@"items"] : @[];
    for (NSDictionary *item in items) {
        if (![item isKindOfClass:[NSDictionary class]]) continue;
        CGRect frame = CGRectZero;
        if (!AURParseFrame(item[@"frame"], &frame)) continue;
        for (NSString *input in AURStringArrayFromInputs(item[@"inputs"])) {
            NSString *canonical = AURCanonicalInputName(input);
            if (canonical && buttonRects[canonical] == nil) {
                buttonRects[canonical] = [NSValue valueWithCGRect:frame];
            }
        }
    }

    NSString *backgroundPath = nil;
    if (backgroundName.length > 0) {
        backgroundPath = [basePath stringByAppendingPathComponent:backgroundName];
    }
    return @{
        @"backgroundPath": backgroundPath ?: @"",
        @"buttonRects": buttonRects
    };
}

static NSSet<NSNumber *> *AURCoreTypesFromMetadata(NSDictionary *json) {
    NSMutableSet<NSNumber *> *types = [NSMutableSet set];

    NSString *gameType = [json[@"gameType"] isKindOfClass:[NSString class]] ? [json[@"gameType"] lowercaseString] : nil;
    NSDictionary<NSString *, NSNumber *> *map = @{
        @"3ds": @(EMULATOR_CORE_TYPE_3DS)
    };
    NSNumber *single = map[gameType ?: @""];
    if (single) [types addObject:single];

    NSArray *systems = [json[@"systems"] isKindOfClass:[NSArray class]] ? json[@"systems"] : nil;
    for (id system in systems) {
        if ([system isKindOfClass:[NSString class]]) {
            NSNumber *value = map[[((NSString *)system) lowercaseString]];
            if (value) [types addObject:value];
        }
    }

    if (types.count == 0) {
        [types addObject:@(EMULATOR_CORE_TYPE_3DS)];
    }
    return types;
}

+ (instancetype)skinWithJSONData:(NSData *)data folderPath:(NSString *)path {
    NSError *error = nil;
    NSDictionary *json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (error || ![json isKindOfClass:[NSDictionary class]]) return nil;

    AURDeltaSkin *skin = [[AURDeltaSkin alloc] init];
    skin.name = [json[@"name"] isKindOfClass:[NSString class]] ? json[@"name"] : @"Delta Skin";
    skin.identifier = [json[@"identifier"] isKindOfClass:[NSString class]] ? json[@"identifier"] : [NSUUID UUID].UUIDString;
    skin.basePath = path;
    skin.supportedCoreTypes = AURCoreTypesFromMetadata(json);

    NSMutableDictionary<NSString *, NSDictionary *> *layoutByTraitKey = [NSMutableDictionary dictionary];
    __block AURControllerSkinConfigurations supportedConfigurations = 0;

    NSDictionary *representations = [json[@"representations"] isKindOfClass:[NSDictionary class]] ? json[@"representations"] : nil;
    if (representations.count > 0) {
        NSDictionary<NSString *, NSNumber *> *deviceMap = @{@"iphone": @(AURControllerSkinDeviceIPhone), @"ipad": @(AURControllerSkinDeviceIPad), @"tv": @(AURControllerSkinDeviceTV)};
        NSDictionary<NSString *, NSNumber *> *displayMap = @{@"standard": @(AURControllerSkinDisplayTypeStandard), @"edgetoedge": @(AURControllerSkinDisplayTypeEdgeToEdge), @"splitview": @(AURControllerSkinDisplayTypeSplitView)};
        NSDictionary<NSString *, NSNumber *> *orientationMap = @{@"portrait": @(AURControllerSkinOrientationPortrait), @"landscape": @(AURControllerSkinOrientationLandscape)};

        [representations enumerateKeysAndObjectsUsingBlock:^(NSString *deviceName, NSDictionary *displayContainer, BOOL *stop) {
            NSNumber *deviceNumber = deviceMap[[deviceName lowercaseString]];
            if (!deviceNumber || ![displayContainer isKindOfClass:[NSDictionary class]]) return;
            AURControllerSkinDevice device = (AURControllerSkinDevice)deviceNumber.integerValue;

            [displayContainer enumerateKeysAndObjectsUsingBlock:^(NSString *displayName, NSDictionary *orientationContainer, BOOL *stop2) {
                NSNumber *displayNumber = displayMap[[displayName lowercaseString]];
                if (!displayNumber || ![orientationContainer isKindOfClass:[NSDictionary class]]) return;
                AURControllerSkinDisplayType displayType = (AURControllerSkinDisplayType)displayNumber.integerValue;

                [orientationContainer enumerateKeysAndObjectsUsingBlock:^(NSString *orientationName, NSDictionary *layout, BOOL *stop3) {
                    NSNumber *orientationNumber = orientationMap[[orientationName lowercaseString]];
                    if (!orientationNumber) return;
                    AURControllerSkinOrientation orientation = (AURControllerSkinOrientation)orientationNumber.integerValue;
                    NSDictionary *layoutData = AURBuildLayoutDictionary(layout, path);
                    if (!layoutData) return;
                    NSString *key = AURTraitKey(device, displayType, orientation);
                    layoutByTraitKey[key] = layoutData;
                    supportedConfigurations |= AURConfigurationForTraits(device, displayType, orientation);
                }];
            }];
        }];
    }

    if (layoutByTraitKey.count == 0) {
        NSDictionary *layouts = [json[@"layouts"] isKindOfClass:[NSDictionary class]] ? json[@"layouts"] : nil;
        for (NSString *orientationName in @[@"portrait", @"landscape"]) {
            NSDictionary *layout = [layouts[orientationName] isKindOfClass:[NSDictionary class]] ? layouts[orientationName] : nil;
            NSDictionary *layoutData = AURBuildLayoutDictionary(layout, path);
            if (!layoutData) continue;
            AURControllerSkinOrientation orientation = [orientationName isEqualToString:@"landscape"] ? AURControllerSkinOrientationLandscape : AURControllerSkinOrientationPortrait;
            NSString *key = AURTraitKey(AURControllerSkinDeviceIPhone, AURControllerSkinDisplayTypeStandard, orientation);
            layoutByTraitKey[key] = layoutData;
            supportedConfigurations |= AURConfigurationForTraits(AURControllerSkinDeviceIPhone, AURControllerSkinDisplayTypeStandard, orientation);
        }
    }

    skin.layoutByTraitKey = layoutByTraitKey;
    skin.supportedConfigurations = supportedConfigurations;
    skin.isStandard = NO;

    AURControllerSkinTraits *defaultTraits = [[AURControllerSkinTraits alloc] init];
    defaultTraits.device = AURControllerSkinDeviceIPhone;
    defaultTraits.displayType = AURControllerSkinDisplayTypeStandard;
    defaultTraits.orientation = AURControllerSkinOrientationPortrait;
    [skin applyLayoutForTraits:defaultTraits];
    return skin;
}

- (NSDictionary *)layoutForTraits:(AURControllerSkinTraits *)traits {
    NSString *fullKey = AURTraitKey(traits.device, traits.displayType, traits.orientation);
    NSDictionary *layout = self.layoutByTraitKey[fullKey];
    if (layout) return layout;
    NSString *fallbackKey = AURTraitKey(traits.device, AURControllerSkinDisplayTypeStandard, traits.orientation);
    layout = self.layoutByTraitKey[fallbackKey];
    if (layout) return layout;
    return self.layoutByTraitKey.allValues.firstObject;
}

- (void)applyLayoutForTraits:(AURControllerSkinTraits *)traits {
    NSDictionary *layout = [self layoutForTraits:traits];
    if (!layout) return;
    NSString *backgroundPath = [layout[@"backgroundPath"] isKindOfClass:[NSString class]] ? layout[@"backgroundPath"] : nil;
    if (backgroundPath.length > 0) {
        self.backgroundImage = [UIImage imageWithContentsOfFile:backgroundPath];
    }
    NSDictionary *rects = [layout[@"buttonRects"] isKindOfClass:[NSDictionary class]] ? layout[@"buttonRects"] : nil;
    if (rects) {
        self.buttonRects = rects;
    }
}

- (BOOL)supportsCoreType:(EmulatorCoreType)coreType {
    return [self.supportedCoreTypes containsObject:@(coreType)];
}

- (BOOL)supportsTraits:(AURControllerSkinTraits *)traits {
    if (![super supportsTraits:traits]) {
        return NO;
    }
    return [self layoutForTraits:traits] != nil;
}

@end
