#import "AURSceneDelegate.h"
#import "AURViewController.h"

@implementation AURSceneDelegate

- (AURViewController* _Nullable)rootViewController {
    if (![self.window.rootViewController isKindOfClass:[AURViewController class]]) {
        return nil;
    }
    return (AURViewController*)self.window.rootViewController;
}

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
    self.window.rootViewController = [AURViewController new];
    [self.window makeKeyAndVisible];
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    (void)scene;
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    (void)scene;
    [[self rootViewController] handleApplicationDidBecomeActive];
}

- (void)sceneWillResignActive:(UIScene*)scene {
    (void)scene;
    [[self rootViewController] handleApplicationWillResignActive];
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    (void)scene;
    [[self rootViewController] handleApplicationDidEnterBackground];
}

- (void)sceneWillEnterForeground:(UIScene*)scene {
    (void)scene;
    [[self rootViewController] handleApplicationWillEnterForeground];
}

@end
