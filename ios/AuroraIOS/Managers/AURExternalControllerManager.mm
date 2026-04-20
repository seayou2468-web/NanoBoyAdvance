#import "AURExternalControllerManager.h"

@implementation AURExternalControllerManager

+ (instancetype)sharedManager {
    static AURExternalControllerManager *shared = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        shared = [[self alloc] init];
    });
    return shared;
}

- (void)startMonitoring {
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerDidConnect:) name:GCControllerDidConnectNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(controllerDidDisconnect:) name:GCControllerDidDisconnectNotification object:nil];

    for (GCController *controller in [GCController controllers]) {
        [self setupController:controller];
    }
}

- (void)controllerDidConnect:(NSNotification *)notification {
    [self setupController:notification.object];
}

- (void)controllerDidDisconnect:(NSNotification *)notification {
    // Handle disconnect
}

- (void)setupController:(GCController *)controller {
    GCExtendedGamepad *profile = controller.extendedGamepad;
    if (profile) {
        __weak __typeof__(self) weakSelf = self;

        profile.buttonA.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_A pressed:pressed];
        };
        profile.buttonB.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_B pressed:pressed];
        };
        profile.leftShoulder.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_L pressed:pressed];
        };
        profile.rightShoulder.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_R pressed:pressed];
        };
        profile.buttonOptions.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_SELECT pressed:pressed];
        };
        profile.buttonMenu.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_START pressed:pressed];
        };

        profile.dpad.up.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_UP pressed:pressed];
        };
        profile.dpad.down.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_DOWN pressed:pressed];
        };
        profile.dpad.left.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_LEFT pressed:pressed];
        };
        profile.dpad.right.pressedChangedHandler = ^(GCControllerButtonInput *button, float value, BOOL pressed) {
            [weakSelf handleKey:EMULATOR_KEY_RIGHT pressed:pressed];
        };
    }
}

- (void)handleKey:(EmulatorKey)key pressed:(BOOL)pressed {
    if (pressed) {
        [self.delegate externalControllerDidPressKey:key];
    } else {
        [self.delegate externalControllerDidReleaseKey:key];
    }
}

@end
