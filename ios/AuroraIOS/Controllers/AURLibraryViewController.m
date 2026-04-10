#import "AURLibraryViewController.h"
#import "AURSettingsViewController.h"
#import "AUREmulatorViewController.h"
#import "../Views/AURGameCollectionViewCell.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURBoxArtManager.h"
#import "../Models/AURGame.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface AURLibraryViewController () <UICollectionViewDelegate, UICollectionViewDataSource, UIDocumentPickerDelegate>
@property (nonatomic, strong) UICollectionView *collectionView;
@property (nonatomic, strong) UISegmentedControl *segmentedControl;
@property (nonatomic, strong) NSArray<AURGame *> *games;
@end

@implementation AURLibraryViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.06 blue:0.09 alpha:1.0];
    self.title = @"Library";

    // Setup Segmented Control for Systems
    self.segmentedControl = [[UISegmentedControl alloc] initWithItems:@[@"GBA", @"GBC", @"GB", @"NES"]];
    self.segmentedControl.selectedSegmentIndex = 0;
    [self.segmentedControl addTarget:self action:@selector(segmentChanged:) forControlEvents:UIControlEventValueChanged];
    self.navigationItem.titleView = self.segmentedControl;

    // Appearance customization for segmented control (Delta-like)
    NSDictionary *normalAttributes = @{NSForegroundColorAttributeName: [UIColor lightGrayColor]};
    NSDictionary *selectedAttributes = @{NSForegroundColorAttributeName: [UIColor whiteColor]};
    [self.segmentedControl setTitleTextAttributes:normalAttributes forState:UIControlStateNormal];
    [self.segmentedControl setTitleTextAttributes:selectedAttributes forState:UIControlStateSelected];

    // Settings Button
    self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc] initWithImage:[UIImage systemImageNamed:@"gear"] style:UIBarButtonItemStylePlain target:self action:@selector(settingsTapped)];
    self.navigationItem.leftBarButtonItem.tintColor = [UIColor systemPinkColor];

    // Add Button
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(addTapped)];
    self.navigationItem.rightBarButtonItem.tintColor = [UIColor systemPinkColor];

    // Setup Collection View
    CGFloat width = (self.view.bounds.size.width - 60) / 3;
    UICollectionViewFlowLayout *layout = [[UICollectionViewFlowLayout alloc] init];
    layout.itemSize = CGSizeMake(width, width * 1.5);
    layout.sectionInset = UIEdgeInsetsMake(20, 20, 20, 20);
    layout.minimumInteritemSpacing = 10;
    layout.minimumLineSpacing = 20;

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

- (void)segmentChanged:(UISegmentedControl *)sender {
    [self reloadData];
}

- (void)reloadData {
    EmulatorCoreType type;
    switch (self.segmentedControl.selectedSegmentIndex) {
        case 0: type = EMULATOR_CORE_TYPE_GBA; break;
        case 1: type = EMULATOR_CORE_TYPE_GB; break;
        case 2: type = EMULATOR_CORE_TYPE_GB; break; // Actually GB
        case 3: type = EMULATOR_CORE_TYPE_NES; break;
        default: type = EMULATOR_CORE_TYPE_GBA; break;
    }
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
        game.title = [[url lastPathComponent] stringByDeletingPathExtension];
        game.romPath = [url path];

        NSString *ext = [[url pathExtension] lowercaseString];
        if ([ext isEqualToString:@"gba"]) game.coreType = EMULATOR_CORE_TYPE_GBA;
        else if ([ext isEqualToString:@"nes"]) game.coreType = EMULATOR_CORE_TYPE_NES;
        else game.coreType = EMULATOR_CORE_TYPE_GB;

        [[AURDatabaseManager sharedManager] addGame:game];
        [self reloadData];
    }
}

#pragma mark - UICollectionViewDelegate

- (UIContextMenuConfiguration *)collectionView:(UICollectionView *)collectionView contextMenuConfigurationForItemAtIndexPath:(NSIndexPath *)indexPath point:(CGPoint)point {
    AURGame *game = self.games[indexPath.item];
    return [UIContextMenuConfiguration configurationWithIdentifier:nil previewProvider:nil actionProvider:^UIMenu * _Nullable(NSArray<UIMenuElement *> * _Nonnull suggestedActions) {
        UIAction *play = [UIAction actionWithTitle:@"Play" image:[UIImage systemImageNamed:@"play.fill"] identifier:nil handler:^(__kindof UIAction * _Nonnull action) {
            [self collectionView:collectionView didSelectItemAtIndexPath:indexPath];
        }];
        UIAction *rename = [UIAction actionWithTitle:@"Rename" image:[UIImage systemImageNamed:@"pencil"] identifier:nil handler:^(__kindof UIAction * _Nonnull action) {}];
        UIAction *delete = [UIAction actionWithTitle:@"Delete" image:[UIImage systemImageNamed:@"trash"] identifier:nil handler:^(__kindof UIAction * _Nonnull action) {}];
        delete.attributes = UIMenuElementAttributesDestructive;

        return [UIMenu menuWithTitle:game.title children:@[play, rename, delete]];
    }];
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    AURGame *game = self.games[indexPath.item];
    AUREmulatorViewController *emuVC = [[AUREmulatorViewController alloc] initWithROMURL:[NSURL fileURLWithPath:game.romPath] coreType:game.coreType];
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
