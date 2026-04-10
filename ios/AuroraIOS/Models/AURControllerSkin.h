#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "AURControllerSkinConfigurations.h"
#import "AURControllerSkinTraits.h"

@interface AURControllerSkin : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, copy) NSString *identifier;
@property (nonatomic, strong) UIImage *backgroundImage;
@property (nonatomic, strong) NSDictionary *buttonRects;
@property (nonatomic, assign) AURControllerSkinConfigurations supportedConfigurations;
@property (nonatomic, assign) BOOL isStandard;

- (BOOL)supportsTraits:(AURControllerSkinTraits *)traits;
@end
