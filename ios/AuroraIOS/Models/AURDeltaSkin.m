#import "AURDeltaSkin.h"

@implementation AURDeltaSkin

+ (instancetype)skinWithJSONData:(NSData *)data folderPath:(NSString *)path {
    NSError *error = nil;
    NSDictionary *json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&error];
    if (error || !json) return nil;

    AURDeltaSkin *skin = [[AURDeltaSkin alloc] init];
    skin.name = json[@"name"];
    skin.identifier = json[@"identifier"];
    skin.gameTypeIdentifier = json[@"gameTypeIdentifier"];

    // Support multiple layouts
    skin.layouts = json[@"layouts"];

    // Default to portrait standard for basic mapping
    NSDictionary *portrait = skin.layouts[@"portrait"];
    if (portrait) {
        NSString *bgName = portrait[@"assets"][@"background"];
        if (bgName) {
            skin.backgroundImage = [UIImage imageWithContentsOfFile:[path stringByAppendingPathComponent:bgName]];
        }

        NSMutableDictionary *rects = [NSMutableDictionary dictionary];
        NSArray *items = portrait[@"items"];
        for (NSDictionary *item in items) {
            NSArray *inputs = item[@"inputs"];
            NSDictionary *frame = item[@"frame"];
            if (inputs.count > 0 && frame) {
                CGRect rect = CGRectMake([frame[@"x"] floatValue], [frame[@"y"] floatValue], [frame[@"width"] floatValue], [frame[@"height"] floatValue]);
                NSString *input = inputs.firstObject;
                rects[input] = [NSValue valueWithCGRect:rect];
            }
        }
        skin.buttonRects = rects;
    }

    return skin;
}

- (BOOL)supportsTraits:(AURControllerSkinTraits *)traits {
    // Better traits support check
    NSString *orientationKey = (traits.orientation == AURControllerSkinOrientationLandscape) ? @"landscape" : @"portrait";
    if (self.layouts[orientationKey]) return YES;
    return NO;
}

@end
