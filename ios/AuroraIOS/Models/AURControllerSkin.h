#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@interface AURControllerSkin : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, strong) UIImage *backgroundImage;
@property (nonatomic, strong) NSDictionary *buttonRects; // Key: EmulatorKey (as string), Value: NSValue (CGRect)
@end
