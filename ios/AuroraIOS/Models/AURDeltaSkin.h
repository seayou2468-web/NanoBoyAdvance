#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import "AURControllerSkin.h"

@interface AURDeltaSkin : AURControllerSkin
@property (nonatomic, copy) NSString *identifier;
@property (nonatomic, strong, readonly) NSSet<NSNumber *> *supportedCoreTypes;
+ (instancetype)skinWithJSONData:(NSData *)data folderPath:(NSString *)path;
- (BOOL)supportsCoreType:(EmulatorCoreType)coreType;
- (void)applyLayoutForTraits:(AURControllerSkinTraits *)traits;
@end
