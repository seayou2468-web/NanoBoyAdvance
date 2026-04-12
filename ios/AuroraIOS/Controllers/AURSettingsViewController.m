#import "AURSettingsViewController.h"
#import "AURPreferredControllerSkinsViewController.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURSkinManager.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface AURSettingsViewController () <UITableViewDelegate, UITableViewDataSource, UIDocumentPickerDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) NSArray *sections;
@property (nonatomic, assign) EmulatorCoreType pickingCoreType;
@property (nonatomic, copy) NSString *pickingBIOSIdentifier;
@end

@implementation AURSettingsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Settings";
    self.view.backgroundColor = [UIColor systemGroupedBackgroundColor];

    [self updateSections];

    self.tableView = [[UITableView alloc] initWithFrame:self.view.bounds style:UITableViewStyleInsetGrouped];
    self.tableView.delegate = self;
    self.tableView.dataSource = self;
    [self.view addSubview:self.tableView];

    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(doneTapped)];
    self.navigationController.navigationBar.tintColor = [UIColor systemPinkColor];
}

- (void)updateSections {
    NSString *gbaBios = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"gba"].lastPathComponent ?: @"Required";
    NSString *gbBios = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"gb"].lastPathComponent ?: @"Optional";
    NSString *gbcBios = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"gbc"].lastPathComponent ?: @"Optional";
    NSString *ndsArm9Bios = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"nds_arm9"].lastPathComponent ?: @"Required (4KB)";
    NSString *ndsArm7Bios = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"nds_arm7"].lastPathComponent ?: @"Required (16KB)";
    NSString *ndsFirmware = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"nds_firmware"].lastPathComponent ?: @"Optional";

    self.sections = @[
        @{@"title": @"User Interface", @"items": @[@"Appearance", @"App Icon"]},
        @{@"title": @"Core Settings (BIOS)",
          @"items": @[@"GBA BIOS", @"GB BIOS", @"GBC BIOS", @"NDS ARM9 BIOS", @"NDS ARM7 BIOS", @"NDS Firmware"],
          @"details": @[gbaBios, gbBios, gbcBios, ndsArm9Bios, ndsArm7Bios, ndsFirmware]},
        @{@"title": @"Controllers", @"items": @[@"Preferred Skins", @"Import Skin"]},
        @{@"title": @"About", @"items": @[@"Version 1.0"]}
    ];
},
        @{@"title": @"Core Settings (BIOS)", @"items": @[@"GBA BIOS", @"GB/GBC BIOS", @"NDS ARM9 BIOS", @"NDS ARM7 BIOS", @"NDS Firmware"], @"details": @[gbaBios, gbBios, ndsArm9Bios, ndsArm7Bios, ndsFirmware]},
        @{@"title": @"Controllers", @"items": @[@"Preferred Skins", @"Import Skin"]},
        @{@"title": @"About", @"items": @[@"Version 1.0"]}
    ];
}

- (void)doneTapped {
    [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITableViewDataSource

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return self.sections.count;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return [self.sections[section][@"items"] count];
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return self.sections[section][@"title"];
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
    if (!cell) {
        cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"Cell"];
    }
    cell.textLabel.text = self.sections[indexPath.section][@"items"][indexPath.row];

    NSArray *details = self.sections[indexPath.section][@"details"];
    if (details) {
        cell.detailTextLabel.text = details[indexPath.row];
    } else {
        cell.detailTextLabel.text = @"";
    }

    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];

        if (indexPath.section == 1) {
        NSArray *ids = @[@"gba", @"gb", @"gbc", @"nds_arm9", @"nds_arm7", @"nds_firmware"];
        self.pickingBIOSIdentifier = ids[indexPath.row];

        UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
        picker.delegate = self;
        [self presentViewController:picker animated:YES completion:nil];
    } else if (indexPath.row == 1) {
            self.pickingCoreType = EMULATOR_CORE_TYPE_GB;
            self.pickingBIOSIdentifier = @"gb";
        } else if (indexPath.row == 2) {
            self.pickingCoreType = EMULATOR_CORE_TYPE_NDS;
            self.pickingBIOSIdentifier = @"nds_arm9";
        } else if (indexPath.row == 3) {
            self.pickingCoreType = EMULATOR_CORE_TYPE_NDS;
            self.pickingBIOSIdentifier = @"nds_arm7";
        } else if (indexPath.row == 4) {
            self.pickingCoreType = EMULATOR_CORE_TYPE_NDS;
            self.pickingBIOSIdentifier = @"nds_firmware";
        }

        UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
        picker.delegate = self;
        [self presentViewController:picker animated:YES completion:nil];
    }
    else if (indexPath.section == 2) {
        if (indexPath.row == 0) {
            AURPreferredControllerSkinsViewController *vc = [[AURPreferredControllerSkinsViewController alloc] init];
            vc.coreType = EMULATOR_CORE_TYPE_GBA;
            [self.navigationController pushViewController:vc animated:YES];
        } else if (indexPath.row == 1) {
            UTType *deltaSkinType = [UTType typeWithFilenameExtension:@"deltaskin"];
            UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[deltaSkinType ?: UTTypeData] asCopy:YES];
            picker.delegate = self;
            picker.view.tag = 1001;
            [self presentViewController:picker animated:YES completion:nil];
        }
    }
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (url) {
        if (controller.view.tag == 1001) {
            [[AURSkinManager sharedManager] importSkinAtURL:url completion:^(BOOL success) {
                if (success) {
                    [self updateSections];
                    [self.tableView reloadData];
                }
            }];
        } else {
            if (self.pickingBIOSIdentifier.length > 0) {
                [[AURDatabaseManager sharedManager] setBIOSURL:url forIdentifier:self.pickingBIOSIdentifier];
            } else {
                [[AURDatabaseManager sharedManager] setBIOSURL:url forCoreType:self.pickingCoreType];
            }
            [self updateSections];
            [self.tableView reloadData];
        }
    }
}

@end
