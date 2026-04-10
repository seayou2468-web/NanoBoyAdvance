#import "AUREmulatorViewController.h"
#import "../Views/AURControllerView.h"
#import "../Views/Metal.h"
#import <QuartzCore/QuartzCore.h>

@interface AUREmulatorViewController () <AURControllerViewDelegate> {
    EmulatorCoreHandle* _core;
    EmulatorCoreType    _coreType;
    EmulatorVideoSpec   _videoSpec;
    BOOL                _running;
}
@property (nonatomic, strong) AURMetalView *imageView;
@property (nonatomic, strong) AURControllerView *controllerView;
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) NSURL *romURL;
@end

@implementation AUREmulatorViewController

- (instancetype)initWithROMURL:(NSURL *)romURL coreType:(EmulatorCoreType)coreType {
    self = [super init];
    if (self) {
        _romURL = romURL;
        _coreType = coreType;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];

    // Setup Metal View
    self.imageView = [[AURMetalView alloc] initWithFrame:CGRectZero];
    [self.view addSubview:self.imageView];

    // Menu Button (Delta-like)
    UIButton *menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [menuButton setImage:[UIImage systemImageNamed:@"ellipsis.circle.fill"] forState:UIControlStateNormal];
    menuButton.tintColor = [UIColor colorWithWhite:1.0 alpha:0.5];
    [menuButton addTarget:self action:@selector(menuTapped) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:menuButton];
    menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [menuButton.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:10],
        [menuButton.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-10],
        [menuButton.widthAnchor constraintEqualToConstant:40],
        [menuButton.heightAnchor constraintEqualToConstant:40]
    ]];

    // Setup Controller View
    self.controllerView = [[AURControllerView alloc] initWithFrame:CGRectZero];
    self.controllerView.delegate = self;
    [self.view addSubview:self.controllerView];

    // Layout
    self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
    self.controllerView.translatesAutoresizingMaskIntoConstraints = NO;

    [NSLayoutConstraint activateConstraints:@[
        [self.imageView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.imageView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.imageView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.imageView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor multiplier:0.4],

        [self.controllerView.topAnchor constraintEqualToAnchor:self.imageView.bottomAnchor],
        [self.controllerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.controllerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.controllerView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor]
    ]];

    [self startEmulator];
}

- (void)startEmulator {
    _core = EmulatorCore_Create(_coreType);
    if (!_core) return;

    if (EmulatorCore_LoadROMFromPath(_core, self.romURL.fileSystemRepresentation)) {
        EmulatorCore_GetVideoSpec(_core, &_videoSpec);
        _running = YES;
        self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(gameLoop)];
        [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    }
}

- (void)gameLoop {
    if (!_running) return;
    EmulatorCore_StepFrame(_core);
    size_t pixelCount = 0;
    const uint32_t* frameRGBA = EmulatorCore_GetFrameBufferRGBA(_core, &pixelCount);
    if (frameRGBA) {
        [self.imageView displayFrameRGBA:frameRGBA width:_videoSpec.width height:_videoSpec.height];
    }
}

- (void)dealloc {
    [self.displayLink invalidate];
    if (_core) EmulatorCore_Destroy(_core);
}

#pragma mark - AURControllerViewDelegate

- (void)controllerViewDidPressKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, true);
}

- (void)controllerViewDidReleaseKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, false);
}

- (void)menuTapped {
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:nil message:nil preferredStyle:UIAlertControllerStyleActionSheet];
    [alert addAction:[UIAlertAction actionWithTitle:@"Save State" style:UIAlertActionStyleDefault handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Load State" style:UIAlertActionStyleDefault handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cheats" style:UIAlertActionStyleDefault handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Fast Forward" style:UIAlertActionStyleDefault handler:nil]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Quit" style:UIAlertActionStyleDestructive handler:^(UIAlertAction *action) {
        [self dismissViewControllerAnimated:YES completion:nil];
    }]];
    [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    [self presentViewController:alert animated:YES completion:nil];
}

@end
