#import "AURPreferredControllerSkinsViewController.h"
#import "AURControllerSkinsViewController.h"
#import "../Managers/AURSkinManager.h"
#import "../Views/AURControllerSkinTableViewCell.h"

@interface AURPreferredControllerSkinsViewController () <AURControllerSkinsViewControllerDelegate>
@property (nonatomic, strong) AURControllerSkin *portraitSkin;
@property (nonatomic, strong) AURControllerSkin *landscapeSkin;
@end

@implementation AURPreferredControllerSkinsViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = self.game.title ?: @"System Skins";
    [self.tableView registerClass:[AURControllerSkinTableViewCell class] forCellReuseIdentifier:@"SkinCell"];
    [self updateSkins];
}

- (void)updateSkins {
    self.portraitSkin = [[AURSkinManager sharedManager] skinForCoreType:self.coreType isLandscape:NO];
    self.landscapeSkin = [[AURSkinManager sharedManager] skinForCoreType:self.coreType isLandscape:YES];
    [self.tableView reloadData];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 2;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return 1;
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return (section == 0) ? @"Portrait" : @"Landscape";
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    AURControllerSkinTableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"SkinCell" forIndexPath:indexPath];
    AURControllerSkin *skin = (indexPath.section == 0) ? self.portraitSkin : self.landscapeSkin;
    cell.controllerSkinImageView.image = skin.backgroundImage;
    return cell;
}

- (CGFloat)tableView:(UITableView *)tableView heightForRowAtIndexPath:(NSIndexPath *)indexPath {
    return 150;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    AURControllerSkinsViewController *vc = [[AURControllerSkinsViewController alloc] init];
    vc.delegate = self;
    vc.coreType = self.coreType;
    AURControllerSkinTraits *traits = [AURControllerSkinTraits defaultTraitsForWindow:self.view.window];
    traits.orientation = (indexPath.section == 0) ? AURControllerSkinOrientationPortrait : AURControllerSkinOrientationLandscape;
    vc.traits = traits;
    [self.navigationController pushViewController:vc animated:YES];
}

#pragma mark - AURControllerSkinsViewControllerDelegate

- (void)controllerSkinsViewController:(AURControllerSkinsViewController *)controller didChooseSkin:(AURControllerSkin *)skin {
    // Save preference and update
    [self updateSkins];
}

@end
