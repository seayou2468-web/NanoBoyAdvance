#import "../Views/AURBackgroundView.h"
#import "AURLibraryViewController.h"
#import "AURSettingsViewController.h"
#import "AUREmulatorViewController.h"
#import "../Views/AURGameCollectionViewCell.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURBoxArtManager.h"
#import "../Models/AURGame.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <Compression/Compression.h>

static BOOL AURCoreTypeForExtension(NSString *ext, EmulatorCoreType *outType) {
    if (ext == nil || outType == nil) {
        return NO;
    }
    NSString *lower = ext.lowercaseString;
    if ([lower isEqualToString:@"3ds"] || [lower isEqualToString:@"cci"] || [lower isEqualToString:@"cxi"] ||
        [lower isEqualToString:@"cia"] || [lower isEqualToString:@"3dsx"] || [lower isEqualToString:@"app"] ||
        [lower isEqualToString:@"elf"] || [lower isEqualToString:@"axf"] || [lower isEqualToString:@"bin"]) {
        *outType = EMULATOR_CORE_TYPE_3DS;
        return YES;
    }
    return NO;
}

static uint16_t AURReadLE16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t AURReadLE32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static NSData * _Nullable AURInflateRawDeflateWithCompressionFramework(const uint8_t *input, size_t inputSize) {
    if (!input || inputSize == 0) {
        return nil;
    }

    compression_stream stream;
    memset(&stream, 0, sizeof(stream));
    if (compression_stream_init(&stream, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB) != COMPRESSION_STATUS_OK) {
        return nil;
    }

    NSMutableData *outData = [NSMutableData data];
    if (!outData) {
        compression_stream_destroy(&stream);
        return nil;
    }

    static const size_t kChunkSize = 64 * 1024;
    stream.src_ptr = input;
    stream.src_size = inputSize;

    while (true) {
        NSUInteger oldLen = outData.length;
        [outData setLength:oldLen + kChunkSize];
        stream.dst_ptr = (uint8_t *)outData.mutableBytes + oldLen;
        stream.dst_size = kChunkSize;

        compression_status status = compression_stream_process(&stream, COMPRESSION_STREAM_FINALIZE);
        if (status == COMPRESSION_STATUS_ERROR) {
            compression_stream_destroy(&stream);
            return nil;
        }

        const size_t written = kChunkSize - stream.dst_size;
        [outData setLength:oldLen + written];

        if (status == COMPRESSION_STATUS_END) {
            break;
        }

        if (status != COMPRESSION_STATUS_OK) {
            compression_stream_destroy(&stream);
            return nil;
        }
    }

    compression_stream_destroy(&stream);
    return outData;
}

@interface AURLibraryViewController () <UICollectionViewDelegate, UICollectionViewDataSource, UIDocumentPickerDelegate>
@property (nonatomic, strong) UICollectionView *collectionView;
@property (nonatomic, strong) UISegmentedControl *segmentedControl;
@property (nonatomic, strong) NSArray<AURGame *> *games;
@property (nonatomic, strong) AURBackgroundView *backgroundView;
- (BOOL)extractFirstROMFromZipURL:(NSURL *)zipURL
                     extractedURL:(NSURL * _Nullable * _Nonnull)outURL
                         coreType:(EmulatorCoreType *)outCoreType
                            title:(NSString * _Nullable * _Nonnull)outTitle;
@end

@implementation AURLibraryViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor blackColor];

    // Background Aurora View
    self.backgroundView = [[AURBackgroundView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:self.backgroundView];
    self.backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.backgroundView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.backgroundView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [self.backgroundView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.backgroundView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]
    ]];

    // Glassmorphic Navigation Bar appearance
    UINavigationBarAppearance *appearance = [[UINavigationBarAppearance alloc] init];
    [appearance configureWithTransparentBackground];
    appearance.backgroundEffect = [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    appearance.titleTextAttributes = @{NSForegroundColorAttributeName: [UIColor whiteColor], NSFontAttributeName: [UIFont systemFontOfSize:22 weight:UIFontWeightBold]};
    self.navigationItem.standardAppearance = appearance;
    self.navigationItem.scrollEdgeAppearance = appearance;
    self.title = @"Aurora";

    // ... (rest of the setup)

    // Setup Segmented Control for Systems (Glassmorphic)
    self.segmentedControl = [[UISegmentedControl alloc] initWithItems:@[@"3DS"]];
    self.segmentedControl.selectedSegmentIndex = 0;
    [self.segmentedControl addTarget:self action:@selector(segmentChanged:) forControlEvents:UIControlEventValueChanged];
    self.navigationItem.titleView = self.segmentedControl;

    // Appearance
    self.segmentedControl.selectedSegmentTintColor = [UIColor colorWithWhite:1.0 alpha:0.2];
    [self.segmentedControl setTitleTextAttributes:@{NSForegroundColorAttributeName: [UIColor whiteColor]} forState:UIControlStateNormal];

    // Settings Button (Modern)
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"line.3.horizontal.circle.fill"] style:UIBarButtonItemStylePlain target:self action:@selector(settingsTapped)];
    self.navigationItem.leftBarButtonItem.tintColor = [UIColor colorWithRed:0.0 green:1.0 blue:0.76 alpha:1.0]; // Aurora Cyan

    // Add Button
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"plus.circle.fill"] style:UIBarButtonItemStylePlain target:self action:@selector(addTapped)];
    self.navigationItem.rightBarButtonItem.tintColor = [UIColor colorWithRed:0.0 green:1.0 blue:0.76 alpha:1.0];

    // Setup Collection View (Compositional Layout)
    UICollectionViewCompositionalLayout *layout = [self _createLayout];
    self.collectionView = [[UICollectionView alloc] initWithFrame:self.view.bounds collectionViewLayout:layout];
    self.collectionView.delegate = self;
    self.collectionView.dataSource = self;
    self.collectionView.backgroundColor = [UIColor clearColor];
    [self.collectionView registerClass:[AURGameCollectionViewCell class] forCellWithReuseIdentifier:@"GameCell"];
    [self.view addSubview:self.collectionView];

    self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.collectionView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [self.collectionView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
        [self.collectionView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [self.collectionView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor]
    ]];

    [self reloadData];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    [self reloadData];
}

- (UICollectionViewCompositionalLayout *)_createLayout {
    return [[UICollectionViewCompositionalLayout alloc] initWithSectionProvider:^NSCollectionLayoutSection * _Nullable(NSInteger sectionIndex, id<NSCollectionLayoutEnvironment>  _Nonnull layoutEnvironment) {
        NSCollectionLayoutSize *itemSize = [NSCollectionLayoutSize sizeWithWidthDimension:[NSCollectionLayoutDimension fractionalWidthDimension:0.33] heightDimension:[NSCollectionLayoutDimension fractionalHeightDimension:1.0]];
        NSCollectionLayoutItem *item = [NSCollectionLayoutItem itemWithLayoutSize:itemSize];
        item.contentInsets = NSDirectionalEdgeInsetsMake(8, 8, 8, 8);

        NSCollectionLayoutSize *groupSize = [NSCollectionLayoutSize sizeWithWidthDimension:[NSCollectionLayoutDimension fractionalWidthDimension:1.0] heightDimension:[NSCollectionLayoutDimension fractionalWidthDimension:0.5]];
        NSCollectionLayoutGroup *group = [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize subitems:@[item]];

        NSCollectionLayoutSection *section = [NSCollectionLayoutSection sectionWithGroup:group];
        section.contentInsets = NSDirectionalEdgeInsetsMake(16, 8, 16, 8);
        return section;
    }];
}

- (void)segmentChanged:(UISegmentedControl *)sender {
    [self reloadData];
}

- (void)reloadData {
    EmulatorCoreType type = EMULATOR_CORE_TYPE_3DS;
    self.games = [[AURDatabaseManager sharedManager] gamesForCoreType:type];
    [self.collectionView reloadData];
}

- (void)settingsTapped {
    AURSettingsViewController *settingsVC = [[AURSettingsViewController alloc] init];
    UINavigationController *nav = [[UINavigationController alloc] initWithRootViewController:settingsVC];
    [self presentViewController:nav animated:YES completion:nil];
}

- (void)addTapped {
    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
    picker.delegate = self;
    [self presentViewController:picker animated:YES completion:nil];
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (url) {
        AURGame *game = [[AURGame alloc] init];
        NSString *ext = [[url pathExtension] lowercaseString];
        if ([ext isEqualToString:@"zip"]) {
            NSURL *extractedURL = nil;
            EmulatorCoreType extractedType = EMULATOR_CORE_TYPE_3DS;
            NSString *extractedTitle = nil;
            if ([self extractFirstROMFromZipURL:url extractedURL:&extractedURL coreType:&extractedType title:&extractedTitle]) {
                game.romPath = extractedURL.path;
                game.coreType = extractedType;
                NSString *fallbackTitle = [[extractedURL lastPathComponent] stringByDeletingPathExtension];
                game.title = extractedTitle.length > 0 ? extractedTitle : fallbackTitle;
            } else {
                return;
            }
        } else {
            EmulatorCoreType detectedType = EMULATOR_CORE_TYPE_3DS;
            if (!AURCoreTypeForExtension(ext, &detectedType)) {
                detectedType = EMULATOR_CORE_TYPE_3DS;
            }
            NSString *persistedROMPath = [[AURDatabaseManager sharedManager] persistImportedROMAtURL:url];
            if (persistedROMPath.length == 0) {
                return;
            }
            game.coreType = detectedType;
            game.romPath = persistedROMPath;
            game.title = [[url lastPathComponent] stringByDeletingPathExtension];
        }

        [[AURDatabaseManager sharedManager] addGame:game];
        [self reloadData];
    }
}

- (BOOL)extractFirstROMFromZipURL:(NSURL *)zipURL
                     extractedURL:(NSURL * _Nullable * _Nonnull)outURL
                         coreType:(EmulatorCoreType *)outCoreType
                            title:(NSString * _Nullable * _Nonnull)outTitle {
    if (!zipURL || !outURL || !outCoreType || !outTitle) {
        NSLog(@"[AUR][Library] ZIP extraction failed: invalid arguments");
        return NO;
    }

    *outURL = nil;
    *outTitle = nil;

    NSData *zipData = [NSData dataWithContentsOfFile:zipURL.path];
    if (!zipData || zipData.length == 0) {
        NSLog(@"[AUR][Library] ZIP extraction failed: cannot read zip data at %@", zipURL.path);
        return NO;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *virtualNANDImportedRoot = [documents stringByAppendingPathComponent:@"Emulator Files/SharedFiles/NAND/title/00040000/imported/extracted"];
    NSString *importDir = [virtualNANDImportedRoot stringByAppendingPathComponent:zipURL.lastPathComponent.stringByDeletingPathExtension];
    [fileManager removeItemAtPath:importDir error:nil];
    [fileManager createDirectoryAtPath:importDir withIntermediateDirectories:YES attributes:nil error:nil];

    const uint8_t *bytes = (const uint8_t *)zipData.bytes;
    const size_t totalSize = zipData.length;
    size_t offset = 0;
    BOOL success = NO;

    while (offset + 30 <= totalSize) {
        const uint32_t signature = AURReadLE32(bytes + offset);
        if (signature == 0x02014B50 || signature == 0x06054B50) {
            break;
        }
        if (signature != 0x04034B50) {
            NSLog(@"[AUR][Library] ZIP extraction failed: invalid local header signature at offset=%zu", offset);
            break;
        }

        const uint16_t flags = AURReadLE16(bytes + offset + 6);
        const uint16_t method = AURReadLE16(bytes + offset + 8);
        const uint32_t compressedSize = AURReadLE32(bytes + offset + 18);
        const uint32_t uncompressedSize = AURReadLE32(bytes + offset + 22);
        const uint16_t nameLen = AURReadLE16(bytes + offset + 26);
        const uint16_t extraLen = AURReadLE16(bytes + offset + 28);

        const size_t headerEnd = offset + 30;
        const size_t nameEnd = headerEnd + nameLen;
        const size_t dataStart = nameEnd + extraLen;
        if (dataStart > totalSize) {
            NSLog(@"[AUR][Library] ZIP extraction failed: malformed entry header at offset=%zu", offset);
            break;
        }

        NSString *entryName = [[NSString alloc] initWithBytes:(bytes + headerEnd) length:nameLen encoding:NSUTF8StringEncoding];
        if (entryName.length == 0) {
            entryName = [NSString stringWithFormat:@"entry_%zu", offset];
        }

        if ((flags & 0x0008) != 0) {
            NSLog(@"[AUR][Library] ZIP extraction failed: data-descriptor ZIP entries are not supported (%@)", entryName);
            break;
        }

        if ((size_t)compressedSize > (totalSize - dataStart)) {
            NSLog(@"[AUR][Library] ZIP extraction failed: compressed size overflow for %@", entryName);
            break;
        }

        const BOOL isDirectory = [entryName hasSuffix:@"/"];
        const uint8_t *payloadPtr = bytes + dataStart;

        if (isDirectory) {
            offset = dataStart + compressedSize;
            continue;
        }

        NSString *entryExt = entryName.pathExtension.lowercaseString;
        EmulatorCoreType type = EMULATOR_CORE_TYPE_3DS;
        if (!AURCoreTypeForExtension(entryExt, &type)) {
            offset = dataStart + compressedSize;
            continue;
        }

        NSData *decoded = nil;
        if (method == 0) {
            decoded = [NSData dataWithBytes:payloadPtr length:compressedSize];
        } else if (method == 8) {
            decoded = AURInflateRawDeflateWithCompressionFramework(payloadPtr, compressedSize);
        } else {
            NSLog(@"[AUR][Library] ZIP extraction failed: unsupported compression method=%u for %@", (unsigned)method, entryName);
            offset = dataStart + compressedSize;
            continue;
        }

        if (!decoded || decoded.length == 0) {
            NSLog(@"[AUR][Library] ZIP extraction failed: decode error for %@ (method=%u, csize=%u, usize=%u)",
                  entryName, (unsigned)method, (unsigned)compressedSize, (unsigned)uncompressedSize);
            offset = dataStart + compressedSize;
            continue;
        }

        NSString *destinationPath = [importDir stringByAppendingPathComponent:entryName.lastPathComponent];
        if (![decoded writeToFile:destinationPath atomically:YES]) {
            NSLog(@"[AUR][Library] ZIP extraction failed: cannot write %@", destinationPath);
            offset = dataStart + compressedSize;
            continue;
        }

        *outCoreType = type;
        *outURL = [NSURL fileURLWithPath:destinationPath];
        *outTitle = entryName.lastPathComponent.stringByDeletingPathExtension;
        success = YES;
        NSLog(@"[AUR][Library] ZIP extraction succeeded via Compression.framework. Selected ROM: %@", entryName);
        break;
    }

    if (!success) {
        NSLog(@"[AUR][Library] ZIP extraction failed: no supported ROM found in archive %@", zipURL.lastPathComponent);
    }
    return success;
}

#pragma mark - UICollectionViewDelegate

- (UIContextMenuConfiguration *)collectionView:(UICollectionView *)collectionView contextMenuConfigurationForItemAtIndexPath:(NSIndexPath *)indexPath point:(CGPoint)point {
    AURGame *game = self.games[indexPath.item];
    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:^UIMenu * _Nullable(NSArray<UIMenuElement *> * _Nonnull suggestedActions) {
        UIAction *play = [UIAction actionWithTitle:@"Play" image:[UIImage systemImageNamed:@"play.fill"] identifier:nil handler:^(__kindof UIAction * _Nonnull action) {
            [self collectionView:collectionView didSelectItemAtIndexPath:indexPath];
        }];
        UIAction *rename = [UIAction actionWithTitle:@"Rename" image:[UIImage systemImageNamed:@"pencil"] identifier:nil handler:^(__kindof UIAction * _Nonnull action) {}];
        UIAction *deleteAction = [UIAction actionWithTitle:@"Delete"
                                             image:[UIImage systemImageNamed:@"trash"]
                                        identifier:nil
                                           handler:^(__kindof UIAction * _Nonnull action) {
    [[AURDatabaseManager sharedManager] removeGame:game removeROMFile:YES];
    [self reloadData];
}];

deleteAction.attributes = UIMenuElementAttributesDestructive;

return [UIMenu menuWithTitle:game.title children:@[play, rename, deleteAction]];
    }];
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    if (self.presentedViewController != nil) {
        return;
    }

    AURGame *game = self.games[indexPath.item];
    if (![[NSFileManager defaultManager] fileExistsAtPath:game.romPath]) {
        [[AURDatabaseManager sharedManager] removeGame:game removeROMFile:NO];
        [self reloadData];
        return;
    }

    NSURL *romURL = [NSURL fileURLWithPath:game.romPath];
    AUREmulatorViewController *emuVC = [[AUREmulatorViewController alloc] initWithROMURL:romURL coreType:game.coreType];
    emuVC.modalPresentationStyle = UIModalPresentationFullScreen;
    [self presentViewController:emuVC animated:YES completion:nil];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    return self.games.count;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    AURGameCollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"GameCell" forIndexPath:indexPath];
    AURGame *game = self.games[indexPath.item];
    cell.titleLabel.text = game.title;

    cell.imageView.image = nil;
    [[AURBoxArtManager sharedManager] fetchBoxArtForGameTitle:game.title completion:^(UIImage *image) {
        if (image) {
            cell.imageView.image = image;
        } else {
            // Fallback image or color
            cell.imageView.backgroundColor = [UIColor colorWithWhite:0.2 alpha:1.0];
        }
    }];

    return cell;
}

@end
