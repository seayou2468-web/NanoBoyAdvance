#import "../Views/AURBackgroundView.h"
#import "AURLibraryViewController.h"
#import "AURSettingsViewController.h"
#import "AUREmulatorViewController.h"
#import "../Views/AURGameCollectionViewCell.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURBoxArtManager.h"
#import "../Models/AURGame.h"
#import "../External/miniz/miniz.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

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
                game.title = extractedTitle ?: [[extractedURL lastPathComponent] stringByDeletingPathExtension];
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
        return NO;
    }

    *outURL = nil;
    *outTitle = nil;

    NSData *zipData = [NSData dataWithContentsOfFile:zipURL.path];
    if (!zipData || zipData.length == 0) {
        return NO;
    }

    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_reader_init_mem(&archive, zipData.bytes, zipData.length, 0)) {
        return NO;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *importDir = [[documents stringByAppendingPathComponent:@"ROMs/Imported"] stringByAppendingPathComponent:zipURL.lastPathComponent.stringByDeletingPathExtension];
    [fileManager createDirectoryAtPath:importDir withIntermediateDirectories:YES attributes:nil error:nil];

    BOOL success = NO;
    mz_uint fileCount = mz_zip_reader_get_num_files(&archive);
    for (mz_uint i = 0; i < fileCount; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&archive, i, &stat)) {
            continue;
        }
        if (mz_zip_reader_is_file_a_directory(&archive, i)) {
            continue;
        }

        NSString *entryName = [NSString stringWithUTF8String:stat.m_filename];
        if (entryName.length == 0) {
            continue;
        }

        NSString *entryFileName = entryName.lastPathComponent;
        NSString *entryExt = entryFileName.pathExtension.lowercaseString;
        EmulatorCoreType type = EMULATOR_CORE_TYPE_3DS;
        if (!AURCoreTypeForExtension(entryExt, &type)) {
            continue;
        }

        size_t extractedSize = 0;
        void *extractedData = mz_zip_reader_extract_to_heap(&archive, i, &extractedSize, 0);
        if (!extractedData || extractedSize == 0) {
            if (extractedData) {
                mz_free(extractedData);
            }
            continue;
        }

        NSString *safeName = entryFileName.length > 0 ? entryFileName : [NSString stringWithFormat:@"entry_%u.%@", i, entryExt];
        NSString *destinationPath = [importDir stringByAppendingPathComponent:safeName];
        NSData *payload = [NSData dataWithBytesNoCopy:extractedData length:extractedSize freeWhenDone:YES];
        if (![payload writeToFile:destinationPath atomically:YES]) {
            continue;
        }

        *outCoreType = type;
        *outURL = [NSURL fileURLWithPath:destinationPath];
        *outTitle = [safeName stringByDeletingPathExtension];
        success = YES;
        break;
    }

    mz_zip_reader_end(&archive);
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
