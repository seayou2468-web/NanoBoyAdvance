#import <UIKit/UIKit.h>
#import "../Models/AURGame.h"

@interface AURPreferredControllerSkinsViewController : UITableViewController
@property (nonatomic, strong) AURGame *game;
@property (nonatomic, assign) EmulatorCoreType coreType;
@end
