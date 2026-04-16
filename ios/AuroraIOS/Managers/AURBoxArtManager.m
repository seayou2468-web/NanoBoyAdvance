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
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        UIImage *image = [UIImage imageNamed:title];
        if (!image) {
            image = [self generateCartridgePlaceholderForTitle:title];
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            completion(image);
        });
    });
}

- (UIImage *)generateCartridgePlaceholderForTitle:(NSString *)title {
    CGSize size = CGSizeMake(200, 220); // Cartridge shape
    UIGraphicsBeginImageContextWithOptions(size, YES, 0);
    CGContextRef context = UIGraphicsGetCurrentContext();

    // Cartridge Plastic (Gray)
    [[UIColor colorWithWhite:0.4 alpha:1.0] setFill];
    [[UIBezierPath bezierPathWithRoundedRect:CGRectMake(0, 0, size.width, size.height) cornerRadius:10] fill];

    // Label Area
    CGRect labelRect = CGRectMake(15, 40, size.width - 30, size.height - 80);
    [[UIColor colorWithWhite:0.9 alpha:1.0] setFill];
    [[UIBezierPath bezierPathWithRect:labelRect] fill];

    // Draw Title on Label
    NSDictionary *attr = @{
        NSFontAttributeName: [UIFont systemFontOfSize:18 weight:UIFontWeightBold],
        NSForegroundColorAttributeName: [UIColor blackColor]
    };
    [title drawInRect:CGRectInset(labelRect, 10, 10) withAttributes:attr];

    // Cartridge Grooves (Delta style detail)
    [[UIColor colorWithWhite:0.3 alpha:1.0] setStroke];
    UIBezierPath *grooves = [UIBezierPath bezierPath];
    [grooves moveToPoint:CGPointMake(20, 10)]; [grooves addLineToPoint:CGPointMake(size.width - 20, 10)];
    [grooves moveToPoint:CGPointMake(20, 20)]; [grooves addLineToPoint:CGPointMake(size.width - 20, 20)];
    grooves.lineWidth = 2;
    [grooves stroke];

    UIImage *image = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();
    return image;
}

@end
