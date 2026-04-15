#import "AURInGameMenuViewController.h"

@interface AURInGameMenuViewController ()
@property (nonatomic, strong) UIVisualEffectView *blurView;
@property (nonatomic, strong) UIStackView *stackView;
@end

@implementation AURInGameMenuViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor clearColor];

    self.blurView = [[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleDark]];
    self.blurView.frame = self.view.bounds;
    self.blurView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:self.blurView];

    self.stackView = [[UIStackView alloc] init];
    self.stackView.axis = UILayoutConstraintAxisVertical;
    self.stackView.distribution = UIStackViewDistributionFillEqually;
    self.stackView.spacing = 20;
    [self.view addSubview:self.stackView];

    self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
        [self.stackView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
        [self.stackView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
        [self.stackView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor multiplier:0.8]
    ]];

    [self addMenuButton:@"Save State" icon:@"square.and.arrow.down"];
    [self addMenuButton:@"Load State" icon:@"square.and.arrow.up"];
    [self addMenuButton:@"Cheat Codes" icon:@"text.badge.plus"];
    [self addMenuButton:@"Fast Forward" icon:@"forward.fill"];
    [self addMenuButton:@"Quit Game" icon:@"xmark.circle.fill" isDestructive:YES];
}

- (void)addMenuButton:(NSString *)title icon:(NSString *)iconName {
    [self addMenuButton:title icon:iconName isDestructive:NO];
}

- (void)addMenuButton:(NSString *)title icon:(NSString *)iconName isDestructive:(BOOL)isDestructive {
    UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setTitle:title forState:UIControlStateNormal];
    [button setImage:[UIImage systemImageNamed:iconName] forState:UIControlStateNormal];
    button.tintColor = isDestructive ? [UIColor systemRedColor] : [UIColor whiteColor];
    button.backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.1];
    button.layer.cornerRadius = 12;
    button.titleLabel.font = [UIFont systemFontOfSize:18 weight:UIFontWeightBold];
    if (@available(iOS 15.0, *)) {
        UIButtonConfiguration *config = button.configuration ?: [UIButtonConfiguration plainButtonConfiguration];
        config.contentInsets = NSDirectionalEdgeInsetsMake(15, 20, 15, 20);
        config.imagePadding = 10.0;
        button.configuration = config;
    } else {
        button.contentEdgeInsets = UIEdgeInsetsMake(15, 20, 15, 20);
        button.imageEdgeInsets = UIEdgeInsetsMake(0, 0, 0, 10);
    }

    [button addTarget:self action:@selector(buttonTapped:) forControlEvents:UIControlEventTouchUpInside];
    [self.stackView addArrangedSubview:button];
}

- (void)buttonTapped:(UIButton *)sender {
    [self dismissViewControllerAnimated:YES completion:^{
        [self.delegate inGameMenuDidSelectAction:sender.currentTitle];
    }];
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self dismissViewControllerAnimated:YES completion:nil];
}

@end
