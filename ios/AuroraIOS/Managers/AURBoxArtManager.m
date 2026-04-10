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
    // Delta uses a specific hash-based or exact-name-based search.
    // For this simulation, we will attempt to find a local image that matches the title
    // or return a placeholder with the game title drawn on it.

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        UIImage *image = [UIImage imageNamed:title];
        if (!image) {
            image = [self generatePlaceholderForTitle:title];
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            completion(image);
        });
    });
}

- (UIImage *)generatePlaceholderForTitle:(NSString *)title {
    CGSize size = CGSizeMake(200, 300);
    UIGraphicsBeginImageContextWithOptions(size, YES, 0);
    CGContextRef context = UIGraphicsGetCurrentContext();

    // Gradient Background (Delta-style)
    CGFloat locations[2] = {0.0, 1.0};
    CGFloat components[8] = {0.2, 0.2, 0.3, 1.0, 0.1, 0.1, 0.15, 1.0};
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    CGGradientRef gradient = CGGradientCreateWithColorComponents(rgb, components, locations, 2);
    CGContextDrawLinearGradient(context, gradient, CGPointMake(0, 0), CGPointMake(0, size.height), 0);
    CGGradientRelease(gradient);
    CGColorSpaceRelease(rgb);

    // Draw Text
    NSDictionary *attr = @{
        NSFontAttributeName: [UIFont systemFontOfSize:24 weight:UIFontWeightBold],
        NSForegroundColorAttributeName: [UIColor whiteColor]
    };
    CGRect textRect = CGRectMake(10, 100, size.width - 20, 100);
    [title drawInRect:textRect withAttributes:attr];

    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image;
}

@end
