#import "AURViewController.h"
#import "Metal.h"

#import <QuartzCore/QuartzCore.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

extern "C" {
#include "../../src/core/gba_core_c_api.h"
}

// GBA 画面解像度定数
static const NSUInteger kGBAWidth  = 240;
static const NSUInteger kGBAHeight = 160;
static const NSUInteger kGBAPixels = kGBAWidth * kGBAHeight;

@interface AURViewController () <UIDocumentPickerDelegate>
@property (nonatomic, strong) AURMetalView*  imageView;
@property (nonatomic, strong) UILabel*       statusLabel;
@property (nonatomic, strong) UIButton*      selectROMButton;
@property (nonatomic, strong) UITextView*    logTextView;
@property (nonatomic, strong) CADisplayLink* displayLink;
- (void)appendLog:(NSString*)message;
@end

@implementation AURViewController {
    GBACoreHandle*  _core;
    BOOL            _running;
    NSUInteger      _frameCounter;
    uint32_t        _lastFrameSample;
    NSUInteger      _staleFrameCount;
    BOOL            _hasFrameSample;
}

// MARK: - View Lifecycle

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;

    // ImageView
    self.imageView = [[AURMetalView alloc] initWithFrame:CGRectZero];
    self.imageView.backgroundColor = UIColor.blackColor;
    self.imageView.opaque = YES;
    self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.imageView];

    // Status label
    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.statusLabel.textColor = UIColor.whiteColor;
    self.statusLabel.numberOfLines = 0;
    self.statusLabel.textAlignment = NSTextAlignmentCenter;
    self.statusLabel.font =
        [UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightRegular];
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
    [self.view addSubview:self.selectROMButton];

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

    [NSLayoutConstraint activateConstraints:@[
        // ImageView: 上部、GBA アスペクト比
        [self.imageView.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
        [self.imageView.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
        [self.imageView.topAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
        [self.imageView.heightAnchor
            constraintEqualToAnchor:self.imageView.widthAnchor
            multiplier:(CGFloat)kGBAHeight / (CGFloat)kGBAWidth],

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

        // Select ROM button: status label 直下・中央
        [self.selectROMButton.centerXAnchor
            constraintEqualToAnchor:self.view.centerXAnchor],
        [self.selectROMButton.topAnchor
            constraintEqualToAnchor:self.statusLabel.bottomAnchor
            constant:20],
        [self.selectROMButton.widthAnchor
            constraintGreaterThanOrEqualToConstant:200],

        // Log view: ボタン下〜画面下
        [self.logTextView.leadingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
            constant:12],
        [self.logTextView.trailingAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
            constant:-12],
        [self.logTextView.topAnchor
            constraintEqualToAnchor:self.selectROMButton.bottomAnchor
            constant:12],
        [self.logTextView.bottomAnchor
            constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor
            constant:-8],
    ]];

    [self appendLog:@"ROM は毎回選択モードです（bookmark 復元なし）"];
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    [self startDisplayLink];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    [self stopDisplayLink];
}

- (void)dealloc {
    [self stopDisplayLink];
    [self teardownCore];
}

// MARK: - ROM Picker

- (void)presentROMPicker {
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

// MARK: - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    NSURL* url = urls.firstObject;
    if (url == nil) { return; }
    [self appendLog:[NSString stringWithFormat:@"ROM 選択: %@", url.path ?: url.lastPathComponent]];

    BOOL accessing = [url startAccessingSecurityScopedResource];

    [self loadROMFromURL:url];

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

    _core = GBA_Create();
    if (_core == NULL) {
        self.statusLabel.text = @"GBA_Create failed";
        [self appendLog:@"GBA_Create failed"];
        return;
    }
    [self appendLog:@"GBA_Create success"];

    NSNumber* fileSize = nil;
    [url getResourceValue:&fileSize forKey:NSURLFileSizeKey error:nil];
    [self appendLog:[NSString stringWithFormat:@"ROM サイズ: %@ bytes", fileSize ?: @0]];

    if (!GBA_LoadROMFromPath(_core, url.fileSystemRepresentation)) {
        const char* err = GBA_GetLastError(_core);
        self.statusLabel.text =
            [NSString stringWithFormat:@"ROM 読み込み失敗:\n%s", err ? err : "unknown"];
        [self appendLog:[NSString stringWithFormat:@"ROM 読み込み失敗: %s", err ? err : "unknown"]];
        GBA_Destroy(_core);
        _core = NULL;
        return;
    }

    _running = YES;
    _frameCounter = 0;
    _staleFrameCount = 0;
    _hasFrameSample = NO;
    _lastFrameSample = 0;
    self.statusLabel.text =
        [NSString stringWithFormat:@"▶ %@", url.lastPathComponent];
    [self appendLog:[NSString stringWithFormat:@"ROM 読み込み成功: %@", url.lastPathComponent]];

    [self stepFrameAndRender];
    [self startDisplayLink];
}

- (void)teardownCore {
    if (_core != NULL) {
        GBA_Destroy(_core);
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
        dl.preferredFrameRateRange = CAFrameRateRangeMake(30.0f, 30.0f, 30.0f);
    } else {
        dl.preferredFramesPerSecond = 30;
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
    CFTimeInterval t0 = CACurrentMediaTime();

    GBA_StepFrame(_core);
    CFTimeInterval t1 = CACurrentMediaTime();

    size_t pixelCount = 0;
    const uint32_t* frameRGBA = GBA_GetFrameBufferRGBA(_core, &pixelCount);
    if (frameRGBA == nullptr || pixelCount < kGBAPixels) {
        self.statusLabel.text = @"Failed to fetch framebuffer";
        const char* err = GBA_GetLastError(_core);
        [self appendLog:[NSString stringWithFormat:@"FrameBuffer 取得失敗: %s", err ? err : "unknown"]];
        return;
    }
    // 取得済みフレームが更新されていない可能性を検知する簡易サンプル
    uint32_t sample = 2166136261u;
    const size_t stride = 97;
    for (size_t i = 0; i < kGBAPixels; i += stride) {
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

    [self.imageView displayFrameRGBA:frameRGBA width:kGBAWidth height:kGBAHeight];
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
}

- (void)appendLog:(NSString*)message {
    if (message.length == 0) { return; }
    NSString* line = [NSString stringWithFormat:@"[log] %@\n", message];
    self.logTextView.text = [self.logTextView.text stringByAppendingString:line];
    NSRange bottom = NSMakeRange(self.logTextView.text.length - 1, 1);
    [self.logTextView scrollRangeToVisible:bottom];
}

@end
