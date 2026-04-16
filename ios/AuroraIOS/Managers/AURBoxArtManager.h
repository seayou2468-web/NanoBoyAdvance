#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface AURBoxArtManager : NSObject
+ (instancetype)sharedManager;
- (void)fetchBoxArtForGameTitle:(NSString *)title completion:(void(^)(UIImage *image))completion;
@end
