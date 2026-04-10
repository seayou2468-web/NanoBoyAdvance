#import "./AURViewController.h"
#import "./Metal.h"

#import <QuartzCore/QuartzCore.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

extern "C" {
#include "../../src/core/emulator_core_c_api.h"
}

// 初期アスペクト比 (GBA)
static const NSUInteger kDefaultWidth  = 240;
static const NSUInteger kDefaultHeight = 160;

@interface AURViewController () <UIDocumentPickerDelegate>
@property (nonatomic, strong) AURMetalView*  imageView;
@property (nonatomic, strong) UILabel*       titleLabel;
@property (nonatomic, strong) UILabel*       statusLabel;
@property (nonatomic, strong) UIButton*      selectROMButton;
@property (nonatomic, strong) UIButton*      selectBIOSButton;
@property (nonatomic, strong) UIButton*      emulatorModeButton;
@property (nonatomic, strong) UIButton*      controllerModeButton;
@property (nonatomic, strong) UIButton*      appMenuButton;
@property (nonatomic, strong) UIView*        menuOverlayView;
@property (nonatomic, strong) UIView*        menuCardView;
@property (nonatomic, strong) UILabel*       menuTitleLabel;
@property (nonatomic, strong) UIStackView*   menuStackView;
@property (nonatomic, strong) UITextView*    logTextView;
@property (nonatomic, strong) UIView*        controlsContainer;
@property (nonatomic, strong) CADisplayLink* displayLink;
@property (nonatomic, strong) NSLayoutConstraint* controlsHeightConstraint;
@property (nonatomic, strong) NSLayoutConstraint* logMinHeightConstraint;
@property (nonatomic, strong) NSLayoutConstraint* imageAspectConstraint;
- (void)appendLog:(NSString*)message;
- (void)presentEmulatorMenu;
- (void)presentControllerMenu;
- (void)presentAppMenu;
- (void)presentVideoSettingsMenu;
- (void)presentCustomMenuWithTitle:(NSString*)title actions:(NSArray<NSDictionary*>*)actions;
- (void)dismissCustomMenu;
- (void)applyControllerPreset;
- (NSString*)coreTypeLabel:(EmulatorCoreType)type;
@end

@implementation AURViewController {
    EmulatorCoreHandle* _core;
    EmulatorCoreType    _coreType;
    EmulatorVideoSpec   _videoSpec;
    BOOL            _running;
    BOOL            _pickingBIOS;
    NSString*       _selectedBIOSPath;
    NSUInteger      _whiteFrameStreak;
    BOOL            _didLogWhiteFrameHint;
    BOOL            _forceCoreType;
    NSInteger       _forcedCoreTypeValue;
    NSInteger       _gbaControllerPreset; // 0: standard, 1: compact
    NSInteger       _nesControllerPreset; // 0: standard, 1: compact
    NSInteger       _gbControllerPreset;  // 0: standard, 1: compact
    UIButton*       _lButton;
    UIButton*       _rButton;
    UIButton*       _aButton;
    UIButton*       _bButton;
    UIButton*       _startButton;
    UIButton*       _selectButton;
    NSLayoutConstraint* _aButtonSize;
    NSLayoutConstraint* _bButtonSize;
    NSLayoutConstraint* _lButtonSize;
    NSLayoutConstraint* _rButtonSize;
    NSLayoutConstraint* _startButtonSize;
    NSLayoutConstraint* _selectButtonSize;
    NSMutableArray*  _menuHandlers;
    float           _videoSaturation;
    float           _videoVibrance;
    float           _videoContrast;
    float           _videoSharpen;
    float           _videoLutMix;
    AURUpscaleMode  _upscaleMode;
#if DEBUG
    NSUInteger      _frameCounter;
    uint32_t        _lastFrameSample;
    NSUInteger      _staleFrameCount;
    BOOL            _hasFrameSample;
#endif
}

// MARK: - View Lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;
    _videoSpec.width = (uint32_t)kDefaultWidth;
    _videoSpec.height = (uint32_t)kDefaultHeight;
    _forceCoreType = NO;
    _forcedCoreTypeValue = EMULATOR_CORE_TYPE_GBA;
    _gbaControllerPreset = 0;
    _nesControllerPreset = 1;
    _gbControllerPreset = 1;
    _menuHandlers = [NSMutableArray array];
    _videoSaturation = 1.08f;
    _videoVibrance = 0.30f;
    _videoContrast = 1.06f;
    _videoSharpen = 0.18f;
    _videoLutMix = 0.15f;
    _upscaleMode = AURUpscaleModeAuto;

    // ImageView
    self.imageView = [[AURMetalView alloc] initWithFrame:CGRectZero];
    self.imageView.backgroundColor = [UIColor colorWithWhite:0.05 alpha:1.0];
    self.imageView.opaque = YES;
    self.imageView.layer.cornerRadius = 14.0;
    self.imageView.layer.masksToBounds = YES;
    self.imageView.layer.borderWidth = 1.0;
    self.imageView.layer.borderColor = [UIColor colorWithWhite:0.25 alpha:1.0].CGColor;
    self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.imageView];

    self.titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.titleLabel.text = @"NanoBoyAdvance";
    self.titleLabel.font = [UIFont systemFontOfSize:22 weight:UIFontWeightBold];
    self.titleLabel.textAlignment = NSTextAlignmentCenter;
    self.titleLabel.textColor = UIColor.labelColor;
    [self.view addSubview:self.titleLabel];

    // Status label
    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.textColor = UIColor.secondaryLabelColor;
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.textAlignment = NSTextAlignmentCenter;
    self.statusLabel.font =
        [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];
    self.statusLabel.text = @"ROMを選択してください";
    [self.view addSubview:self.statusLabel];

    // Select ROM button
    self.selectROMButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.selectROMButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.selectROMButton addTarget:self
                             action:@selector(presentROMPicker)
                   forControlEvents:UIControlEventTouchUpInside];

    if (@available(iOS 15.0, *)) {
        UIButtonConfiguration* cfg = [UIButtonConfiguration filledButtonConfiguration];
        cfg.title = @"ROM を選択…";
        cfg.image = [UIImage systemImageNamed:@"folder.badge.plus"];
        cfg.imagePadding = 8;
        cfg.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        cfg.baseBackgroundColor = UIColor.systemIndigoColor;
        self.selectROMButton.configuration = cfg;
    } else {
        [self.selectROMButton setTitle:@"ROM を選択…" forState:UIControlStateNormal];
        self.selectROMButton.titleLabel.font =
            [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];
    }

    // Select BIOS button
    self.selectBIOSButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.selectBIOSButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.selectBIOSButton addTarget:self
                              action:@selector(presentBIOSPicker)
                    forControlEvents:UIControlEventTouchUpInside];
    if (@available(iOS 15.0, *)) {
        UIButtonConfiguration* cfg = [UIButtonConfiguration tintedButtonConfiguration];
        cfg.title = @"BIOS を選択…";
        cfg.image = [UIImage systemImageNamed:@"cpu"];
        cfg.imagePadding = 8;
        cfg.cornerStyle = UIButtonConfigurationCornerStyleLarge;
        self.selectBIOSButton.configuration = cfg;
    } else {
        [self.selectBIOSButton setTitle:@"BIOS を選択…" forState:UIControlStateNormal];
    }

    self.emulatorModeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.emulatorModeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.emulatorModeButton addTarget:self
                                action:@selector(presentEmulatorMenu)
                      forControlEvents:UIControlEventTouchUpInside];
    [self.emulatorModeButton setTitle:@"エミュ: 自動" forState:UIControlStateNormal];

    self.controllerModeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.controllerModeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.controllerModeButton addTarget:self
                                  action:@selector(presentControllerMenu)
                        forControlEvents:UIControlEventTouchUpInside];
    [self.controllerModeButton setTitle:@"コントローラー" forState:UIControlStateNormal];

    self.appMenuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    self.appMenuButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self.appMenuButton addTarget:self
                           action:@selector(presentAppMenu)
                 forControlEvents:UIControlEventTouchUpInside];
    [self.appMenuButton setTitle:@"メニュー" forState:UIControlStateNormal];

    UIStackView* modeButtonStack = [[UIStackView alloc] initWithArrangedSubviews:@[
        self.emulatorModeButton, self.controllerModeButton, self.appMenuButton
    ]];
    modeButtonStack.translatesAutoresizingMaskIntoConstraints = NO;
    modeButtonStack.axis = UILayoutConstraintAxisHorizontal;
    modeButtonStack.distribution = UIStackViewDistributionFillEqually;
    modeButtonStack.spacing = 10.0;
    [self.view addSubview:modeButtonStack];

    UIStackView* fileButtonStack = [[UIStackView alloc] initWithArrangedSubviews:@[self.selectROMButton, self.selectBIOSButton]];
    fileButtonStack.translatesAutoresizingMaskIntoConstraints = NO;
    fileButtonStack.axis = UILayoutConstraintAxisHorizontal;
    fileButtonStack.distribution = UIStackViewDistributionFillEqually;
    fileButtonStack.spacing = 12.0;
    [self.view addSubview:fileButtonStack];

    // ROM/実行ログ
    self.logTextView = [[UITextView alloc] initWithFrame:CGRectZero];
    self.logTextView.translatesAutoresizingMaskIntoConstraints = NO;
    self.logTextView.editable = NO;
    self.logTextView.selectable = YES;
    self.logTextView.font =
        [UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];
    self.logTextView.backgroundColor = UIColor.secondarySystemBackgroundColor;
    self.logTextView.textColor = UIColor.labelColor;
    self.logTextView.text = @"[log] 起動\n";
    [self.view addSubview:self.logTextView];

    // Virtual controller
    self.controlsContainer = [[UIView alloc] initWithFrame:CGRectZero];
    self.controlsContainer.translatesAutoresizingMaskIntoConstraints = NO;
    self.controlsContainer.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.85];
    self.controlsContainer.layer.cornerRadius = 14.0;
    self.controlsContainer.layer.masksToBounds = YES;
    [self.view addSubview:self.controlsContainer];
    UIButton* (^makeKeyButton)(NSString*, NSInteger, CGFloat) = ^UIButton* (NSString* title, NSInteger key, CGFloat size) {
        UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
        button.translatesAutoresizingMaskIntoConstraints = NO;
        button.tag = key;
        [button setTitle:title forState:UIControlStateNormal];
        button.tintColor = UIColor.whiteColor;
        button.backgroundColor = [UIColor colorWithRed:0.18 green:0.20 blue:0.26 alpha:1.0];
        button.layer.cornerRadius = size * 0.28;
        button.layer.borderColor = [UIColor colorWithWhite:1.0 alpha:0.08].CGColor;
        button.layer.borderWidth = 1.0;
        button.titleLabel.font = [UIFont systemFontOfSize:(size > 50 ? 22 : 14) weight:UIFontWeightSemibold];
        [button.widthAnchor constraintEqualToConstant:size].active = YES;
        [button.heightAnchor constraintEqualToConstant:size].active = YES;
        [button addTarget:self action:@selector(onVirtualKeyDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(onVirtualKeyUp:) forControlEvents:UIControlEventTouchUpInside];
        [button addTarget:self action:@selector(onVirtualKeyUp:) forControlEvents:UIControlEventTouchUpOutside];
        [button addTarget:self action:@selector(onVirtualKeyUp:) forControlEvents:UIControlEventTouchCancel];
        return button;
    };

    UIView* dpadSpacer1 = [[UIView alloc] initWithFrame:CGRectZero];
    UIView* dpadSpacer2 = [[UIView alloc] initWithFrame:CGRectZero];
    UIView* dpadSpacer3 = [[UIView alloc] initWithFrame:CGRectZero];
    UIView* dpadSpacer4 = [[UIView alloc] initWithFrame:CGRectZero];
    UIButton* upBtn = makeKeyButton(@"↑", EMULATOR_KEY_UP, 52);
    UIButton* downBtn = makeKeyButton(@"↓", EMULATOR_KEY_DOWN, 52);
    UIButton* leftBtn = makeKeyButton(@"←", EMULATOR_KEY_LEFT, 52);
    UIButton* rightBtn = makeKeyButton(@"→", EMULATOR_KEY_RIGHT, 52);

    UIStackView* dpadRow1 = [[UIStackView alloc] initWithArrangedSubviews:@[dpadSpacer1, upBtn, dpadSpacer2]];
    UIStackView* dpadRow2 = [[UIStackView alloc] initWithArrangedSubviews:@[leftBtn, [[UIView alloc] initWithFrame:CGRectZero], rightBtn]];
    UIStackView* dpadRow3 = [[UIStackView alloc] initWithArrangedSubviews:@[dpadSpacer3, downBtn, dpadSpacer4]];
    for (UIStackView* row in @[dpadRow1, dpadRow2, dpadRow3]) {
        row.axis = UILayoutConstraintAxisHorizontal;
        row.spacing = 8.0;
        row.distribution = UIStackViewDistributionFillEqually;
    }
    UIStackView* dpadStack = [[UIStackView alloc] initWithArrangedSubviews:@[dpadRow1, dpadRow2, dpadRow3]];
    dpadStack.axis = UILayoutConstraintAxisVertical;
    dpadStack.spacing = 8.0;
    dpadStack.translatesAutoresizingMaskIntoConstraints = NO;

    UIButton* selectBtn = makeKeyButton(@"Select", EMULATOR_KEY_SELECT, 40);
    UIButton* startBtn = makeKeyButton(@"Start", EMULATOR_KEY_START, 40);
    _selectButton = selectBtn;
    _startButton = startBtn;
    selectBtn.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    startBtn.titleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightSemibold];
    UIStackView* centerStack = [[UIStackView alloc] initWithArrangedSubviews:@[selectBtn, startBtn]];
    centerStack.axis = UILayoutConstraintAxisVertical;
    centerStack.spacing = 10.0;
    centerStack.translatesAutoresizingMaskIntoConstraints = NO;

    UIButton* lBtn = makeKeyButton(@"L", EMULATOR_KEY_L, 40);
    UIButton* rBtn = makeKeyButton(@"R", EMULATOR_KEY_R, 40);
    UIButton* bBtn = makeKeyButton(@"B", EMULATOR_KEY_B, 56);
    UIButton* aBtn = makeKeyButton(@"A", EMULATOR_KEY_A, 56);
    _lButton = lBtn;
    _rButton = rBtn;
    _aButton = aBtn;
    _bButton = bBtn;
    for (NSLayoutConstraint* c in aBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _aButtonSize = c; }
    }
    for (NSLayoutConstraint* c in bBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _bButtonSize = c; }
    }
    for (NSLayoutConstraint* c in lBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _lButtonSize = c; }
    }
    for (NSLayoutConstraint* c in rBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _rButtonSize = c; }
    }
    for (NSLayoutConstraint* c in startBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _startButtonSize = c; }
    }
    for (NSLayoutConstraint* c in selectBtn.constraints) {
        if (c.firstAttribute == NSLayoutAttributeWidth) { _selectButtonSize = c; }
    }
    UIStackView* shoulderRow = [[UIStackView alloc] initWithArrangedSubviews:@[lBtn, rBtn]];
    shoulderRow.axis = UILayoutConstraintAxisHorizontal;
    shoulderRow.spacing = 10.0;
    UIStackView* actionRow = [[UIStackView alloc] initWithArrangedSubviews:@[bBtn, aBtn]];
    actionRow.axis = UILayoutConstraintAxisHorizontal;
    actionRow.spacing = 12.0;
    UIStackView* actionStack = [[UIStackView alloc] initWithArrangedSubviews:@[shoulderRow, actionRow]];
    actionStack.axis = UILayoutConstraintAxisVertical;
    actionStack.spacing = 12.0;
    actionStack.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* controlsMainStack = [[UIStackView alloc] initWithArrangedSubviews:@[dpadStack, centerStack, actionStack]];
    controlsMainStack.axis = UILayoutConstraintAxisHorizontal;
    controlsMainStack.alignment = UIStackViewAlignmentCenter;
    controlsMainStack.distribution = UIStackViewDistributionEqualSpacing;
    controlsMainStack.spacing = 14.0;
    controlsMainStack.translatesAutoresizingMaskIntoConstraints = NO;
    [self.controlsContainer addSubview:controlsMainStack];

    self.controlsHeightConstraint = [self.controlsContainer.heightAnchor constraintEqualToConstant:176];
    self.logMinHeightConstraint = [self.logTextView.heightAnchor constraintGreaterThanOrEqualToConstant:72];
    self.logMinHeightConstraint.priority = UILayoutPriorityDefaultHigh;

    self.imageAspectConstraint =
        [self.imageView.heightAnchor constraintEqualToAnchor:self.imageView.widthAnchor
                                                  multiplier:(CGFloat)kDefaultHeight / (CGFloat)kDefaultWidth];

    [NSLayoutConstraint activateConstraints:@[
        [self.titleLabel.topAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
            constant:6],
        [self.titleLabel.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:16],
        [self.titleLabel.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-16],

        // ImageView
        [self.imageView.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:6],
        [self.imageView.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-6],
        [self.imageView.topAnchor
            constraintEqualToAnchor:self.titleLabel.bottomAnchor
            constant:8],
        self.imageAspectConstraint,

        // Status label: ImageView 直下
        [self.statusLabel.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:16],
        [self.statusLabel.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-16],
        [self.statusLabel.topAnchor
            constraintEqualToAnchor:self.imageView.bottomAnchor
            constant:12],

        [fileButtonStack.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:16],
        [fileButtonStack.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-16],
        [fileButtonStack.topAnchor
            constraintEqualToAnchor:modeButtonStack.bottomAnchor
            constant:10],

        [modeButtonStack.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:16],
        [modeButtonStack.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-16],
        [modeButtonStack.topAnchor
            constraintEqualToAnchor:self.statusLabel.bottomAnchor
            constant:10],

        [self.controlsContainer.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:12],
        [self.controlsContainer.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-12],
        [self.controlsContainer.topAnchor
            constraintEqualToAnchor:fileButtonStack.bottomAnchor
            constant:12],
        self.controlsHeightConstraint,

        // Log view: ボタン下〜画面下
        [self.logTextView.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:12],
        [self.logTextView.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-12],
        [self.logTextView.topAnchor
            constraintEqualToAnchor:self.controlsContainer.bottomAnchor
            constant:12],
        [self.logTextView.bottomAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
            constant:-8],
        self.logMinHeightConstraint,
    ]];

    [NSLayoutConstraint activateConstraints:@[
        [controlsMainStack.leadingAnchor constraintEqualToAnchor:self.controlsContainer.leadingAnchor constant:12],
        [controlsMainStack.trailingAnchor constraintEqualToAnchor:self.controlsContainer.trailingAnchor constant:-12],
        [controlsMainStack.topAnchor constraintEqualToAnchor:self.controlsContainer.topAnchor constant:10],
        [controlsMainStack.bottomAnchor constraintEqualToAnchor:self.controlsContainer.bottomAnchor constant:-10],
    ]];

    self.menuOverlayView = [[UIView alloc] initWithFrame:CGRectZero];
    self.menuOverlayView.translatesAutoresizingMaskIntoConstraints = NO;
    self.menuOverlayView.hidden = YES;
    self.menuOverlayView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.42];
    UITapGestureRecognizer* overlayTap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(dismissCustomMenu)];
    overlayTap.cancelsTouchesInView = NO;
    [self.menuOverlayView addGestureRecognizer:overlayTap];
    [self.view addSubview:self.menuOverlayView];

    self.menuCardView = [[UIView alloc] initWithFrame:CGRectZero];
    self.menuCardView.translatesAutoresizingMaskIntoConstraints = NO;
    self.menuCardView.backgroundColor = UIColor.secondarySystemBackgroundColor;
    self.menuCardView.layer.cornerRadius = 16.0;
    self.menuCardView.layer.masksToBounds = YES;
    self.menuCardView.userInteractionEnabled = YES;
    [self.menuOverlayView addSubview:self.menuCardView];

    self.menuTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.menuTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.menuTitleLabel.font = [UIFont systemFontOfSize:18 weight:UIFontWeightBold];
    self.menuTitleLabel.textAlignment = NSTextAlignmentCenter;
    [self.menuCardView addSubview:self.menuTitleLabel];

    self.menuStackView = [[UIStackView alloc] initWithFrame:CGRectZero];
    self.menuStackView.translatesAutoresizingMaskIntoConstraints = NO;
    self.menuStackView.axis = UILayoutConstraintAxisVertical;
    self.menuStackView.spacing = 8.0;
    [self.menuCardView addSubview:self.menuStackView];

    [NSLayoutConstraint activateConstraints:@[
        [self.menuOverlayView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.menuOverlayView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.menuOverlayView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.menuOverlayView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [self.menuCardView.centerXAnchor constraintEqualToAnchor:self.menuOverlayView.centerXAnchor],
        [self.menuCardView.centerYAnchor constraintEqualToAnchor:self.menuOverlayView.centerYAnchor],
        [self.menuCardView.widthAnchor constraintLessThanOrEqualToConstant:360],
        [self.menuCardView.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.menuOverlayView.leadingAnchor constant:20],
        [self.menuCardView.trailingAnchor constraintLessThanOrEqualToAnchor:self.menuOverlayView.trailingAnchor constant:-20],
        [self.menuTitleLabel.leadingAnchor constraintEqualToAnchor:self.menuCardView.leadingAnchor constant:16],
        [self.menuTitleLabel.trailingAnchor constraintEqualToAnchor:self.menuCardView.trailingAnchor constant:-16],
        [self.menuTitleLabel.topAnchor constraintEqualToAnchor:self.menuCardView.topAnchor constant:16],
        [self.menuStackView.leadingAnchor constraintEqualToAnchor:self.menuCardView.leadingAnchor constant:16],
        [self.menuStackView.trailingAnchor constraintEqualToAnchor:self.menuCardView.trailingAnchor constant:-16],
        [self.menuStackView.topAnchor constraintEqualToAnchor:self.menuTitleLabel.bottomAnchor constant:14],
        [self.menuStackView.bottomAnchor constraintEqualToAnchor:self.menuCardView.bottomAnchor constant:-16],
    ]];

    [self appendLog:@"ROM は毎回選択モードです（bookmark 復元なし）"];
    [self appendLog:@"メニュー追加: エミュ選択 / コントローラー選択 / 設定・セーブ・チート"];
    [self.imageView setUpscaleMode:_upscaleMode];
    [self.imageView setPostProcessSaturation:_videoSaturation
                                    vibrance:_videoVibrance
                                    contrast:_videoContrast
                                     sharpen:_videoSharpen
                                      lutMix:_videoLutMix];
    [self applyControllerPreset];
    [self updateAdaptiveLayoutForBounds:self.view.bounds.size];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self startDisplayLink];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self stopDisplayLink];
}

- (void)viewWillLayoutSubviews {
    [super viewWillLayoutSubviews];
    [self updateAdaptiveLayoutForBounds:self.view.bounds.size];
}

- (void)dealloc {
    [self stopDisplayLink];
    [self teardownCore];
}

- (void)updateAdaptiveLayoutForBounds:(CGSize)size {
    if (size.width <= 0.0 || size.height <= 0.0) { return; }
    BOOL isLandscape = size.width > size.height;
    CGFloat shortEdge = MIN(size.width, size.height);
    CGFloat controlsHeight = shortEdge <= 390.0 ? 150.0 : 176.0;
    if (isLandscape) {
        controlsHeight = MAX(128.0, shortEdge * 0.34);
    }
    self.controlsHeightConstraint.constant = MIN(220.0, controlsHeight);

    CGFloat minLogHeight = isLandscape ? 56.0 : (shortEdge <= 390.0 ? 64.0 : 84.0);
    self.logMinHeightConstraint.constant = minLogHeight;
}

// MARK: - ROM Picker

- (void)presentROMPicker {
    _pickingBIOS = NO;
    [self appendLog:@"ROM ピッカーを表示"];
    // GBA ROM 専用 UTType は存在しないため汎用バイナリ (data) を使用
    // asCopy:YES でアプリ側にコピーして扱う
    UIDocumentPickerViewController* picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
            asCopy:YES];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    picker.modalPresentationStyle = UIModalPresentationFormSheet;
    [self presentViewController:picker animated:YES completion:nil];
}

- (void)presentBIOSPicker {
    _pickingBIOS = YES;
    [self appendLog:@"BIOS ピッカーを表示"];
    UIDocumentPickerViewController* picker =
        [[UIDocumentPickerViewController alloc]
            initForOpeningContentTypes:@[UTTypeData]
            asCopy:YES];
    picker.delegate = self;
    picker.allowsMultipleSelection = NO;
    picker.modalPresentationStyle = UIModalPresentationFormSheet;
    [self presentViewController:picker animated:YES completion:nil];
}

// MARK: - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    NSURL* url = urls.firstObject;
    if (url == nil) { return; }
    [self appendLog:[NSString stringWithFormat:@"ROM 選択: %@", url.path ?: url.lastPathComponent]];

    BOOL accessing = [url startAccessingSecurityScopedResource];

    if (_pickingBIOS) {
        _selectedBIOSPath = [url.path copy];
        [self appendLog:[NSString stringWithFormat:@"BIOS 選択: %@", url.lastPathComponent]];
        self.statusLabel.text = [NSString stringWithFormat:@"BIOS: %@", url.lastPathComponent];
    } else {
        [self loadROMFromURL:url];
    }

    if (accessing) {
        [url stopAccessingSecurityScopedResource];
    }
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    // キャンセル時は何もしない
    [self appendLog:@"ROM ピッカー: キャンセル"];
}

// MARK: - Core Management

- (void)loadROMFromURL:(NSURL*)url {
    [self appendLog:[NSString stringWithFormat:@"ROM 読み込み要求: %@", url.lastPathComponent]];
    [self stopDisplayLink];
    [self teardownCore];
    _running = NO;
    [self.imageView clearFrame];

    NSString* ext = url.pathExtension.lowercaseString ?: @"";
    EmulatorCoreType detectedType = EMULATOR_CORE_TYPE_GBA;
    if ([ext isEqualToString:@"nes"]) {
        detectedType = EMULATOR_CORE_TYPE_NES;
    } else if ([ext isEqualToString:@"gb"] || [ext isEqualToString:@"gbc"]) {
        detectedType = EMULATOR_CORE_TYPE_GB;
    }
    _coreType = _forceCoreType ? (EmulatorCoreType)_forcedCoreTypeValue : detectedType;
    _core = EmulatorCore_Create(_coreType);
    if (_core == NULL) {
        self.statusLabel.text = @"EmulatorCore_Create failed";
        [self appendLog:@"EmulatorCore_Create failed"];
        return;
    }
    [self appendLog:[NSString stringWithFormat:@"EmulatorCore_Create success (%s / %@)",
                     EmulatorCoreTypeName(_coreType), [self coreTypeLabel:_coreType]]];
    [self applyControllerPreset];

    NSNumber* fileSize = nil;
    [url getResourceValue:&fileSize forKey:NSURLFileSizeKey error:nil];
    [self appendLog:[NSString stringWithFormat:@"ROM サイズ: %@ bytes", fileSize ?: @0]];

    if ((_coreType == EMULATOR_CORE_TYPE_GBA || _coreType == EMULATOR_CORE_TYPE_GB) && _selectedBIOSPath.length > 0) {
        [self appendLog:[NSString stringWithFormat:@"BIOS 読み込み: %@", _selectedBIOSPath.lastPathComponent]];
        if (!EmulatorCore_LoadBIOSFromPath(_core, _selectedBIOSPath.fileSystemRepresentation)) {
            const char* err = EmulatorCore_GetLastError(_core);
            self.statusLabel.text =
                [NSString stringWithFormat:@"BIOS 読み込み失敗:\n%s", err ? err : "unknown"];
            [self appendLog:[NSString stringWithFormat:@"BIOS 読み込み失敗: %s", err ? err : "unknown"]];
            EmulatorCore_Destroy(_core);
            _core = NULL;
            return;
        }
    }

    if (!EmulatorCore_LoadROMFromPath(_core, url.fileSystemRepresentation)) {
        const char* err = EmulatorCore_GetLastError(_core);
        self.statusLabel.text =
            [NSString stringWithFormat:@"ROM 読み込み失敗:\n%s", err ? err : "unknown"];
        [self appendLog:[NSString stringWithFormat:@"ROM 読み込み失敗: %s", err ? err : "unknown"]];
        EmulatorCore_Destroy(_core);
        _core = NULL;
        return;
    }

    if (!EmulatorCore_GetVideoSpec(_core, &_videoSpec) || _videoSpec.width == 0 || _videoSpec.height == 0) {
        self.statusLabel.text = @"VideoSpec 取得失敗";
        [self appendLog:@"VideoSpec 取得失敗"];
        EmulatorCore_Destroy(_core);
        _core = NULL;
        return;
    }
    [self appendLog:[NSString stringWithFormat:@"VideoSpec: %ux%u", _videoSpec.width, _videoSpec.height]];
    if (self.imageAspectConstraint != nil) {
        self.imageAspectConstraint.active = NO;
    }
    self.imageAspectConstraint =
        [self.imageView.heightAnchor constraintEqualToAnchor:self.imageView.widthAnchor
                                                  multiplier:(CGFloat)_videoSpec.height / (CGFloat)_videoSpec.width];
    self.imageAspectConstraint.active = YES;

    _running = YES;
    _whiteFrameStreak = 0;
    _didLogWhiteFrameHint = NO;
#if DEBUG
    _frameCounter = 0;
    _staleFrameCount = 0;
    _hasFrameSample = NO;
    _lastFrameSample = 0;
#endif
    self.statusLabel.text =
        [NSString stringWithFormat:@"▶ %@", url.lastPathComponent];
    [self appendLog:[NSString stringWithFormat:@"ROM 読み込み成功: %@", url.lastPathComponent]];

    [self stepFrameAndRender];
    [self startDisplayLink];
}

- (void)teardownCore {
    if (_core != NULL) {
        EmulatorCore_Destroy(_core);
        _core = NULL;
    }
}

// MARK: - DisplayLink

- (void)startDisplayLink {
    if (!_running || self.displayLink != nil) { return; }

    CADisplayLink* dl =
        [CADisplayLink displayLinkWithTarget:self
                                    selector:@selector(stepFrameAndRender)];
    if (@available(iOS 15.0, *)) {
        dl.preferredFrameRateRange = CAFrameRateRangeMake(59.0f, 120.0f, 60.0f);
    } else {
        dl.preferredFramesPerSecond = 60;
    }
    [dl addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    self.displayLink = dl;
}

- (void)stopDisplayLink {
    [self.displayLink invalidate];
    self.displayLink = nil;
}

// MARK: - App Lifecycle Hooks

- (void)handleApplicationDidBecomeActive    { [self startDisplayLink]; }
- (void)handleApplicationWillResignActive   { [self stopDisplayLink]; }
- (void)handleApplicationDidEnterBackground { [self stopDisplayLink]; }
- (void)handleApplicationWillEnterForeground { [self startDisplayLink]; }

// MARK: - Render

- (void)stepFrameAndRender {
    if (_core == NULL) { return; }
#if DEBUG
    CFTimeInterval t0 = CACurrentMediaTime();
#endif

    EmulatorCore_StepFrame(_core);
#if DEBUG
    CFTimeInterval t1 = CACurrentMediaTime();
#endif

    size_t pixelCount = 0;
    const size_t expectedPixels = (size_t)_videoSpec.width * (size_t)_videoSpec.height;
    const uint32_t* frameRGBA = EmulatorCore_GetFrameBufferRGBA(_core, &pixelCount);
    if (frameRGBA == nullptr || pixelCount < expectedPixels) {
        self.statusLabel.text = @"Failed to fetch framebuffer";
        const char* err = EmulatorCore_GetLastError(_core);
        [self appendLog:[NSString stringWithFormat:@"FrameBuffer 取得失敗: %s", err ? err : "unknown"]];
        return;
    }
    // 軽量な白画面検知: サンプル点の大半が 0xFFFFFFFF の状態が続く場合に原因候補をログする。
    {
        size_t whiteSamples = 0;
        constexpr size_t kSampleStep = 601;
        size_t sampleCount = 0;
        for (size_t i = 0; i < expectedPixels; i += kSampleStep) {
            whiteSamples += (frameRGBA[i] == 0xFFFFFFFFu) ? 1u : 0u;
            sampleCount++;
        }

        if (sampleCount > 0 && whiteSamples * 100u >= sampleCount * 95u) {
            _whiteFrameStreak++;
        } else {
            _whiteFrameStreak = 0;
        }

        if (!_didLogWhiteFrameHint && _whiteFrameStreak >= 180u) {
            _didLogWhiteFrameHint = YES;
            [self appendLog:@"白画面が継続: 可能性1) コア出力が常時 forced-blank 2) BIOS未ロード依存ROM 3) Metal転送失敗"];
        }
    }
#if DEBUG
    // 取得済みフレームが更新されていない可能性を検知する簡易サンプル
    uint32_t sample = 2166136261u;
    const size_t stride = 97;
    for (size_t i = 0; i < expectedPixels; i += stride) {
        sample ^= frameRGBA[i];
        sample *= 16777619u;
    }
    if (_hasFrameSample && sample == _lastFrameSample) {
        _staleFrameCount++;
        if (_staleFrameCount == 120u) {
            [self appendLog:@"警告: フレームサンプルが 120 フレーム連続で不変（バッファ更新停滞の可能性）"];
        }
    } else {
        _staleFrameCount = 0;
    }
    _lastFrameSample = sample;
    _hasFrameSample = YES;
    CFTimeInterval t2 = CACurrentMediaTime();
#endif

    [self.imageView displayFrameRGBA:frameRGBA width:_videoSpec.width height:_videoSpec.height];
#if DEBUG
    CFTimeInterval t3 = CACurrentMediaTime();
    _frameCounter++;
    if ((_frameCounter % 30u) == 0u) {
        [self appendLog:[NSString stringWithFormat:
            @"frame=%lu step=%.2fms convert=%.2fms ui=%.2fms total=%.2fms",
            (unsigned long)_frameCounter,
            (t1 - t0) * 1000.0,
            (t2 - t1) * 1000.0,
            (t3 - t2) * 1000.0,
            (t3 - t0) * 1000.0]];
    }
#endif
}

- (NSString*)coreTypeLabel:(EmulatorCoreType)type {
    if (type == EMULATOR_CORE_TYPE_NES) { return @"NES"; }
    if (type == EMULATOR_CORE_TYPE_GB) { return @"GB/GBC"; }
    return @"GBA";
}

- (void)applyControllerPreset {
    BOOL isNES = (_coreType == EMULATOR_CORE_TYPE_NES);
    BOOL isGB  = (_coreType == EMULATOR_CORE_TYPE_GB);
    NSInteger preset = isNES ? _nesControllerPreset : (isGB ? _gbControllerPreset : _gbaControllerPreset);
    BOOL compact = (preset == 1);
    CGFloat actionSize = compact ? 50.0 : 58.0;
    CGFloat shoulderSize = compact ? 0.0 : 40.0;
    CGFloat centerSize = compact ? 36.0 : 44.0;
    if (isNES || isGB) {
        actionSize = compact ? 54.0 : 62.0;
        centerSize = compact ? 34.0 : 42.0;
    }
    if (_aButtonSize) _aButtonSize.constant = actionSize;
    if (_bButtonSize) _bButtonSize.constant = actionSize;
    if (_lButtonSize) _lButtonSize.constant = shoulderSize;
    if (_rButtonSize) _rButtonSize.constant = shoulderSize;
    if (_startButtonSize) _startButtonSize.constant = centerSize;
    if (_selectButtonSize) _selectButtonSize.constant = centerSize;

    _lButton.hidden = isNES || isGB || compact;
    _rButton.hidden = isNES || isGB || compact;

    if (isNES) {
        [_aButton setTitle:@"A" forState:UIControlStateNormal];
        [_bButton setTitle:@"B" forState:UIControlStateNormal];
        [_startButton setTitle:@"START" forState:UIControlStateNormal];
        [_selectButton setTitle:@"SELECT" forState:UIControlStateNormal];
        _aButton.backgroundColor = [UIColor colorWithRed:0.58 green:0.16 blue:0.16 alpha:1.0];
        _bButton.backgroundColor = [UIColor colorWithRed:0.30 green:0.12 blue:0.52 alpha:1.0];
    } else if (isGB) {
        [_aButton setTitle:@"A" forState:UIControlStateNormal];
        [_bButton setTitle:@"B" forState:UIControlStateNormal];
        [_startButton setTitle:@"START" forState:UIControlStateNormal];
        [_selectButton setTitle:@"SELECT" forState:UIControlStateNormal];
        _aButton.backgroundColor = [UIColor colorWithRed:0.26 green:0.44 blue:0.78 alpha:1.0];
        _bButton.backgroundColor = [UIColor colorWithRed:0.14 green:0.29 blue:0.56 alpha:1.0];
    } else {
        [_aButton setTitle:@"A" forState:UIControlStateNormal];
        [_bButton setTitle:@"B" forState:UIControlStateNormal];
        [_startButton setTitle:@"START" forState:UIControlStateNormal];
        [_selectButton setTitle:@"SELECT" forState:UIControlStateNormal];
        _aButton.backgroundColor = [UIColor colorWithRed:0.18 green:0.20 blue:0.26 alpha:1.0];
        _bButton.backgroundColor = [UIColor colorWithRed:0.18 green:0.20 blue:0.26 alpha:1.0];
    }
    NSString* controllerText = [NSString stringWithFormat:@"%@ %@",
                                [self coreTypeLabel:_coreType], (compact ? @"簡易" : @"標準")];
    [self.controllerModeButton setTitle:[NSString stringWithFormat:@"コントローラー: %@", controllerText]
                               forState:UIControlStateNormal];
}

- (void)presentEmulatorMenu {
    __weak typeof(self) weakSelf = self;
    [self presentCustomMenuWithTitle:@"エミュレーター選択"
                             actions:@[
        @{ @"title": @"自動判定", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_forceCoreType = NO;
            [strongSelf.emulatorModeButton setTitle:@"エミュ: 自動" forState:UIControlStateNormal];
            [strongSelf appendLog:@"エミュレーター選択: 自動判定"];
        } copy]},
        @{ @"title": @"GBA 固定", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_forceCoreType = YES;
            strongSelf->_forcedCoreTypeValue = EMULATOR_CORE_TYPE_GBA;
            [strongSelf.emulatorModeButton setTitle:@"エミュ: GBA 固定" forState:UIControlStateNormal];
            [strongSelf appendLog:@"エミュレーター選択: GBA 固定"];
        } copy]},
        @{ @"title": @"NES 固定", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_forceCoreType = YES;
            strongSelf->_forcedCoreTypeValue = EMULATOR_CORE_TYPE_NES;
            [strongSelf.emulatorModeButton setTitle:@"エミュ: NES 固定" forState:UIControlStateNormal];
            [strongSelf appendLog:@"エミュレーター選択: NES 固定"];
        } copy]},
        @{ @"title": @"GB/GBC 固定", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_forceCoreType = YES;
            strongSelf->_forcedCoreTypeValue = EMULATOR_CORE_TYPE_GB;
            [strongSelf.emulatorModeButton setTitle:@"エミュ: GB/GBC 固定" forState:UIControlStateNormal];
            [strongSelf appendLog:@"エミュレーター選択: GB/GBC 固定"];
        } copy]},
    ]];
}

- (void)presentControllerMenu {
    __weak typeof(self) weakSelf = self;
    [self presentCustomMenuWithTitle:@"コントローラー設定"
                             actions:@[
        @{ @"title": @"GBA: 標準", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_gbaControllerPreset = 0;
            [strongSelf appendLog:@"コントローラー設定: GBA 標準"];
            [strongSelf applyControllerPreset];
        } copy]},
        @{ @"title": @"GBA: 簡易", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_gbaControllerPreset = 1;
            [strongSelf appendLog:@"コントローラー設定: GBA 簡易"];
            [strongSelf applyControllerPreset];
        } copy]},
        @{ @"title": @"NES: 標準", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_nesControllerPreset = 0;
            [strongSelf appendLog:@"コントローラー設定: NES 標準"];
            [strongSelf applyControllerPreset];
        } copy]},
        @{ @"title": @"NES: 簡易", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_nesControllerPreset = 1;
            [strongSelf appendLog:@"コントローラー設定: NES 簡易"];
            [strongSelf applyControllerPreset];
        } copy]},
        @{ @"title": @"GB/GBC: 標準", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_gbControllerPreset = 0;
            [strongSelf appendLog:@"コントローラー設定: GB/GBC 標準"];
            [strongSelf applyControllerPreset];
        } copy]},
        @{ @"title": @"GB/GBC: 簡易", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_gbControllerPreset = 1;
            [strongSelf appendLog:@"コントローラー設定: GB/GBC 簡易"];
            [strongSelf applyControllerPreset];
        } copy]},
    ]];
}

- (void)presentAppMenu {
    __weak typeof(self) weakSelf = self;
    [self presentCustomMenuWithTitle:@"マルチエミュメニュー"
                             actions:@[
        @{ @"title": @"映像設定 (画質/シェーダー/彩度)", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            [strongSelf presentVideoSettingsMenu];
        } copy]},
        @{ @"title": @"ステートセーブ", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf.statusLabel.text = @"選択: ステートセーブ";
            [strongSelf appendLog:@"メニュー選択: ステートセーブ"];
        } copy]},
        @{ @"title": @"ステートロード", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf.statusLabel.text = @"選択: ステートロード";
            [strongSelf appendLog:@"メニュー選択: ステートロード"];
        } copy]},
        @{ @"title": @"チート管理", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf.statusLabel.text = @"選択: チート管理";
            [strongSelf appendLog:@"メニュー選択: チート管理"];
        } copy]},
        @{ @"title": @"エミュ切替メニュー", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            [strongSelf presentEmulatorMenu];
        } copy]},
    ]];
}

- (void)presentVideoSettingsMenu {
    __weak typeof(self) weakSelf = self;
    [self presentCustomMenuWithTitle:@"映像設定"
                             actions:@[
        @{ @"title": @"画質: 高画質 (Bicubic)", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_upscaleMode = AURUpscaleModeQuality;
            [strongSelf.imageView setUpscaleMode:strongSelf->_upscaleMode];
            [strongSelf appendLog:@"映像設定: 高画質アップスケール"];
        } copy]},
        @{ @"title": @"画質: バランス (自動)", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_upscaleMode = AURUpscaleModeAuto;
            [strongSelf.imageView setUpscaleMode:strongSelf->_upscaleMode];
            [strongSelf appendLog:@"映像設定: バランス（自動）"];
        } copy]},
        @{ @"title": @"画質: 省電力 (高速)", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_upscaleMode = AURUpscaleModePerformance;
            [strongSelf.imageView setUpscaleMode:strongSelf->_upscaleMode];
            [strongSelf appendLog:@"映像設定: 省電力アップスケール"];
        } copy]},
        @{ @"title": @"見え方: 鮮やか", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_videoSaturation = 1.16f;
            strongSelf->_videoVibrance   = 0.40f;
            strongSelf->_videoContrast   = 1.10f;
            strongSelf->_videoSharpen    = 0.22f;
            strongSelf->_videoLutMix     = 0.22f;
            [strongSelf.imageView setPostProcessSaturation:strongSelf->_videoSaturation
                                                  vibrance:strongSelf->_videoVibrance
                                                  contrast:strongSelf->_videoContrast
                                                   sharpen:strongSelf->_videoSharpen
                                                    lutMix:strongSelf->_videoLutMix];
            [strongSelf appendLog:@"見え方補正: 鮮やかプリセット"];
        } copy]},
        @{ @"title": @"見え方: 標準", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_videoSaturation = 1.08f;
            strongSelf->_videoVibrance   = 0.30f;
            strongSelf->_videoContrast   = 1.06f;
            strongSelf->_videoSharpen    = 0.18f;
            strongSelf->_videoLutMix     = 0.15f;
            [strongSelf.imageView setPostProcessSaturation:strongSelf->_videoSaturation
                                                  vibrance:strongSelf->_videoVibrance
                                                  contrast:strongSelf->_videoContrast
                                                   sharpen:strongSelf->_videoSharpen
                                                    lutMix:strongSelf->_videoLutMix];
            [strongSelf appendLog:@"見え方補正: 標準プリセット"];
        } copy]},
        @{ @"title": @"見え方: ナチュラル", @"handler": [^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf) { return; }
            strongSelf->_videoSaturation = 1.00f;
            strongSelf->_videoVibrance   = 0.12f;
            strongSelf->_videoContrast   = 1.02f;
            strongSelf->_videoSharpen    = 0.08f;
            strongSelf->_videoLutMix     = 0.05f;
            [strongSelf.imageView setPostProcessSaturation:strongSelf->_videoSaturation
                                                  vibrance:strongSelf->_videoVibrance
                                                  contrast:strongSelf->_videoContrast
                                                   sharpen:strongSelf->_videoSharpen
                                                    lutMix:strongSelf->_videoLutMix];
            [strongSelf appendLog:@"見え方補正: ナチュラルプリセット"];
        } copy]},
    ]];
}

- (void)onCustomMenuActionTapped:(UIButton*)sender {
    NSInteger idx = sender.tag;
    [self dismissCustomMenu];
    if (idx < 0 || idx >= (NSInteger)_menuHandlers.count) { return; }
    id blockObj = _menuHandlers[(NSUInteger)idx];
    if (blockObj && blockObj != [NSNull null]) {
        void (^handler)(void) = blockObj;
        handler();
    }
}

- (void)presentCustomMenuWithTitle:(NSString*)title actions:(NSArray<NSDictionary*>*)actions {
    [self dismissCustomMenu];
    self.menuTitleLabel.text = title;
    [_menuHandlers removeAllObjects];

    for (UIView* v in self.menuStackView.arrangedSubviews) {
        [self.menuStackView removeArrangedSubview:v];
        [v removeFromSuperview];
    }

    NSInteger idx = 0;
    for (NSDictionary* action in actions) {
        NSString* actionTitle = action[@"title"];
        id handler = action[@"handler"];
        if (![actionTitle isKindOfClass:[NSString class]]) { continue; }
        UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
        button.translatesAutoresizingMaskIntoConstraints = NO;
        [button setTitle:actionTitle forState:UIControlStateNormal];
        button.tag = idx++;
        button.layer.cornerRadius = 10.0;
        button.backgroundColor = [UIColor colorWithRed:0.20 green:0.23 blue:0.31 alpha:1.0];
        button.tintColor = UIColor.whiteColor;
        button.titleLabel.font = [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];
        [button.heightAnchor constraintEqualToConstant:44].active = YES;
        [button addTarget:self action:@selector(onCustomMenuActionTapped:) forControlEvents:UIControlEventTouchUpInside];
        [self.menuStackView addArrangedSubview:button];
        [_menuHandlers addObject:(handler ?: [NSNull null])];
    }

    UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
    [closeButton setTitle:@"閉じる" forState:UIControlStateNormal];
    closeButton.layer.cornerRadius = 10.0;
    closeButton.backgroundColor = UIColor.tertiarySystemBackgroundColor;
    [closeButton.heightAnchor constraintEqualToConstant:42].active = YES;
    [closeButton addTarget:self action:@selector(dismissCustomMenu) forControlEvents:UIControlEventTouchUpInside];
    [self.menuStackView addArrangedSubview:closeButton];

    self.menuOverlayView.alpha = 0.0;
    self.menuOverlayView.hidden = NO;
    [UIView animateWithDuration:0.18 animations:^{
        self.menuOverlayView.alpha = 1.0;
    }];
}

- (void)dismissCustomMenu {
    if (self.menuOverlayView.hidden) { return; }
    self.menuOverlayView.hidden = YES;
    self.menuOverlayView.alpha = 0.0;
}

- (void)appendLog:(NSString*)message {
    if (message.length == 0) { return; }
    NSString* line = [NSString stringWithFormat:@"[log] %@\n", message];
    self.logTextView.text = [self.logTextView.text stringByAppendingString:line];
    NSRange bottom = NSMakeRange(self.logTextView.text.length - 1, 1);
    [self.logTextView scrollRangeToVisible:bottom];
}

- (void)onVirtualKeyDown:(UIButton*)sender {
    if (_core == NULL) { return; }
    EmulatorCore_SetKeyStatus(_core, (EmulatorKey)sender.tag, true);
}

- (void)onVirtualKeyUp:(UIButton*)sender {
    if (_core == NULL) { return; }
    EmulatorCore_SetKeyStatus(_core, (EmulatorKey)sender.tag, false);
}

@end
