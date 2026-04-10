#import "AURBoxArtManager.h"

@implementation AURBoxArtManager

+ (instancetype)sharedManager {
    static AURBoxArtManager *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

- (void)fetchBoxArtForGameTitle:(NSString *)title completion:(void(^)(UIImage *image))completion {
    // Delta uses OpenVGDB or similar. For now, we mock it or use a placeholder.
    // In a real scenario, this would call an API like IGDB or GamesDB.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        // Mock delay
        [NSThread sleepForTimeInterval:0.5];
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(nil);
        });
    });
}

@end
