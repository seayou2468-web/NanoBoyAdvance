#import "AURControllerSkinTableViewCell.h"

@implementation AURControllerSkinTableViewCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style reuseIdentifier:(NSString *)reuseIdentifier {
    self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
    if (self) {
        self.controllerSkinImageView = [[UIImageView alloc] init];
        self.controllerSkinImageView.contentMode = UIViewContentModeScaleAspectFit;
        self.controllerSkinImageView.translatesAutoresizingMaskIntoConstraints = NO;
        [self.contentView addSubview:self.controllerSkinImageView];

        self.activityIndicatorView = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
        self.activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
        self.activityIndicatorView.hidesWhenStopped = YES;
        [self.contentView addSubview:self.activityIndicatorView];

        [NSLayoutConstraint activateConstraints:@[
            [self.controllerSkinImageView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor constant:10],
            [self.controllerSkinImageView.bottomAnchor constraintEqualToAnchor:self.contentView.bottomAnchor constant:-10],
            [self.controllerSkinImageView.leadingAnchor constraintEqualToAnchor:self.contentView.leadingAnchor constant:20],
            [self.controllerSkinImageView.trailingAnchor constraintEqualToAnchor:self.contentView.trailingAnchor constant:-20],

            [self.activityIndicatorView.centerXAnchor constraintEqualToAnchor:self.contentView.centerXAnchor],
            [self.activityIndicatorView.centerYAnchor constraintEqualToAnchor:self.contentView.centerYAnchor]
        ]];
    }
    return self;
}

@end
