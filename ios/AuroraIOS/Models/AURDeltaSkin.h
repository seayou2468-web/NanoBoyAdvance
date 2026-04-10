#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "AURControllerSkin.h"

@interface AURDeltaSkin : AURControllerSkin
@property (nonatomic, copy) NSString *identifier;
@property (nonatomic, strong) NSDictionary *layouts; // portrait, landscape
+ (instancetype)skinWithJSONData:(NSData *)data folderPath:(NSString *)path;
@end
