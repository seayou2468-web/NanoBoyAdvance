#import <UIKit/UIKit.h>

@protocol AURInGameMenuDelegate <NSObject>
- (void)inGameMenuDidSelectAction:(NSString *)action;
@end

@interface AURInGameMenuViewController : UIViewController
@property (nonatomic, weak) id<AURInGameMenuDelegate> delegate;
@end
