#import "AURAppDelegate.h"
#import "AURSceneDelegate.h"

@implementation AURAppDelegate

- (BOOL)application:(UIApplication*)application
    didFinishLaunchingWithOptions:(NSDictionary* _Nullable)launchOptions {
    (void)application;
    (void)launchOptions;
    // ウィンドウ生成は AURSceneDelegate に委譲。
    // iOS 13+ SceneDelegate が有効な場合、ここでは何もしない。
    return YES;
}

// MARK: - UISceneSession Lifecycle

- (UISceneConfiguration*)application:(UIApplication*)application
    configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession
                                    options:(UISceneConnectionOptions*)options {
    (void)application;
    (void)options;
    // Info.plist の "Application Scene Manifest" エントリを参照。
    // プログラム的に構成する場合はここで設定。
    UISceneConfiguration* config =
        [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
                                       sessionRole:connectingSceneSession.role];
    config.delegateClass = [AURSceneDelegate class];
    // Storyboard を使わないので sceneClass / storyboard は省略。
    return config;
}

- (void)application:(UIApplication*)application
    didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
    (void)application;
    (void)sceneSessions;
}

@end
