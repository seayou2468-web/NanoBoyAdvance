#import "AUREmulatorViewController.h"
#import "AURInGameMenuViewController.h"
#import "../Views/AURControllerView.h"
#import "../Managers/AURSkinManager.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURExternalControllerManager.h"
#import "../Metal.h"
#import <QuartzCore/QuartzCore.h>
#import <stdarg.h>
#include <algorithm>
#include <array>
#include <cstring>

@interface AUREmulatorViewController () <AURControllerViewDelegate, AURExternalControllerDelegate, AURInGameMenuDelegate> {
    EmulatorCoreHandle* _core;
    EmulatorCoreType    _coreType;
    EmulatorVideoSpec   _videoSpec;
    BOOL                _running;
}
@property (nonatomic, strong) AURMetalView *imageView;
@property (nonatomic, strong) AURMetalView *ndsBottomImageView;
@property (nonatomic, strong) UIView *ndsContainerView;
@property (nonatomic, strong) AURControllerView *controllerView;
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) NSURL *romURL;
@property (nonatomic, strong) NSURL *logFileURL;
@property (nonatomic, assign) uint64_t frameCounter;
@property (nonatomic, assign) uint64_t lastStatusLogFrame;
@property (nonatomic, assign) uint64_t consecutiveMostlyBlackSamples;
@property (nonatomic, strong) NSLayoutConstraint *imageHeightConstraint;
@end

@implementation AUREmulatorViewController

static const NSUInteger kAURMaxLogFileBytes = 2 * 1024 * 1024;
static const uint64_t kAURStatusLogFrameInterval = 120;
static const uint64_t kAURBlackSampleInterval = 60;
static const uint64_t kAURBlackSampleWarnThreshold = 8;
static const uint64_t kAURBlackSampleWarmupFrames = 1200;

- (NSURL *)prepareLogFileURL {
    NSFileManager *fm = [NSFileManager defaultManager];
    NSURL *docs = [fm URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    if (!docs) return nil;

    NSURL *logsDir = [docs URLByAppendingPathComponent:@"Logs" isDirectory:YES];
    NSError *dirError = nil;
    [fm createDirectoryAtURL:logsDir withIntermediateDirectories:YES attributes:nil error:&dirError];
    if (dirError) {
        NSLog(@"[AUR][Emu] Failed to create Logs directory: %@", dirError.localizedDescription);
        return nil;
    }

    NSURL *logURL = [logsDir URLByAppendingPathComponent:@"mikage_runtime.log"];
    NSDictionary<NSFileAttributeKey, id> *attrs = [fm attributesOfItemAtPath:logURL.path error:nil];
    unsigned long long size = [attrs[NSFileSize] unsignedLongLongValue];
    if (size > kAURMaxLogFileBytes) {
        NSURL *oldURL = [logsDir URLByAppendingPathComponent:@"mikage_runtime.previous.log"];
        [fm removeItemAtURL:oldURL error:nil];
        [fm moveItemAtURL:logURL toURL:oldURL error:nil];
    }

    if (![fm fileExistsAtPath:logURL.path]) {
        [@"" writeToURL:logURL atomically:YES encoding:NSUTF8StringEncoding error:nil];
    }
    return logURL;
}

- (void)emuLog:(NSString *)format, ... NS_FORMAT_FUNCTION(1,2) {
    va_list args;
    va_start(args, format);
    NSString *body = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    NSString *line = [NSString stringWithFormat:@"[AUR][Emu] %@", body];
    NSLog(@"%@", line);

    if (!self.logFileURL) return;

    static NSDateFormatter *fmt = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        fmt = [[NSDateFormatter alloc] init];
        fmt.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
        fmt.dateFormat = @"yyyy-MM-dd HH:mm:ss.SSS";
    });
    NSString *timestamp = [fmt stringFromDate:[NSDate date]];
    NSString *record = [NSString stringWithFormat:@"%@ %@\n", timestamp, line];

    NSFileHandle *fh = [NSFileHandle fileHandleForWritingToURL:self.logFileURL error:nil];
    if (!fh) return;
    @try {
        [fh seekToEndOfFile];
        [fh writeData:[record dataUsingEncoding:NSUTF8StringEncoding]];
    } @catch (__unused NSException *ex) {
    } @finally {
        [fh closeFile];
    }
}

- (BOOL)validateROMURL:(NSURL *)url reason:(NSString * _Nullable __autoreleasing *)reason {
    if (!url || !url.isFileURL) {
        if (reason) *reason = @"ROM URL が無効です（file URL ではありません）";
        return NO;
    }

    NSString *path = url.path;
    NSFileManager *fm = [NSFileManager defaultManager];
    BOOL isDir = NO;
    if (![fm fileExistsAtPath:path isDirectory:&isDir] || isDir) {
        if (reason) *reason = @"ROM パスが存在しないか、通常ファイルではありません";
        return NO;
    }
    if (![fm isReadableFileAtPath:path]) {
        if (reason) *reason = @"ROM ファイルに読み取り権限がありません";
        return NO;
    }

    NSError *attrError = nil;
    NSDictionary<NSFileAttributeKey, id> *attrs = [fm attributesOfItemAtPath:path error:&attrError];
    unsigned long long fileSize = [attrs fileSize];
    if (attrError) {
        if (reason) *reason = [NSString stringWithFormat:@"ROM 属性の取得に失敗: %@", attrError.localizedDescription];
        return NO;
    }
    if (fileSize == 0) {
        if (reason) *reason = @"ROM ファイルサイズが 0 byte です";
        return NO;
    }

    NSFileHandle *fh = [NSFileHandle fileHandleForReadingAtPath:path];
    NSData *headerData = nil;
    if (fh) {
        @try {
            headerData = [fh readDataOfLength:4];
        } @catch (__unused NSException *ex) {
            headerData = nil;
        } @finally {
            [fh closeFile];
        }
    }
    if (headerData.length >= 4) {
        const uint8_t *bytes = (const uint8_t *)headerData.bytes;
        const BOOL isZip = bytes[0] == 0x50 && bytes[1] == 0x4B && bytes[2] == 0x03 && bytes[3] == 0x04;
        const BOOL is7z = bytes[0] == 0x37 && bytes[1] == 0x7A && bytes[2] == 0xBC && bytes[3] == 0xAF;
        const BOOL isRar = bytes[0] == 0x52 && bytes[1] == 0x61 && bytes[2] == 0x72 && bytes[3] == 0x21;
        if (isZip || is7z || isRar) {
            if (reason) *reason = @"ROMが圧縮アーカイブのままです。zip/7z/rar を展開して .3ds/.cci/.cxi/.app/.ncch/.3dsx/.elf/.axf を指定してください";
            return NO;
        }
    }

    if ([path containsString:@"/Documents/Data/Application/"]) {
        [self emuLog:@"ROM path looks re-imported/nested inside Documents: %@ (this is supported but may indicate duplicate import history)", path];
    }

    [self emuLog:@"ROM preflight ok. path=%@ size=%llu", path, fileSize];
    return YES;
}

- (instancetype)initWithROMURL:(NSURL *)romURL coreType:(EmulatorCoreType)coreType {
    self = [super init];
    if (self) {
        _romURL = romURL;
        _coreType = coreType;
        _frameCounter = 0;
        _lastStatusLogFrame = 0;
    }
    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];
    self.logFileURL = [self prepareLogFileURL];
    NSString *romPath = self.romURL.path.length > 0 ? self.romURL.path : @"(null)";
    const char *coreNameC = EmulatorCoreTypeName(_coreType);
    NSString *coreName = coreNameC ? [NSString stringWithUTF8String:coreNameC] : @"(unknown)";
    [self emuLog:@"Session start. core=%d core_name=%@ rom=%@", (int)_coreType, coreName, romPath];

    // Setup Metal View(s)
    self.imageView = [[AURMetalView alloc] initWithFrame:CGRectZero];
    [self.imageView setFramePixelFormat:AURFramePixelFormatRGBA8888];
    [self.view addSubview:self.imageView];

    // Menu Button (Glassmorphic)
    UIVisualEffectView *blurEffectView = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    blurEffectView.frame = CGRectMake(0, 0, 40, 40);
    blurEffectView.layer.cornerRadius = 20;
    blurEffectView.clipsToBounds = YES;

    UIButton *menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [menuButton setImage:[UIImage systemImageNamed:@"line.3.horizontal.circle.fill"] forState:UIControlStateNormal];
    menuButton.tintColor = [UIColor colorWithRed:0.0 green:1.0 blue:0.76 alpha:1.0];
    [menuButton addTarget:self action:@selector(menuTapped) forControlEvents:UIControlEventTouchUpInside];
    [blurEffectView.contentView addSubview:menuButton];
    menuButton.frame = blurEffectView.bounds;

    [self.view addSubview:blurEffectView];
    blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [blurEffectView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:10],
        [blurEffectView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-16],
        [blurEffectView.widthAnchor constraintEqualToConstant:40],
        [blurEffectView.heightAnchor constraintEqualToConstant:40]
    ]];

    // Setup Controller View
    self.controllerView = [[AURControllerView alloc] initWithFrame:CGRectZero];
    self.controllerView.delegate = self;
    [self.view addSubview:self.controllerView];

    // Layout
    self.imageView.translatesAutoresizingMaskIntoConstraints = NO;
    self.controllerView.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat aspectMultiplier = (480.0 / 400.0);
    self.imageHeightConstraint = [self.imageView.heightAnchor constraintEqualToAnchor:self.imageView.widthAnchor multiplier:aspectMultiplier];
    self.imageHeightConstraint.priority = UILayoutPriorityRequired;

    NSLayoutConstraint *imageMaxHeight = [self.imageView.heightAnchor constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide.heightAnchor multiplier:0.55];
    NSLayoutConstraint *controllerMinHeight = [self.controllerView.heightAnchor constraintGreaterThanOrEqualToConstant:180.0];
    NSLayoutConstraint *imageBottomToController = [self.imageView.bottomAnchor constraintLessThanOrEqualToAnchor:self.controllerView.topAnchor constant:-8.0];
    NSLayoutConstraint *controllerTopFromImage = [self.controllerView.topAnchor constraintGreaterThanOrEqualToAnchor:self.imageView.bottomAnchor constant:8.0];

    NSLayoutConstraint *preferredImageWidth = [self.imageView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor constant:-40.0];
    preferredImageWidth.priority = UILayoutPriorityDefaultHigh;

    [NSLayoutConstraint activateConstraints:@[
        [self.imageView.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:20.0],
        [self.imageView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [self.imageView.leadingAnchor constraintGreaterThanOrEqualToAnchor:self.view.leadingAnchor constant:20.0],
        [self.imageView.trailingAnchor constraintLessThanOrEqualToAnchor:self.view.trailingAnchor constant:-20.0],
        [self.imageView.widthAnchor constraintLessThanOrEqualToAnchor:self.view.widthAnchor constant:-40.0],
        preferredImageWidth,
        self.imageHeightConstraint,
        imageMaxHeight,
        imageBottomToController,

        controllerTopFromImage,
        controllerMinHeight,
        [self.controllerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.controllerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.controllerView.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor]
    ]];

    [self startEmulator];
}

- (void)startEmulator {
    [self stopEmulator];
    self.frameCounter = 0;
    self.lastStatusLogFrame = 0;
    self.consecutiveMostlyBlackSamples = 0;
    _core = EmulatorCore_Create(_coreType);
    if (!_core) {
        [self emuLog:@"Failed to create core: %d", (int)_coreType];
        return;
    }
    if (_coreType == EMULATOR_CORE_TYPE_3DS) {
        [self emuLog:@"Renderer note: 3DS core uses software rasterizer path in this build (RendererSw fallback)."];
    }

    // Load 3DS BIOS files from DatabaseManager (persisted in Documents)
    NSString *boot9 = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_boot9"];
    NSString *boot11 = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_boot11"];
    NSString *firmware = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_firmware"];
    NSString *sharedFont = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_shared_font"];
    if (firmware.length > 0) {
        [self emuLog:@"3ds_firmware was set (%@), but this core currently expects boot9/boot11 and sysdata keys/seeddb. Standalone firmware files are not directly consumed in this load path.", firmware.lastPathComponent];
    }

    void (^loadBiosIfPresent)(NSString *, NSString *) = ^(NSString *identifier, NSString *path) {
        if (path.length == 0) return;
        if (!EmulatorCore_LoadBIOSFromPath(_core, path.UTF8String)) {
            const char *biosError = EmulatorCore_GetLastError(_core);
            [self emuLog:@"BIOS load failed (%@): %s", identifier, biosError ? biosError : "unknown error"];
        } else {
            NSString *displayName = path.lastPathComponent.length > 0 ? path.lastPathComponent : @"(unknown)";
            [self emuLog:@"BIOS load ok (%@): %@", identifier, displayName];
        }
    };
    loadBiosIfPresent(@"3ds_boot9", boot9);
    loadBiosIfPresent(@"3ds_boot11", boot11);
    loadBiosIfPresent(@"3ds_firmware", firmware);
    loadBiosIfPresent(@"3ds_shared_font", sharedFont);

    NSString *boot9Display = boot9.length > 0 ? boot9 : @"(null)";
    NSString *boot11Display = boot11.length > 0 ? boot11 : @"(null)";
    NSString *firmwareDisplay = firmware.length > 0 ? firmware : @"(null)";
    NSString *sharedFontDisplay = sharedFont.length > 0 ? sharedFont : @"(null)";
    [self emuLog:@"BIOS paths boot9=%@ boot11=%@ firmware=%@ shared_font=%@",
     boot9Display, boot11Display, firmwareDisplay, sharedFontDisplay];
    if (!boot9 && !boot11 && !firmware) {
        [self emuLog:@"3DS BIOS/Firmware not set. Attempting HLE boot without external firmware."];
    }

    NSString *ext = self.romURL.pathExtension.lowercaseString;
    NSSet<NSString *> *allowed = [NSSet setWithArray:@[@"3ds", @"cci", @"cxi", @"app", @"ncch", @"3dsx", @"elf", @"axf"]];
    if ([ext isEqualToString:@"toml"] || [self.romURL.lastPathComponent.lowercaseString isEqualToString:@"config.toml"]) {
        [self emuLog:@"Refusing to load config file as ROM: %@", self.romURL.path];
        [self stopEmulator];
        return;
    }
    if (![allowed containsObject:ext]) {
        [self emuLog:@"Unsupported ROM extension: %@", ext];
        [self stopEmulator];
        return;
    }
    if ([ext isEqualToString:@"3ds"] || [ext isEqualToString:@"cci"] ||
        [ext isEqualToString:@"cxi"] || [ext isEqualToString:@"app"] ||
        [ext isEqualToString:@"ncch"]) {
        [self emuLog:@"ROM format %@ is loaded as a container directly (NCCH/RomFS are parsed by core at runtime; no pre-extraction step).", ext];
    }
    NSString *preflightReason = nil;
    if (![self validateROMURL:self.romURL reason:&preflightReason]) {
        NSString *reasonText = preflightReason.length > 0 ? preflightReason : @"unknown";
        [self emuLog:@"ROM preflight failed: %@", reasonText];
        [self stopEmulator];
        return;
    }

    const char *path = self.romURL.path.fileSystemRepresentation;
    if (path && EmulatorCore_LoadROMFromPath(_core, path)) {
        EmulatorCore_GetVideoSpec(_core, &_videoSpec);
        NSString *pixelFormat = (_videoSpec.pixel_format == EMULATOR_PIXEL_FORMAT_ARGB8888) ? @"ARGB8888" : @"RGBA8888";
        [self emuLog:@"ROM loaded successfully. video=%ux%u pixel_format=%d(%@)",
         (unsigned)_videoSpec.width, (unsigned)_videoSpec.height, (int)_videoSpec.pixel_format, pixelFormat];
        AURFramePixelFormat framePixelFormat = (_videoSpec.pixel_format == EMULATOR_PIXEL_FORMAT_ARGB8888)
            ? AURFramePixelFormatBGRA8888
            : AURFramePixelFormatRGBA8888;
        [self.imageView setFramePixelFormat:framePixelFormat];
        _running = YES;
        self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(gameLoop)];
        [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    } else {
        const char *loadError = EmulatorCore_GetLastError(_core);
        [self emuLog:@"ROM load failed (%@): %s", self.romURL.lastPathComponent, loadError ? loadError : "unknown error"];
        [self stopEmulator];
    }
}

- (void)stopEmulator {
    _running = NO;
    [self.displayLink invalidate];
    self.displayLink = nil;
    if (_core) {
        [self emuLog:@"Destroying emulator core."];
        EmulatorCore_Destroy(_core);
        _core = nullptr;
    }
}

- (void)gameLoop {
    if (!_running || !_core) return;
    self.frameCounter += 1;
    EmulatorCore_StepFrame(_core);
    const char *stepError = EmulatorCore_GetLastError(_core);
    if (stepError && stepError[0] != '\0') {
        [self emuLog:@"Frame %llu step warning: %s", self.frameCounter, stepError];
    }

    size_t pixelCount = 0;
    const uint32_t* frameRGBA = EmulatorCore_GetFrameBufferRGBA(_core, &pixelCount);
    if (!frameRGBA) {
        [self emuLog:@"Frame %llu missing framebuffer pointer from core.", self.frameCounter];
        return;
    }

    const size_t expectedPixels = (size_t)_videoSpec.width * (size_t)_videoSpec.height;
    if (pixelCount < expectedPixels) {
        [self emuLog:@"Frame %llu framebuffer underflow: got %zu expected %zu", self.frameCounter, pixelCount, expectedPixels];
        return;
    }

    if (self.frameCounter >= kAURBlackSampleWarmupFrames &&
        (self.frameCounter % kAURBlackSampleInterval) == 0) {
        const size_t sampleCols = 8;
        const size_t sampleRows = 8;
        const size_t stepX = std::max<size_t>(1, _videoSpec.width / sampleCols);
        const size_t stepY = std::max<size_t>(1, _videoSpec.height / sampleRows);
        size_t nonBlackSamples = 0;
        for (size_t y = 0; y < _videoSpec.height; y += stepY) {
            for (size_t x = 0; x < _videoSpec.width; x += stepX) {
                const uint32_t px = frameRGBA[y * _videoSpec.width + x];
                if ((px & 0x00FFFFFFu) != 0) {
                    nonBlackSamples += 1;
                }
            }
        }

        if (nonBlackSamples == 0) {
            self.consecutiveMostlyBlackSamples += 1;
            if (self.consecutiveMostlyBlackSamples >= kAURBlackSampleWarnThreshold) {
                [self emuLog:@"Frame sample diagnostic: framebuffer remains mostly black for %llu checks after warmup(%llu frames). Possible causes: title-specific boot stall, missing keys/seeddb for encrypted content, or missing GPU screen buffers (check mikage_runtime.log).",
                 self.consecutiveMostlyBlackSamples, kAURBlackSampleWarmupFrames];
                self.consecutiveMostlyBlackSamples = 0;
            }
        } else {
            self.consecutiveMostlyBlackSamples = 0;
        }
    }

    if ((self.frameCounter - self.lastStatusLogFrame) >= kAURStatusLogFrameInterval) {
        self.lastStatusLogFrame = self.frameCounter;
        [self emuLog:@"Frame %llu status ok. pixels=%zu expected=%zu", self.frameCounter, pixelCount, expectedPixels];
    }

    [self.imageView displayFrameRGBA:frameRGBA width:_videoSpec.width height:_videoSpec.height];
}

- (void)dealloc {
    [self stopEmulator];
}

- (void)viewDidDisappear:(BOOL)animated {
    [super viewDidDisappear:animated];
    [self stopEmulator];
}

#pragma mark - AURControllerViewDelegate

- (void)controllerViewDidPressKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, true);
}

- (void)controllerViewDidReleaseKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, false);
}

#pragma mark - AURExternalControllerDelegate

- (void)externalControllerDidPressKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, true);
}

- (void)externalControllerDidReleaseKey:(EmulatorKey)key {
    if (_core) EmulatorCore_SetKeyStatus(_core, key, false);
}

- (void)menuTapped {
    AURInGameMenuViewController *menuVC = [[AURInGameMenuViewController alloc] init];
    menuVC.delegate = self;
    menuVC.modalPresentationStyle = UIModalPresentationOverFullScreen;
    menuVC.modalTransitionStyle = UIModalTransitionStyleCrossDissolve;
    [self presentViewController:menuVC animated:YES completion:nil];
}

#pragma mark - AURInGameMenuDelegate

- (void)inGameMenuDidSelectAction:(NSString *)action {
    if ([action isEqualToString:@"Quit Game"]) {
        [self stopEmulator];
        if (self.presentingViewController) {
            [self.presentingViewController dismissViewControllerAnimated:YES completion:nil];
        } else if (self.navigationController) {
            [self.navigationController popViewControllerAnimated:YES];
        } else {
            [self dismissViewControllerAnimated:YES completion:nil];
        }
    }
}

@end
