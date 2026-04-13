#import "AURControllerSkinsViewController.h"
#import "../Views/AURControllerSkinTableViewCell.h"
#import "../Managers/AURSkinManager.h"
#import "../Models/AURDeltaSkin.h"

@interface AURControllerSkinsViewController ()
@property (nonatomic, strong) NSArray<AURControllerSkin *> *skins;
@end

@implementation AURControllerSkinsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"Controller Skins";
    [self.tableView registerClass:[AURControllerSkinTableViewCell class] forCellReuseIdentifier:@"SkinCell"];
    [self updateDataSource];
}

- (void)updateDataSource {
    NSArray *allSkins = [[AURSkinManager sharedManager] allSkins];
    NSPredicate *predicate = [NSPredicate predicateWithBlock:^BOOL(AURControllerSkin *skin, NSDictionary *bindings) {
        if (![skin supportsTraits:self.traits]) {
            return NO;
        }
        if ([skin isKindOfClass:[AURDeltaSkin class]]) {
            AURDeltaSkin *deltaSkin = (AURDeltaSkin *)skin;
            return [deltaSkin supportsCoreType:self.coreType];
        }
        return YES;
    }];
    self.skins = [allSkins filteredArrayUsingPredicate:predicate];

    for (AURControllerSkin *skin in self.skins) {
        if ([skin isKindOfClass:[AURDeltaSkin class]]) {
            [(AURDeltaSkin *)skin applyLayoutForTraits:self.traits];
        }
    }
    [self.tableView reloadData];
}

#pragma mark - Table view data source

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return self.skins.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    AURControllerSkinTableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"SkinCell" forIndexPath:indexPath];
    AURControllerSkin *skin = self.skins[indexPath.row];
    cell.controllerSkinImageView.image = skin.backgroundImage;
    return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
    return 200;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    AURControllerSkin *skin = self.skins[indexPath.row];
    [self.delegate controllerSkinsViewController:self didChooseSkin:skin];
    [self.navigationController popViewControllerAnimated:YES];
}

@end
