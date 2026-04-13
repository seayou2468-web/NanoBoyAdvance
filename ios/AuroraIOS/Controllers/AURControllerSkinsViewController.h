#import <UIKit/UIKit.h>
#import "../Models/AURControllerSkin.h"
#include "../../../src/core/api/emulator_core_c_api.h"

@class AURControllerSkinsViewController;

@protocol AURControllerSkinsViewControllerDelegate <NSObject>
- (void)controllerSkinsViewController:(AURControllerSkinsViewController *)controller didChooseSkin:(AURControllerSkin *)skin;
@end

@interface AURControllerSkinsViewController : UITableViewController
@property (nonatomic, weak) id<AURControllerSkinsViewControllerDelegate> delegate;
@property (nonatomic, assign) EmulatorCoreType coreType;
@property (nonatomic, strong) AURControllerSkinTraits *traits;
@end
