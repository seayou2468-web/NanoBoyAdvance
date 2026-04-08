#import <UIKit/UIKit.h>

@interface AURViewController : UIViewController
- (void)handleApplicationDidBecomeActive;
- (void)handleApplicationWillResignActive;
- (void)handleApplicationDidEnterBackground;
- (void)handleApplicationWillEnterForeground;
@end
