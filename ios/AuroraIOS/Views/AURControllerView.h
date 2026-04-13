#import <UIKit/UIKit.h>
#import "../Models/AURControllerSkin.h"
#include "../../../src/core/api/emulator_core_c_api.h"

@protocol AURControllerViewDelegate <NSObject>
- (void)controllerViewDidPressKey:(EmulatorKey)key;
- (void)controllerViewDidReleaseKey:(EmulatorKey)key;
@end

@interface AURControllerView : UIView
@property (nonatomic, weak) id<AURControllerViewDelegate> delegate;
- (void)applySkin:(AURControllerSkin *)skin;
@end
