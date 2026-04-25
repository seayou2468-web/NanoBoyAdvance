#import "AURSettingsViewController.h"
#import "AURPreferredControllerSkinsViewController.h"
#import "../Managers/AURDatabaseManager.h"
#import "../Managers/AURSkinManager.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface AURSettingsViewController () <UITableViewDelegate, UITableViewDataSource, UIDocumentPickerDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) NSArray *sections;
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
}

- (void)updateSections {
    NSString *threeDsBoot9 = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_boot9"].lastPathComponent ?: @"Optional";
    NSString *threeDsBoot11 = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_boot11"].lastPathComponent ?: @"Optional";
    NSString *threeDsFirmware = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_firmware"].lastPathComponent ?: @"Optional";
    NSString *threeDsSharedFont = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_shared_font"].lastPathComponent ?: @"Optional";
    NSString *threeDsAESKeys = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_aes_keys"].lastPathComponent ?: @"Optional";
    NSString *threeDsSeedDB = [[AURDatabaseManager sharedManager] BIOSPathForIdentifier:@"3ds_seeddb"].lastPathComponent ?: @"Optional";

    self.sections = @[
        @{@"title": @"User Interface", @"items": @[@"Appearance", @"App Icon"]},
        @{@"title": @"Core Settings (BIOS)",
          @"items": @[@"3DS Boot9", @"3DS Boot11", @"3DS Firmware", @"3DS Shared Font (.bin)", @"3DS AES Keys (.txt)", @"3DS SeedDB (.bin)"],
          @"details": @[threeDsBoot9, threeDsBoot11, threeDsFirmware, threeDsSharedFont, threeDsAESKeys, threeDsSeedDB]},
        @{@"title": @"Controllers", @"items": @[@"Preferred Skins", @"Import Skin"]},
        @{@"title": @"About", @"items": @[@"Version 1.0"]}
    ];
}

- (void)doneTapped {
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return self.sections.count; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { return [self.sections[section][@"items"] count]; }
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section { return self.sections[section][@"title"]; }

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
    if (!cell) cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleValue1 reuseIdentifier:@"Cell"];
    cell.textLabel.text = self.sections[indexPath.section][@"items"][indexPath.row];
    NSArray *details = self.sections[indexPath.section][@"details"];
    if (details && indexPath.row < details.count) cell.detailTextLabel.text = details[indexPath.row];
    else cell.detailTextLabel.text = @"";
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
    return cell;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    [tableView deselectRowAtIndexPath:indexPath animated:YES];
    if (indexPath.section == 1) {
        NSArray *ids = @[@"3ds_boot9", @"3ds_boot11", @"3ds_firmware", @"3ds_shared_font", @"3ds_aes_keys", @"3ds_seeddb"];
        if (indexPath.row < ids.count) {
            self.pickingBIOSIdentifier = ids[indexPath.row];
            UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
            picker.delegate = self;
            [self presentViewController:picker animated:YES completion:nil];
        }
    } else if (indexPath.section == 2) {
        if (indexPath.row == 0) {
            AURPreferredControllerSkinsViewController *vc = [[AURPreferredControllerSkinsViewController alloc] init];
            vc.coreType = EMULATOR_CORE_TYPE_3DS;
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

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (url) {
        if (controller.view.tag == 1001) {
            [[AURSkinManager sharedManager] importSkinAtURL:url completion:^(BOOL success) {
                if (success) {
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self updateSections];
                        [self.tableView reloadData];
                    });
                }
            }];
        } else {
            if (self.pickingBIOSIdentifier) {
                [[AURDatabaseManager sharedManager] setBIOSURL:url forIdentifier:self.pickingBIOSIdentifier];
                [self updateSections];
                [self.tableView reloadData];
            }
        }
    }
}
@end
