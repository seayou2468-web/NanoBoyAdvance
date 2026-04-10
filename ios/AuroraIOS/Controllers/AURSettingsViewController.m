#import "AURSettingsViewController.h"
#import "../Managers/AURDatabaseManager.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface AURSettingsViewController () <UITableViewDelegate, UITableViewDataSource, UIDocumentPickerDelegate>
@property (nonatomic, strong) UITableView *tableView;
@property (nonatomic, strong) NSArray *sections;
@property (nonatomic, assign) EmulatorCoreType pickingCoreType;
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
    NSString *gbaBios = [[AURDatabaseManager sharedManager] BIOSPathForCoreType:EMULATOR_CORE_TYPE_GBA].lastPathComponent ?: @"Required";
    NSString *gbBios = [[AURDatabaseManager sharedManager] BIOSPathForCoreType:EMULATOR_CORE_TYPE_GB].lastPathComponent ?: @"Optional";

    self.sections = @[
        @{@"title": @"User Interface", @"items": @[@"Appearance", @"App Icon"]},
        @{@"title": @"Core Settings (BIOS)", @"items": @[@"GBA BIOS", @"GB/GBC BIOS"], @"details": @[gbaBios, gbBios]},
        @{@"title": @"Controllers", @"items": @[@"Controller Skins", @"Game Controllers"]},
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
        if (indexPath.row == 0) self.pickingCoreType = EMULATOR_CORE_TYPE_GBA;
        else if (indexPath.row == 1) self.pickingCoreType = EMULATOR_CORE_TYPE_GB;

        UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
        picker.delegate = self;
        [self presentViewController:picker animated:YES completion:nil];
    }
}

#pragma mark - UIDocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController *)controller didPickDocumentsAtURLs:(NSArray<NSURL *> *)urls {
    NSURL *url = urls.firstObject;
    if (url) {
        [[AURDatabaseManager sharedManager] setBIOSPath:url.path forCoreType:self.pickingCoreType];
        [self updateSections];
        [self.tableView reloadData];
    }
}

@end
