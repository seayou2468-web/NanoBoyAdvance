#import "AURSceneDelegate.h"
#import "Controllers/AURLibraryViewController.h"

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

    AURLibraryViewController *libraryVC = [[AURLibraryViewController alloc] init];
    UINavigationController *navController = [[UINavigationController alloc] initWithRootViewController:libraryVC];

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
