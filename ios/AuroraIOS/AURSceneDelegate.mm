#import "AURSceneDelegate.h"
#if defined(NBA_CI_MINIMAL_APP)
#import <UIKit/UIKit.h>
#else
#import "Controllers/AURLibraryViewController.h"
#endif

@implementation AURSceneDelegate

// MARK: - UIWindowSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
               options:(UISceneConnectionOptions*)connectionOptions {
    (void)session;
    (void)connectionOptions;

    if (![scene isKindOfClass:[UIWindowScene class]]) {
        return;
    }
    UIWindowScene* windowScene = (UIWindowScene*)scene;

    self.window = [[UIWindow alloc] initWithWindowScene:windowScene];

#if defined(NBA_CI_MINIMAL_APP)
    UIViewController *rootVC = [[UIViewController alloc] init];
    rootVC.view.backgroundColor = [UIColor blackColor];
    rootVC.title = @"Aurora CI";
#else
    AURLibraryViewController *rootVC = [[AURLibraryViewController alloc] init];
#endif
    UINavigationController *navController = [[UINavigationController alloc] initWithRootViewController:rootVC];

    // Delta-like appearance for Navigation Bar
    navController.navigationBar.prefersLargeTitles = YES;
    navController.navigationBar.barStyle = UIBarStyleBlack;
    navController.navigationBar.tintColor = [UIColor systemPinkColor];

    self.window.rootViewController = navController;
    [self.window makeKeyAndVisible];
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    (void)scene;
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    (void)scene;
}

- (void)sceneWillResignActive:(UIScene*)scene {
    (void)scene;
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    (void)scene;
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
    (void)scene;
}

@end
